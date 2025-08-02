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
        result_.root = std::make_unique<ASTNode>(
            nullptr, context); // nullptr represents TranslationUnit
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
        if (decl_to_node_.find(matched_decl) != decl_to_node_.end()) {
            return;
        }

        // Create a temporary node to check if it's a template instantiation
        auto temp_node = std::make_unique<ASTNode>(matched_decl, context);
        
        // Only keep template instantiations, skip definitions
        if (!temp_node->is_template_instantiation()) {
            return;
        }

        // Track this node to avoid duplicates
        ASTNode *node_ptr = temp_node.get();
        decl_to_node_[matched_decl] = node_ptr;

        // Find or create the proper parent in the hierarchy
        ASTNode *parent = find_or_create_parent(matched_decl, context);
        parent->add_child(std::move(temp_node));
        
        result_.nodes_processed++;
    }
}

ASTNode *ASTMatcherCallback::find_or_create_parent(const clang::Decl *decl,
                                                   clang::ASTContext *context)
{
    // Walk up the parent chain to find the proper hierarchical parent
    const clang::DeclContext *parent_context = decl->getDeclContext();

    while (parent_context) {
        // Convert DeclContext back to Decl if it represents a declaration
        const clang::Decl *parent_decl = nullptr;

        if (const auto *ns_decl =
                dyn_cast<clang::NamespaceDecl>(parent_context)) {
            parent_decl = ns_decl;
        } else if (const auto *class_decl =
                       dyn_cast<clang::CXXRecordDecl>(parent_context)) {
            parent_decl = class_decl;
        } else if (const auto *func_decl =
                       dyn_cast<clang::FunctionDecl>(parent_context)) {
            parent_decl = func_decl;
        } else if (isa<clang::TranslationUnitDecl>(parent_context)) {
            // Reached the translation unit - use root
            return result_.root.get();
        }

        if (parent_decl) {
            // Check if we already have a node for this parent
            auto it = decl_to_node_.find(parent_decl);
            if (it != decl_to_node_.end()) {
                return it->second;
            }

            // Create parent node if it doesn't exist
            auto parent_node = create_node_from_decl(parent_decl, context);
            if (parent_node) {
                ASTNode *parent_ptr = parent_node.get();
                decl_to_node_[parent_decl] = parent_ptr;

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
    return result_.root.get();
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
            result.root = std::make_unique<ASTNode>(nullptr, &context);
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