#include <algorithm>
#include <cassert>
#include <cmath>
#include <execution>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <queue>
#include <ranges>
#include <string>
#include <vector>

#include <imgui.h> // For ImVec2 -> TODO create custom vec type to avoid this dependency here
#include <tracy/Tracy.hpp>

namespace treemap
{

template <typename T>
concept TreeNode = requires(T t) {
    { t.size() } -> std::convertible_to<float>;
    { t.children() } -> std::convertible_to<std::vector<const T *>>;
};

struct Rect {
    float x, y, width, height;
};

float shorter_side(const Rect &r) { return std::min(r.width, r.height); }
float area(const Rect &r) { return r.width * r.height; }
enum class RectOrientation { horizontal, vertical };
RectOrientation orientation(const Rect &r)
{
    return r.width >= r.height ? RectOrientation::horizontal
                               : RectOrientation::vertical;
}
ImVec2 rect_min(const Rect &r) { return {r.x, r.y}; }
ImVec2 rect_max(const Rect &r) { return {r.x + r.width, r.y + r.height}; }

bool less_than_or_equal(const ImVec2 &a, const ImVec2 &b)
{
    return a.x <= b.x || a.y <= b.y;
}

bool overlaps(const Rect &a, const Rect &b)
{
    return !(less_than_or_equal(rect_max(a), rect_min(b)) ||
             less_than_or_equal(rect_max(b), rect_min(a)));
}

bool within_bounds(const Rect &rect, const Rect &bounds)
{
    return rect.x >= bounds.x && rect.y >= bounds.y &&
           rect.x + rect.width <= bounds.x + bounds.width &&
           rect.y + rect.height <= bounds.y + bounds.height;
}

template <TreeNode T> struct RenderedRect {
    const T *node_;
    Rect rect_;

    RenderedRect(const T *n, Rect rect) : node_(n), rect_(rect) {}
};

template <TreeNode T>
auto hit_test(ImVec2 test, const std::vector<RenderedRect<T>> &rects,
              ImVec2 offset) -> const T *
{
    auto hovered_rect_it =
        std::find_if(rects.cbegin(), rects.cend(),
                     [&offset, &test](const RenderedRect<T> &rect) -> bool {
                         const ImVec2 rect_min(offset.x + rect.rect_.x,
                                               offset.y + rect.rect_.y);
                         const ImVec2 rect_max(rect_min.x + rect.rect_.width,
                                               rect_min.y + rect.rect_.height);
                         return test.x >= rect_min.x && test.x <= rect_max.x &&
                                test.y >= rect_min.y && test.y <= rect_max.y;
                     });

    if (hovered_rect_it == rects.cend()) {
        return nullptr;
    } else {
        return hovered_rect_it->node_;
    }
};

template <TreeNode T>
float worst_aspect_ratio(const std::vector<const T *> &row, float row_width)
{
    ZoneScoped;
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
    for (size_t i = 0; i < layout.size(); i++) {
        const auto &rect_i = layout[i].rect_;
        assert(within_bounds(rect_i, available_rect));
        for (size_t j = 0; j < layout.size(); j++) {
            if (i == j)
                continue;
            const auto &rect_j = layout[j].rect_;
            assert(!overlaps(rect_i, rect_j));
        }
    }
}

/// @brief  Main entry point for calculating treemap layout - handles scaling
/// relative scaling of screen coordinates/space and size of elements to place
/// @tparam T
/// @param root
/// @param available_rect
/// @return
template <TreeNode T>
std::vector<RenderedRect<T>> layout(const T &root, const Rect &available_rect,
                                    bool parallelize = false)
{
    ZoneScoped;

    // Remove x/y offset and scale width and height, so the area is equal to the
    // total sum of elements to be placed
    const double total_size = static_cast<double>(root.size());
    const double available_size =
        static_cast<double>(available_rect.height * available_rect.width);
    const double scaling_factor = std::sqrt(total_size / available_size);

    Rect available_rect_scaled{
        .x = 0,
        .y = 0,
        .width = static_cast<float>(available_rect.width * scaling_factor),
        .height = static_cast<float>(available_rect.height * scaling_factor),
    };

    // const float calculated_area =
    //     available_rect_scaled.height * available_rect_scaled.width;
    // const float error = std::abs(calculated_area - total_size);
    // if (error >= 1.0f) {
    //     std::cerr << "Area calculation error: " << error
    //               << " (calculated: " << calculated_area
    //               << ", expected: " << total_size << ")" << std::endl;
    //     std::cerr << "Scaling factor: " << scaling_factor << std::endl;
    //     std::cerr << "Available rect: " << available_rect.width << "x"
    //               << available_rect.height << std::endl;
    // }
    // assert(error < 1.0f);

    auto layout_result =
        layout_tree_traversal(root, available_rect_scaled, parallelize);

    // Rescale layout result to screen coordinates and reapply initial screen
    // space offset
    for (auto &rect : layout_result) {
        rect.rect_.x = rect.rect_.x / scaling_factor + available_rect.x;
        rect.rect_.y = rect.rect_.y / scaling_factor + available_rect.y;
        rect.rect_.height /= scaling_factor;
        rect.rect_.width /= scaling_factor;
    }

    // Validate the result
    // validate_layout(layout_result, available_rect);

    return layout_result;
}

template <TreeNode T>
std::vector<RenderedRect<T>> layout_tree_traversal(const T &root,
                                                   const Rect &available_rect,
                                                   bool parallelize)
{
    ZoneScoped;
    auto children = root.children();
    if (children.empty()) {
        // Leaf node - return single rectangle
        return {RenderedRect<T>(&root, Rect{.x = available_rect.x,
                                            .y = available_rect.y,
                                            .width = available_rect.width,
                                            .height = available_rect.height})};
    }

    auto child_layouts = squarify(children, available_rect);

    // Sequential version
    auto sequential_impl = [&]() -> std::vector<RenderedRect<T>> {
        std::vector<RenderedRect<T>> result;
        for (const auto &layout : child_layouts) {
            auto child_rects =
                layout_tree_traversal(*layout.node_, layout.rect_, parallelize);
            result.insert(result.end(), child_rects.begin(), child_rects.end());
        }
        return result;
    };

#if ENABLE_PARALLEL_EXECUTION
    if (parallelize) {
        // Parallel version
        return std::transform_reduce(
            std::execution::par, child_layouts.begin(), child_layouts.end(),
            std::vector<RenderedRect<T>>{},
            [](std::vector<RenderedRect<T>> a, std::vector<RenderedRect<T>> b) {
                a.insert(a.end(), b.begin(), b.end());
                return a;
            },
            [parallelize](const auto &layout) {
                return layout_tree_traversal(*layout.node_, layout.rect_,
                                             parallelize);
            });
    } else {
        return sequential_impl();
    }
#else
    return sequential_impl();
#endif
}

template <typename T> float row_area(const std::vector<const T *> &row)
{
    return std::accumulate(row.cbegin(), row.cend(), 0.0F,
                           [](float total_area, const T *node) {
                               return total_area + node->size();
                           });
}

// Core squarify algorithm - iterative implementation for single-level layout
template <TreeNode T>
std::vector<RenderedRect<T>> squarify(const std::vector<const T *> &children,
                                      const Rect &available_rect)
{
    ZoneScoped;
    if (children.empty()) {
        return {};
    }

    auto cmp = [](const T *left, const T *right) {
        return left->size() < right->size();
    };
    std::priority_queue<const T *, std::vector<const T *>, decltype(cmp)>
        remaining_children(children.begin(), children.end(), cmp);

    std::vector<RenderedRect<T>> results;
    results.reserve(children.size());

    std::vector<const T *> current_row;
    Rect current_rect = available_rect;

    while (!remaining_children.empty()) {
        const T *largest_remaining = remaining_children.top();
        remaining_children.pop();

        std::vector<const T *> test_row = current_row;
        test_row.push_back(largest_remaining);

        float w = shorter_side(current_rect);
        float current_worst = worst_aspect_ratio(current_row, w);
        float test_worst = worst_aspect_ratio(test_row, w);

        if (current_row.empty() || test_worst <= current_worst) {
            // Add to current row - aspect ratio improves or stays same
            current_row.push_back(largest_remaining);
        } else {
            // Flush current row and start new one - aspect ratio would worsen
            auto [row_results, remaining_space] =
                layoutrow<T>(current_row, current_rect);
            results.insert(results.end(), row_results.begin(),
                           row_results.end());

            current_rect = remaining_space;
            current_row = {largest_remaining};
        }
    }

    // Flush final row
    if (!current_row.empty()) {
        auto [row_results, remaining_space] =
            layoutrow<T>(current_row, current_rect);
        results.insert(results.end(), row_results.begin(), row_results.end());
        // assert(std::abs(area(remaining_space)) < 1.0F);
    }

    return results;
}

/// @brief Layout one row in the available rectangle, i.e. calculate sizes of
/// each element
/// @tparam T
/// @param row Nodes to layout
/// @param available_rect coordinates and available space
/// @return
template <TreeNode T>
std::pair<std::vector<RenderedRect<T>>, Rect>
layoutrow(const std::vector<const T *> &row, const Rect &available_rect)
{
    ZoneScoped;
    if (row.empty()) {
        return {{}, available_rect};
    }

    // assert(row_area(row) <= area(available_rect));

    std::vector<RenderedRect<T>> results;
    results.reserve(row.size());

    const auto rect_orientation = orientation(available_rect);
    if (rect_orientation == RectOrientation::horizontal) {
        float x_offset = 0;
        for (const T *node : row) {
            const float rect_width = node->size() / available_rect.height;
            results.emplace_back(node, Rect{
                                           .x = available_rect.x + x_offset,
                                           .y = available_rect.y,
                                           .width = rect_width,
                                           .height = available_rect.height,
                                       });
            x_offset += rect_width;
        }
        return {results, Rect{.x = available_rect.x + x_offset,
                              .y = available_rect.y,
                              .width = available_rect.width - x_offset,
                              .height = available_rect.height}};
    } else {
        float y_offset = 0;
        for (const T *node : row) {
            const float rect_height = node->size() / available_rect.width;
            results.emplace_back(node, Rect{
                                           .x = available_rect.x,
                                           .y = available_rect.y + y_offset,
                                           .width = available_rect.width,
                                           .height = rect_height,
                                       });
            y_offset += rect_height;
        }
        return {results, Rect{.x = available_rect.x,
                              .y = available_rect.y + y_offset,
                              .width = available_rect.width,
                              .height = available_rect.height - y_offset}};
    }
}

} // namespace treemap