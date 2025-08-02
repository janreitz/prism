#pragma once

#include "ast_node.h"
#include "ast_matcher.h"
#include "treemap_widget.h"
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>
#include <map>

class ASTMatcherView {
public:
    ASTMatcherView();
    
    // Main render function - returns true if view should remain open
    bool render();
    
    // Configuration
    void set_source_code(const std::string& code, const std::string& filename = "source.cpp");
    void add_predefined_matcher(const std::string& name, const std::string& description);
    
    // State queries
    bool has_valid_analysis() const { return treemap_ != nullptr; }
    
private:
    // Source input
    std::string source_code_;
    std::string filename_ = "source.cpp";
    char source_buffer_[4096];
    
    // Matcher input
    char matcher_input_[512];
    std::string current_matcher_;
    std::string error_message_;
    
    // Predefined matchers
    struct PredefinedMatcher {
        std::string name;
        std::string matcher_code;
        std::string description;
    };
    std::vector<PredefinedMatcher> predefined_matchers_;
    int selected_predefined_ = -1;
    
    // Analysis results
    ASTAnalysisResult analysis_result_;
    std::unique_ptr<TreeMapWidget<ASTNode>> treemap_;
    
    // UI state
    enum class ColoringMode { NodeType, Complexity };
    ColoringMode coloring_mode_ = ColoringMode::NodeType;
    
    // Interactive state
    std::string hovered_info_ = "Hover over an AST node to see details";
    const ASTNode* selected_node_ = nullptr;
    
    // Internal methods
    void render_source_input();
    void render_matcher_controls();
    void render_treemap();
    void render_interactive_info();
    void render_statistics();
    void render_match_results();
    void refresh_analysis();
    void update_coloring_strategy();
    void register_treemap_callbacks();
    void initialize_predefined_matchers();
    
    // Analysis methods
    bool apply_matcher_to_source();
};