#pragma once
#include <vector>
#include <string>
#include <memory>
#include <imgui.h>

class ExampleTreeNode {
public:
    ExampleTreeNode(const std::string& name, float size_value, ExampleTreeNode* parent = nullptr)
        : name_(name), size_(size_value), parent_(parent) {}
    
    void add_child(std::unique_ptr<ExampleTreeNode> child) {
        child->parent_ = this;
        children_.push_back(std::move(child));
    }
    
    // TreeNode concept interface
    float size() const { return size_; }
    ExampleTreeNode* parent() const { return parent_; }
    std::vector<ExampleTreeNode*> children() const {
        std::vector<ExampleTreeNode*> result;
        for (const auto& child : children_) {
            result.push_back(child.get());
        }
        return result;
    }
    
    // Additional interface for display/debugging
    const std::string& name() const { return name_; }
    void set_size(float new_size) { size_ = new_size; }
    
    // Factory method for easier tree building
    static std::unique_ptr<ExampleTreeNode> create_sample_tree() {
        auto root = std::make_unique<ExampleTreeNode>("Project", 0); // Root size will be sum of children
        
        // Source code
        auto src = std::make_unique<ExampleTreeNode>("src/", 0);
        src->add_child(std::make_unique<ExampleTreeNode>("main.cpp", 1200));
        src->add_child(std::make_unique<ExampleTreeNode>("utils.cpp", 800));
        src->add_child(std::make_unique<ExampleTreeNode>("config.cpp", 600));
        root->add_child(std::move(src));
        
        // Headers
        auto headers = std::make_unique<ExampleTreeNode>("include/", 0);
        headers->add_child(std::make_unique<ExampleTreeNode>("utils.h", 400));
        headers->add_child(std::make_unique<ExampleTreeNode>("config.h", 300));
        headers->add_child(std::make_unique<ExampleTreeNode>("types.h", 250));
        root->add_child(std::move(headers));
        
        // Tests
        auto tests = std::make_unique<ExampleTreeNode>("tests/", 0);
        tests->add_child(std::make_unique<ExampleTreeNode>("test_main.cpp", 500));
        tests->add_child(std::make_unique<ExampleTreeNode>("test_utils.cpp", 350));
        root->add_child(std::move(tests));
        
        // Docs
        auto docs = std::make_unique<ExampleTreeNode>("docs/", 0);
        docs->add_child(std::make_unique<ExampleTreeNode>("README.md", 200));
        docs->add_child(std::make_unique<ExampleTreeNode>("API.md", 150));
        root->add_child(std::move(docs));
        
        // Build files
        root->add_child(std::make_unique<ExampleTreeNode>("CMakeLists.txt", 100));
        root->add_child(std::make_unique<ExampleTreeNode>(".gitignore", 50));
        
        // Calculate parent sizes based on children
        calculate_sizes(*root);
        
        return root;
    }

private:
    std::string name_;
    float size_;
    ExampleTreeNode* parent_;
    std::vector<std::unique_ptr<ExampleTreeNode>> children_;
    
    static void calculate_sizes(ExampleTreeNode& node) {
        float total = 0;
        for (auto& child : node.children_) {
            calculate_sizes(*child);
            total += child->size();
        }
        // If node has children, size is sum of children; otherwise keep original size
        if (!node.children_.empty()) {
            node.size_ = total;
        }
    }
};