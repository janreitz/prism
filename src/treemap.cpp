#include "treemap.h"

namespace treemap
{
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

float worst_aspect_ratio(float total_size, float max_element_size,
                         float min_element_size, float w)
{
#if TRACY_ENABLE
    ZoneScoped;
#endif
    return std::max((w * w * max_element_size) / (total_size * total_size),
                    (total_size * total_size) / (w * w * min_element_size));
}
} // namespace treemap