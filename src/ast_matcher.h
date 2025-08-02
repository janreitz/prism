#pragma once

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

class ASTMatcherCallback
    : public clang::ast_matchers::MatchFinder::MatchCallback
{
  public:
    explicit ASTMatcherCallback(ASTAnalysisResult &result);

    // Called on every match by the MatchFinder.
    void
    run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override;

    void setMatcherName(const std::string &name)
    {
        current_matcher_name_ = name;
    }

  private:
    ASTAnalysisResult &result_;
    std::string current_matcher_name_;

    // Track seen nodes to avoid duplicates
    std::unordered_map<const clang::Decl *, ASTNode *> decl_to_node_;
};

// Main ASTMatcher analysis function
ASTAnalysisResult
analyze_with_matcher(const std::string &source_code,
                     const std::string &matcher_expression,
                     const std::string &filename = "source.cpp");

// Helper function to validate matcher expressions
bool validate_matcher_expression(const std::string &matcher_expression);