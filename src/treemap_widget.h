#pragma once
#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <imgui.h>
#include <iostream>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include "treemap.h"
#if TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

struct WindowCoordinate {
    float x = 0.0F;
    float y = 0.0F;
    ImVec2 to_imvec2() const;
    static WindowCoordinate from_imvec2(ImVec2 vec2);
};

struct CanvasCoordinate {
    float x = 0.0F;
    float y = 0.0F;
    ImVec2 to_imvec2() const;
    static CanvasCoordinate from_imvec2(ImVec2 vec2);
};

struct TreemapCoordinate {
    float x = 0.0F;
    float y = 0.0F;
    ImVec2 to_imvec2() const;
    static TreemapCoordinate from_imvec2(ImVec2 vec2);
};

WindowCoordinate to_window(CanvasCoordinate canvas_coord,
                           WindowCoordinate canvas_pos);

CanvasCoordinate to_canvas(WindowCoordinate win_coord,
                           WindowCoordinate canvas_pos);

CanvasCoordinate to_canvas(TreemapCoordinate map_coord, TreemapCoordinate pan,
                           float zoom);

TreemapCoordinate to_treemap(CanvasCoordinate canvas_coord,
                             TreemapCoordinate pan, float zoom);

template <treemap::TreeNode T> class TreeMapWidget
{
  public:
    explicit TreeMapWidget(const T &root);

    void render(const char *label, const ImVec2 &size, bool parallelize);

    void reset_view();

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

    CanvasCoordinate canvas_size_;
    // Pan and zoom state
    TreemapCoordinate pan_{.x = 0.0f, .y = 0.0f};
    float zoom_{1.0f};
    WindowCoordinate last_mouse_pos_{0.0f, 0.0f};
    static constexpr float MIN_ZOOM = 0.1f;
    static constexpr float MAX_ZOOM = 10.0f;
    static constexpr float ZOOM_SPEED = 0.1f;

    treemap::Layout<T> layout_;

    // Rendering methods
    std::vector<std::pair<const T *, treemap::Rect>>
    position_row(const std::vector<const T *> &row,
                 const treemap::Rect &available_rect);
};

// Template implementation
template <treemap::TreeNode T>
TreeMapWidget<T>::TreeMapWidget(const T &root) : root_(root)
{
}

template <treemap::TreeNode T>
void TreeMapWidget<T>::set_coloring_strategy(
    const std::function<ImU32(const T &)> &coloring_strategy)
{
    coloring_strategy_ = coloring_strategy;
}

template <treemap::TreeNode T>
const T &TreeMapWidget<T>::get_hovered_node() const
{
    if (!hovered_node_)
        throw std::runtime_error("No hovered node");
    return *hovered_node_;
}

template <treemap::TreeNode T>
const T &TreeMapWidget<T>::get_selected_node() const
{
    if (!selected_node_)
        throw std::runtime_error("No selected node");
    return *selected_node_;
}

template <treemap::TreeNode T> void TreeMapWidget<T>::reset_view()
{
    pan_ = ImVec2(0.0f, 0.0f);
    zoom_ = 1.0f;
}

template <treemap::TreeNode T>
void TreeMapWidget<T>::render(const char *label, const ImVec2 &size,
                              bool parallelize)
{
#if TRACY_ENABLE
    ZoneScoped;
#endif
    ImGui::BeginChild(label, size, ImGuiChildFlags_ResizeY);

    const auto canvas_pos =
        WindowCoordinate::from_imvec2(ImGui::GetCursorScreenPos());
    const auto current_canvas_size =
        CanvasCoordinate::from_imvec2(ImGui::GetContentRegionAvail());

    if (current_canvas_size.x <= 0.0F || current_canvas_size.y <= 0.0F) {
        std::cerr << "Can't render treemap in canvas of size ("
                  << current_canvas_size.x << " | " << current_canvas_size.y
                  << ")\n";
        ImGui::EndChild();
        return;
    }

    // Only layout if canvas size changed (layout in world coordinates)
    if (current_canvas_size.x != canvas_size_.x ||
        current_canvas_size.y != canvas_size_.y) {
        treemap::Rect available_rect{0, 0, current_canvas_size.x,
                                     current_canvas_size.y};
        layout_ = treemap::layout(root_.get(), available_rect, parallelize);
        canvas_size_ = current_canvas_size;
    }

    ImGui::InvisibleButton("treemap_canvas", canvas_size_.to_imvec2());
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);

    // Handle input
    if (ImGui::IsItemHovered()) {
        const auto mouse_pos =
            WindowCoordinate::from_imvec2(ImGui::GetMousePos());
        const TreemapCoordinate map_mouse_pos =
            to_treemap(to_canvas(mouse_pos, canvas_pos), pan_, zoom_);

        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            // Store the treemap coordinate under mouse BEFORE zoom change
            const TreemapCoordinate old_map_mouse_pos = map_mouse_pos;

            const float old_zoom = zoom_;
            zoom_ *= (1.0f + wheel * ZOOM_SPEED);
            zoom_ = std::clamp(zoom_, MIN_ZOOM, MAX_ZOOM);

            // Calculate new treemap coordinate under mouse AFTER zoom
            const TreemapCoordinate new_map_mouse_pos =
                to_treemap(to_canvas(mouse_pos, canvas_pos), pan_, zoom_);

            // Adjust pan to keep the same point under the mouse
            pan_.x += (old_map_mouse_pos.x - new_map_mouse_pos.x);
            pan_.y += (old_map_mouse_pos.y - new_map_mouse_pos.y);
        }

        // Handle panning
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            const ImVec2 delta =
                ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
            pan_.x -= delta.x / zoom_;
            pan_.y -= delta.y / zoom_;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
        }

        const auto *currently_hovered_node = treemap::hit_test(
            map_mouse_pos.to_imvec2(), layout_.leaves, ImVec2(0, 0));

        // Only execute callbacks if hovered node changed and is currently not
        // nullptr
        if (currently_hovered_node && currently_hovered_node != hovered_node_) {
            for (const auto &callback : node_hover_cbs_) {
                callback(*currently_hovered_node);
            }
        }
        hovered_node_ = currently_hovered_node;

        // Handle left click
        if (hovered_node_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            selected_node_ = hovered_node_;
            for (const auto &callback : node_clicked_cbs_) {
                callback(*hovered_node_);
            }
        }

        last_mouse_pos_ = mouse_pos;
    }

    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    // Set up clipping rectangle
    draw_list->PushClipRect(
        canvas_pos.to_imvec2(),
        ImVec2(canvas_pos.x + canvas_size_.x, canvas_pos.y + canvas_size_.y),
        true);

    // Render rectangles with culling
    const TreemapCoordinate canvas_origin_map_coords =
        to_treemap(CanvasCoordinate{.x = 0.0F, .y = 0.0F}, pan_, zoom_);
    // Size in treemap coordinates is canvas size divided by zoom
    const float treemap_width = current_canvas_size.x / zoom_;
    const float treemap_height = current_canvas_size.y / zoom_;

    const treemap::Rect canvas_rect{
        .x = canvas_origin_map_coords.x,
        .y = canvas_origin_map_coords.y,
        .width = treemap_width,
        .height = treemap_height,
    };

    for (auto &rect : layout_.leaves) {
        // Skip rectangles outside view
        if (!treemap::overlaps(rect.rect_, canvas_rect)) {
            continue;
        }

        ImU32 fill_color = coloring_strategy_
                               ? (*coloring_strategy_)(*rect.node_)
                               : IM_COL32(100, 150, 200, 255);

        if (rect.node_ == hovered_node_) {
            // Brighten hovered color
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
        if (rect.node_ == selected_node_) {
            fill_color = IM_COL32(255, 255, 0, 100);
        }

        const WindowCoordinate win_min = to_window(
            to_canvas(TreemapCoordinate{.x = rect.rect_.x, .y = rect.rect_.y},
                      pan_, zoom_),
            canvas_pos);
        const WindowCoordinate win_max = to_window(
            to_canvas(TreemapCoordinate{.x = rect.rect_.x + rect.rect_.width,
                                        .y = rect.rect_.y + rect.rect_.height},
                      pan_, zoom_),
            canvas_pos);
        ;

        draw_list->AddRectFilled(win_min.to_imvec2(), win_max.to_imvec2(),
                                 fill_color);

        // Only draw borders if rectangle is reasonably sized
        if ((win_max.x - win_min.x) > 2.0f && (win_max.y - win_min.y) > 2.0f) {
            draw_list->AddRect(win_min.to_imvec2(), win_max.to_imvec2(),
                               IM_COL32(255, 255, 255, 180), 0.0f, 0, 0.5f);
        }
    }

    // Render frames
    for (auto &frame : layout_.frames) {
        if (!treemap::overlaps(frame.rect_, canvas_rect)) {
            continue;
        }

        const WindowCoordinate win_min = to_window(
            to_canvas(TreemapCoordinate{.x = frame.rect_.x, .y = frame.rect_.y},
                      pan_, zoom_),
            canvas_pos);
        const WindowCoordinate win_max = to_window(
            to_canvas(
                TreemapCoordinate{.x = frame.rect_.x + frame.rect_.width,
                                  .y = frame.rect_.y + frame.rect_.height},
                pan_, zoom_),
            canvas_pos);

        if ((win_max.x - win_min.x) > 4.0f && (win_max.y - win_min.y) > 4.0f) {
            draw_list->AddRect(win_min.to_imvec2(), win_max.to_imvec2(),
                               IM_COL32(0, 0, 0, 180), 0.0f, 0, 2.0f);
        }
    }

    draw_list->PopClipRect();

    ImGui::EndChild();
}