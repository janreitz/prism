#include "../src/filesystem_node.h"
#include "../src/treemap.h"
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <memory>
#include <variant>
#include <vector>

using namespace treemap;

// Mock tree node for testing
class MockTreeNode
{
  public:
    // Constructor for leaf node
    MockTreeNode(const std::string &name, float size)
        : size_or_children_(size), name_(name)
    {
    }

    // Constructor for branch node
    MockTreeNode(const std::string &name,
                 std::vector<std::unique_ptr<MockTreeNode>> &&children)
        : size_or_children_(std::move(children)), name_(name)
    {
        // Set parent pointers for all children
        for (auto &child : std::get<std::vector<std::unique_ptr<MockTreeNode>>>(
                 size_or_children_)) {
            child->parent_ = this;
        }
    }

    float size() const
    {
        if (std::holds_alternative<float>(size_or_children_)) {
            return std::get<float>(size_or_children_);
        } else {
            float total = 0.0f;
            for (const auto &child :
                 std::get<std::vector<std::unique_ptr<MockTreeNode>>>(
                     size_or_children_)) {
                total += child->size();
            }
            return total;
        }
    }

    MockTreeNode *parent() const { return parent_; }

    std::vector<const MockTreeNode *> children() const
    {
        if (std::holds_alternative<std::vector<std::unique_ptr<MockTreeNode>>>(
                size_or_children_)) {
            const auto &owned_children =
                std::get<std::vector<std::unique_ptr<MockTreeNode>>>(
                    size_or_children_);
            std::vector<const MockTreeNode *> result;
            result.reserve(owned_children.size());
            for (const auto &child : owned_children) {
                result.push_back(child.get());
            }
            return result;
        } else {
            return {};
        }
    }

    void add_child(std::unique_ptr<MockTreeNode> child)
    {
        child->parent_ = this;

        if (std::holds_alternative<float>(size_or_children_)) {
            // Convert from leaf to parent node
            size_or_children_ = std::vector<std::unique_ptr<MockTreeNode>>();
        }

        std::get<std::vector<std::unique_ptr<MockTreeNode>>>(size_or_children_)
            .push_back(std::move(child));
    }

    const std::string &name() const { return name_; }

  private:
    std::variant<float, std::vector<std::unique_ptr<MockTreeNode>>>
        size_or_children_;
    std::string name_;
    MockTreeNode *parent_ = nullptr;
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
        std::cout << "Rect " << i << " ('" << rects[i].node_->name() << "'): ";
        print_rect("", rects[i].rect_);
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
            if (overlaps(rects[i].rect_, rects[j].rect_)) {
                std::cout << "OVERLAP FOUND!\n";
                std::cout << "  Rect " << i << " ('" << rects[i].node_->name()
                          << "'): ";
                print_rect("", rects[i].rect_);
                std::cout << "  Rect " << j << " ('" << rects[j].node_->name()
                          << "'): ";
                print_rect("", rects[j].rect_);
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
        if (!within_bounds(rects[i].rect_, bounds)) {
            std::cout << "OUT OF BOUNDS!\n";
            std::cout << "  Rect " << i << " ('" << rects[i].node_->name()
                      << "'): ";
            print_rect("", rects[i].rect_);
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
    std::vector<std::unique_ptr<MockTreeNode>> children;
    children.push_back(std::make_unique<MockTreeNode>("child1", 60.0f));
    children.push_back(std::make_unique<MockTreeNode>("child2", 40.0f));
    auto root = std::make_unique<MockTreeNode>("root", std::move(children));

    Rect available_rect{0, 0, 100, 100};

    // Test the layout calculation directly
    std::cout << "\n=== TEST: Simple Case ===\n";
    std::cout << "Available rect: ";
    print_rect("", available_rect);

    auto result = layout(*root, available_rect);

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
    std::vector<std::unique_ptr<MockTreeNode>> children;
    children.push_back(std::make_unique<MockTreeNode>("large", 50.0f));
    children.push_back(std::make_unique<MockTreeNode>("medium", 30.0f));
    children.push_back(std::make_unique<MockTreeNode>("small", 20.0f));
    auto root = std::make_unique<MockTreeNode>("root", std::move(children));

    Rect available_rect{0, 0, 200, 100};

    std::cout << "\n=== TEST: Three Children ===\n";
    std::cout << "Available rect: ";
    print_rect("", available_rect);

    auto result = layout(*root, available_rect);

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
    std::vector<std::unique_ptr<MockTreeNode>> dir1_children;
    dir1_children.push_back(
        std::make_unique<MockTreeNode>("dir1_file1", 40.0f));
    dir1_children.push_back(
        std::make_unique<MockTreeNode>("dir1_file2", 30.0f));
    auto dir1 =
        std::make_unique<MockTreeNode>("dir1", std::move(dir1_children));

    std::vector<std::unique_ptr<MockTreeNode>> dir2_children;
    dir2_children.push_back(
        std::make_unique<MockTreeNode>("dir2_file1", 20.0f));
    dir2_children.push_back(
        std::make_unique<MockTreeNode>("dir2_file2", 10.0f));
    auto dir2 =
        std::make_unique<MockTreeNode>("dir2", std::move(dir2_children));

    std::vector<std::unique_ptr<MockTreeNode>> root_children;
    root_children.push_back(std::move(dir1));
    root_children.push_back(std::move(dir2));
    auto root =
        std::make_unique<MockTreeNode>("root", std::move(root_children));

    Rect available_rect{0, 0, 400, 300};

    std::cout << "\n=== TEST: Nested Structure ===\n";
    std::cout << "Available rect: ";
    print_rect("", available_rect);

    auto result = layout(*root, available_rect);

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
    std::vector<std::unique_ptr<MockTreeNode>> children;
    children.push_back(std::make_unique<MockTreeNode>("only_child", 100.0f));
    auto root = std::make_unique<MockTreeNode>("root", std::move(children));

    Rect available_rect{0, 0, 100, 100};

    std::cout << "\n=== TEST: Single Child ===\n";
    std::cout << "Available rect: ";
    print_rect("", available_rect);

    auto result = layout(*root, available_rect);

    print_rendered_rects(result);

    // Check for overlaps
    REQUIRE_FALSE(check_overlaps_detailed(result));

    // Check bounds
    REQUIRE_FALSE(check_bounds_detailed(result, available_rect));

    // Should have 1 rectangle
    REQUIRE(result.size() == 1);

    // Should fill the entire available space
    REQUIRE(result[0].rect_.x == 0);
    REQUIRE(result[0].rect_.y == 0);
    REQUIRE(result[0].rect_.width == 100);
    REQUIRE(result[0].rect_.height == 100);
}

TEST_CASE("TreeMap Layout - Leaf Node", "[layout]")
{
    // Edge case: leaf node only
    auto root = std::make_unique<MockTreeNode>("leaf", 100.0f);

    Rect available_rect{10, 20, 80, 60};

    std::cout << "\n=== TEST: Leaf Node ===\n";
    std::cout << "Available rect: ";
    print_rect("", available_rect);

    auto result = layout(*root, available_rect);

    print_rendered_rects(result);

    // Check for overlaps
    REQUIRE_FALSE(check_overlaps_detailed(result));

    // Check bounds
    REQUIRE_FALSE(check_bounds_detailed(result, available_rect));

    // Should have 1 rectangle matching the available rect
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].rect_.x == 10.0F);
    REQUIRE(result[0].rect_.y == 20.0F);
    REQUIRE(result[0].rect_.width == 80.0F);
    REQUIRE(result[0].rect_.height == 60.0F);
}

// ========================================
// UNIT TESTS FOR INDIVIDUAL FUNCTIONS
// ========================================

TEST_CASE("Geometry Functions", "[geometry]")
{
    SECTION("width function")
    {
        Rect rect1{0, 0, 100, 50}; // wider than tall
        REQUIRE(shorter_side(rect1) == 50);

        Rect rect2{0, 0, 30, 80}; // taller than wide
        REQUIRE(shorter_side(rect2) == 30);

        Rect rect3{0, 0, 50, 50}; // square
        REQUIRE(shorter_side(rect3) == 50);
    }

    SECTION("overlaps function")
    {
        Rect rect1{0, 0, 50, 50};
        Rect rect2{25, 25, 50, 50}; // overlaps
        Rect rect3{60, 60, 30, 30}; // no overlap
        Rect rect4{50, 0, 30, 30};  // touching edge (no overlap)

        REQUIRE(overlaps(rect1, rect2) == true);
        REQUIRE(overlaps(rect1, rect3) == false);
        REQUIRE(overlaps(rect1, rect4) ==
                false); // touching doesn't count as overlap
    }

    SECTION("within_bounds function")
    {
        Rect bounds{0, 0, 100, 100};
        Rect inside{10, 10, 80, 80};
        Rect outside{50, 50, 80, 80};   // exceeds bounds
        Rect negative{-10, 10, 50, 50}; // negative position

        REQUIRE(within_bounds(inside, bounds) == true);
        REQUIRE(within_bounds(outside, bounds) == false);
        REQUIRE(within_bounds(negative, bounds) == false);
    }
}

TEST_CASE("Squarify Function", "[squarify]")
{
    // SECTION("Two elements")
    // {
    //     auto node1 = std::make_unique<MockTreeNode>("large", 100.0f);
    //     auto node2 = std::make_unique<MockTreeNode>("small", 50.0f);

    //     std::vector<const MockTreeNode *> children = {node1.get(),
    //     node2.get()}; Rect available{0, 0, 10.0F, 15.0F};

    //     auto result = squarify(children, available);

    //     std::cout << "\n=== Squarify Test: Two Elements ===\n";
    //     print_rect("Available", available);
    //     for (size_t i = 0; i < result.size(); ++i) {
    //         const auto &[node, rect] = result[i];
    //         std::cout << "Node " << node->name() << " (size=" << node->size()
    //                   << "): ";
    //         print_rect("", rect);
    //     }

    //     REQUIRE(result.size() == 2);

    //     // Check no overlaps
    //     REQUIRE_FALSE(overlaps(result[0].rect_, result[1].rect_));

    //     // Check within bounds
    //     REQUIRE(within_bounds(result[0].rect_, available));
    //     REQUIRE(within_bounds(result[1].rect_, available));
    // }

    // SECTION("Three elements")
    // {
    //     auto node1 = std::make_unique<MockTreeNode>("large", 100.0f);
    //     auto node2 = std::make_unique<MockTreeNode>("medium", 50.0f);
    //     auto node3 = std::make_unique<MockTreeNode>("small", 25.0f);

    //     std::vector<const MockTreeNode *> children = {node1.get(),
    //     node2.get(),
    //                                                   node3.get()};
    //     Rect available{0, 0, 17.5F, 10.0F};

    //     auto result = squarify(children, available);

    //     std::cout << "\n=== Squarify Test: Three Elements ===\n";
    //     print_rect("Available", available);
    //     for (size_t i = 0; i < result.size(); ++i) {
    //         const auto &[node, rect] = result[i];
    //         std::cout << "Node " << node->name() << " (size=" << node->size()
    //                   << "): ";
    //         print_rect("", rect);
    //     }

    //     REQUIRE(result.size() == 3);

    //     // Check no overlaps between any pair
    //     for (size_t i = 0; i < result.size(); ++i) {
    //         for (size_t j = i + 1; j < result.size(); ++j) {
    //             REQUIRE_FALSE(overlaps(result[i].rect_, result[j].rect_));
    //         }
    //     }

    //     // Check within bounds
    //     for (const auto &[node, rect] : result) {
    //         REQUIRE(within_bounds(rect, available));
    //     }
    // }

    // SECTION("Empty input")
    // {
    //     std::vector<const MockTreeNode *> empty_children;
    //     Rect available{0, 0, 100, 100};

    //     auto result = squarify(empty_children, available);
    //     REQUIRE(result.empty());
    // }

    SECTION("Paper example")
    {
        auto node1 = std::make_unique<MockTreeNode>("A", 6.0f);
        auto node2 = std::make_unique<MockTreeNode>("B", 6.0f);
        auto node3 = std::make_unique<MockTreeNode>("C", 4.0f);
        auto node4 = std::make_unique<MockTreeNode>("D", 3.0f);
        auto node5 = std::make_unique<MockTreeNode>("E", 2.0f);
        auto node6 = std::make_unique<MockTreeNode>("F", 2.0f);
        auto node7 = std::make_unique<MockTreeNode>("G", 1.0f);

        std::vector<const MockTreeNode *> children = {
            node1.get(), node2.get(), node3.get(), node4.get(),
            node5.get(), node6.get(), node7.get()};
        Rect available{0, 0, 6.0F, 4.0F};

        auto result = squarify(children, available);

        std::cout << "\n=== Squarify Test: Paper Example ===\n";
        print_rect("Available", available);
        for (size_t i = 0; i < result.size(); ++i) {
            const auto &[node, rect] = result[i];
            std::cout << "Node " << node->name() << " (size=" << node->size()
                      << "): ";
            print_rect("", rect);
        }
    }
}

TEST_CASE("Tree Traversal Coordinate Bug", "[tree_traversal]")
{
    SECTION(
        "Child rectangles should be positioned in parent's coordinate space")
    {
        // Create a simple nested structure
        std::vector<std::unique_ptr<MockTreeNode>> dir1_children;
        dir1_children.push_back(
            std::make_unique<MockTreeNode>("dir1_file1", 30.0f));
        dir1_children.push_back(
            std::make_unique<MockTreeNode>("dir1_file2", 30.0f));
        auto dir1 =
            std::make_unique<MockTreeNode>("dir1", std::move(dir1_children));

        std::vector<std::unique_ptr<MockTreeNode>> dir2_children;
        dir2_children.push_back(
            std::make_unique<MockTreeNode>("dir2_file1", 20.0f));
        dir2_children.push_back(
            std::make_unique<MockTreeNode>("dir2_file2", 20.0f));
        auto dir2 =
            std::make_unique<MockTreeNode>("dir2", std::move(dir2_children));

        std::vector<std::unique_ptr<MockTreeNode>> root_children;
        root_children.push_back(std::move(dir1));
        root_children.push_back(std::move(dir2));
        auto root =
            std::make_unique<MockTreeNode>("root", std::move(root_children));

        Rect available{0, 0, 5, 20};

        std::cout << "\n=== Tree Traversal Coordinate Test ===\n";
        std::cout << "Available rect: ";
        print_rect("", available);

        // First, let's see what squarify does at the root level
        std::vector<const MockTreeNode *> root_child_ptrs = {
            root->children()[0], root->children()[1]};
        auto root_layout = squarify(root_child_ptrs, available);

        std::cout << "\nRoot level layout (dir1 and dir2):\n";
        for (const auto &[node, rect] : root_layout) {
            std::cout << "  " << node->name() << ": ";
            print_rect("", rect);
        }

        // Now let's see what happens in layout
        auto result = layout(*root, available);

        std::cout << "\nFull tree layout result:\n";
        for (size_t i = 0; i < result.size(); ++i) {
            std::cout << "  Rect " << i << " ('" << result[i].node_->name()
                      << "'): ";
            print_rect("", result[i].rect_);
        }

        // The bug: child rectangles should be positioned within their parent's
        // allocated space Let's check if dir2's children are positioned in
        // dir2's space, not overlapping with dir1

        // Find dir2's allocated rectangle
        Rect dir2_space;
        for (const auto &[node, rect] : root_layout) {
            if (node->name() == "dir2") {
                dir2_space = rect;
                break;
            }
        }

        std::cout << "\nDir2's allocated space: ";
        print_rect("", dir2_space);

        // All of dir2's children should be within dir2's space
        for (const auto &rendered : result) {
            if (rendered.node_->name().find("dir2_") == 0) {
                std::cout << "Checking if " << rendered.node_->name()
                          << " is within dir2's space:\n";
                std::cout << "  Child rect: ";
                print_rect("", rendered.rect_);
                std::cout << "  Within parent space: "
                          << within_bounds(rendered.rect_, dir2_space) << "\n";

                // This should pass but probably fails due to coordinate bug
                REQUIRE(within_bounds(rendered.rect_, dir2_space));
            }
        }
    }
}