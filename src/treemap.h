#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include <imgui.h> // For ImVec2 -> TODO create custom vec type to avoid this dependency here

namespace treemap
{

template <typename T>
concept TreeNode = requires(T t) {
    { t.size() } -> std::convertible_to<float>;
    { t.parent() } -> std::convertible_to<T *>;
    { t.children() } -> std::convertible_to<std::vector<T *>>;
};

struct Rect {
    float x, y, width, height;
};

float width(const Rect &r) { return std::min(r.width, r.height); }
bool is_horizontal(const Rect &r) { return r.width >= r.height; }
ImVec2 rect_min(const Rect &r) { return {r.x, r.y}; }
ImVec2 rect_max(const Rect &r) { return {r.x + r.width, r.y + r.height}; }

bool overlaps(const Rect &a, const Rect &b)
{
    // Two rectangles overlap if they overlap in both x and y dimensions
    bool x_overlap = a.x < b.x + b.width && b.x < a.x + a.width;
    bool y_overlap = a.y < b.y + b.height && b.y < a.y + a.height;
    return x_overlap && y_overlap;
}

bool within_bounds(const Rect &rect, const Rect &bounds)
{
    return rect.x >= bounds.x && rect.y >= bounds.y &&
           rect.x + rect.width <= bounds.x + bounds.width &&
           rect.y + rect.height <= bounds.y + bounds.height;
}

template <TreeNode T> struct RenderedRect {
    const T *node;
    Rect rect;

    RenderedRect(const T *n, float x_, float y_, float w_, float h_)
        : node(n), rect(Rect{.x = x_, .y = y_, .width = w_, .height = h_})
    {
    }
};

template <TreeNode T>
auto hit_test(ImVec2 test, const std::vector<RenderedRect<T>> &rects,
              ImVec2 offset) -> const T *
{
    auto hovered_rect_it =
        std::find_if(rects.cbegin(), rects.cend(),
                     [&offset, &test](const RenderedRect<T> &rect) -> bool {
                         const ImVec2 rect_min(offset.x + rect.rect.x,
                                               offset.y + rect.rect.y);
                         const ImVec2 rect_max(rect_min.x + rect.rect.width,
                                               rect_min.y + rect.rect.height);
                         return test.x >= rect_min.x && test.x <= rect_max.x &&
                                test.y >= rect_min.y && test.y <= rect_max.y;
                     });

    if (hovered_rect_it == rects.cend()) {
        return nullptr;
    } else {
        return hovered_rect_it->node;
    }
};

template <TreeNode T>
float worst_aspect_ratio(const std::vector<const T *> &row, float row_width)
{
    if (row.empty() || row_width <= 0) {
        return std::numeric_limits<float>::max();
    }

    float total_area = 0;
    for (const T *node : row) {
        total_area += node->size();
    }

    if (total_area <= 0) {
        return std::numeric_limits<float>::max();
    }

    float row_height = total_area / row_width;
    float max_aspect_ratio = 0;

    for (const T *node : row) {
        float rect_width = node->size() / row_height;
        float aspect_ratio =
            std::max(rect_width / row_height, row_height / rect_width);
        max_aspect_ratio = std::max(max_aspect_ratio, aspect_ratio);
    }

    return max_aspect_ratio;
}

template <TreeNode T>
void validate_layout(const std::vector<RenderedRect<T>> &layout,
                     const Rect &available_rect)
{
    // Assert rectangles are non-overlapping and within bounds
    auto rect_view =
        layout | std::views::transform([](const auto &rr) { return rr.rect; });
    auto combination_view = std::views::cartesian_product(rect_view, rect_view);

    // Check no overlaps
    assert(std::ranges::none_of(combination_view, [](const auto &pair) {
        const auto &[rect1, rect2] = pair;
        return &rect1 != &rect2 && overlaps(rect1, rect2);
    }));

    // Check all rectangles are within bounds
    assert(std::ranges::all_of(rect_view, [&](const Rect &rect) {
        return within_bounds(rect, available_rect);
    }));
}

// Main entry point for calculating layout - handles tree traversal and
// flattening
template <TreeNode T>
std::vector<RenderedRect<T>> calculate_layout(const T &root,
                                              const Rect &available_rect)
{
    auto children = root.children();
    if (children.empty()) {
        // Leaf node - return single rectangle
        return {RenderedRect<T>(&root, available_rect.x, available_rect.y,
                                available_rect.width, available_rect.height)};
    }

    // Convert to const pointers and sort by decreasing size
    std::vector<const T *> sorted_children;
    for (T *child : children) {
        sorted_children.push_back(child);
    }
    std::sort(sorted_children.begin(), sorted_children.end(),
              [](const T *a, const T *b) { return a->size() > b->size(); });

    auto child_layouts = squarify(sorted_children, available_rect);

    // Recursively calculate layout for each child and flatten results
    std::vector<RenderedRect<T>> result;
    for (const auto &[child_node, child_rect] : child_layouts) {
        auto child_rects = calculate_layout(*child_node, child_rect);
        result.insert(result.end(), child_rects.begin(), child_rects.end());
    }

    // Validate the result
    validate_layout(result, available_rect);

    return result;
}

// Calculate remaining space after laying out a row (was layout_row)
template <TreeNode T>
Rect remaining_space_after_row(const std::vector<const T *> &row,
                               const Rect &available_rect)
{
    if (row.empty()) {
        return available_rect;
    }

    float total_area = 0;
    for (const T *node : row) {
        total_area += node->size();
    }

    if (total_area <= 0) {
        return available_rect;
    }

    // Determine row orientation based on available space
    bool horizontal = is_horizontal(available_rect);
    float row_width = width(available_rect);
    float row_height = total_area / row_width;

    // Calculate remaining space after placing this row
    Rect remaining_rect = available_rect;
    if (horizontal) {
        remaining_rect.x += row_width;
        remaining_rect.width -= row_width;
    } else {
        remaining_rect.y += row_height;
        remaining_rect.height -= row_height;
    }

    return remaining_rect;
}

// Core squarify algorithm - iterative implementation for single-level layout
template <TreeNode T>
std::vector<std::pair<const T *, Rect>>
squarify(const std::vector<const T *> &children, const Rect &available_rect)
{
    std::vector<std::pair<const T *, Rect>> results;

    if (children.empty()) {
        return results;
    }

    std::vector<const T *> remaining_children = children;
    std::vector<const T *> current_row;
    Rect current_rect = available_rect;

    while (!remaining_children.empty()) {
        const T *next_child = remaining_children[0];
        remaining_children.erase(remaining_children.begin());

        std::vector<const T *> test_row = current_row;
        test_row.push_back(next_child);

        float w = width(current_rect);
        float current_worst = worst_aspect_ratio(current_row, w);
        float test_worst = worst_aspect_ratio(test_row, w);

        if (current_row.empty() || test_worst <= current_worst) {
            // Add to current row - aspect ratio improves or stays same
            current_row.push_back(next_child);
        } else {
            // Flush current row and start new one - aspect ratio would worsen
            auto row_results = layoutrow<T>(current_row, current_rect);
            results.insert(results.end(), row_results.begin(),
                           row_results.end());

            current_rect =
                remaining_space_after_row<T>(current_row, current_rect);
            current_row = {next_child};
        }
    }

    // Flush final row
    if (!current_row.empty()) {
        auto row_results = layoutrow<T>(current_row, current_rect);
        results.insert(results.end(), row_results.begin(), row_results.end());
    }

    return results;
}

template <TreeNode T>
std::vector<std::pair<const T *, Rect>>
layoutrow(const std::vector<const T *> &row, const Rect &available_rect)
{
    std::vector<std::pair<const T *, Rect>> results;

    if (row.empty()) {
        return results;
    }

    float total_area = 0;
    for (const T *node : row) {
        total_area += node->size();
    }

    if (total_area <= 0) {
        return results;
    }

    // Determine row orientation based on available space
    bool horizontal = is_horizontal(available_rect);
    float row_width = shorter_side(available_rect);
    float row_height = total_area / row_width;

    // Position each rectangle in the row
    float current_offset = 0;
    for (const T *node : row) {
        Rect rect;
        if (horizontal) {
            float rect_height = node->size() / row_width;
            rect = {available_rect.x, available_rect.y + current_offset,
                    row_width, rect_height};
            current_offset += rect_height;
        } else {
            float rect_width = node->size() / row_height;
            rect = {available_rect.x + current_offset, available_rect.y,
                    rect_width, row_height};
            current_offset += rect_width;
        }
        results.emplace_back(node, rect);
    }

    return results;
}

} // namespace treemap