#include "ast_node.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/TemplateBase.h"
#include "clang/Basic/SourceManager.h"
#include <algorithm>

using namespace clang;

ASTNode::ASTNode(const clang::Decl *decl, const clang::ASTContext *context)
    : clang_decl_(decl), ctx_(context),
      locs_(context
                ? calculate_lines_of_code(decl, &context->getSourceManager())
                : 0)
{
    // Explicit root construction requires both ptrs to be null
    // Normal construction requires both two be non-null
    assert((context == nullptr) == (decl == nullptr));
}

void ASTNode::add_child(std::unique_ptr<ASTNode> &&child)
{
    children_.push_back(std::move(child));
}

float ASTNode::size() const
{
    if (children_.empty()) {
        return static_cast<float>(locs_);
    }

    // Internal node - sum of children sizes
    float total = 0.0f;
    for (const auto &child : children_) {
        total += child->size();
    }

    // Ensure minimum size of 1.0f to prevent zero-sized treemap nodes
    return std::max(total, 1.0f);
}

std::vector<const ASTNode *> ASTNode::children() const
{
    std::vector<const ASTNode *> result;
    result.reserve(children_.size());
    for (const auto &child : children_) {
        result.push_back(child.get());
    }
    return result;
}

std::string ASTNode::type_string() const
{
    switch (node_type()) {
    case ASTNodeType::TranslationUnit:
        return "TranslationUnit";
    case ASTNodeType::Namespace:
        return "Namespace";
    case ASTNodeType::Class:
        return "Class";
    case ASTNodeType::Function:
        return "Function";
    case ASTNodeType::Variable:
        return "Variable";
    case ASTNodeType::Statement:
        return "Statement";
    default:
        return "Unknown";
    }
}

size_t ASTNode::locs() const { return locs_; }

std::string ASTNode::name() const
{
    if (const auto *named_decl = dyn_cast<NamedDecl>(clang_decl_)) {
        return named_decl->getNameAsString();
    } else {
        return "unnamed";
    }
}

ASTNodeType ASTNode::node_type() const
{
    if (!clang_decl_) {
        return ASTNodeType::TranslationUnit;
    }
    if (isa<FunctionDecl>(clang_decl_)) {
        return ASTNodeType::Function;
    } else if (isa<CXXRecordDecl>(clang_decl_)) {
        return ASTNodeType::Class;
    } else if (isa<VarDecl>(clang_decl_)) {
        return ASTNodeType::Variable;
    } else if (isa<NamespaceDecl>(clang_decl_)) {
        return ASTNodeType::Namespace;
    }
    return ASTNodeType::Statement;
}

std::string ASTNode::get_qualified_name() const
{
    if (!clang_decl_) {
        return "TranslationUnit";
    }

    if (const auto *named_decl = dyn_cast<NamedDecl>(clang_decl_)) {
        return named_decl->getQualifiedNameAsString();
    } else {
        return name();
    }
}

clang::SourceLocation ASTNode::source_location() const
{
    if (!clang_decl_)
        return clang::SourceLocation(); // Invalid location

    return clang_decl_->getLocation();
}

std::string ASTNode::source_location_string() const
{
    return format_source_location(ctx_->getSourceManager(), source_location());
}

size_t calculate_lines_of_code(const clang::Decl *decl,
                               const clang::SourceManager *sm)
{
    if (!decl || !sm)
        return 1;

    SourceLocation start = decl->getBeginLoc();
    SourceLocation end = decl->getEndLoc();

    // For functions, prefer using the body range for more accurate LOC
    if (const auto *func_decl = dyn_cast<FunctionDecl>(decl)) {
        if (func_decl->hasBody()) {
            const Stmt *body = func_decl->getBody();
            if (body) {
                start = body->getBeginLoc();
                end = body->getEndLoc();
            }
        }
    }

    // Calculate actual line difference using SourceManager
    //    if (start.isValid() && end.isValid()) {
    unsigned start_line = sm->getExpansionLineNumber(start);
    unsigned end_line = sm->getExpansionLineNumber(end);
    return std::max(1ul, static_cast<size_t>(end_line - start_line + 1));
    //  }
}

std::string format_source_location(const clang::SourceManager &sm,
                                   const clang::SourceLocation &src_loc)
{
    if (!src_loc.isValid()) {
        return "<unknown>";
    }

    std::string inst_filename = sm.getFilename(src_loc).str();
    if (inst_filename.empty())
        inst_filename = "<stdin>";
    unsigned inst_line = sm.getExpansionLineNumber(src_loc);
    unsigned inst_column = sm.getExpansionColumnNumber(src_loc);

    return inst_filename + ":" + std::to_string(inst_line) + ":" +
           std::to_string(inst_column);
}