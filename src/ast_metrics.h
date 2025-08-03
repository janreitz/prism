#pragma once

#include <algorithm>
#include <cstddef>
#include <variant>

// Forward declarations for Clang AST types
namespace clang
{
class ASTContext;
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