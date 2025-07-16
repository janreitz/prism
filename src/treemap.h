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

float shorter_side(const Rect &r) { return std::min(r.width, r.height); }
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
std::vector<RenderedRect<T>> calculate_layout(const T &root,
                                              const Rect &available_rect)
{
    auto children = root.children();
    if (children.empty()) {
        // Leaf node - return single rectangle
        return {RenderedRect<T>(&root, available_rect.x, available_rect.y,
                                available_rect.width, available_rect.height)};
    }

    // Convert to const pointers
    std::vector<const T *> const_children;
    for (T *child : children) {
        const_children.push_back(child);
    }

    // Calculate layout for all children using squarify
    auto child_layouts = squarify_layout(const_children, available_rect);

    // Recursively calculate layout for each child and flatten results
    std::vector<RenderedRect<T>> result;
    for (const auto &[child_node, child_rect] : child_layouts) {
        auto child_rects = calculate_layout(*child_node, child_rect);
        result.insert(result.end(), child_rects.begin(), child_rects.end());
    }

    // Assert rectangles are non-overlapping and within bounds
    auto rect_view =
        result | std::views::transform([](const auto &rr) { return rr.rect; });
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

    return result;
}

template <TreeNode T>
std::vector<std::pair<const T *, Rect>>
squarify_layout(const std::vector<const T *> &children,
                const Rect &available_rect)
{
    if (children.empty() || available_rect.width <= 0 ||
        available_rect.height <= 0)
        return {};

    if (children.size() == 1) {
        return {{children[0], available_rect}};
    }

    // Sort children by decreasing size for better layout
    std::vector<const T *> sorted_children = children;
    std::sort(sorted_children.begin(), sorted_children.end(),
              [](const T *a, const T *b) { return a->size() > b->size(); });

    // Start squarify algorithm
    return squarify_recursive(sorted_children, {}, available_rect);
}

template <TreeNode T>
std::vector<std::pair<const T *, Rect>>
squarify_recursive(const std::vector<const T *> &children,
                   const std::vector<const T *> &current_row,
                   const Rect &available_rect)
{
    if (children.empty()) {
        if (current_row.empty()) {
            return {};
        }
        // Layout the final row and return positioned rectangles
        return position_row(current_row, available_rect);
    }

    const T *next_child = children[0];
    std::vector<const T *> remaining_children(children.begin() + 1,
                                              children.end());

    if (current_row.empty()) {
        // Start new row with first child
        std::vector<const T *> new_row = {next_child};
        return squarify_recursive(remaining_children, new_row, available_rect);
    }

    // Test if adding next child to current row improves layout
    std::vector<const T *> test_row = current_row;
    test_row.push_back(next_child);

    float current_worst =
        worst_aspect_ratio(current_row, shorter_side(available_rect));
    float test_worst =
        worst_aspect_ratio(test_row, shorter_side(available_rect));

    if (test_worst <= current_worst) {
        // Adding child improves layout - continue building current row
        return squarify_recursive(remaining_children, test_row, available_rect);
    } else {
        // Adding child worsens layout - finalize current row and start new
        // one
        auto row_results = position_row(current_row, available_rect);
        Rect remaining_rect = layout_row(current_row, available_rect);

        // Continue with remaining children in remaining space
        std::vector<const T *> new_row = {next_child};
        auto remaining_results =
            squarify_recursive(remaining_children, new_row, remaining_rect);

        // Combine results
        row_results.insert(row_results.end(), remaining_results.begin(),
                           remaining_results.end());
        return row_results;
    }
}

template <TreeNode T>
std::vector<std::pair<const T *, Rect>>
position_row(const std::vector<const T *> &row, const Rect &available_rect)
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

template <TreeNode T>
Rect layout_row(const std::vector<const T *> &row, const Rect &available_rect)
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
    float row_width = shorter_side(available_rect);
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
} // namespace treemap