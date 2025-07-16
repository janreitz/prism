#include "../src/filesystem_node.h"
#include "../src/treemap.h"
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <memory>
#include <vector>

using namespace treemap;

// Mock tree node for testing
class MockTreeNode
{
  public:
    MockTreeNode(float size, const std::string &name) : size_(size), name_(name)
    {
    }

    float size() const { return size_; }
    MockTreeNode *parent() const { return parent_; }
    std::vector<MockTreeNode *> children() const { return children_; }

    void add_child(std::unique_ptr<MockTreeNode> child)
    {
        child->parent_ = this;
        children_.push_back(child.get());
        owned_children_.push_back(std::move(child));
    }

    const std::string &name() const { return name_; }

  private:
    float size_;
    std::string name_;
    MockTreeNode *parent_ = nullptr;
    std::vector<MockTreeNode *> children_;
    std::vector<std::unique_ptr<MockTreeNode>> owned_children_;
};

// Helper function to print rectangle details
void print_rect(const std::string &label, const Rect &rect)
{
    std::cout << label << ": (" << rect.x << ", " << rect.y << ", "
              << rect.width << ", " << rect.height << ")\n";
}

// Helper function to print all rendered rectangles
void print_rendered_rects(const std::vector<RenderedRect<MockTreeNode>> &rects)
{
    std::cout << "\n=== Rendered Rectangles ===\n";
    for (size_t i = 0; i < rects.size(); ++i) {
        std::cout << "Rect " << i << " ('" << rects[i].node->name() << "'): ";
        print_rect("", rects[i].rect);
    }
    std::cout << "========================\n\n";
}

// Helper function to check for overlaps with detailed output
bool check_overlaps_detailed(
    const std::vector<RenderedRect<MockTreeNode>> &rects)
{
    std::cout << "\n=== Overlap Check ===\n";
    bool found_overlap = false;

    for (size_t i = 0; i < rects.size(); ++i) {
        for (size_t j = i + 1; j < rects.size(); ++j) {
            if (overlaps(rects[i].rect, rects[j].rect)) {
                std::cout << "OVERLAP FOUND!\n";
                std::cout << "  Rect " << i << " ('" << rects[i].node->name()
                          << "'): ";
                print_rect("", rects[i].rect);
                std::cout << "  Rect " << j << " ('" << rects[j].node->name()
                          << "'): ";
                print_rect("", rects[j].rect);
                found_overlap = true;
            }
        }
    }

    if (!found_overlap) {
        std::cout << "No overlaps found.\n";
    }
    std::cout << "====================\n\n";

    return found_overlap;
}

// Helper function to check bounds with detailed output
bool check_bounds_detailed(const std::vector<RenderedRect<MockTreeNode>> &rects,
                           const Rect &bounds)
{
    std::cout << "\n=== Bounds Check ===\n";
    print_rect("Bounds", bounds);

    bool found_out_of_bounds = false;

    for (size_t i = 0; i < rects.size(); ++i) {
        if (!within_bounds(rects[i].rect, bounds)) {
            std::cout << "OUT OF BOUNDS!\n";
            std::cout << "  Rect " << i << " ('" << rects[i].node->name()
                      << "'): ";
            print_rect("", rects[i].rect);
            found_out_of_bounds = true;
        }
    }

    if (!found_out_of_bounds) {
        std::cout << "All rectangles within bounds.\n";
    }
    std::cout << "==================\n\n";

    return found_out_of_bounds;
}

TEST_CASE("TreeMap Layout - Simple Case", "[layout]")
{
    // Create a simple tree: root with two children
    auto root = std::make_unique<MockTreeNode>(100.0f, "root");
    root->add_child(std::make_unique<MockTreeNode>(60.0f, "child1"));
    root->add_child(std::make_unique<MockTreeNode>(40.0f, "child2"));

    Rect available_rect{0, 0, 100, 100};

    // Test the layout calculation directly
    std::cout << "\n=== TEST: Simple Case ===\n";
    std::cout << "Available rect: ";
    print_rect("", available_rect);

    auto result = calculate_layout(*root, available_rect);

    print_rendered_rects(result);

    // Check for overlaps
    REQUIRE_FALSE(check_overlaps_detailed(result));

    // Check bounds
    REQUIRE_FALSE(check_bounds_detailed(result, available_rect));

    // Should have 2 rectangles (only leaf nodes)
    REQUIRE(result.size() == 2);
}

TEST_CASE("TreeMap Layout - Three Children", "[layout]")
{
    // Create a tree with three children of different sizes
    auto root = std::make_unique<MockTreeNode>(100.0f, "root");
    root->add_child(std::make_unique<MockTreeNode>(50.0f, "large"));
    root->add_child(std::make_unique<MockTreeNode>(30.0f, "medium"));
    root->add_child(std::make_unique<MockTreeNode>(20.0f, "small"));

    Rect available_rect{0, 0, 200, 100};

    std::cout << "\n=== TEST: Three Children ===\n";
    std::cout << "Available rect: ";
    print_rect("", available_rect);

    auto result = calculate_layout(*root, available_rect);

    print_rendered_rects(result);

    // Check for overlaps
    REQUIRE_FALSE(check_overlaps_detailed(result));

    // Check bounds
    REQUIRE_FALSE(check_bounds_detailed(result, available_rect));

    // Should have 3 rectangles
    REQUIRE(result.size() == 3);
}

TEST_CASE("TreeMap Layout - Nested Structure", "[layout]")
{
    // Create a nested structure
    auto root = std::make_unique<MockTreeNode>(100.0f, "root");

    auto dir1 = std::make_unique<MockTreeNode>(70.0f, "dir1");
    dir1->add_child(std::make_unique<MockTreeNode>(40.0f, "dir1_file1"));
    dir1->add_child(std::make_unique<MockTreeNode>(30.0f, "dir1_file2"));

    auto dir2 = std::make_unique<MockTreeNode>(30.0f, "dir2");
    dir2->add_child(std::make_unique<MockTreeNode>(20.0f, "dir2_file1"));
    dir2->add_child(std::make_unique<MockTreeNode>(10.0f, "dir2_file2"));

    root->add_child(std::move(dir1));
    root->add_child(std::move(dir2));

    Rect available_rect{0, 0, 400, 300};

    std::cout << "\n=== TEST: Nested Structure ===\n";
    std::cout << "Available rect: ";
    print_rect("", available_rect);

    auto result = calculate_layout(*root, available_rect);

    print_rendered_rects(result);

    // Check for overlaps
    REQUIRE_FALSE(check_overlaps_detailed(result));

    // Check bounds
    REQUIRE_FALSE(check_bounds_detailed(result, available_rect));

    // Should have 4 rectangles (only leaf nodes)
    REQUIRE(result.size() == 4);
}

TEST_CASE("TreeMap Layout - Single Child", "[layout]")
{
    // Edge case: single child
    auto root = std::make_unique<MockTreeNode>(100.0f, "root");
    root->add_child(std::make_unique<MockTreeNode>(100.0f, "only_child"));

    Rect available_rect{0, 0, 100, 100};

    std::cout << "\n=== TEST: Single Child ===\n";
    std::cout << "Available rect: ";
    print_rect("", available_rect);

    auto result = calculate_layout(*root, available_rect);

    print_rendered_rects(result);

    // Check for overlaps
    REQUIRE_FALSE(check_overlaps_detailed(result));

    // Check bounds
    REQUIRE_FALSE(check_bounds_detailed(result, available_rect));

    // Should have 1 rectangle
    REQUIRE(result.size() == 1);

    // Should fill the entire available space
    REQUIRE(result[0].rect.x == 0);
    REQUIRE(result[0].rect.y == 0);
    REQUIRE(result[0].rect.width == 100);
    REQUIRE(result[0].rect.height == 100);
}

TEST_CASE("TreeMap Layout - Leaf Node", "[layout]")
{
    // Edge case: leaf node only
    auto root = std::make_unique<MockTreeNode>(100.0f, "leaf");

    Rect available_rect{10, 20, 80, 60};

    std::cout << "\n=== TEST: Leaf Node ===\n";
    std::cout << "Available rect: ";
    print_rect("", available_rect);

    auto result = calculate_layout(*root, available_rect);

    print_rendered_rects(result);

    // Check for overlaps
    REQUIRE_FALSE(check_overlaps_detailed(result));

    // Check bounds
    REQUIRE_FALSE(check_bounds_detailed(result, available_rect));

    // Should have 1 rectangle matching the available rect
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].rect.x == 10);
    REQUIRE(result[0].rect.y == 20);
    REQUIRE(result[0].rect.width == 80);
    REQUIRE(result[0].rect.height == 60);
}