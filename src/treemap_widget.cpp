#include "treemap_widget.h"

ImVec2 WindowCoordinate::to_imvec2() const { return ImVec2(x, y); }
WindowCoordinate WindowCoordinate::from_imvec2(ImVec2 vec2)
{
    return WindowCoordinate{
        .x = vec2.x,
        .y = vec2.y,
    };
}

ImVec2 CanvasCoordinate::to_imvec2() const { return ImVec2(x, y); }
CanvasCoordinate CanvasCoordinate::from_imvec2(ImVec2 vec2)
{
    return CanvasCoordinate{
        .x = vec2.x,
        .y = vec2.y,
    };
}

ImVec2 TreemapCoordinate::to_imvec2() const { return ImVec2(x, y); }
TreemapCoordinate TreemapCoordinate::from_imvec2(ImVec2 vec2)
{
    return TreemapCoordinate{
        .x = vec2.x,
        .y = vec2.y,
    };
}

WindowCoordinate to_window(CanvasCoordinate canvas_coord,
                           WindowCoordinate canvas_pos)
{
    return WindowCoordinate{
        .x = canvas_coord.x + canvas_pos.x,
        .y = canvas_coord.y + canvas_pos.y,
    };
}

CanvasCoordinate to_canvas(WindowCoordinate win_coord,
                           WindowCoordinate canvas_pos)
{
    return CanvasCoordinate{
        .x = win_coord.x - canvas_pos.x,
        .y = win_coord.y - canvas_pos.y,
    };
}

TreemapCoordinate to_treemap(CanvasCoordinate canvas_coord, TreemapCoordinate pan, float zoom) {
    return TreemapCoordinate{
        .x = (canvas_coord.x / zoom) + pan.x,
        .y = (canvas_coord.y / zoom) + pan.y
    };
}

CanvasCoordinate to_canvas(TreemapCoordinate treemap_coord, TreemapCoordinate pan, float zoom) {
    return CanvasCoordinate{
        .x = (treemap_coord.x - pan.x) * zoom,
        .y = (treemap_coord.y - pan.y) * zoom
    };
}