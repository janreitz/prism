#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <imgui.h>
#include <iostream>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

template <typename T>
concept TreeNode = requires(T t) {
    { t.size() } -> std::convertible_to<float>;
    { t.parent() } -> std::convertible_to<T *>;
    { t.children() } -> std::convertible_to<std::vector<T *>>;
};

// Layout structures - pure calculation, no rendering
struct Rect {
    float x, y, width, height;

    float shorter_side() const { return std::min(width, height); }
    bool is_horizontal() const { return width >= height; }
    float area() const { return width * height; }
};

// Rendering structures - for visualization
template <TreeNode T> struct RenderedRect {
    const T *node;
    float x, y, width, height;
    bool hovered = false;

    RenderedRect(const T *n, float x_, float y_, float w_, float h_)
        : node(n), x(x_), y(y_), width(w_), height(h_)
    {
    }
};

template <TreeNode T> class TreeMapWidget
{
  public:
    explicit TreeMapWidget(const T &root);

    bool render(const char *label, const ImVec2 &size);

    void set_coloring_strategy(
        const std::function<ImU32(const T &)> &coloring_strategy);

    void add_on_node_hover(std::function<void(const T &)> callback)
    {
        node_hover_cbs_.push_back(callback);
    }
    void add_on_node_click(std::function<void(const T &)> callback)
    {
        node_clicked_cbs_.push_back(callback);
    }

    const T &get_hovered_node() const;
    const T &get_selected_node() const;

  private:
    std::optional<std::function<ImU32(const T &)>> coloring_strategy_;

    std::reference_wrapper<const T> root_;
    std::vector<std::function<void(const T &)>> node_hover_cbs_;
    std::vector<std::function<void(const T &)>> node_clicked_cbs_;
    const T *selected_node_ = nullptr;
    const T *hovered_node_ = nullptr;

    std::vector<RenderedRect<T>> rendered_rects_;

    // Layout calculation methods
    std::vector<RenderedRect<T>> calculate_layout(const T &root,
                                                  const Rect &available_rect);
    std::vector<std::pair<const T *, Rect>>
    squarify_layout(const std::vector<const T *> &children,
                    const Rect &available_rect);
    std::vector<std::pair<const T *, Rect>>
    squarify_recursive(const std::vector<const T *> &children,
                       const std::vector<const T *> &current_row,
                       const Rect &available_rect);
    Rect layout_row(const std::vector<const T *> &row,
                    const Rect &available_rect);
    float worst_aspect_ratio(const std::vector<const T *> &row,
                             float row_width);

    // Rendering methods
    std::vector<std::pair<const T *, Rect>>
    position_row(const std::vector<const T *> &row, const Rect &available_rect);
};

// Template implementation
template <TreeNode T>
TreeMapWidget<T>::TreeMapWidget(const T &root) : root_(root)
{
}

template <TreeNode T>
void TreeMapWidget<T>::set_coloring_strategy(
    const std::function<ImU32(const T &)> &coloring_strategy)
{
    coloring_strategy_ = coloring_strategy;
}

template <TreeNode T> const T &TreeMapWidget<T>::get_hovered_node() const
{
    if (!hovered_node_)
        throw std::runtime_error("No hovered node");
    return *hovered_node_;
}

template <TreeNode T> const T &TreeMapWidget<T>::get_selected_node() const
{
    if (!selected_node_)
        throw std::runtime_error("No selected node");
    return *selected_node_;
}

template <TreeNode T>
std::vector<RenderedRect<T>>
TreeMapWidget<T>::calculate_layout(const T &root, const Rect &available_rect)
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

    return result;
}

template <TreeNode T>
std::vector<std::pair<const T *, Rect>>
TreeMapWidget<T>::squarify_layout(const std::vector<const T *> &children,
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
TreeMapWidget<T>::squarify_recursive(const std::vector<const T *> &children,
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
        worst_aspect_ratio(current_row, available_rect.shorter_side());
    float test_worst =
        worst_aspect_ratio(test_row, available_rect.shorter_side());

    if (test_worst <= current_worst) {
        // Adding child improves layout - continue building current row
        return squarify_recursive(remaining_children, test_row, available_rect);
    } else {
        // Adding child worsens layout - finalize current row and start new one
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
TreeMapWidget<T>::position_row(const std::vector<const T *> &row,
                               const Rect &available_rect)
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
    bool horizontal = available_rect.is_horizontal();
    float row_width = available_rect.shorter_side();
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
float TreeMapWidget<T>::worst_aspect_ratio(const std::vector<const T *> &row,
                                           float row_width)
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
Rect TreeMapWidget<T>::layout_row(const std::vector<const T *> &row,
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
    bool horizontal = available_rect.is_horizontal();
    float row_width = available_rect.shorter_side();
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

template <TreeNode T>
bool TreeMapWidget<T>::render(const char *label, const ImVec2 &size)
{
    ImGui::BeginChild(label, size, true, ImGuiWindowFlags_NoScrollbar);

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    // Use actual content region size for layout calculation
    if (canvas_size.x < 50.0f)
        canvas_size.x = 50.0f;
    if (canvas_size.y < 50.0f)
        canvas_size.y = 50.0f;

    // Calculate layout and get rendered rectangles directly
    Rect available_rect{0, 0, canvas_size.x, canvas_size.y};
    rendered_rects_ = calculate_layout(root_.get(), available_rect);

    // Debug: Ensure all rectangles are within bounds
    for (auto &rect : rendered_rects_) {
        rect.x = std::max(0.0f, std::min(rect.x, canvas_size.x - rect.width));
        rect.y = std::max(0.0f, std::min(rect.y, canvas_size.y - rect.height));
        rect.width = std::min(rect.width, canvas_size.x - rect.x);
        rect.height = std::min(rect.height, canvas_size.y - rect.y);
    }

    // TODO assert recatngles are non-overlapping

    ImGui::InvisibleButton("treemap_canvas", canvas_size);
    bool is_hovered = ImGui::IsItemHovered();
    ImVec2 mouse_pos = ImGui::GetMousePos();

    hovered_node_ = nullptr;

    // TODO Refactor rect lookup
    for (auto &rect : rendered_rects_) {
        ImVec2 rect_min(canvas_pos.x + rect.x, canvas_pos.y + rect.y);
        ImVec2 rect_max(rect_min.x + rect.width, rect_min.y + rect.height);

        rect.hovered = is_hovered && mouse_pos.x >= rect_min.x &&
                       mouse_pos.x <= rect_max.x && mouse_pos.y >= rect_min.y &&
                       mouse_pos.y <= rect_max.y;

        if (rect.hovered) {
            hovered_node_ = rect.node;
            for (const auto &callback : node_hover_cbs_) {
                callback(*rect.node);
            }
        }

        ImU32 fill_color = IM_COL32(100, 150, 200, 255); // Default color
        if (coloring_strategy_) {
            fill_color = (*coloring_strategy_)(*rect.node);
        }

        if (rect.hovered) {
            float r = ((fill_color >> 0) & 0xFF) / 255.0f;
            float g = ((fill_color >> 8) & 0xFF) / 255.0f;
            float b = ((fill_color >> 16) & 0xFF) / 255.0f;
            float a = ((fill_color >> 24) & 0xFF) / 255.0f;

            r = std::min(1.0f, r * 1.2f);
            g = std::min(1.0f, g * 1.2f);
            b = std::min(1.0f, b * 1.2f);

            fill_color =
                IM_COL32(static_cast<int>(r * 255), static_cast<int>(g * 255),
                         static_cast<int>(b * 255), static_cast<int>(a * 255));
        }

        if (selected_node_ == rect.node) {
            draw_list->AddRectFilled(rect_min, rect_max,
                                     IM_COL32(255, 255, 0, 100));
        }

        draw_list->AddRectFilled(rect_min, rect_max, fill_color);
        draw_list->AddRect(rect_min, rect_max, IM_COL32(255, 255, 255, 180),
                           0.0f, 0, 1.5f);
    }

    bool clicked = false;
    if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (hovered_node_) {
            selected_node_ = hovered_node_;
            for (const auto &callback : node_clicked_cbs_) {
                callback(*hovered_node_);
            }
            clicked = true;
        }
    }

    ImGui::EndChild();
    return clicked;
}