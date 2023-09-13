#pragma once

namespace lib3d::raster
{

template<uint32_t size>
struct interpolator
{
    bool back_cull;

    struct gradient
    {
        float dx;
        float dy;
        float d;
    };
    gradient g[size];

    void setup(const config* cfg)
    {
        back_cull = cfg->back_cull;
    }

    bool setup_face(const float* pv[], uint32_t vertex_count, bool& is_clockwise)
    {
        constexpr float discard_thresh{ 1.f };

        constexpr uint32_t num_max_triangles{ num_max_vertices - 2 };
        const uint32_t num_triangles{ vertex_count - 2 };

        float v0[num_max_triangles][2];
        float v1[num_max_triangles][2];

        float area2xy{ 0.f };

        constexpr uint32_t nv0{ 0 };
        uint32_t nv1{ 1 };
        uint32_t nv2{ 2 };
        for (uint32_t nt{ 0 }; nt < num_triangles; ++nt)
        {
            // v0->v1
            v0[nt][0] = pv[nv1][0] - pv[nv0][0];
            v0[nt][1] = pv[nv1][1] - pv[nv0][1];

            // v0->v2
            v1[nt][0] = pv[nv2][0] - pv[nv0][0];
            v1[nt][1] = pv[nv2][1] - pv[nv0][1];

            area2xy += math::cross2(v0[nt][0], v0[nt][1], v1[nt][0], v1[nt][1]);

            ++nv1;
            ++nv2;
        }

        if (area2xy > -discard_thresh && area2xy < discard_thresh)
            return false;

        bool clockwise = area2xy > 0.f;

        if (back_cull && !clockwise)
            return false;

        is_clockwise = clockwise;

        float area2cx[num_max_attributes]{};
        float area2cy[num_max_attributes]{};

        //nv0 = 0;
        nv1 = 1;
        nv2 = 2;
        for (uint32_t nt{ 0 }; nt < num_triangles; ++nt)
        {
            for (uint32_t n{ 0 }, nc{ 2 }; n < size; ++n, ++nc)
            {
                float v0c{ pv[nv1][nc] - pv[nv0][nc] }; // v0->v1
                float v1c{ pv[nv2][nc] - pv[nv0][nc] }; // v0->v2
                area2cx[n] += math::cross2(v0c, v0[nt][0], v1c, v1[nt][0]);
                area2cy[n] += math::cross2(v0c, v0[nt][1], v1c, v1[nt][1]);
            }

            ++nv1;
            ++nv2;
        }

        float area2xyinv{ 1.f / area2xy };

        for (uint32_t n{ 0 }, nc{ 2 }; n < size; ++n, ++nc)
        {
            g[n].dx = area2cy[n] * area2xyinv;
            g[n].dy = -area2cx[n] * area2xyinv;
            g[n].d = pv[nv0][nc] - g[n].dx * pv[nv0][0] - g[n].dy * pv[nv0][1];
        }

        return true;
    };
};

} // namespace lib3d::raster
