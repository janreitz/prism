#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <imgui.h>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

template <typename T>
concept TreeNode = requires(T t) {
    { t.size() } -> std::convertible_to<float>;
    { t.parent() } -> std::convertible_to<T *>;
    { t.children() } -> std::convertible_to<std::vector<T *>>;
};

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

    bool Render(const char *label, const ImVec2 &size);

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
    std::vector<const T *> flattened_nodes_;

    void calculate_layout(const ImVec2 &availableSize);
    void layout_recursive(const T &node, float x, float y, float width,
                          float height);
    void squarify_children(const std::vector<const T *> &children, float x,
                           float y, float width, float height);
    float calculate_aspect_ratio(const std::vector<const T *> &children,
                                 float side);
    void place_rectangles(const std::vector<const T *> &children, float x,
                          float y, float width, float height, bool horizontal);
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
void TreeMapWidget<T>::calculate_layout(const ImVec2 &availableSize)
{
    rendered_rects_.clear();

    // Start recursive layout from root, using entire available space
    layout_recursive(root_.get(), 0, 0, availableSize.x, availableSize.y);
}

template <TreeNode T>
void TreeMapWidget<T>::layout_recursive(const T &node, float x, float y,
                                        float width, float height)
{
    auto children = node.children();

    if (width <= 0 || height <= 0) {
        return;
    }

    if (children.empty()) {
        rendered_rects_.emplace_back(&node, x, y, width, height);
        return;
    }

    // If it's a directory, recursively layout children within this space
    if (!children.empty()) {
        // Convert to const pointers
        std::vector<const T *> const_children;
        for (T *child : children) {
            const_children.push_back(child);
        }
        squarify_children(const_children, x, y, width, height);
    }
}

template <TreeNode T>
void TreeMapWidget<T>::squarify_children(const std::vector<const T *> &children,
                                         float x, float y, float width,
                                         float height)
{
    if (children.empty() || width <= 0 || height <= 0)
        return;

    if (children.size() == 1) {
        layout_recursive(*children[0], x, y, width, height);
        return;
    }

    // For simplicity, let's use a basic row-based layout first
    // This will help debug the hierarchy before adding complexity

    float total_size = 0;
    for (const T *child : children) {
        total_size += child->size();
    }

    if (total_size == 0)
        return;

    // Simple horizontal or vertical split based on aspect ratio
    bool horizontal = width >= height;
    float current_offset = 0;

    for (const T *child : children) {
        float proportion = child->size() / total_size;

        if (horizontal) {
            float child_width = proportion * width;
            layout_recursive(*child, x + current_offset, y, child_width,
                             height);
            current_offset += child_width;
        } else {
            float child_height = proportion * height;
            layout_recursive(*child, x, y + current_offset, width,
                             child_height);
            current_offset += child_height;
        }
    }
}

template <TreeNode T>
bool TreeMapWidget<T>::Render(const char *label, const ImVec2 &size)
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

    calculate_layout(canvas_size);

    // Debug: Ensure all rectangles are within bounds
    for (auto &rect : rendered_rects_) {
        rect.x = std::max(0.0f, std::min(rect.x, canvas_size.x - rect.width));
        rect.y = std::max(0.0f, std::min(rect.y, canvas_size.y - rect.height));
        rect.width = std::min(rect.width, canvas_size.x - rect.x);
        rect.height = std::min(rect.height, canvas_size.y - rect.y);
    }

    ImGui::InvisibleButton("treemap_canvas", canvas_size);
    bool is_hovered = ImGui::IsItemHovered();
    ImVec2 mouse_pos = ImGui::GetMousePos();

    hovered_node_ = nullptr;

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