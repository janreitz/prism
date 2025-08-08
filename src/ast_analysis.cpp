#include "ast_analysis.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Stmt.h>
#include <clang/Frontend/ASTUnit.h>

using namespace clang;

ASTAnalysis::ASTAnalysis() : root(std::make_unique<ASTNode>(nullptr, nullptr))
{
}

void ASTAnalysis::add_decl(const clang::Decl *decl,
                           const clang::ASTContext *ctx)
{
    get_or_create_node(decl, ctx);
}

void ASTAnalysis::add_analyzed_tu(const clang::ASTUnit *tu)
{
    analyzed_units_.insert(tu);
}

bool ASTAnalysis::tu_has_been_analyzed(const clang::ASTUnit *tu) const
{
    return analyzed_units_.contains(tu);
}

ASTNode *ASTAnalysis::get_or_create_node(const clang::Decl *decl,
                                         const clang::ASTContext *ctx)
{
    if (decl == nullptr) {
        return root.get();
    }

    auto *named_decl = clang::dyn_cast<clang::NamedDecl>(decl);
    if (!named_decl) {
        // Skip over unnamed decls
        const clang::DeclContext *parent_context = decl->getDeclContext();
        assert(parent_context != nullptr ||
               dyn_cast<DeclContext>(decl)->isTranslationUnit() &&
                   "I assume only TUs have no DeclContext");
        return get_or_create_node(dyn_cast_or_null<clang::Decl>(parent_context),
                                  ctx);
    }

    // Avoid duplicates
    const auto qualified_name = named_decl->getQualifiedNameAsString();
    auto found_iter = qualified_name_to_nodes_.find(qualified_name);
    if (found_iter != qualified_name_to_nodes_.end()) {
        return found_iter->second;
    }

    auto temp_node = std::make_unique<ASTNode>(decl, ctx);
    auto temp_node_ptr = temp_node.get();
    qualified_name_to_nodes_[qualified_name] = temp_node_ptr;

    update_metrics(temp_node_ptr);

    // Walk up the parent chain to find the proper hierarchical parent
    const clang::DeclContext *parent_context = decl->getDeclContext();
    assert(parent_context != nullptr ||
           dyn_cast<DeclContext>(decl)->isTranslationUnit() &&
               "I assume only TUs have no DeclContext");
    auto *parent_node =
        get_or_create_node(dyn_cast_or_null<clang::Decl>(parent_context), ctx);
    parent_node->add_child(std::move(temp_node));

    return temp_node_ptr;
}

void ASTAnalysis::update_metrics(const ASTNode *node)
{
    const auto *decl = node->clang_decl();

    nodes_processed++;
    // Try to match different node types
    if (const auto *func_decl = dyn_cast<FunctionDecl>(decl)) {
        functions_found++;

        const auto func_metrics = compute_function_metrics(func_decl);
        max_complexity =
            std::max(max_complexity, func_metrics.cyclomatic_complexity);
        min_complexity =
            std::min(min_complexity, func_metrics.cyclomatic_complexity);
    }
}

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

FunctionMetrics compute_function_metrics(const clang::FunctionDecl *func_decl)
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
                                   const clang::ASTContext &ctx)
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
