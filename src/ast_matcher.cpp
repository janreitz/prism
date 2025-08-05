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
    ASTNode *parent = find_or_create_parent(matched_decl, ctx);
    parent->add_child(std::move(temp_node));

    analysis_result_.nodes_processed++;
}

ASTNode *ASTMatcherCallback::find_or_create_parent(const clang::Decl *decl,
                                                   clang::ASTContext *context)
{
    // Walk up the parent chain to find the proper hierarchical parent
    const clang::DeclContext *parent_context = decl->getDeclContext();

    while (parent_context) {
        // Convert DeclContext back to Decl if it represents a declaration
        // Direct cast to NamedDecl
        const auto *parent_decl = dyn_cast<clang::Decl>(parent_context);

        if (parent_decl) {
            // Check if we already have a node for this parent
            auto it = analysis_result_.decl_to_node_.find(parent_decl);
            if (it != analysis_result_.decl_to_node_.end()) {
                return it->second;
            }

            // Create parent node if it doesn't exist
            auto parent_node = create_node_from_decl(parent_decl, context);
            if (parent_node) {
                ASTNode *parent_ptr = parent_node.get();
                analysis_result_.decl_to_node_[parent_decl] = parent_ptr;

                // Recursively find the parent's parent
                ASTNode *grandparent =
                    find_or_create_parent(parent_decl, context);
                grandparent->add_child(std::move(parent_node));

                return parent_ptr;
            }
        }

        // Move up the hierarchy
        parent_context = parent_context->getParent();
    }

    // Fallback to root if no proper parent found
    return analysis_result_.root.get();
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
