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
#include "llvm/Support/raw_ostream.h"
#include <iostream>

using namespace clang;
using namespace clang::ast_matchers;

ASTMatcherCallback::ASTMatcherCallback(ASTAnalysisResult &result)
    : result_(result), current_matcher_name_("unknown")
{
}

void ASTMatcherCallback::run(const MatchFinder::MatchResult &Result)
{
    ASTContext *context = Result.Context;

    // Ensure we have a root node
    if (!result_.root) {
        result_.root = std::make_unique<ASTNode>(nullptr); // nullptr represents TranslationUnit
        decl_to_node_[nullptr] = result_.root.get(); // Map translation unit
    }

    const Decl *matched_decl = nullptr;

    // Try to match different node types
    if (const auto *func_decl =
            Result.Nodes.getNodeAs<FunctionDecl>("function")) {
        matched_decl = func_decl;
        result_.functions_found++;
    } else if (const auto *class_decl =
                   Result.Nodes.getNodeAs<CXXRecordDecl>("class")) {
        matched_decl = class_decl;
        result_.classes_found++;
    } else if (const auto *var_decl =
                   Result.Nodes.getNodeAs<VarDecl>("variable")) {
        matched_decl = var_decl;
    }

    if (matched_decl) {
        // Create the matched node using the global function
        auto node = create_node_from_decl(matched_decl, context);
        if (node) {
            result_.root->add_child(std::move(node));
        }
        result_.nodes_processed++;
    }
}

ASTAnalysisResult analyze_with_matcher(const std::string &source_code,
                                       const std::string &matcher_expression,
                                       const std::string &filename)
{
    ASTAnalysisResult result;

    try {
        // Create ASTUnit to keep the AST alive
        std::vector<std::string> args = {"-std=c++17"};
        
        // Parse source code into AST using ASTUnit - this keeps the AST alive
        result.ast_unit = clang::tooling::buildASTFromCodeWithArgs(
            source_code, args, filename);
            
        if (!result.ast_unit) {
            result.errors.push_back(
                ASTAnalysisError{"Failed to parse source code", filename});
            return result;
        }

        // Get the ASTContext from the unit
        ASTContext &context = result.ast_unit->getASTContext();
        
        // Create a MatchFinder and callback
        clang::ast_matchers::MatchFinder finder;
        ASTMatcherCallback callback(result);
        callback.setMatcherName(matcher_expression);

        // Parse the matcher expression and create appropriate matchers
        // For now, handle common cases explicitly
        if (matcher_expression == "functionDecl()") {
            finder.addMatcher(functionDecl().bind("function"), &callback);
        } else if (matcher_expression == "cxxRecordDecl()") {
            finder.addMatcher(cxxRecordDecl().bind("class"), &callback);
        } else if (matcher_expression == "cxxMethodDecl(isPublic())") {
            finder.addMatcher(cxxMethodDecl(isPublic()).bind("function"),
                              &callback);
        } else if (matcher_expression ==
                   "functionDecl(hasBody(compoundStmt()))") {
            finder.addMatcher(
                functionDecl(hasBody(compoundStmt())).bind("function"),
                &callback);
        } else if (matcher_expression == "cxxConstructorDecl()") {
            finder.addMatcher(cxxConstructorDecl().bind("function"), &callback);
        } else if (matcher_expression == "cxxMethodDecl(isVirtual())") {
            finder.addMatcher(cxxMethodDecl(isVirtual()).bind("function"),
                              &callback);
        } else if (matcher_expression == "varDecl()") {
            finder.addMatcher(varDecl().bind("variable"), &callback);
        } else {
            // Default fallback - match all function declarations
            finder.addMatcher(functionDecl().bind("function"), &callback);
        }

        // Run the matcher on the AST
        finder.matchAST(context);
        
        // Ensure we have a root node
        if (!result.root) {
            result.root = std::make_unique<ASTNode>(nullptr);
        }

    } catch (const std::exception &e) {
        result.errors.push_back(ASTAnalysisError{e.what(), filename});
    }

    return result;
}

bool validate_matcher_expression(const std::string &matcher_expression)
{
    // For now, just check against known valid expressions
    static const std::vector<std::string> valid_expressions = {
        "functionDecl()",
        "cxxRecordDecl()",
        "cxxMethodDecl(isPublic())",
        "functionDecl(hasBody(compoundStmt()))",
        "cxxConstructorDecl()",
        "cxxMethodDecl(isVirtual())",
        "varDecl()",
        "forStmt()",
        "ifStmt()",
        "forStmt(hasDescendant(forStmt()))"};

    return std::find(valid_expressions.begin(), valid_expressions.end(),
                     matcher_expression) != valid_expressions.end();
}