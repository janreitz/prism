#pragma once

#include <chrono>
#include <expected>
#include <functional>
#include <imgui.h>
#include <memory>
#include <string>
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
class SourceLocation;
class SourceManager;
} // namespace clang

enum class ASTNodeType {
    TranslationUnit,
    Namespace,
    Class,
    Function,
    Variable,
    Statement
};

struct ASTAnalysisError {
    std::string what;
    std::string node_name;
};

class ASTNode
{
  public:
    explicit ASTNode(const clang::Decl *decl = nullptr,
                     clang::ASTContext *context = nullptr);

    void add_child(std::unique_ptr<ASTNode> child);

    // TreeNode concept interface
    float size() const;
    std::vector<const ASTNode *> children() const;

    size_t locs() const;
    std::string name() const;
    ASTNodeType node_type() const;
    const clang::Decl *clang_decl() const { return clang_decl_; }

    // Source location (to be processed by view layer with SourceManager)
    clang::SourceLocation source_location() const;

    // Template instantiation analysis
    bool is_template_instantiation() const;
    clang::SourceLocation template_definition_location() const;
    std::string template_instantiation_info() const;

    // Utility functions
    std::string type_string() const;
    std::string get_qualified_name() const;

  private:
    std::vector<std::unique_ptr<ASTNode>> children_;

    // Single source of truth: everything derived from clang_decl_
    // IMPORTANT: clang_decl_ is only valid while the owning ASTUnit is alive
    const clang::Decl *clang_decl_;

    // Cache lines of code (computed once during construction for fast size()
    // calls)
    size_t locs_;
};

std::unique_ptr<ASTNode> create_node_from_decl(const clang::Decl *decl,
                                               clang::ASTContext *context);

size_t calculate_lines_of_code(const clang::Decl *decl,
                               clang::SourceManager *sm);

std::string format_source_location(const clang::SourceManager &sm,
                                   const clang::SourceLocation &src_loc);

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
