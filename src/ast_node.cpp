#include "ast_node.h"
#include "ast_matcher.h"
#include "ast_metrics.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/TemplateBase.h"
#include "clang/Basic/SourceManager.h"
#include <algorithm>

using namespace clang;

ASTNode::ASTNode(const clang::Decl *decl, clang::ASTContext *context)
    : clang_decl_(decl),
      locs_(calculate_lines_of_code(decl, &context->getSourceManager()))
{
}

void ASTNode::add_child(std::unique_ptr<ASTNode> child)
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
    if (!clang_decl_) {
        return "TranslationUnit";
    } else if (const auto *named_decl = dyn_cast<NamedDecl>(clang_decl_)) {
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

bool ASTNode::is_template_instantiation() const
{
    if (!clang_decl_)
        return false;

    if (const auto *func_decl = dyn_cast<FunctionDecl>(clang_decl_)) {
        return func_decl->getTemplateSpecializationKind() != TSK_Undeclared;
    } else if (const auto *class_decl = dyn_cast<CXXRecordDecl>(clang_decl_)) {
        return class_decl->getTemplateSpecializationKind() != TSK_Undeclared;
    }

    return false;
}

clang::SourceLocation ASTNode::template_definition_location() const
{
    if (!clang_decl_)
        return clang::SourceLocation();

    if (const auto *func_decl = dyn_cast<FunctionDecl>(clang_decl_)) {
        // Try different ways to get the original template definition
        if (const auto *pattern =
                func_decl->getTemplateInstantiationPattern()) {
            // Found via getTemplateInstantiationPattern
            return pattern->getLocation();
        }
        if (const auto *member_func =
                func_decl->getInstantiatedFromMemberFunction()) {
            // Found via getInstantiatedFromMemberFunction
            return member_func->getLocation();
        }
        // For function template specializations
        if (const auto *spec_info =
                func_decl->getTemplateSpecializationInfo()) {
            if (auto *template_decl = spec_info->getTemplate()) {
                // Found via getTemplate from specialization info
                return template_decl->getLocation();
            }
        }
    } else if (const auto *class_decl = dyn_cast<CXXRecordDecl>(clang_decl_)) {
        // For class template specializations
        if (const auto *spec_decl =
                dyn_cast<ClassTemplateSpecializationDecl>(class_decl)) {
            if (auto *template_decl = spec_decl->getSpecializedTemplate()) {
                // Found via getSpecializedTemplate
                return template_decl->getLocation();
            }
        }
        if (const auto *pattern =
                class_decl->getTemplateInstantiationPattern()) {
            // Found via getTemplateInstantiationPattern
            return pattern->getLocation();
        }
    }

    return clang::SourceLocation(); // No template definition found
}

std::string ASTNode::template_instantiation_info() const
{
    if (!clang_decl_)
        return "";

    if (const auto *func_decl = dyn_cast<FunctionDecl>(clang_decl_)) {
        if (func_decl->getTemplateSpecializationKind() != TSK_Undeclared) {
            std::string result = "Function template instantiation";

            // Try to get template argument information
            if (const auto *spec_info =
                    func_decl->getTemplateSpecializationInfo()) {
                if (const auto *args = spec_info->TemplateArguments) {
                    result += " with arguments: ";
                    for (unsigned i = 0; i < args->size(); ++i) {
                        if (i > 0)
                            result += ", ";
                        const auto &arg = args->get(i);

                        // Format template argument based on its kind
                        switch (arg.getKind()) {
                        case TemplateArgument::Type:
                            result += arg.getAsType().getAsString();
                            break;
                        case TemplateArgument::Integral:
                            result += std::to_string(
                                arg.getAsIntegral().getLimitedValue());
                            break;
                        default:
                            result += "<complex_arg>";
                            break;
                        }
                    }
                }
            } else {
                result = "Member function instantiation";
            }
            return result;
        }
    } else if (const auto *class_decl = dyn_cast<CXXRecordDecl>(clang_decl_)) {
        if (const auto *spec_decl =
                dyn_cast<ClassTemplateSpecializationDecl>(class_decl)) {
            std::string result = "Class template instantiation";

            const auto &args = spec_decl->getTemplateArgs();
            if (args.size() > 0) {
                result += " with arguments: ";
                for (unsigned i = 0; i < args.size(); ++i) {
                    if (i > 0)
                        result += ", ";
                    const auto &arg = args[i];

                    // Format template argument based on its kind
                    switch (arg.getKind()) {
                    case TemplateArgument::Type:
                        result += arg.getAsType().getAsString();
                        break;
                    case TemplateArgument::Integral:
                        result += std::to_string(
                            arg.getAsIntegral().getLimitedValue());
                        break;
                    default:
                        result += "<complex_arg>";
                        break;
                    }
                }
            }
            return result;
        }
    }

    return "";
}

size_t calculate_lines_of_code(const clang::Decl *decl,
                               clang::SourceManager *sm)
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

std::unique_ptr<ASTNode> create_node_from_decl(const Decl *decl,
                                               ASTContext *context)
{
    if (!decl)
        return nullptr;

    // Create node - everything derived from clang_decl_, LOC computed with real
    // SourceManager
    auto node = std::make_unique<ASTNode>(decl, context);

    // Metrics are computed on-demand in the metrics() method

    return node;
}

std::function<ImU32(const ASTNode &)>
create_complexity_coloring_strategy(const ASTAnalysisResult &context)
{
    return [&context](const ASTNode &node) -> ImU32 {
        float complexity = 1.0f;

        // Compute complexity on-demand using direct casting
        if (context.ast_unit && node.clang_decl()) {
            const clang::Decl *decl = node.clang_decl();
            clang::ASTContext &ctx = context.ast_unit->getASTContext();

            if (const auto *func_decl =
                    clang::dyn_cast<clang::FunctionDecl>(decl)) {
                auto metrics = compute_function_metrics(func_decl, ctx);
                complexity = static_cast<float>(metrics.cyclomatic_complexity);
            }
            // Other node types don't have complexity, keep default value
            // of 1.0f
        }

        float max_complexity = static_cast<float>(context.max_complexity);

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
        switch (node.node_type()) {
        case ASTNodeType::Function:
            return IM_COL32(100, 150, 255, 255); // Blue
        case ASTNodeType::Class:
            return IM_COL32(255, 150, 100, 255); // Orange
        case ASTNodeType::Variable:
            return IM_COL32(150, 255, 100, 255); // Green
        case ASTNodeType::Namespace:
            return IM_COL32(200, 100, 255, 255); // Purple
        default:
            return IM_COL32(128, 128, 128, 255); // Gray
        }
    };
}
