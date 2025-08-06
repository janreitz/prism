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

ASTMatcherCallback::ASTMatcherCallback(ASTAnalysis &result)
    : analysis_result_(result)
{
}

void ASTMatcherCallback::run(const MatchFinder::MatchResult &Result)
{
    if (const auto *func_decl =
            Result.Nodes.getNodeAs<FunctionDecl>("function")) {
        analysis_result_.add_decl(func_decl, *Result.Context);
    }
}

ASTAnalysis
analyze_with_matcher(clang::ASTContext &ctx,
                     const clang::ast_matchers::DeclarationMatcher &matcher,
                     const std::string &filename)
{
    ASTAnalysis result;

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
