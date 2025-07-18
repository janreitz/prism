#pragma once
#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <imgui.h>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
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
    FileSystemNode(const std::filesystem::path &path, FileInfo file_info);

    void add_child(std::unique_ptr<FileSystemNode> child);

    // TreeNode concept interface
    float size() const;
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
    std::vector<std::unique_ptr<FileSystemNode>> children_;
    size_t size_ = 0;
    FileInfo file_info_; // Always valid - no std::expected needed
};

// Factory function for creating nodes with error handling
std::expected<std::unique_ptr<FileSystemNode>, FileAccessError>
try_create_filesystem_node(const std::filesystem::path &path);

struct ModificationTimeStatistics {
    std::time_t min_modified = std::numeric_limits<std::time_t>::max();
    std::time_t max_modified = std::numeric_limits<std::time_t>::min();
};

// Analysis result with error tracking
struct AnalysisResult {
    std::unique_ptr<FileSystemNode> root;
    std::vector<FileAccessError> errors;
    size_t total_attempted = 0;
    size_t successful_nodes = 0;

    ModificationTimeStatistics modification_time_stats;

    std::map<std::string, int> extension_counts;

    // Size statistics
    float min_size = std::numeric_limits<float>::max();
    float max_size = std::numeric_limits<float>::min();
    float total_size = 0.0f;

    // Directory vs file counts
    size_t directory_count = 0;
    size_t file_count = 0;

    bool has_errors() const { return !errors.empty(); }
    double success_rate() const
    {
        return total_attempted > 0 ? (double)successful_nodes / total_attempted
                                   : 1.0;
    }
};

void recurse_fs(FileSystemNode &node, AnalysisResult &analysis);

// Analysis function with comprehensive error tracking
AnalysisResult scan_fs(const std::filesystem::path &root_path,
                       int max_depth = 5, bool include_hidden = false);

// Context-aware coloring strategy factories
std::function<ImU32(const FileSystemNode &)>
create_relative_time_strategy(const ModificationTimeStatistics &context);
std::function<ImU32(const FileSystemNode &)>
create_balanced_extension_strategy(const std::map<std::string, int> &context);

// Color utility functions
ImU32 hsv_to_rgb(float h, float s, float v);
