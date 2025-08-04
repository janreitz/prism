#include "ast_metrics.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/ASTUnit.h>

using namespace clang;

size_t count_statements(const clang::Stmt *stmt)
{
    if (!stmt)
        return 0;

    size_t count = 1; // Count this statement

    // Recursively count child statements
    for (const auto *child : stmt->children()) {
        count += count_statements(child);
    }

    return count;
}

size_t count_decision_points(const clang::Stmt *stmt)
{
    if (!stmt)
        return 0;

    size_t count = 0;

    // Count decision points (if, while, for, switch, etc.)
    if (isa<IfStmt>(stmt) || isa<WhileStmt>(stmt) || isa<ForStmt>(stmt) ||
        isa<SwitchStmt>(stmt) || isa<ConditionalOperator>(stmt)) {
        count++;
    }

    // Recursively count in child statements
    for (const auto *child : stmt->children()) {
        count += count_decision_points(child);
    }

    return count;
}

FunctionMetrics compute_function_metrics(const clang::FunctionDecl *func_decl,
                                         clang::ASTContext &ctx)
{
    FunctionMetrics metrics;

    if (!func_decl) {
        return metrics; // Default values
    }

    // Parameter count
    metrics.parameter_count = func_decl->getNumParams();

    // Statement count and cyclomatic complexity
    if (func_decl->hasBody()) {
        const Stmt *body = func_decl->getBody();
        metrics.statement_count = count_statements(body);

        // Basic cyclomatic complexity (1 + decision points)
        metrics.cyclomatic_complexity = 1 + count_decision_points(body);
    } else {
        metrics.statement_count = 0;
        metrics.cyclomatic_complexity = 1;
    }

    return metrics;
}

ClassMetrics compute_class_metrics(const clang::CXXRecordDecl *class_decl,
                                   clang::ASTContext &ctx)
{
    ClassMetrics metrics;

    if (!class_decl) {
        return metrics; // Default values
    }

    // Count members and methods
    for (const auto *decl : class_decl->decls()) {
        if (isa<CXXMethodDecl>(decl)) {
            metrics.method_count++;
            if (decl->getAccess() == AS_public) {
                metrics.public_member_count++;
            } else if (decl->getAccess() == AS_private) {
                metrics.private_member_count++;
            }
        } else if (isa<FieldDecl>(decl)) {
            metrics.member_count++;
        }
    }

    return metrics;
}

NamespaceMetrics compute_namespace_metrics(const clang::Decl *decl,
                                           clang::ASTContext &ctx,
                                           size_t child_count)
{
    NamespaceMetrics metrics;
    metrics.child_count = child_count;
    return metrics;
}

// TODO refactor so unit is not needed and complexity metrics don't have to be
// recomputed each time
std::function<ImU32(const ASTNode &)>
create_complexity_coloring_strategy(const ASTAnalysisResult &analysis_result,
                                    clang::ASTUnit *unit)
{
    return [&analysis_result, unit](const ASTNode &node) -> ImU32 {
        float complexity = 1.0f;

        // Compute complexity on-demand using direct casting
        if (node.clang_decl()) {
            const clang::Decl *decl = node.clang_decl();
            clang::ASTContext &ctx = unit->getASTContext();
            if (const auto *func_decl =
                    clang::dyn_cast<clang::FunctionDecl>(decl)) {
                auto metrics = compute_function_metrics(func_decl, ctx);
                complexity = static_cast<float>(metrics.cyclomatic_complexity);
            }
        }

        float max_complexity =
            static_cast<float>(analysis_result.max_complexity);

        if (max_complexity == 0)
            return IM_COL32(128, 128, 128, 255);

        float ratio = complexity / max_complexity;

        // Green to red gradient based on complexity
        int red = static_cast<int>(255 * ratio);
        int green = static_cast<int>(255 * (1.0f - ratio));
        return IM_COL32(red, green, 0, 255);
    };
}

std::function<ImU32(const ASTNode &)> create_type_based_coloring_strategy()
{
    return [](const ASTNode &node) -> ImU32 {
        const auto *func_decl =
            clang::dyn_cast<clang::FunctionDecl>(node.clang_decl());

        if (!func_decl) {
            return IM_COL32(128, 128, 128, 255); // Gray
        }

        if (func_decl->isTemplated()) {
            // Primary template
            return IM_COL32(255, 150, 100, 255); // Orange
        }

        if (func_decl->isFunctionTemplateSpecialization()) {
            switch (func_decl->getTemplateSpecializationKind()) {
            case clang::TemplateSpecializationKind::TSK_ImplicitInstantiation:
                return IM_COL32(100, 150, 255, 255); // Blue
            case clang::TemplateSpecializationKind::
                TSK_ExplicitInstantiationDefinition:
                return IM_COL32(200, 100, 255, 255); // purple
            }
        }

        return IM_COL32(150, 255, 100, 255); // Green
    };
}
