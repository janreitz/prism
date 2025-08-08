#include "ast_matcher.h"

#include "ast_analysis.h"

#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/ASTUnit.h"

using namespace clang;
using namespace clang::ast_matchers;

ASTMatcherCallback::ASTMatcherCallback(ASTAnalysis &result)
    : analysis_result_(result)
{
}

void ASTMatcherCallback::run(const MatchFinder::MatchResult &Result)
{
    if (const auto *func_decl =
            Result.Nodes.getNodeAs<FunctionDecl>("function")) {
        analysis_result_.add_decl(func_decl, Result.Context);
    }
}

void analyze_with_matcher(
    ASTAnalysis &result, std::unique_ptr<clang::ASTUnit> &unit,
    const clang::ast_matchers::DeclarationMatcher &matcher,
    const std::string &filename)
{
    if (result.tu_has_been_analyzed(unit.get())) {
        return;
    }
    try {
        result.add_analyzed_tu(unit.get());
        clang::ast_matchers::MatchFinder finder;
        ASTMatcherCallback callback(result);
        finder.addMatcher(matcher, &callback);
        finder.matchAST(unit->getASTContext());
    } catch (const std::exception &e) {
        result.errors.push_back(ASTAnalysisError{e.what(), filename});
    }
}
