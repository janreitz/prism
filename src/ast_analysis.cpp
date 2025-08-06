#include "ast_analysis.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/ASTUnit.h>

using namespace clang;

ASTAnalysis::ASTAnalysis(ASTContext &ctx)
    : root(std::make_unique<ASTNode>(ctx.getTranslationUnitDecl(), &ctx))
{
    // Associate TranslationUnit with filename since it is not a NamedDecl
    auto &source_manager = ctx.getSourceManager();
    auto main_file_id = source_manager.getMainFileID();

    std::string filename;
    if (main_file_id.isValid()) {
        if (const auto maybe_file_entry =
                source_manager.getFileEntryRefForID(main_file_id)) {
            filename = (*maybe_file_entry).getName().str();
        }
    }

    // Use filename as the key for TranslationUnit
    if (filename.empty()) {
        filename = "<translation_unit>"; // Fallback
    }

    qualified_name_to_nodes_.insert({filename, root.get()});
}

void ASTAnalysis::add_decl(const clang::Decl *decl,
                           const clang::ASTContext &ctx)
{
    get_or_create_node(decl, ctx);
}

ASTNode *ASTAnalysis::get_or_create_node(const clang::Decl *decl,
                                         const clang::ASTContext &ctx)
{
    if (decl == root->clang_decl()) {
        return root.get();
    }
    auto *named_decl = clang::dyn_cast<clang::NamedDecl>(decl);
    if (!named_decl) {
        // Skip over unnamed decls
        const clang::DeclContext *parent_context = decl->getDeclContext();
        return get_or_create_node(dyn_cast<clang::Decl>(parent_context), ctx);
    }

    // Avoid duplicates
    const auto qualified_name = named_decl->getQualifiedNameAsString();
    auto found_iter = qualified_name_to_nodes_.find(qualified_name);
    if (found_iter != qualified_name_to_nodes_.end()) {
        return found_iter->second;
    }

    auto temp_node = std::make_unique<ASTNode>(decl, &ctx);
    auto temp_node_ptr = temp_node.get();
    qualified_name_to_nodes_[qualified_name] = temp_node_ptr;

    update_metrics(temp_node_ptr, ctx);

    // Walk up the parent chain to find the proper hierarchical parent
    const clang::DeclContext *parent_context = decl->getDeclContext();
    auto *parent_node =
        get_or_create_node(dyn_cast<clang::Decl>(parent_context), ctx);
    parent_node->add_child(std::move(temp_node));

    return temp_node_ptr;
}

void ASTAnalysis::update_metrics(const ASTNode *node,
                                 const clang::ASTContext &context)
{
    const auto *decl = node->clang_decl();

    nodes_processed++;
    // Try to match different node types
    if (const auto *func_decl = dyn_cast<FunctionDecl>(decl)) {
        functions_found++;

        const auto func_metrics = compute_function_metrics(func_decl, context);
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

FunctionMetrics compute_function_metrics(const clang::FunctionDecl *func_decl,
                                         const clang::ASTContext &ctx)
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
create_complexity_coloring_strategy(const ASTAnalysis &analysis_result,
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

        const auto templated_kind = func_decl->getTemplatedKind();
        // Not templated.
        if (templated_kind ==
            clang::FunctionDecl::TemplatedKind::TK_NonTemplate)
            return IM_COL32(150, 255, 100, 255); // Green
        // The pattern in a function template declaration.
        if (templated_kind ==
            clang::FunctionDecl::TemplatedKind::TK_FunctionTemplate)
            return IM_COL32(255, 150, 100, 255); // Orange
        // A non-template function that is an instantiation or explicit
        // specialization of a member of a templated class.
        if (templated_kind ==
            clang::FunctionDecl::TemplatedKind::TK_MemberSpecialization)
            return IM_COL32(100, 150, 255, 255); // Blue
        // An instantiation or explicit specialization of a function
        // template.
        // Note: this might have been instantiated from a templated class if it
        // is a class-scope explicit specialization.
        if (templated_kind == clang::FunctionDecl::TemplatedKind::
                                  TK_FunctionTemplateSpecialization)
            return IM_COL32(200, 100, 255, 255); // purple
        // A function template specialization that hasn't yet been resolved
        // to a particular specialized function template.
        if (templated_kind == clang::FunctionDecl::TemplatedKind::
                                  TK_DependentFunctionTemplateSpecialization)
            return IM_COL32(200, 100, 255, 255); // purple

        return IM_COL32(255, 0, 0, 255); // Red
    };
}
