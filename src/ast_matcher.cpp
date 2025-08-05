#include "ast_matcher.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Stmt.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;
using namespace clang::ast_matchers;

ASTMatcherCallback::ASTMatcherCallback(ASTAnalysisResult &result)
    : analysis_result_(result)
{
}

void ASTMatcherCallback::run(const MatchFinder::MatchResult &Result)
{
    ASTContext *ctx = Result.Context;
    const Decl *matched_decl = nullptr;

    // Try to match different node types
    if (const auto *func_decl =
            Result.Nodes.getNodeAs<FunctionDecl>("function")) {
        // TODO Skip template definitions, only show instantiations
        matched_decl = func_decl;
        analysis_result_.functions_found++;

        const auto func_metrics = compute_function_metrics(func_decl, *ctx);
        analysis_result_.max_complexity =
            std::max(analysis_result_.max_complexity,
                     func_metrics.cyclomatic_complexity);
        analysis_result_.min_complexity =
            std::min(analysis_result_.min_complexity,
                     func_metrics.cyclomatic_complexity);

    } else if (const auto *class_decl =
                   Result.Nodes.getNodeAs<CXXRecordDecl>("class")) {
        matched_decl = class_decl;
        analysis_result_.classes_found++;
    } else if (const auto *ns_decl =
                   Result.Nodes.getNodeAs<NamespaceDecl>("namespace")) {
        matched_decl = ns_decl;
    }

    if (!matched_decl) {
        return;
    }

    if (analysis_result_.decl_to_node_.find(matched_decl) !=
        analysis_result_.decl_to_node_.end()) {
        return;
    }
    // Track this node to avoid duplicates
    auto temp_node = std::make_unique<ASTNode>(matched_decl, ctx);
    analysis_result_.decl_to_node_[matched_decl] = temp_node.get();

    // Find or create the proper parent in the hierarchy
    ASTNode *parent = analysis_result_.find_or_create_parent(matched_decl, ctx);
    parent->add_child(std::move(temp_node));

    analysis_result_.nodes_processed++;
}

ASTAnalysisResult
analyze_with_matcher(clang::ASTContext &ctx,
                     const clang::ast_matchers::DeclarationMatcher &matcher,
                     const std::string &filename)
{
    ASTAnalysisResult result(ctx);

    try {
        clang::ast_matchers::MatchFinder finder;
        ASTMatcherCallback callback(result);

        finder.addMatcher(matcher, &callback);

        finder.matchAST(ctx);
    } catch (const std::exception &e) {
        result.errors.push_back(ASTAnalysisError{e.what(), filename});
    }

    return result;
}
