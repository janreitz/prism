#pragma once
#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <imgui.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <variant>
#include <vector>

struct FileInfo {
    bool is_directory_;
    float file_size_;
    std::time_t last_modified_;
};

struct FileAccessError {
    std::string what;
};

std::expected<FileInfo, FileAccessError>
get_file_info(const std::filesystem::path &path);

class FileSystemNode
{
  public:
    FileSystemNode(const std::filesystem::path &path, FileSystemNode *parent,
                   FileInfo file_info);

    void add_child(std::unique_ptr<FileSystemNode> child);

    // TreeNode concept interface
    float size() const;
    FileSystemNode *parent() const { return parent_; }
    std::vector<const FileSystemNode *> children() const;

    // Additional interface
    const std::string &name() const { return name_; }
    const std::filesystem::path &path() const { return path_; }
    bool is_directory() const { return file_info_.is_directory_; }
    float file_size() const { return file_info_.file_size_; }
    std::time_t last_modified() const { return file_info_.last_modified_; }

    std::string get_extension() const;
    std::string get_relative_path() const;
    std::string format_size() const;
    std::chrono::duration<double> time_since_modified() const;
    double days_since_modified() const;

  private:
    std::filesystem::path path_;
    std::string name_;
    FileSystemNode *parent_;
    std::vector<std::unique_ptr<FileSystemNode>> children_;
    FileInfo file_info_; // Always valid - no std::expected needed
};

// Factory function for creating nodes with error handling
std::expected<std::unique_ptr<FileSystemNode>, FileAccessError>
try_create_filesystem_node(const std::filesystem::path &path,
                           FileSystemNode *parent = nullptr);

// Analysis result with error tracking
struct AnalysisResult {
    std::unique_ptr<FileSystemNode> root;
    std::vector<FileAccessError> errors;
    size_t total_attempted = 0;
    size_t successful_nodes = 0;

    bool has_errors() const { return !errors.empty(); }
    double success_rate() const
    {
        return total_attempted > 0 ? (double)successful_nodes / total_attempted
                                   : 1.0;
    }
};

// Analysis function with comprehensive error tracking
AnalysisResult analyze_filesystem(const std::filesystem::path &root_path,
                                  int max_depth = 5,
                                  bool include_hidden = false);

// Contextual coloring support
struct ColoringContext {
    // Modification time statistics
    std::time_t min_modified = 0;
    std::time_t max_modified = 0;
    double min_days_since_modified = 0.0;
    double max_days_since_modified = 0.0;

    // File extension statistics
    std::vector<std::string> unique_extensions;
    std::map<std::string, ImU32> extension_colors;
    std::map<std::string, int> extension_counts;

    // Size statistics
    float min_size = 0.0f;
    float max_size = 0.0f;

    // Directory vs file counts
    int directory_count = 0;
    int file_count = 0;

    bool has_data() const
    {
        return max_modified > min_modified || !unique_extensions.empty();
    }
};

// Context analysis
ColoringContext analyze_coloring_context(const FileSystemNode &root);

// Context-aware coloring strategy factories
std::function<ImU32(const FileSystemNode &)>
create_relative_time_strategy(const ColoringContext &context);
std::function<ImU32(const FileSystemNode &)>
create_balanced_extension_strategy(const ColoringContext &context);

// Color utility functions
ImU32 hsv_to_rgb(float h, float s, float v);
