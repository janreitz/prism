#pragma once

#include "ast_matcher.h"
#include "ast_node.h"
#include "treemap_widget.h"

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/ASTUnit.h"

#include <imgui.h>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ASTMatcherView
{
  public:
    ASTMatcherView();

    // Main render function - returns true if view should remain open
    bool render();

    // Configuration
    void set_source_code(const std::string &code,
                         const std::string &filename = "source.cpp");
    void add_predefined_matcher(const std::string &name,
                                const std::string &description);

    // State queries
    bool has_valid_analysis() const { return treemap_ != nullptr; }

  private:
    // Source input
    std::string source_code_;
    std::string filename_ = "source.cpp";
    char source_buffer_[4096];

    // Matcher input
    size_t current_matcher_idx_ = 0;
    std::string error_message_;

    const std::vector<
        std::pair<std::string, clang::ast_matchers::DeclarationMatcher>>
        predefined_matchers_{
            {"functionDecl()",
             clang::ast_matchers::functionDecl().bind("function")},
            {"cxxMethodDecl(isPublic())",
             clang::ast_matchers::cxxMethodDecl(clang::ast_matchers::isPublic())
                 .bind("function")},
            {"functionDecl(hasBody(compoundStmt()))",
             clang::ast_matchers::functionDecl(
                 clang::ast_matchers::hasBody(
                     clang::ast_matchers::compoundStmt()))
                 .bind("function")},
            {"cxxConstructorDecl()",
             clang::ast_matchers::cxxConstructorDecl().bind("function")},
            {"cxxMethodDecl(isVirtual())", clang::ast_matchers::cxxMethodDecl(
                                               clang::ast_matchers::isVirtual())
                                               .bind("function")},
        };

    // Analysis results
    std::unique_ptr<clang::ASTUnit> ast_unit_;
    ASTAnalysisResult analysis_result_;
    std::unique_ptr<TreeMapWidget<ASTNode>> treemap_;

    // UI state
    enum class ColoringMode { NodeType, Complexity };
    ColoringMode coloring_mode_ = ColoringMode::NodeType;

    // Interactive state
    std::string hovered_info_ = "Hover over an AST node to see details";
    const ASTNode *selected_node_ = nullptr;

    void render_source_input();
    void parse_ast();
    void render_matcher_controls();
    bool apply_matcher_to_source();
    void render_treemap();
    void render_interactive_info();
    void render_statistics();
    void render_selection_details();
    void update_coloring_strategy();
    void register_treemap_callbacks();
};