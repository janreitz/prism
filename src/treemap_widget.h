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

template <treemap::TreeNode T> class TreeMapWidget
{
  public:
    explicit TreeMapWidget(const T &root);

    bool render(const char *label, const ImVec2 &size, bool parallelize);

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

    ImVec2 canvas_size_ = {0.0F, 0.0F};
    // Pan and zoom state
    ImVec2 pan_offset_{0.0f, 0.0f};
    float zoom_level_{1.0f};
    bool is_panning_{false};
    ImVec2 last_mouse_pos_{0.0f, 0.0f};
    static constexpr float MIN_ZOOM = 0.1f;
    static constexpr float MAX_ZOOM = 10.0f;
    static constexpr float ZOOM_SPEED = 0.1f;

    treemap::Layout<T> layout_;

    // Rendering methods
    std::vector<std::pair<const T *, treemap::Rect>>
    position_row(const std::vector<const T *> &row,
                 const treemap::Rect &available_rect);

    ImVec2 world_to_screen(const ImVec2 &world_pos,
                           const ImVec2 &canvas_pos) const
    {
        return ImVec2(
            canvas_pos.x + (world_pos.x * zoom_level_) + pan_offset_.x,
            canvas_pos.y + (world_pos.y * zoom_level_) + pan_offset_.y);
    }

    ImVec2 screen_to_world(const ImVec2 &screen_pos,
                           const ImVec2 &canvas_pos) const
    {
        return ImVec2(
            (screen_pos.x - canvas_pos.x - pan_offset_.x) / zoom_level_,
            (screen_pos.y - canvas_pos.y - pan_offset_.y) / zoom_level_);
    }

    bool is_rect_visible(const treemap::Rect &rect, const ImVec2 &canvas_pos,
                         const ImVec2 &canvas_size) const
    {
        ImVec2 rect_min = world_to_screen(ImVec2(rect.x, rect.y), canvas_pos);
        ImVec2 rect_max = world_to_screen(
            ImVec2(rect.x + rect.width, rect.y + rect.height), canvas_pos);

        return !(rect_max.x < canvas_pos.x ||
                 rect_min.x > canvas_pos.x + canvas_size.x ||
                 rect_max.y < canvas_pos.y ||
                 rect_min.y > canvas_pos.y + canvas_size.y);
    }
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
    pan_offset_ = ImVec2(0.0f, 0.0f);
    zoom_level_ = 1.0f;
}

template <treemap::TreeNode T>
bool TreeMapWidget<T>::render(const char *label, const ImVec2 &size,
                              bool parallelize)
{
#if TRACY_ENABLE
    ZoneScoped;
#endif
    ImGui::BeginChild(label, size, ImGuiChildFlags_ResizeY);

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 current_canvas_size = ImGui::GetContentRegionAvail();

    if (current_canvas_size.x <= 0.0F || current_canvas_size.y <= 0.0F) {
        std::cerr << "Can't render treemap in canvas of size ("
                  << current_canvas_size.x << " | " << current_canvas_size.y
                  << ")\n";
        ImGui::EndChild();
        return false;
    }

    // Only layout if canvas size changed (layout in world coordinates)
    if (current_canvas_size.x != canvas_size_.x ||
        current_canvas_size.y != canvas_size_.y) {
        treemap::Rect available_rect{0, 0, current_canvas_size.x,
                                     current_canvas_size.y};
        layout_ = treemap::layout(root_.get(), available_rect, parallelize);
        canvas_size_ = current_canvas_size;
    }

    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    // Set up clipping rectangle
    draw_list->PushClipRect(
        canvas_pos,
        ImVec2(canvas_pos.x + canvas_size_.x, canvas_pos.y + canvas_size_.y),
        true);

    ImGui::InvisibleButton("treemap_canvas", canvas_size_);
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);

    bool is_hovered = ImGui::IsItemHovered();
    bool clicked = false;

    // Handle input
    if (is_hovered) {
        ImVec2 mouse_pos = ImGui::GetMousePos();

        // Zoom with mouse wheel
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            // Zoom towards mouse position
            ImVec2 world_mouse_pos = screen_to_world(mouse_pos, canvas_pos);

            float old_zoom = zoom_level_;
            zoom_level_ *= (1.0f + wheel * ZOOM_SPEED);
            zoom_level_ = std::clamp(zoom_level_, MIN_ZOOM, MAX_ZOOM);

            // Adjust pan to zoom towards mouse
            if (zoom_level_ != old_zoom) {
                float zoom_factor = zoom_level_ / old_zoom;
                ImVec2 new_world_mouse_pos =
                    screen_to_world(mouse_pos, canvas_pos);
                pan_offset_.x += (world_mouse_pos.x - new_world_mouse_pos.x);
                pan_offset_.y += (world_mouse_pos.y - new_world_mouse_pos.y);
            }
        }

        // Pan with middle mouse or right mouse drag
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            is_panning_ = true;
            last_mouse_pos_ = mouse_pos;
        }

        // Node hover detection (transform mouse to world coordinates)
        ImVec2 world_mouse_pos = screen_to_world(mouse_pos, canvas_pos);
        const auto *currently_hovered_node =
            treemap::hit_test(world_mouse_pos, layout_.leaves, ImVec2(0, 0));

        // Only execute callbacks if hovered node changed and is currently not
        // nullptr
        if (currently_hovered_node && currently_hovered_node != hovered_node_) {
            for (const auto &callback : node_hover_cbs_) {
                callback(*currently_hovered_node);
            }
        }
        hovered_node_ = currently_hovered_node;

        // Handle left click
        if (hovered_node_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !is_panning_) {
            selected_node_ = hovered_node_;
            for (const auto &callback : node_clicked_cbs_) {
                callback(*hovered_node_);
            }
            clicked = true;
        }
    }

    // Handle panning
    if (is_panning_) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
            ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            ImVec2 delta = ImVec2(mouse_pos.x - last_mouse_pos_.x,
                                  mouse_pos.y - last_mouse_pos_.y);
            pan_offset_.x += delta.x / zoom_level_;
            pan_offset_.y += delta.y / zoom_level_;
            last_mouse_pos_ = mouse_pos;
        } else {
            is_panning_ = false;
        }
    }

    // Render rectangles with culling
    for (auto &rect : layout_.leaves) {
        // Skip rectangles outside view
        if (!is_rect_visible(rect.rect_, canvas_pos, canvas_size_)) {
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

        // Transform to screen coordinates
        ImVec2 screen_min =
            world_to_screen(ImVec2(rect.rect_.x, rect.rect_.y), canvas_pos);
        ImVec2 screen_max =
            world_to_screen(ImVec2(rect.rect_.x + rect.rect_.width,
                                   rect.rect_.y + rect.rect_.height),
                            canvas_pos);

        draw_list->AddRectFilled(screen_min, screen_max, fill_color);

        // Only draw borders if rectangle is reasonably sized
        if ((screen_max.x - screen_min.x) > 2.0f &&
            (screen_max.y - screen_min.y) > 2.0f) {
            draw_list->AddRect(screen_min, screen_max,
                               IM_COL32(255, 255, 255, 180), 0.0f, 0, 0.5f);
        }
    }

    // Render frames
    for (auto &frame : layout_.frames) {
        if (!is_rect_visible(frame.rect_, canvas_pos, canvas_size_)) {
            continue;
        }

        ImVec2 screen_min =
            world_to_screen(ImVec2(frame.rect_.x, frame.rect_.y), canvas_pos);
        ImVec2 screen_max =
            world_to_screen(ImVec2(frame.rect_.x + frame.rect_.width,
                                   frame.rect_.y + frame.rect_.height),
                            canvas_pos);

        if ((screen_max.x - screen_min.x) > 4.0f &&
            (screen_max.y - screen_min.y) > 4.0f) {
            draw_list->AddRect(screen_min, screen_max, IM_COL32(0, 0, 0, 180),
                               0.0f, 0, 2.0f);
        }
    }

    draw_list->PopClipRect();

    ImGui::EndChild();
    return clicked;
}