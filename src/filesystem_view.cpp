#include "filesystem_view.h"
#include <cstring>
#include <filesystem>
#include <iostream>

FilesystemView::FilesystemView()
{
    // Initialize with current directory
    current_path_ = std::filesystem::current_path().string();
    std::strncpy(directory_buffer_, current_path_.c_str(),
                 sizeof(directory_buffer_) - 1);
    directory_buffer_[sizeof(directory_buffer_) - 1] = '\0';

    // Perform initial analysis
    refresh_analysis();
}

bool FilesystemView::render()
{
    bool keep_open = true;

    if (ImGui::Begin("Filesystem Analysis", &keep_open)) {
        render_controls();

        if (treemap_) {
            ImGui::Separator();
            render_treemap();
            ImGui::Separator();
            render_interactive_info();
            ImGui::Separator();
            render_statistics();

            if (coloring_mode_ == ColoringMode::FileType) {
                ImGui::Separator();
                render_extension_legend();
            }
        } else if (!error_message_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s",
                               error_message_.c_str());
        }
    }
    ImGui::End();

    return keep_open;
}

void FilesystemView::set_root_path(const std::string &path)
{
    current_path_ = path;
    std::strncpy(directory_buffer_, path.c_str(),
                 sizeof(directory_buffer_) - 1);
    directory_buffer_[sizeof(directory_buffer_) - 1] = '\0';
    refresh_analysis();
}

void FilesystemView::render_controls()
{
    ImGui::Text("Directory Analysis Controls");

    if (ImGui::InputText("Root Directory", directory_buffer_,
                         sizeof(directory_buffer_))) {
        current_path_ = std::string(directory_buffer_);
    }

    ImGui::SameLine();
    if (ImGui::Button("Browse Current")) {
        current_path_ = std::filesystem::current_path().string();
        std::strncpy(directory_buffer_, current_path_.c_str(),
                     sizeof(directory_buffer_) - 1);
        directory_buffer_[sizeof(directory_buffer_) - 1] = '\0';
    }

    bool needs_refresh = false;

    if (ImGui::SliderInt("Max Depth", &max_depth_, 1, 10)) {
        needs_refresh = true;
    }

    if (ImGui::Checkbox("Include Hidden Files", &include_hidden_)) {
        needs_refresh = true;
    }

    if (ImGui::Button("Refresh Analysis") || needs_refresh) {
        refresh_analysis();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Parallelize Layout", &parallelize_layout_);

    ImGui::Text("Coloring Strategy:");
    if (ImGui::RadioButton("File Type",
                           coloring_mode_ == ColoringMode::FileType)) {
        coloring_mode_ = ColoringMode::FileType;
        update_coloring_strategy();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Modification Time",
                           coloring_mode_ == ColoringMode::ModificationTime)) {
        coloring_mode_ = ColoringMode::ModificationTime;
        update_coloring_strategy();
    }
}

void FilesystemView::render_treemap()
{
    ImVec2 available_size = ImGui::GetContentRegionAvail();
    available_size.y =
        std::max(available_size.y - 200.0f,
                 200.0f); // Reserve space for info and statistics

    treemap_->render("Filesystem TreeMap", available_size, parallelize_layout_);
}

void FilesystemView::render_statistics()
{
    ImGui::Text("Analysis Statistics");

    if (analysis_result_.has_errors()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                           "Success Rate: %.1f%% (%zu/%zu nodes)",
                           analysis_result_.success_rate() * 100.0,
                           analysis_result_.successful_nodes,
                           analysis_result_.total_attempted);

        if (ImGui::TreeNode("Errors")) {
            for (const auto &error : analysis_result_.errors) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "• %s",
                                   error.what.c_str());
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::Text("✓ All %zu nodes processed successfully",
                    analysis_result_.successful_nodes);
    }

    ImGui::Text("Total Size: %s", analysis_result_.root->format_size().c_str());
    ImGui::Text("Files: %zu, Directories: %zu", analysis_result_.file_count,
                analysis_result_.directory_count);
}

void FilesystemView::refresh_analysis()
{
    error_message_.clear();

    try {
        analysis_result_ = scan_fs(current_path_, max_depth_, include_hidden_);

        if (analysis_result_.root) {
            treemap_ = std::make_unique<TreeMapWidget<FileSystemNode>>(
                *analysis_result_.root);
            update_coloring_strategy();
            register_treemap_callbacks();
        } else {
            treemap_.reset();
            error_message_ = "Failed to analyze directory: " + current_path_;
        }
    } catch (const std::exception &e) {
        treemap_.reset();
        error_message_ = std::string("Exception during analysis: ") + e.what();
    }
}

void FilesystemView::update_coloring_strategy()
{
    if (!treemap_)
        return;

    switch (coloring_mode_) {
    case ColoringMode::FileType:
        treemap_->set_coloring_strategy(create_balanced_extension_strategy(
            analysis_result_.extension_counts));
        break;
    case ColoringMode::ModificationTime:
        treemap_->set_coloring_strategy(create_relative_time_strategy(
            analysis_result_.modification_time_stats));
        break;
    }
}

void FilesystemView::render_interactive_info()
{
    ImGui::Text("%s", hovered_info_.c_str());
    ImGui::Text("%s", selected_info_.c_str());
}

void FilesystemView::render_extension_legend()
{
    ImGui::Text("File Extensions Found (%zu types):",
                analysis_result_.extension_counts.size());

    // Show directories first
    if (analysis_result_.directory_count > 0) {
        ImGui::TextColored(
            ImVec4(186 / 255.0f, 85 / 255.0f, 211 / 255.0f, 1.0f),
            "■ Directories (%zu)", analysis_result_.directory_count);
        if (!analysis_result_.extension_counts.empty()) {
            ImGui::SameLine();
        }
    }

    // Generate the same colors as the extension strategy uses
    std::map<std::string, ImU32> extension_to_color;
    const float color_increment =
        1.0f / static_cast<float>(analysis_result_.extension_counts.size());
    float color = 0;
    for (const auto &[extension, count] : analysis_result_.extension_counts) {
        const float hue = (1.0f - color) * 120.0f; // 120° (green) to 0° (red)
        const float saturation = 0.8f;
        const float value = 0.9f;
        extension_to_color[extension] = hsv_to_rgb(hue, saturation, value);
        color += color_increment;
    }

    int i = 0;
    for (const auto &[ext, count] : analysis_result_.extension_counts) {
        ImU32 color = extension_to_color.at(ext);

        float r = ((color >> 0) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = ((color >> 16) & 0xFF) / 255.0f;

        std::string display_ext = ext.empty() ? "no extension" : ext;
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "■ %s (%d)",
                           display_ext.c_str(), count);

        // Layout in rows of 4
        if ((i + 1) % 4 != 0) {
            ImGui::SameLine();
        }
        i++;
    }
}

void FilesystemView::register_treemap_callbacks()
{
    if (!treemap_)
        return;

    treemap_->add_on_node_hover([this](const FileSystemNode &node) {
        std::string path_info = node.is_directory() ? "Directory: " : "File: ";
        hovered_info_ = path_info + node.get_relative_path() + " (" +
                        node.format_size() + ")";
        if (!node.is_directory()) {
            hovered_info_ +=
                " - Modified " +
                std::to_string(static_cast<int>(node.days_since_modified())) +
                " days ago";
        }
    });

    treemap_->add_on_node_click([this](const FileSystemNode &node) {
        std::string path_info =
            node.is_directory() ? "Selected Directory: " : "Selected File: ";
        selected_info_ = path_info + node.get_relative_path() + " (" +
                         node.format_size() + ")";
        std::cout << "Clicked: " << node.path() << " (" << node.format_size()
                  << ")" << std::endl;
    });
}