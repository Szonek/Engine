#include "nav_mesh_debug_draw.h"
#include "nav_mesh.h"

#include <DebugDraw.h>
#include <DetourDebugDraw.h>

namespace
{
// DebugUtils' draw interface on top of the debug lines: the debug-line pass draws lines only, so filled triangles and
// points are skipped (the polygon outlines are lines)
class DebugLinesDraw : public duDebugDraw
{
public:
    explicit DebugLinesDraw(std::vector<engine::DebugLineGpuData>& out_lines)
        : out_lines_(out_lines)
    {
    }

    void depthMask(bool) override {}
    void texture(bool) override {}
    void begin(duDebugDrawPrimitives prim, float) override
    {
        drawing_lines_ = prim == DU_DRAW_LINES;
        has_line_start_ = false;
    }
    void vertex(const float* pos, unsigned int color) override { vertex(pos[0], pos[1], pos[2], color); }
    void vertex(const float* pos, unsigned int color, const float*) override { vertex(pos[0], pos[1], pos[2], color); }
    void vertex(const float x, const float y, const float z, unsigned int color, const float, const float) override { vertex(x, y, z, color); }
    void vertex(const float x, const float y, const float z, unsigned int color) override
    {
        if (!drawing_lines_)
        {
            return;
        }
        if (!has_line_start_)
        {
            line_start_ = { x, y, z };
            has_line_start_ = true;
            return;
        }
        engine::DebugLineGpuData line{};
        line.from = line_start_;
        line.to = { x, y, z };
        // duRGBA packs r, g, b, a from the lowest byte
        line.color = glm::vec3(color & 0xff, (color >> 8) & 0xff, (color >> 16) & 0xff) / 255.0f;
        out_lines_.push_back(line);
        has_line_start_ = false;
    }
    void end() override { drawing_lines_ = false; }

private:
    std::vector<engine::DebugLineGpuData>& out_lines_;
    bool drawing_lines_ = false;
    bool has_line_start_ = false;
    glm::vec3 line_start_{};
};
}  // namespace

void engine::collect_nav_mesh_debug_lines(const NavMesh& nav_mesh, std::vector<DebugLineGpuData>& out_lines)
{
    DebugLinesDraw draw(out_lines);
    duDebugDrawNavMesh(&draw, nav_mesh.get_detour_nav_mesh(), 0);
}
