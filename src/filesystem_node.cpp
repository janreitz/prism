#include "filesystem_node.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>

std::expected<FileInfo, FileAccessError>
get_file_info(const std::filesystem::path &path)
{
    try {
        bool is_directory = std::filesystem::is_directory(path);
        return FileInfo{
            .is_directory_ = is_directory,
            .file_size_ = is_directory ? 0.0f
                                       : static_cast<float>(
                                             std::filesystem::file_size(path)),
            .last_modified_ = std::chrono::system_clock::to_time_t(
                std::chrono::time_point_cast<
                    std::chrono::system_clock::duration>(
                    std::filesystem::last_write_time(path) -
                    std::filesystem::file_time_type::clock::now() +
                    std::chrono::system_clock::now()))};
    } catch (const std::filesystem::filesystem_error &e) {
        return std::unexpected(FileAccessError{.what = e.what()});
    }
}

FileSystemNode::FileSystemNode(const std::filesystem::path &path,
                               FileInfo file_info)
    : path_(path), file_info_(file_info)
{
    name_ = path_.filename().string();
    if (name_.empty()) {
        name_ = path_.string(); // For root paths
    }
}

void FileSystemNode::add_child(std::unique_ptr<FileSystemNode> child)
{
    size_ += child->size();
    children_.push_back(std::move(child));
}

float FileSystemNode::size() const
{
    if (!file_info_.is_directory_) {
        return file_info_.file_size_; // Ensure minimum size for visualization
    }

    return static_cast<float>(size_);
}

std::vector<const FileSystemNode *> FileSystemNode::children() const
{
    std::vector<const FileSystemNode *> result;
    for (const auto &child : children_) {
        result.push_back(child.get());
    }
    return result;
}

std::string FileSystemNode::get_extension() const
{
    return path_.extension().string();
}

std::string FileSystemNode::get_relative_path() const
{
    return std::filesystem::relative(path_).string();
}

std::string FileSystemNode::format_size() const
{
    float sz = size();
    if (sz < 1024)
        return std::to_string(static_cast<int>(sz)) + " B";
    if (sz < 1024 * 1024)
        return std::to_string(static_cast<int>(sz / 1024)) + " KB";
    if (sz < 1024 * 1024 * 1024)
        return std::to_string(static_cast<int>(sz / (1024 * 1024))) + " MB";
    return std::to_string(static_cast<int>(sz / (1024 * 1024 * 1024))) + " GB";
}

std::chrono::duration<double> FileSystemNode::time_since_modified() const
{
    auto now = std::chrono::system_clock::now();
    auto mod_time =
        std::chrono::system_clock::from_time_t(file_info_.last_modified_);
    return now - mod_time;
}

double FileSystemNode::days_since_modified() const
{
    return time_since_modified().count() / (24.0 * 3600.0);
}

// Factory function for creating nodes with error handling
std::expected<std::unique_ptr<FileSystemNode>, FileAccessError>
try_create_filesystem_node(const std::filesystem::path &path)
{
    return get_file_info(path).transform([&](FileInfo file_info) {
        return std::make_unique<FileSystemNode>(path, std::move(file_info));
    });
}

void recurse_fs(FileSystemNode &node, AnalysisResult &analysis,
                bool include_hidden, int depth)
{
    if (!node.is_directory()) {
        analysis.file_count++;

        const std::time_t mod_time = node.last_modified();
        analysis.modification_time_stats.min_modified =
            std::min(mod_time, analysis.modification_time_stats.min_modified);
        analysis.modification_time_stats.max_modified =
            std::max(mod_time, analysis.modification_time_stats.max_modified);

        const float size = node.file_size();
        analysis.total_size += size;
        analysis.min_size = std::min(size, analysis.min_size);
        analysis.max_size = std::max(size, analysis.max_size);

        analysis.extension_counts[node.get_extension()]++;
        return;
    } else {
        analysis.directory_count++;

        for (const auto &entry :
             std::filesystem::directory_iterator(node.path())) {

            std::string filename = entry.path().filename().string();
            if (!include_hidden && filename.starts_with(".")) {
                continue;
            }

            analysis.total_attempted++;
            auto child_result = try_create_filesystem_node(entry.path());

            if (!child_result) {
                analysis.errors.push_back(child_result.error());
                continue;
            }

            analysis.successful_nodes++;
            recurse_fs(**child_result, analysis, include_hidden, depth - 1);
            node.add_child(std::move(*child_result));
        }
    }
}

AnalysisResult scan_fs(const std::filesystem::path &path, int max_depth,
                       bool include_hidden)
{
    AnalysisResult analysis;
    analysis.total_attempted++;
    auto root_result = try_create_filesystem_node(path);

    if (!root_result) {
        analysis.errors.push_back(root_result.error());
        return analysis; // Can't proceed without root
    }

    analysis.root = std::move(*root_result);
    analysis.successful_nodes++;

    recurse_fs(*analysis.root, analysis, include_hidden, max_depth);
    return analysis;
}

// HSV to RGB conversion helper
ImU32 hsv_to_rgb(float h, float s, float v)
{
    float c = v * s;
    float x = c * (1 - std::abs(std::fmod(h / 60.0f, 2) - 1));
    float m = v - c;

    float r, g, b;
    if (h >= 0 && h < 60) {
        r = c;
        g = x;
        b = 0;
    } else if (h >= 60 && h < 120) {
        r = x;
        g = c;
        b = 0;
    } else if (h >= 120 && h < 180) {
        r = 0;
        g = c;
        b = x;
    } else if (h >= 180 && h < 240) {
        r = 0;
        g = x;
        b = c;
    } else if (h >= 240 && h < 300) {
        r = x;
        g = 0;
        b = c;
    } else {
        r = c;
        g = 0;
        b = x;
    }

    return IM_COL32(static_cast<int>((r + m) * 255),
                    static_cast<int>((g + m) * 255),
                    static_cast<int>((b + m) * 255), 255);
}

// Context-aware relative time coloring strategy
std::function<ImU32(const FileSystemNode &)>
create_relative_time_strategy(const ModificationTimeStatistics &context)
{
    const auto range = context.max_modified - context.min_modified;
    const auto offset = context.min_modified;
    return [offset, range](const FileSystemNode &node) -> ImU32 {
        if (node.is_directory()) {
            return IM_COL32(186, 85, 211, 255); // Purple for directories
        }

        const auto t =
            (static_cast<float>(node.last_modified()) - offset) / range;

        // Green (fresh) -> Yellow -> Orange -> Red (old)
        // Use HSV color space for smooth transitions
        const float hue = (1.0F - t) * 120.0F; // 120° (green) to 0° (red)
        const float saturation = 0.8F;
        const float value = 0.9F;

        return hsv_to_rgb(hue, saturation, value);
    };
}

// Context-aware balanced extension coloring strategy
std::function<ImU32(const FileSystemNode &)> create_balanced_extension_strategy(
    const std::map<std::string, int> &extension_counts)
{
    std::map<std::string, ImU32> extension_to_color;
    const float color_increment =
        1.0F / static_cast<float>(extension_counts.size());
    float color = 0;
    for (const auto &[extension, count] : extension_counts) {
        const float hue = (1.0F - color) * 120.0F; // 120° (green) to 0° (red)
        const float saturation = 0.8F;
        const float value = 0.9F;
        extension_to_color.insert(
            {extension, hsv_to_rgb(hue, saturation, value)});
        color += color_increment;
    }

    return [extension_to_color](const FileSystemNode &node) -> ImU32 {
        if (node.is_directory()) {
            return IM_COL32(186, 85, 211, 255); // Purple for directories
        }
        return extension_to_color.at(node.get_extension());
    };
}