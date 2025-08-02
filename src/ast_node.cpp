#include "ast_node.h"
#include "ast_matcher.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/Basic/SourceManager.h"
#include <algorithm>

using namespace clang;

ASTNode::ASTNode(const clang::Decl *decl)
    : clang_decl_(decl)
{
}

void ASTNode::add_child(std::unique_ptr<ASTNode> child)
{
    children_.push_back(std::move(child));
}

float ASTNode::size() const
{
    if (children_.empty()) {
        // Leaf node - return size from type-specific metrics
        return std::visit([](const auto& m) { return m.size(); }, metrics());
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
    // TODO: Build qualified name by traversing up the parent chain
    return name();
}

std::string ASTNode::file_path() const
{
    if (!clang_decl_)
        return "";

    // TODO: Extract from SourceManager when available
    return "source.cpp";
}

unsigned ASTNode::line_number() const
{
    if (!clang_decl_)
        return 0;

    // TODO: Extract from SourceManager when available
    return 1;
}

unsigned ASTNode::column_number() const
{
    if (!clang_decl_)
        return 0;

    // TODO: Extract from SourceManager when available
    return 1;
}

const NodeMetrics& ASTNode::metrics() const
{
    if (!cached_metrics_.has_value()) {
        // Compute appropriate metrics based on node type
        ASTNodeType type = node_type();
        switch (type) {
            case ASTNodeType::Function:
                cached_metrics_ = FunctionMetrics{5, 2, 3, 10}; // TODO: compute from clang_decl_
                break;
            case ASTNodeType::Class:
                cached_metrics_ = ClassMetrics{20, 5, 3, 2, 3}; // TODO: compute from clang_decl_
                break;
            case ASTNodeType::Namespace:
                cached_metrics_ = NamespaceMetrics{1, static_cast<size_t>(children_.size())};
                break;
            case ASTNodeType::Variable:
                cached_metrics_ = VariableMetrics{};
                break;
            default:
                cached_metrics_ = DefaultMetrics{1};
                break;
        }
    }
    return cached_metrics_.value();
}

std::unique_ptr<ASTNode> create_node_from_decl(const Decl *decl,
                                               ASTContext *context)
{
    if (!decl)
        return nullptr;

    // Create node - everything derived from clang_decl_
    auto node = std::make_unique<ASTNode>(decl);
    
    // Metrics are computed on-demand in the metrics() method
    
    return node;
}

NodeMetrics calculate_metrics(const Decl *decl, ASTContext *context)
{
    if (!decl) {
        return DefaultMetrics{1};
    }
    
    // Determine node type and create appropriate metrics
    if (isa<FunctionDecl>(decl)) {
        FunctionMetrics metrics;
        if (!context) {
            metrics.lines_of_code = 1;
            metrics.cyclomatic_complexity = 1;
            return metrics;
        }
        
        // Basic line count estimation for functions
        SourceManager &sm = context->getSourceManager();
        SourceLocation start = decl->getBeginLoc();
        SourceLocation end = decl->getEndLoc();

        if (start.isValid() && end.isValid()) {
            unsigned start_line = sm.getExpansionLineNumber(start);
            unsigned end_line = sm.getExpansionLineNumber(end);
            metrics.lines_of_code = end_line - start_line + 1;
        } else {
            metrics.lines_of_code = 1;
        }

        const auto *func_decl = dyn_cast<FunctionDecl>(decl);
        metrics.cyclomatic_complexity = 1; // Base complexity
        metrics.parameter_count = func_decl->getNumParams();

        // Simple complexity calculation
        if (func_decl->hasBody()) {
            // TODO: Implement proper cyclomatic complexity calculation
            metrics.cyclomatic_complexity = std::max(1ul, metrics.lines_of_code / 10);
        }
        
        return metrics;
    }
    else if (isa<CXXRecordDecl>(decl)) {
        ClassMetrics metrics;
        if (context) {
            SourceManager &sm = context->getSourceManager();
            SourceLocation start = decl->getBeginLoc();
            SourceLocation end = decl->getEndLoc();

            if (start.isValid() && end.isValid()) {
                unsigned start_line = sm.getExpansionLineNumber(start);
                unsigned end_line = sm.getExpansionLineNumber(end);
                metrics.lines_of_code = end_line - start_line + 1;
            }
        }
        
        const auto *class_decl = dyn_cast<CXXRecordDecl>(decl);
        metrics.member_count = std::distance(class_decl->decls_begin(), class_decl->decls_end());
        
        // Count methods vs data members
        for (const auto* member : class_decl->decls()) {
            if (isa<CXXMethodDecl>(member)) {
                metrics.method_count++;
                if (member->getAccess() == AS_public) {
                    metrics.public_member_count++;
                } else if (member->getAccess() == AS_private) {
                    metrics.private_member_count++;
                }
            }
        }
        
        return metrics;
    }
    else if (isa<NamespaceDecl>(decl)) {
        NamespaceMetrics metrics;
        if (context) {
            SourceManager &sm = context->getSourceManager();
            SourceLocation start = decl->getBeginLoc();
            SourceLocation end = decl->getEndLoc();

            if (start.isValid() && end.isValid()) {
                unsigned start_line = sm.getExpansionLineNumber(start);
                unsigned end_line = sm.getExpansionLineNumber(end);
                metrics.lines_of_code = end_line - start_line + 1;
            }
        }
        
        const auto *ns_decl = dyn_cast<NamespaceDecl>(decl);
        metrics.child_count = std::distance(ns_decl->decls_begin(), ns_decl->decls_end());
        
        return metrics;
    }
    else if (isa<VarDecl>(decl)) {
        return VariableMetrics{};
    }
    else {
        DefaultMetrics metrics;
        if (context) {
            SourceManager &sm = context->getSourceManager();
            SourceLocation start = decl->getBeginLoc();
            SourceLocation end = decl->getEndLoc();

            if (start.isValid() && end.isValid()) {
                unsigned start_line = sm.getExpansionLineNumber(start);
                unsigned end_line = sm.getExpansionLineNumber(end);
                metrics.lines_of_code = end_line - start_line + 1;
            }
        }
        return metrics;
    }
}

std::function<ImU32(const ASTNode &)>
create_complexity_coloring_strategy(const ASTAnalysisResult &context)
{
    return [&context](const ASTNode &node) -> ImU32 {
        float complexity = 1.0f;
        
        // Extract complexity from the variant metrics
        std::visit([&complexity](const auto& metrics) {
            if constexpr (requires { metrics.cyclomatic_complexity; }) {
                complexity = static_cast<float>(metrics.cyclomatic_complexity);
            }
        }, node.metrics());
        
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
