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

// ========================================
// UNIT TESTS FOR INDIVIDUAL FUNCTIONS
// ========================================

TEST_CASE("Geometry Functions", "[geometry]") {
    SECTION("width function") {
        Rect rect1{0, 0, 100, 50};  // wider than tall
        REQUIRE(width(rect1) == 50);
        
        Rect rect2{0, 0, 30, 80};   // taller than wide
        REQUIRE(width(rect2) == 30);
        
        Rect rect3{0, 0, 50, 50};   // square
        REQUIRE(width(rect3) == 50);
    }
    
    SECTION("is_horizontal function") {
        Rect rect1{0, 0, 100, 50};  // wider than tall
        REQUIRE(is_horizontal(rect1) == true);
        
        Rect rect2{0, 0, 30, 80};   // taller than wide
        REQUIRE(is_horizontal(rect2) == false);
        
        Rect rect3{0, 0, 50, 50};   // square
        REQUIRE(is_horizontal(rect3) == true);  // width >= height
    }
    
    SECTION("overlaps function") {
        Rect rect1{0, 0, 50, 50};
        Rect rect2{25, 25, 50, 50};  // overlaps
        Rect rect3{60, 60, 30, 30};  // no overlap
        Rect rect4{50, 0, 30, 30};   // touching edge (no overlap)
        
        REQUIRE(overlaps(rect1, rect2) == true);
        REQUIRE(overlaps(rect1, rect3) == false);
        REQUIRE(overlaps(rect1, rect4) == false);  // touching doesn't count as overlap
    }
    
    SECTION("within_bounds function") {
        Rect bounds{0, 0, 100, 100};
        Rect inside{10, 10, 80, 80};
        Rect outside{50, 50, 80, 80};  // exceeds bounds
        Rect negative{-10, 10, 50, 50}; // negative position
        
        REQUIRE(within_bounds(inside, bounds) == true);
        REQUIRE(within_bounds(outside, bounds) == false);
        REQUIRE(within_bounds(negative, bounds) == false);
    }
}

TEST_CASE("Worst Aspect Ratio Function", "[worst_aspect_ratio]") {
    auto node1 = std::make_unique<MockTreeNode>(100.0f, "node1");
    auto node2 = std::make_unique<MockTreeNode>(50.0f, "node2");
    auto node3 = std::make_unique<MockTreeNode>(25.0f, "node3");
    
    std::vector<const MockTreeNode*> row1 = {node1.get()};
    std::vector<const MockTreeNode*> row2 = {node1.get(), node2.get()};
    std::vector<const MockTreeNode*> row3 = {node1.get(), node2.get(), node3.get()};
    
    SECTION("Single element row") {
        float ratio = worst_aspect_ratio(row1, 100.0f);
        REQUIRE(ratio > 0);
        std::cout << "Single element aspect ratio: " << ratio << std::endl;
    }
    
    SECTION("Two element row") {
        float ratio = worst_aspect_ratio(row2, 100.0f);
        REQUIRE(ratio > 0);
        std::cout << "Two element aspect ratio: " << ratio << std::endl;
    }
    
    SECTION("Three element row") {
        float ratio = worst_aspect_ratio(row3, 100.0f);
        REQUIRE(ratio > 0);
        std::cout << "Three element aspect ratio: " << ratio << std::endl;
    }
    
    SECTION("Empty row") {
        std::vector<const MockTreeNode*> empty_row;
        float ratio = worst_aspect_ratio(empty_row, 100.0f);
        REQUIRE(ratio == std::numeric_limits<float>::max());
    }
    
    SECTION("Zero width") {
        float ratio = worst_aspect_ratio(row1, 0.0f);
        REQUIRE(ratio == std::numeric_limits<float>::max());
    }
}

TEST_CASE("Layoutrow Function", "[layoutrow]") {
    auto node1 = std::make_unique<MockTreeNode>(100.0f, "node1");
    auto node2 = std::make_unique<MockTreeNode>(50.0f, "node2");
    
    std::vector<const MockTreeNode*> row = {node1.get(), node2.get()};
    
    SECTION("Horizontal rectangle") {
        Rect available{0, 0, 200, 100};  // wider than tall
        auto result = layoutrow(row, available);
        
        REQUIRE(result.size() == 2);
        
        std::cout << "\n=== Layoutrow Test: Horizontal ===\n";
        for (size_t i = 0; i < result.size(); ++i) {
            const auto& [node, rect] = result[i];
            std::cout << "Node " << node->name() << " (size=" << node->size() << "): ";
            print_rect("", rect);
        }
        
        // Check that rectangles don't overlap
        REQUIRE_FALSE(overlaps(result[0].second, result[1].second));
        
        // Check that both are within bounds
        REQUIRE(within_bounds(result[0].second, available));
        REQUIRE(within_bounds(result[1].second, available));
    }
    
    SECTION("Vertical rectangle") {
        Rect available{0, 0, 100, 200};  // taller than wide
        auto result = layoutrow(row, available);
        
        REQUIRE(result.size() == 2);
        
        std::cout << "\n=== Layoutrow Test: Vertical ===\n";
        for (size_t i = 0; i < result.size(); ++i) {
            const auto& [node, rect] = result[i];
            std::cout << "Node " << node->name() << " (size=" << node->size() << "): ";
            print_rect("", rect);
        }
        
        // Check that rectangles don't overlap
        REQUIRE_FALSE(overlaps(result[0].second, result[1].second));
        
        // Check that both are within bounds
        REQUIRE(within_bounds(result[0].second, available));
        REQUIRE(within_bounds(result[1].second, available));
    }
    
    SECTION("Empty row") {
        std::vector<const MockTreeNode*> empty_row;
        Rect available{0, 0, 100, 100};
        auto result = layoutrow(empty_row, available);
        
        REQUIRE(result.empty());
    }
}

TEST_CASE("Remaining Space After Row", "[remaining_space]") {
    auto node1 = std::make_unique<MockTreeNode>(100.0f, "node1");
    auto node2 = std::make_unique<MockTreeNode>(50.0f, "node2");
    
    std::vector<const MockTreeNode*> row = {node1.get(), node2.get()};
    
    SECTION("Horizontal rectangle") {
        Rect available{0, 0, 200, 100};  // wider than tall
        Rect remaining = remaining_space_after_row(row, available);
        
        std::cout << "\n=== Remaining Space Test: Horizontal ===\n";
        print_rect("Original", available);
        print_rect("Remaining", remaining);
        
        // Remaining space should be within original bounds
        REQUIRE(within_bounds(remaining, available));
        
        // Remaining space should have positive dimensions
        REQUIRE(remaining.width > 0);
        REQUIRE(remaining.height > 0);
    }
    
    SECTION("Vertical rectangle") {
        Rect available{0, 0, 100, 200};  // taller than wide
        Rect remaining = remaining_space_after_row(row, available);
        
        std::cout << "\n=== Remaining Space Test: Vertical ===\n";
        print_rect("Original", available);
        print_rect("Remaining", remaining);
        
        // Remaining space should be within original bounds
        REQUIRE(within_bounds(remaining, available));
        
        // Remaining space should have positive dimensions
        REQUIRE(remaining.width > 0);
        REQUIRE(remaining.height > 0);
    }
    
    SECTION("Empty row") {
        std::vector<const MockTreeNode*> empty_row;
        Rect available{0, 0, 100, 100};
        Rect remaining = remaining_space_after_row(empty_row, available);
        
        // Should return unchanged rectangle
        REQUIRE(remaining.x == available.x);
        REQUIRE(remaining.y == available.y);
        REQUIRE(remaining.width == available.width);
        REQUIRE(remaining.height == available.height);
    }
}

TEST_CASE("Squarify Function", "[squarify]") {
    SECTION("Two elements") {
        auto node1 = std::make_unique<MockTreeNode>(100.0f, "large");
        auto node2 = std::make_unique<MockTreeNode>(50.0f, "small");
        
        std::vector<const MockTreeNode*> children = {node1.get(), node2.get()};
        Rect available{0, 0, 200, 100};
        
        auto result = squarify(children, available);
        
        std::cout << "\n=== Squarify Test: Two Elements ===\n";
        print_rect("Available", available);
        for (size_t i = 0; i < result.size(); ++i) {
            const auto& [node, rect] = result[i];
            std::cout << "Node " << node->name() << " (size=" << node->size() << "): ";
            print_rect("", rect);
        }
        
        REQUIRE(result.size() == 2);
        
        // Check no overlaps
        REQUIRE_FALSE(overlaps(result[0].second, result[1].second));
        
        // Check within bounds
        REQUIRE(within_bounds(result[0].second, available));
        REQUIRE(within_bounds(result[1].second, available));
    }
    
    SECTION("Three elements") {
        auto node1 = std::make_unique<MockTreeNode>(100.0f, "large");
        auto node2 = std::make_unique<MockTreeNode>(50.0f, "medium");
        auto node3 = std::make_unique<MockTreeNode>(25.0f, "small");
        
        std::vector<const MockTreeNode*> children = {node1.get(), node2.get(), node3.get()};
        Rect available{0, 0, 300, 200};
        
        auto result = squarify(children, available);
        
        std::cout << "\n=== Squarify Test: Three Elements ===\n";
        print_rect("Available", available);
        for (size_t i = 0; i < result.size(); ++i) {
            const auto& [node, rect] = result[i];
            std::cout << "Node " << node->name() << " (size=" << node->size() << "): ";
            print_rect("", rect);
        }
        
        REQUIRE(result.size() == 3);
        
        // Check no overlaps between any pair
        for (size_t i = 0; i < result.size(); ++i) {
            for (size_t j = i + 1; j < result.size(); ++j) {
                REQUIRE_FALSE(overlaps(result[i].second, result[j].second));
            }
        }
        
        // Check within bounds
        for (const auto& [node, rect] : result) {
            REQUIRE(within_bounds(rect, available));
        }
    }
    
    SECTION("Empty input") {
        std::vector<const MockTreeNode*> empty_children;
        Rect available{0, 0, 100, 100};
        
        auto result = squarify(empty_children, available);
        REQUIRE(result.empty());
    }
}