#pragma once

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersInternal.h"
#include "clang/Frontend/ASTUnit.h"

#include <expected>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

class ASTAnalysis;

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

std::expected<clang::ast_matchers::internal::DynTypedMatcher, std::string>
parse_matcher_expression(const std::string &matcher_expression);

// Main ASTMatcher analysis function
void analyze_with_matcher(
    ASTAnalysis &result, std::unique_ptr<clang::ASTUnit> &unit,
    const std::variant<clang::ast_matchers::DeclarationMatcher,
                       clang::ast_matchers::internal::DynTypedMatcher
                       // clang::ast_matchers::StatementMatcher,
                       // clang::ast_matchers::TypeMatcher,
                       > &matcher,
    const std::string &filename = "source.cpp");