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
#if TRACY_ENABLE
#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>
#endif

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

template <TreeNode T> struct RenderRect {
    const T *node_;
    Rect rect_;

    RenderRect(const T *n, Rect rect) : node_(n), rect_(rect) {}
};

template <TreeNode T>
auto hit_test(ImVec2 test, const std::vector<RenderRect<T>> &rects,
              ImVec2 offset) -> const T *
{
    auto hovered_rect_it =
        std::find_if(rects.cbegin(), rects.cend(),
                     [&offset, &test](const RenderRect<T> &rect) -> bool {
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
void validate_layout(const std::vector<RenderRect<T>> &layout,
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

template <typename T> struct Layout {
    std::vector<RenderRect<T>> leaves;
    std::vector<RenderRect<T>> frames;
};

/// @brief  Main entry point for calculating treemap layout - handles scaling
/// relative scaling of screen coordinates/space and size of elements to place
/// @tparam T
/// @param root
/// @param available_rect
/// @return
template <TreeNode T>
Layout<T> layout(const T &root, const Rect &available_rect,
                 bool parallelize = false)
{
#if TRACY_ENABLE
    ZoneScoped;
#endif

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

    Layout<T> layout_result;
    layout_tree_traversal(layout_result, root, available_rect_scaled,
                          parallelize);

    auto rescale_rect = [&](auto &rect) -> auto & {
        rect.rect_.x = rect.rect_.x / scaling_factor + available_rect.x;
        rect.rect_.y = rect.rect_.y / scaling_factor + available_rect.y;
        rect.rect_.height /= scaling_factor;
        rect.rect_.width /= scaling_factor;
        return rect;
    };

    std::ranges::for_each(layout_result.leaves, rescale_rect);
    std::ranges::for_each(layout_result.frames, rescale_rect);

    // Validate the result
    // validate_layout(layout_result, available_rect);

    return layout_result;
}

template <TreeNode T>
void layout_tree_traversal(Layout<T> &result, const T &root,
                           const Rect &available_rect, bool parallelize)
{
#if TRACY_ENABLE
    ZoneScoped;
#endif
    auto children = root.children();
    if (children.empty()) {
        // Leaf node - return single rectangle
        result.leaves.emplace_back(&root, available_rect);
        return;
    }

    result.frames.emplace_back(&root, available_rect);

    auto child_layouts = squarify(children, available_rect);

    // Sequential version
    auto sequential_impl = [&]() {
        for (const auto &layout : child_layouts) {
            layout_tree_traversal(result, *layout.node_, layout.rect_,
                                  parallelize);
        }
        return;
    };

#if ENABLE_PARALLEL_EXECUTION
    if (parallelize) {
        // Parallel version
        // return std::transform_reduce(
        //     std::execution::par, child_layouts.begin(), child_layouts.end(),
        //     std::vector<RenderRect<T>>{},
        //     [](std::vector<RenderRect<T>> a, std::vector<RenderRect<T>>
        //     b) {
        //         a.insert(a.end(), b.begin(), b.end());
        //         return a;
        //     },
        //     [parallelize](const auto &layout) {
        //         return layout_tree_traversal(*layout.node_, layout.rect_,
        //                                      parallelize);
        //     });
    } else {
        sequential_impl();
    }
#else
    sequential_impl();
#endif
}

float worst_aspect_ratio(float total_size, float max_element_size,
                         float min_element_size, float w)
{
#if TRACY_ENABLE
    ZoneScoped;
#endif
    return std::max((w * w * max_element_size) / (total_size * total_size),
                    (total_size * total_size) / (w * w * min_element_size));
}

template <typename T> struct Row {
    Row(const Rect &_rect, const T *initial_element)
        : rect(_rect), w(shorter_side(_rect)),
          max_element_size(initial_element->size())
    {
#if TRACY_ENABLE
        ZoneScoped;
#endif
        push(initial_element);
    }

    size_t element_count() const { return elements.size(); }

    bool test(float element_size) const
    {
        return worst_aspect_ratio(size + element_size, max_element_size,
                                  element_size, w) <= current_worst;
    }

    void push(const T *element)
    {
        const float element_size = element->size();
        elements.push_back(element);
        size += element_size;
        // Elements are only pushed in decreasing size order, so max_element
        // doesn't have to be updated and min element can be assigned without
        // testing
        min_element_size = element_size;
        current_worst =
            worst_aspect_ratio(size, max_element_size, min_element_size, w);
    }

    Rect rect;
    float w;
    std::vector<const T *> elements;
    float size = 0;
    float max_element_size;
    float min_element_size = std::numeric_limits<float>::max();
    float current_worst = std::numeric_limits<float>::min();
};

// Core squarify algorithm - iterative implementation for single-level layout
template <TreeNode T>
std::vector<RenderRect<T>> squarify(const std::vector<const T *> &children,
                                    const Rect &available_rect)
{
#if TRACY_ENABLE
    ZoneScoped;
#endif
    if (children.empty()) {
        return {};
    }

    // Elements are layed out in size-sorted order
    auto cmp = [](const T *left, const T *right) {
        return left->size() < right->size();
    };

#if TRACY_ENABLE
    TracyCZoneN(ctx, "squarify Priority Queue Creation", true);
#endif
    std::priority_queue<const T *, std::vector<const T *>, decltype(cmp)>
        remaining_children(children.begin(), children.end(), cmp);
#if TRACY_ENABLE
    TracyCZoneEnd(ctx);
#endif

#if TRACY_ENABLE
    TracyCZoneN(ctx_1, "squarify Result vector allocation", true);
#endif
    std::vector<RenderRect<T>> results;
    results.reserve(children.size());
#if TRACY_ENABLE
    TracyCZoneEnd(ctx_1);
#endif

    Row<T> current_row{available_rect, remaining_children.top()};
    remaining_children.pop();

    while (!remaining_children.empty()) {
        const T *largest_remaining = remaining_children.top();
        remaining_children.pop();

        if (current_row.test(largest_remaining->size())) {
            current_row.push(largest_remaining);
        } else {
            // Flush current row and start new one - aspect ratio would worsen
            auto [row_results, remaining_space] = layoutrow<T>(current_row);
            results.insert(results.end(), row_results.begin(),
                           row_results.end());
            current_row = Row<T>(remaining_space, largest_remaining);
        }
    }

    // Flush final row
    if (current_row.element_count() != 0) {
        auto [row_results, remaining_space] = layoutrow<T>(current_row);
        results.insert(results.end(), row_results.begin(), row_results.end());
    }

    return results;
}

/// @brief Layout one row in the available rectangle, i.e. calculate sizes of
/// each element
/// @tparam T
/// @param row Nodes to layout
/// @param available_rect coordinates and available space
/// @return Layouted rects in screen coordinates and remaining space after
/// layout operation
template <TreeNode T>
std::pair<std::vector<RenderRect<T>>, Rect> layoutrow(const Row<T> &row)
{
#if TRACY_ENABLE
    ZoneScoped;
#endif
    const auto &available_rect = row.rect;

    if (row.element_count() == 0) {
        return {{}, available_rect};
    }

    std::vector<RenderRect<T>> results;
    results.reserve(row.element_count());

    // If the available rect is wider than high, "split" it by laying out
    // elements vertically
    const bool layout_horizontally =
        available_rect.width < available_rect.height;
    if (layout_horizontally) {
        float x_offset = 0;
        float row_height = row.size / available_rect.width;
        for (const T *node : row.elements) {
            const float node_width = node->size() / row_height;
            results.emplace_back(node, Rect{
                                           .x = available_rect.x + x_offset,
                                           .y = available_rect.y,
                                           .width = node_width,
                                           .height = row_height,
                                       });
            x_offset += node_width;
        }
        return {results, Rect{.x = available_rect.x,
                              .y = available_rect.y + row_height,
                              .width = available_rect.width,
                              .height = available_rect.height - row_height}};
    } else {
        float y_offset = 0;
        float row_width = row.size / available_rect.height;
        for (const T *node : row.elements) {
            const float node_height = node->size() / row_width;
            results.emplace_back(node, Rect{
                                           .x = available_rect.x,
                                           .y = available_rect.y + y_offset,
                                           .width = row_width,
                                           .height = node_height,
                                       });
            y_offset += node_height;
        }
        return {results, Rect{.x = available_rect.x + row_width,
                              .y = available_rect.y,
                              .width = available_rect.width - row_width,
                              .height = available_rect.height}};
    }
}

} // namespace treemap