#pragma once

#include "ast_analysis.h"
#include "ast_node.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

const std::vector<
    std::pair<std::string, clang::ast_matchers::DeclarationMatcher>>
    predefined_matchers{
        {"functionDecl()",
         clang::ast_matchers::functionDecl().bind("function")},
        {"functionDecl(isDefinition(),unless(isInStdNamespace()))",
         clang::ast_matchers::functionDecl(
             clang::ast_matchers::isDefinition(),
             clang::ast_matchers::unless(
                 clang::ast_matchers::isInStdNamespace()))
             .bind("function")},
        {"cxxMethodDecl(isPublic())",
         clang::ast_matchers::cxxMethodDecl(clang::ast_matchers::isPublic())
             .bind("function")},
        {"functionDecl(hasBody(compoundStmt()))",
         clang::ast_matchers::functionDecl(
             clang::ast_matchers::hasBody(clang::ast_matchers::compoundStmt()))
             .bind("function")},
        {"cxxConstructorDecl()",
         clang::ast_matchers::cxxConstructorDecl().bind("function")},
        {"cxxMethodDecl(isVirtual())",
         clang::ast_matchers::cxxMethodDecl(clang::ast_matchers::isVirtual())
             .bind("function")},
    };

class ASTMatcherCallback
    : public clang::ast_matchers::MatchFinder::MatchCallback
{
  public:
    explicit ASTMatcherCallback(ASTAnalysis &result);

    // Called on every match by the MatchFinder.
    void
    run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override;

  private:
    ASTAnalysis &analysis_result_;

    ASTNode *find_or_create_parent(const clang::Decl *decl,
                                   clang::ASTContext *context);
};

// Main ASTMatcher analysis function
void analyze_with_matcher(
    ASTAnalysis &result, std::unique_ptr<clang::ASTUnit> &unit,
    const clang::ast_matchers::DeclarationMatcher &matcher,
    const std::string &filename = "source.cpp");