#pragma once

#include "filesystem_node.h"
#include "treemap_widget.h"
#include <imgui.h>
#include <memory>
#include <string>

class FilesystemView
{
  public:
    FilesystemView();

    bool render();

    void set_root_path(const std::string &path);
    void set_max_depth(int depth) { max_depth_ = depth; }
    void set_include_hidden(bool include) { include_hidden_ = include; }

    const std::string &root_path() const { return current_path_; }
    bool has_valid_analysis() const { return treemap_ != nullptr; }

  private:
    std::string current_path_;
    char directory_buffer_[512];
    int max_depth_ = 4;
    bool include_hidden_ = false;
    std::string error_message_;

    AnalysisResult analysis_result_;
    std::unique_ptr<TreeMapWidget<FileSystemNode>> treemap_;

    enum class ColoringMode { FileType, ModificationTime };
    ColoringMode coloring_mode_ = ColoringMode::FileType;
    bool parallelize_layout_ = false;

    std::string hovered_info_ = "Hover over a file to see details";
    std::string selected_info_ = "Click on a file to select it";

    void render_controls();
    void render_treemap();
    void render_statistics();
    void render_interactive_info();
    void render_extension_legend();
    void refresh_analysis();
    void update_coloring_strategy();
    void register_treemap_callbacks();
};