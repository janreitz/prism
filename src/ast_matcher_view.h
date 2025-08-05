#pragma once

#include "ast_matcher.h"
#include "ast_node.h"
#include "treemap_widget.h"

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Tooling/CompilationDatabase.h"

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

  private:
    // Source input
    std::string source_code_;
    std::vector<std::string> args_ = {"-std=c++17"};
    std::string filename_ = "source.cpp";
    char source_buffer_[4096];
    // Project input
    std::unique_ptr<clang::tooling::CompilationDatabase> compilation_db_;
    std::vector<std::string> source_files_;

    // Matcher input
    size_t current_matcher_idx_ = 0;
    std::string error_message_;

    // Analysis results
    std::vector<std::unique_ptr<clang::ASTUnit>> ast_units_;
    ASTAnalysisResult analysis_result_;
    std::unique_ptr<TreeMapWidget<ASTNode>> treemap_;

    // UI state
    enum class ColoringMode { NodeType, Complexity };
    ColoringMode coloring_mode_ = ColoringMode::NodeType;

    // Interactive state
    std::string hovered_info_ = "Hover over an AST node to see details";
    const ASTNode *selected_node_ = nullptr;

    void render_source_input();
    void render_string_input();
    void render_project_input();
    void render_matcher_controls();
    bool apply_matcher_to_source();
    void render_treemap();
    void render_interactive_info();
    void render_statistics();
    void render_selection_details();
    void update_coloring_strategy();
    void register_treemap_callbacks();
};