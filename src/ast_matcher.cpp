#include "ast_matcher.h"

#include "ast_analysis.h"

#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersInternal.h"
#include "clang/Frontend/ASTUnit.h"

#include "clang/ASTMatchers/Dynamic/Diagnostics.h"
#include "clang/ASTMatchers/Dynamic/Parser.h"

#include <expected>
#include <string>

using namespace clang;
using namespace clang::ast_matchers;

std::expected<clang::ast_matchers::internal::DynTypedMatcher, std::string>
parse_matcher_expression(const std::string &matcher_expression)
{
    clang::StringRef ref(matcher_expression);
    clang::ast_matchers::dynamic::Diagnostics diagnostics;
    auto result = clang::ast_matchers::dynamic::Parser::parseMatcherExpression(
        ref, &diagnostics);

    if (result.has_value()) {
        return result.value();
    } else {
        return std::unexpected(diagnostics.toStringFull());
    }
}

class ASTMatcherCallback
    : public clang::ast_matchers::MatchFinder::MatchCallback
{
  public:
    explicit ASTMatcherCallback(ASTAnalysis &result) : analysis_result_(result)
    {
    }

    // Called on every match by the MatchFinder.
    void
    run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override
    {
        if (const auto *func_decl =
                Result.Nodes.getNodeAs<FunctionDecl>("function")) {
            analysis_result_.add_decl(func_decl, Result.Context);
        }
    }

  private:
    ASTAnalysis &analysis_result_;
};

void analyze_with_matcher(
    ASTAnalysis &result, std::unique_ptr<clang::ASTUnit> &unit,
    const std::variant<clang::ast_matchers::DeclarationMatcher,
                       clang::ast_matchers::internal::DynTypedMatcher
                       // clang::ast_matchers::StatementMatcher,
                       // clang::ast_matchers::TypeMatcher,
                       > &matcher,
    const std::string &filename)
{
    if (result.tu_has_been_analyzed(unit.get())) {
        return;
    }
    try {
        result.add_analyzed_tu(unit.get());
        clang::ast_matchers::MatchFinder finder;
        ASTMatcherCallback callback(result);
        std::visit(
            [&finder, &callback](const auto &m) {
                using MatcherType = std::decay_t<decltype(m)>;

                if constexpr (std::is_same_v<MatcherType,
                                             clang::ast_matchers::internal::
                                                 DynTypedMatcher>) {
                    finder.addDynamicMatcher(m, &callback);
                } else {
                    finder.addMatcher(m, &callback);
                }
            },
            matcher);

        finder.matchAST(unit->getASTContext());
    } catch (const std::exception &e) {
        result.errors.push_back(ASTAnalysisError{e.what(), filename});
    }
}
