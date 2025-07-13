#pragma once
#include <vector>
#include <string>
#include <memory>
#include <filesystem>
#include <chrono>
#include <imgui.h>

class FileSystemNode {
public:
    FileSystemNode(const std::filesystem::path& path, FileSystemNode* parent = nullptr);
    
    void add_child(std::unique_ptr<FileSystemNode> child);
    
    // TreeNode concept interface
    float size() const;
    FileSystemNode* parent() const { return parent_; }
    std::vector<FileSystemNode*> children() const;
    
    // Additional interface
    const std::string& name() const { return name_; }
    const std::filesystem::path& path() const { return path_; }
    bool is_directory() const { return is_directory_; }
    float file_size() const { return file_size_; }
    std::time_t last_modified() const { return last_modified_; }
    
    std::string get_extension() const;
    std::string get_relative_path() const;
    std::string format_size() const;
    std::chrono::duration<double> time_since_modified() const;
    double days_since_modified() const;

private:
    std::filesystem::path path_;
    std::string name_;
    FileSystemNode* parent_;
    std::vector<std::unique_ptr<FileSystemNode>> children_;
    bool is_directory_;
    float file_size_;
    std::time_t last_modified_;
};

// Analysis function that scans a directory tree
std::unique_ptr<FileSystemNode> analyze_filesystem(const std::filesystem::path& root_path, 
                                                   int max_depth = 5,
                                                   bool include_hidden = false);

// Coloring strategies
ImU32 file_type_coloring(const FileSystemNode& node);
ImU32 modification_time_coloring(const FileSystemNode& node);