#pragma once

#include "ast_node.h"

#include <clang/AST/ASTContext.h>

#include <cstddef>
#include <limits>

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

class ASTAnalysis
{
  public:
    ASTAnalysis();
    void add_decl(const clang::Decl *decl, const clang::ASTContext &ctx);

    bool has_errors() const { return !errors.empty(); }
    double success_rate() const
    {
        return nodes_processed > 0
                   ? (double)(nodes_processed - errors.size()) / nodes_processed
                   : 1.0;
    }

    std::unique_ptr<ASTNode> root;
    std::vector<ASTAnalysisError> errors;

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

  private:
    ASTNode *get_or_create_node(const clang::Decl *decl,
                                const clang::ASTContext &ctx);
    void update_metrics(const ASTNode *node, const clang::ASTContext &context);

    // Track seen nodes to avoid duplicates
    std::unordered_map<std::string, ASTNode *> qualified_name_to_nodes_;
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

// Pure metric computation functions
FunctionMetrics compute_function_metrics(const clang::FunctionDecl *func_decl,
                                         const clang::ASTContext &ctx);

ClassMetrics compute_class_metrics(const clang::CXXRecordDecl *class_decl,
                                   const clang::ASTContext &ctx);

// Utility functions for complexity analysis
size_t count_statements(const clang::Stmt *stmt);
size_t count_decision_points(const clang::Stmt *stmt);

std::function<ImU32(const ASTNode &)>
create_complexity_coloring_strategy(const ASTAnalysis &analysis_result,
                                    clang::ASTUnit *unit);

std::function<ImU32(const ASTNode &)> create_type_based_coloring_strategy();
