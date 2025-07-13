#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <imgui.h>
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

    void flatten_tree(const T &node);
    void calculate_layout(const ImVec2 &availableSize);
    void squarify_layout(std::vector<int> &indices, float x, float y,
                         float width, float height);
    float calculate_aspect_ratio(const std::vector<int> &indices, float side);
    void place_rectangles(const std::vector<int> &indices, float x, float y,
                          float width, float height, bool horizontal);
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

template <TreeNode T> void TreeMapWidget<T>::flatten_tree(const T &node)
{
    flattened_nodes_.push_back(&node);

    auto children = node.children();
    for (const T *child : children) {
        flatten_tree(*child);
    }
}

template <TreeNode T>
void TreeMapWidget<T>::calculate_layout(const ImVec2 &availableSize)
{
    flattened_nodes_.clear();
    rendered_rects_.clear();

    flatten_tree(root_.get());

    if (flattened_nodes_.empty())
        return;

    // Sort nodes by size (descending)
    std::sort(flattened_nodes_.begin(), flattened_nodes_.end(),
              [](const T *a, const T *b) { return a->size() > b->size(); });

    std::vector<int> indices;
    for (size_t i = 0; i < flattened_nodes_.size(); ++i) {
        indices.push_back(static_cast<int>(i));
    }

    squarify_layout(indices, 0, 0, availableSize.x, availableSize.y);
}

template <TreeNode T>
void TreeMapWidget<T>::squarify_layout(std::vector<int> &indices, float x,
                                       float y, float width, float height)
{
    if (indices.empty())
        return;

    if (indices.size() == 1) {
        const T *node = flattened_nodes_[indices[0]];
        rendered_rects_.emplace_back(node, x, y, width, height);
        return;
    }

    bool horizontal = width >= height;
    float side = horizontal ? height : width;

    std::vector<int> row;
    float current_area = 0;
    float total_area = 0;

    for (int idx : indices) {
        total_area += flattened_nodes_[idx]->size();
    }

    float scale = (width * height) / total_area;

    size_t i = 0;
    while (i < indices.size()) {
        row.push_back(indices[i]);
        current_area += flattened_nodes_[indices[i]]->size() * scale;

        float current_ratio = calculate_aspect_ratio(row, side);

        if (i + 1 < indices.size()) {
            std::vector<int> next_row = row;
            next_row.push_back(indices[i + 1]);
            float next_ratio = calculate_aspect_ratio(next_row, side);

            if (next_ratio > current_ratio) {
                break;
            }
        }

        i++;
    }

    place_rectangles(row, x, y, width, height, horizontal);

    std::vector<int> remaining(indices.begin() + row.size(), indices.end());
    if (!remaining.empty()) {
        float row_thickness = current_area / side;

        if (horizontal) {
            squarify_layout(remaining, x, y + row_thickness, width,
                            height - row_thickness);
        } else {
            squarify_layout(remaining, x + row_thickness, y,
                            width - row_thickness, height);
        }
    }
}

template <TreeNode T>
float TreeMapWidget<T>::calculate_aspect_ratio(const std::vector<int> &indices,
                                               float side)
{
    if (indices.empty())
        return 1.0f;

    float total_area = 0;
    float min_value = std::numeric_limits<float>::max();
    float max_value = 0;

    for (int idx : indices) {
        float value = flattened_nodes_[idx]->size();
        total_area += value;
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
    }

    if (total_area == 0)
        return 1.0f;

    float thickness = total_area / side;
    float min_side = min_value / thickness;
    float max_side = max_value / thickness;

    float ratio1 = thickness / min_side;
    float ratio2 = max_side / thickness;

    return std::max(ratio1, ratio2);
}

template <TreeNode T>
void TreeMapWidget<T>::place_rectangles(const std::vector<int> &indices,
                                        float x, float y, float width,
                                        float height, bool horizontal)
{
    if (indices.empty())
        return;

    float total_value = 0;
    for (int idx : indices) {
        total_value += flattened_nodes_[idx]->size();
    }

    if (total_value == 0)
        return;

    float current_offset = 0;
    float total_area = width * height;
    float scale = total_area / total_value;

    for (int idx : indices) {
        const T *node = flattened_nodes_[idx];
        float node_area = node->size() * scale;

        if (horizontal) {
            float node_width = node_area / height;
            rendered_rects_.emplace_back(node, x + current_offset, y,
                                         node_width, height);
            current_offset += node_width;
        } else {
            float node_height = node_area / width;
            rendered_rects_.emplace_back(node, x, y + current_offset, width,
                                         node_height);
            current_offset += node_height;
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

    if (canvas_size.x < 50.0f)
        canvas_size.x = 50.0f;
    if (canvas_size.y < 50.0f)
        canvas_size.y = 50.0f;

    calculate_layout(canvas_size);

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