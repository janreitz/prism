#pragma once

#include <chrono>
#include <expected>
#include <functional>
#include <imgui.h>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Forward declarations for Clang AST types
namespace clang
{
class ASTContext;
class ASTUnit;
class Decl;
class FunctionDecl;
class CXXRecordDecl;
class VarDecl;
class Stmt;
} // namespace clang

enum class ASTNodeType {
    TranslationUnit,
    Namespace,
    Class,
    Function,
    Variable,
    Statement
};

struct FunctionMetrics {
    size_t lines_of_code = 0;
    size_t cyclomatic_complexity = 1;
    size_t parameter_count = 0;
    size_t statement_count = 0;

    float size() const
    {
        // For functions, size is based on complexity and length
        float computed =
            static_cast<float>(lines_of_code + cyclomatic_complexity * 2);
        return std::max(computed, 1.0f);
    }
};

struct ClassMetrics {
    size_t lines_of_code = 0;
    size_t member_count = 0;
    size_t method_count = 0;
    size_t public_member_count = 0;
    size_t private_member_count = 0;

    float size() const
    {
        // For classes, size is based on number of members and methods
        float computed = static_cast<float>(lines_of_code + member_count * 3 +
                                            method_count * 2);
        return std::max(computed, 1.0f);
    }
};

struct NamespaceMetrics {
    size_t lines_of_code = 0;
    size_t child_count = 0;

    float size() const
    {
        // For namespaces, size is primarily structural
        float computed = static_cast<float>(lines_of_code + child_count);
        return std::max(computed, 1.0f);
    }
};

struct VariableMetrics {
    size_t lines_of_code = 1; // Variables are typically single line

    float size() const
    {
        return 1.0f; // Variables have minimal individual impact
    }
};

struct DefaultMetrics {
    size_t lines_of_code = 1;

    float size() const
    {
        return std::max(static_cast<float>(lines_of_code), 1.0f);
    }
};

using NodeMetrics =
    std::variant<FunctionMetrics, ClassMetrics, NamespaceMetrics,
                 VariableMetrics, DefaultMetrics>;

struct ASTAnalysisError {
    std::string what;
    std::string node_name;
};

class ASTNode
{
  public:
    explicit ASTNode(const clang::Decl *decl = nullptr);

    void add_child(std::unique_ptr<ASTNode> child);

    // TreeNode concept interface
    float size() const;
    std::vector<const ASTNode *> children() const;

    std::string name() const;
    ASTNodeType node_type() const;
    const clang::Decl *clang_decl() const { return clang_decl_; }

    std::string file_path() const;
    unsigned line_number() const;
    unsigned column_number() const;

    // Get type-specific metrics
    const NodeMetrics &metrics() const;

    // Utility functions
    std::string type_string() const;
    std::string get_qualified_name() const;

  private:
    std::vector<std::unique_ptr<ASTNode>> children_;

    // Single source of truth: everything derived from clang_decl_
    // IMPORTANT: clang_decl_ is only valid while the owning ASTUnit is alive
    const clang::Decl *clang_decl_;

    // Cache computed metrics (computed on first access)
    mutable std::optional<NodeMetrics> cached_metrics_;
};

std::unique_ptr<ASTNode> create_node_from_decl(const clang::Decl *decl,
                                               clang::ASTContext *context);

NodeMetrics calculate_metrics(const clang::Decl *decl,
                              clang::ASTContext *context);

// Analysis result with error tracking (similar to AnalysisResult)
struct ASTAnalysisResult {
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

    // CRITICAL: Keep the AST alive so clang_decl_ pointers remain valid
    std::unique_ptr<clang::ASTUnit> ast_unit;

    bool has_errors() const { return !errors.empty(); }
    double success_rate() const
    {
        return nodes_processed > 0
                   ? (double)(nodes_processed - errors.size()) / nodes_processed
                   : 1.0;
    }
};

std::function<ImU32(const ASTNode &)>
create_complexity_coloring_strategy(const ASTAnalysisResult &context);

std::function<ImU32(const ASTNode &)> create_type_based_coloring_strategy();
