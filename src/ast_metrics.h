#pragma once

#include "ast_node.h"

#include <algorithm>
#include <cstddef>
#include <variant>

// Forward declarations for Clang AST types
namespace clang
{
class Decl;
class FunctionDecl;
class CXXRecordDecl;
class VarDecl;
class Stmt;
class IfStmt;
class WhileStmt;
class ForStmt;
class SwitchStmt;
class ConditionalOperator;
class CXXMethodDecl;
class FieldDecl;
} // namespace clang

struct ASTAnalysisResult {
    ASTAnalysisResult() = default;
    ASTAnalysisResult(clang::ASTContext &ctx);
    std::unique_ptr<ASTNode> root;
    std::vector<ASTAnalysisError> errors;

    // Track seen nodes to avoid duplicates
    std::unordered_map<const clang::Decl *, ASTNode *> decl_to_node_;

    size_t nodes_processed = 0;
    size_t functions_found = 0;
    size_t classes_found = 0;

    // Complexity statistics
    size_t min_complexity = std::numeric_limits<size_t>::max();
    size_t max_complexity = 0;
    size_t total_complexity = 0;

    // Size statistics
    size_t min_size = std::numeric_limits<size_t>::max();
    size_t max_size = 0;
    size_t total_size = 0;

    bool has_errors() const { return !errors.empty(); }
    double success_rate() const
    {
        return nodes_processed > 0
                   ? (double)(nodes_processed - errors.size()) / nodes_processed
                   : 1.0;
    }
};

// Metric structures (unchanged from ast_node.h)
struct FunctionMetrics {
    size_t cyclomatic_complexity = 1;
    size_t parameter_count = 0;
    size_t statement_count = 0;
};

struct ClassMetrics {
    size_t member_count = 0;
    size_t method_count = 0;
    size_t public_member_count = 0;
    size_t private_member_count = 0;
};

struct NamespaceMetrics {
    size_t child_count = 0;
};

// Pure metric computation functions - no variant needed
FunctionMetrics compute_function_metrics(const clang::FunctionDecl *func_decl,
                                         clang::ASTContext &ctx);

ClassMetrics compute_class_metrics(const clang::CXXRecordDecl *class_decl,
                                   clang::ASTContext &ctx);

NamespaceMetrics compute_namespace_metrics(const clang::Decl *decl,
                                           clang::ASTContext &ctx,
                                           size_t child_count);

// Utility functions for complexity analysis
size_t count_statements(const clang::Stmt *stmt);
size_t count_decision_points(const clang::Stmt *stmt);

std::function<ImU32(const ASTNode &)>
create_complexity_coloring_strategy(const ASTAnalysisResult &analysis_result,
                                    clang::ASTUnit *unit);

std::function<ImU32(const ASTNode &)> create_type_based_coloring_strategy();
