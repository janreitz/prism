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

template <treemap::TreeNode T>
bool TreeMapWidget<T>::render(const char *label, const ImVec2 &size,
                              bool parallelize)
{
#if TRACY_ENABLE
    ZoneScoped;
#endif
    ImGui::BeginChild(label, size, ImGuiChildFlags_ResizeY);

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    // Ensure minimum canvas size to prevent zero-sized InvisibleButton
    canvas_size.x = std::max(canvas_size.x, 10.0f);
    canvas_size.y = std::max(canvas_size.y, 10.0f);

    // Calculate layout and get rendered rectangles directly
    treemap::Rect available_rect{0, 0, canvas_size.x, canvas_size.y};
    layout_ = treemap::layout(root_.get(), available_rect, parallelize);

    ImGui::InvisibleButton("treemap_canvas", canvas_size);

    if (ImGui::IsItemHovered()) {

        const auto *currently_hovered_node =
            treemap::hit_test(ImGui::GetMousePos(), layout_.leaves, canvas_pos);

        // Only execute callbacks if hovered node changed and is currently not
        // nullptr
        if (currently_hovered_node && currently_hovered_node != hovered_node_) {
            for (const auto &callback : node_hover_cbs_) {
                callback(*currently_hovered_node);
            }
        }
        hovered_node_ = currently_hovered_node;
    }

    for (auto &rect : layout_.leaves) {

        ImU32 fill_color = coloring_strategy_
                               ? (*coloring_strategy_)(*rect.node_)
                               : IM_COL32(100, 150, 200, 255); // Default color
        if (rect.node_ == hovered_node_) {
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

        ImVec2 screen_min(canvas_pos.x + rect.rect_.x,
                          canvas_pos.y + rect.rect_.y);
        ImVec2 screen_max(screen_min.x + rect.rect_.width,
                          screen_min.y + rect.rect_.height);

        draw_list->AddRectFilled(screen_min, screen_max, fill_color);
        draw_list->AddRect(screen_min, screen_max, IM_COL32(255, 255, 255, 180),
                           0.0f, 0, 0.5f);
    }

    bool clicked = false;
    if (hovered_node_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {

        selected_node_ = hovered_node_;
        for (const auto &callback : node_clicked_cbs_) {
            callback(*hovered_node_);
        }
        clicked = true;
    }

    for (auto &frame : layout_.frames) {
        const ImVec2 screen_min(canvas_pos.x + frame.rect_.x,
                                canvas_pos.y + frame.rect_.y);
        const ImVec2 screen_max(screen_min.x + frame.rect_.width,
                                screen_min.y + frame.rect_.height);

        draw_list->AddRect(screen_min, screen_max, IM_COL32(0, 0, 0, 180), 0.0f,
                           0, 2.0f);
    }

    ImGui::EndChild();
    return clicked;
}