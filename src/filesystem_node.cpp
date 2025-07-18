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
    children_.push_back(std::move(child));
}

float FileSystemNode::size() const
{
    if (!file_info_.is_directory_) {
        return std::max(
            1.0f,
            file_info_.file_size_); // Ensure minimum size for visualization
    }

    return std::accumulate(children_.cbegin(), children_.cend(),
                           // Ensure minimum size for visualization
                           1.0F, [](float size, const auto &child) {
                               return size + child->size();
                           });
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

// Analysis function with comprehensive error tracking
AnalysisResult analyze_filesystem(const std::filesystem::path &root_path,
                                  int max_depth, bool include_hidden)
{
    AnalysisResult result;

    auto root_result = try_create_filesystem_node(root_path);
    result.total_attempted++;

    if (!root_result) {
        result.errors.push_back(root_result.error());
        return result; // Can't proceed without root
    }

    result.root = std::move(*root_result);
    result.successful_nodes++;

    std::function<void(FileSystemNode &, int)> scan_recursive =
        [&](FileSystemNode &node, int depth) {
            if (depth <= 0)
                return;

            try {
                if (!node.is_directory())
                    return;

                for (const auto &entry :
                     std::filesystem::directory_iterator(node.path())) {
                    // Skip hidden files/directories unless requested
                    std::string filename = entry.path().filename().string();
                    if (!include_hidden && filename.starts_with(".")) {
                        continue;
                    }

                    result.total_attempted++;
                    auto child_result =
                        try_create_filesystem_node(entry.path());

                    if (child_result) {
                        result.successful_nodes++;
                        if ((*child_result)->is_directory()) {
                            scan_recursive(**child_result, depth - 1);
                        }
                        node.add_child(std::move(*child_result));
                    } else {
                        result.errors.push_back(child_result.error());
                    }
                }
            } catch (const std::filesystem::filesystem_error &e) {
                result.errors.push_back(FileAccessError{e.what()});
            }
        };

    scan_recursive(*result.root, max_depth);
    return result;
}

// Context analysis function
ColoringContext analyze_coloring_context(const FileSystemNode &root)
{
    ColoringContext context;
    std::set<std::string> extensions_set;

    std::function<void(const FileSystemNode &)> analyze_recursive =
        [&](const FileSystemNode &node) {
            // Count directories vs files
            if (node.is_directory()) {
                context.directory_count++;
            } else {
                context.file_count++;

                // Analyze modification time
                std::time_t mod_time = node.last_modified();
                if (context.min_modified == 0 ||
                    mod_time < context.min_modified) {
                    context.min_modified = mod_time;
                }
                if (mod_time > context.max_modified) {
                    context.max_modified = mod_time;
                }

                // Analyze file size
                float size = node.file_size();
                if (context.min_size == 0.0f || size < context.min_size) {
                    context.min_size = size;
                }
                if (size > context.max_size) {
                    context.max_size = size;
                }

                // Collect extensions
                std::string ext = node.get_extension();
                if (!ext.empty()) {
                    extensions_set.insert(ext);
                    context.extension_counts[ext]++;
                }
            }

            // Recurse to children
            for (const FileSystemNode *child : node.children()) {
                analyze_recursive(*child);
            }
        };

    analyze_recursive(root);

    // Calculate days since modified range
    if (context.max_modified > context.min_modified) {
        auto now = std::chrono::system_clock::now();
        auto min_time =
            std::chrono::system_clock::from_time_t(context.min_modified);
        auto max_time =
            std::chrono::system_clock::from_time_t(context.max_modified);

        context.max_days_since_modified =
            std::chrono::duration<double>(now - min_time).count() /
            (24.0 * 3600.0);
        context.min_days_since_modified =
            std::chrono::duration<double>(now - max_time).count() /
            (24.0 * 3600.0);
    }

    // Convert extensions set to vector for stable ordering
    context.unique_extensions =
        std::vector<std::string>(extensions_set.begin(), extensions_set.end());
    std::sort(context.unique_extensions.begin(),
              context.unique_extensions.end());

    return context;
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
create_relative_time_strategy(const ColoringContext &context)
{
    return [context](const FileSystemNode &node) -> ImU32 {
        if (node.is_directory()) {
            return IM_COL32(186, 85, 211, 255); // Purple for directories
        }

        if (!context.has_data() || context.max_days_since_modified <=
                                       context.min_days_since_modified) {
            return IM_COL32(128, 128, 128, 255); // Gray fallback
        }

        double days = node.days_since_modified();

        // Normalize to 0-1 range based on actual data range
        float t = static_cast<float>((days - context.min_days_since_modified) /
                                     (context.max_days_since_modified -
                                      context.min_days_since_modified));
        t = std::clamp(t, 0.0f, 1.0f);

        // Green (fresh) -> Yellow -> Orange -> Red (old)
        // Use HSV color space for smooth transitions
        float hue = (1.0f - t) * 120.0f; // 120° (green) to 0° (red)
        float saturation = 0.8f;
        float value = 0.9f;

        return hsv_to_rgb(hue, saturation, value);
    };
}

// Context-aware balanced extension coloring strategy
std::function<ImU32(const FileSystemNode &)>
create_balanced_extension_strategy(const ColoringContext &context)
{
    return [context](const FileSystemNode &node) -> ImU32 {
        if (node.is_directory()) {
            return IM_COL32(186, 85, 211, 255); // Purple for directories
        }

        std::string ext = node.get_extension();

        // Check if we have a pre-computed color
        if (context.extension_colors.find(ext) !=
            context.extension_colors.end()) {
            return context.extension_colors.at(ext);
        }

        // Find index of extension
        auto it = std::find(context.unique_extensions.begin(),
                            context.unique_extensions.end(), ext);
        if (it == context.unique_extensions.end()) {
            return IM_COL32(200, 200, 200, 255); // Light gray for unknown
        }

        size_t index = std::distance(context.unique_extensions.begin(), it);

        // Generate evenly spaced colors in HSV space
        if (context.unique_extensions.size() <= 1) {
            return IM_COL32(100, 150, 200, 255); // Default blue
        }

        float hue = (360.0f * index) / context.unique_extensions.size();
        float saturation = 0.75f;
        float value = 0.85f;

        return hsv_to_rgb(hue, saturation, value);
    };
}