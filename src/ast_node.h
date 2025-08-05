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
    explicit ASTNode(const clang::Decl *decl, const clang::ASTContext *context);

    void add_child(std::unique_ptr<ASTNode> &&child);

    // TreeNode concept interface
    float size() const;
    std::vector<const ASTNode *> children() const;

    size_t locs() const;
    std::string name() const;
    ASTNodeType node_type() const;
    const clang::Decl *clang_decl() const { return clang_decl_; }

    // Source location (to be processed by view layer with SourceManager)
    clang::SourceLocation source_location() const;

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
    const clang::ASTContext *ctx_;

    // Cache lines of code (computed once during construction for fast size()
    // calls)
    size_t locs_;
};

std::unique_ptr<ASTNode>
create_node_from_decl(const clang::Decl *decl,
                      const clang::ASTContext *context);

size_t calculate_lines_of_code(const clang::Decl *decl,
                               const clang::SourceManager *sm);

std::string format_source_location(const clang::SourceManager &sm,
                                   const clang::SourceLocation &src_loc);
