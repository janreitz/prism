#include "filesystem_node.h"
#include <functional>
#include <algorithm>

FileSystemNode::FileSystemNode(const std::filesystem::path& path, FileSystemNode* parent)
    : path_(path), parent_(parent), file_size_(0), is_directory_(false), last_modified_(0) {
    
    name_ = path_.filename().string();
    if (name_.empty()) {
        name_ = path_.string(); // For root paths
    }
    
    try {
        if (std::filesystem::exists(path_)) {
            is_directory_ = std::filesystem::is_directory(path_);
            
            if (!is_directory_) {
                file_size_ = static_cast<float>(std::filesystem::file_size(path_));
            }
            
            // Get last write time
            auto ftime = std::filesystem::last_write_time(path_);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
            );
            last_modified_ = std::chrono::system_clock::to_time_t(sctp);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        // Handle permission errors, etc. gracefully
        is_directory_ = false;
        file_size_ = 0;
        last_modified_ = 0;
    }
}

void FileSystemNode::add_child(std::unique_ptr<FileSystemNode> child) {
    child->parent_ = this;
    children_.push_back(std::move(child));
}

float FileSystemNode::size() const {
    if (!is_directory_) {
        return std::max(1.0f, file_size_); // Ensure minimum size for visualization
    }
    
    // For directories, size is sum of children
    float total = 0;
    for (const auto& child : children_) {
        total += child->size();
    }
    return std::max(1.0f, total); // Ensure directories have minimum size too
}

std::vector<FileSystemNode*> FileSystemNode::children() const {
    std::vector<FileSystemNode*> result;
    for (const auto& child : children_) {
        result.push_back(child.get());
    }
    return result;
}

std::string FileSystemNode::get_extension() const {
    return path_.extension().string();
}

std::string FileSystemNode::get_relative_path() const {
    return std::filesystem::relative(path_).string();
}

std::string FileSystemNode::format_size() const {
    float sz = size();
    if (sz < 1024) return std::to_string(static_cast<int>(sz)) + " B";
    if (sz < 1024 * 1024) return std::to_string(static_cast<int>(sz / 1024)) + " KB";
    if (sz < 1024 * 1024 * 1024) return std::to_string(static_cast<int>(sz / (1024 * 1024))) + " MB";
    return std::to_string(static_cast<int>(sz / (1024 * 1024 * 1024))) + " GB";
}

std::chrono::duration<double> FileSystemNode::time_since_modified() const {
    auto now = std::chrono::system_clock::now();
    auto mod_time = std::chrono::system_clock::from_time_t(last_modified_);
    return now - mod_time;
}

double FileSystemNode::days_since_modified() const {
    return time_since_modified().count() / (24.0 * 3600.0);
}

// Analysis function that scans a directory tree
std::unique_ptr<FileSystemNode> analyze_filesystem(const std::filesystem::path& root_path, 
                                                   int max_depth,
                                                   bool include_hidden) {
    auto root = std::make_unique<FileSystemNode>(root_path);
    
    std::function<void(FileSystemNode&, int)> scan_recursive = [&](FileSystemNode& node, int depth) {
        if (depth <= 0) return;
        
        try {
            if (std::filesystem::is_directory(node.path())) {
                for (const auto& entry : std::filesystem::directory_iterator(node.path())) {
                    // Skip hidden files/directories unless requested
                    std::string filename = entry.path().filename().string();
                    if (!include_hidden && filename.starts_with(".")) {
                        continue;
                    }
                    
                    auto child = std::make_unique<FileSystemNode>(entry.path(), &node);
                    
                    if (entry.is_directory()) {
                        scan_recursive(*child, depth - 1);
                    }
                    
                    node.add_child(std::move(child));
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            // Handle permission errors gracefully
        }
    };
    
    scan_recursive(*root, max_depth);
    return root;
}

// File type coloring strategy
ImU32 file_type_coloring(const FileSystemNode& node) {
    if (node.is_directory()) {
        return IM_COL32(186, 85, 211, 255); // Purple for directories
    }
    
    std::string ext = node.get_extension();
    
    // Programming languages
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") return IM_COL32(70, 130, 180, 255);    // Blue
    if (ext == ".h" || ext == ".hpp" || ext == ".hxx") return IM_COL32(255, 140, 0, 255);     // Orange
    if (ext == ".py") return IM_COL32(255, 215, 0, 255);      // Gold
    if (ext == ".js" || ext == ".ts") return IM_COL32(240, 230, 140, 255);  // Khaki
    if (ext == ".java") return IM_COL32(255, 99, 71, 255);    // Tomato
    if (ext == ".rs") return IM_COL32(222, 165, 164, 255);    // Rust color
    
    // Documentation
    if (ext == ".md" || ext == ".txt" || ext == ".rst") return IM_COL32(60, 179, 113, 255);   // Green
    
    // Config files
    if (ext == ".json" || ext == ".yaml" || ext == ".yml" || ext == ".toml" || ext == ".ini") {
        return IM_COL32(255, 182, 193, 255); // Light pink
    }
    
    // Build files
    if (node.name() == "CMakeLists.txt" || node.name() == "Makefile" || ext == ".cmake") {
        return IM_COL32(139, 69, 19, 255); // Saddle brown
    }
    
    // Images
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".bmp") {
        return IM_COL32(255, 20, 147, 255); // Deep pink
    }
    
    // Archives
    if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".7z") {
        return IM_COL32(128, 0, 128, 255); // Purple
    }
    
    return IM_COL32(220, 220, 220, 255); // Light gray for unknown types
}

// Modification time coloring strategy
ImU32 modification_time_coloring(const FileSystemNode& node) {
    double days = node.days_since_modified();
    
    // Fresh (0-1 days): Bright green
    if (days < 1.0) {
        return IM_COL32(0, 255, 0, 255);
    }
    // Recent (1-7 days): Green to yellow
    else if (days < 7.0) {
        float t = static_cast<float>((days - 1.0) / 6.0); // 0-1
        int r = static_cast<int>(t * 255);
        int g = 255;
        int b = 0;
        return IM_COL32(r, g, b, 255);
    }
    // Medium (7-30 days): Yellow to orange
    else if (days < 30.0) {
        float t = static_cast<float>((days - 7.0) / 23.0); // 0-1
        int r = 255;
        int g = static_cast<int>(255 * (1.0f - t * 0.5f)); // 255 to 127
        int b = 0;
        return IM_COL32(r, g, b, 255);
    }
    // Old (30-90 days): Orange to red
    else if (days < 90.0) {
        float t = static_cast<float>((days - 30.0) / 60.0); // 0-1
        int r = 255;
        int g = static_cast<int>(127 * (1.0f - t)); // 127 to 0
        int b = 0;
        return IM_COL32(r, g, b, 255);
    }
    // Very old (90+ days): Dark red to gray
    else {
        float t = std::min(1.0f, static_cast<float>((days - 90.0) / 180.0)); // 0-1 over 6 months
        int intensity = static_cast<int>(128 * (1.0f - t * 0.5f)); // 128 to 64
        return IM_COL32(intensity, 0, 0, 255);
    }
}