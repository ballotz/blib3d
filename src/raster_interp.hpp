#pragma once

namespace lib3d::raster
{

template<uint32_t size>
struct interp_base
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

        float area2cx[num_max_attributes];
        float area2cy[num_max_attributes];
        for (uint32_t n{ 0 }; n < size; ++n)
        {
            area2cx[n] = 0.f;
            area2cy[n] = 0.f;
        }

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

constexpr int32_t interp_span_block_size{ 16 };
constexpr float interp_span_block_size_f{ 16.f };
constexpr int32_t interp_span_block_size_shift{ 4 };

template<uint32_t size>
struct interp : public interp_base<size>
{
    static constexpr size_t attrib_int_offset{ 2 };
    static constexpr size_t attrib_int_size{ size - attrib_int_offset };

    float attrib[size];
    float depth;
    int32_t dx_int[attrib_int_size]; // 16.16
    int32_t attrib_int[attrib_int_size]; // 16.16
    int32_t attrib_int_next[attrib_int_size]; // 16.16

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        for (uint32_t n{ 0 }; n < size; ++n)
            attrib[n] =
                interp_base<size>::g[n].dx * x0f +
                interp_base<size>::g[n].dy * y0f +
                interp_base<size>::g[n].d;
        float w{ (float)0x10000 / attrib[1] };
        for (uint32_t n{ 0 }; n < attrib_int_size; ++n)
            attrib_int_next[n] = (int32_t)(attrib[n + attrib_int_offset] * w);
    }

    force_inline void setup_subspan(int32_t count)
    {
        if (count == interp_span_block_size)
        {
            depth = attrib[0];
            for (uint32_t n{ 0 }; n < size; ++n)
                attrib[n] += interp_base<size>::g[n].dx * interp_span_block_size_f;
            for (uint32_t n{ 0 }; n < attrib_int_size; ++n)
                attrib_int[n] = attrib_int_next[n];
            float w{ (float)0x10000 / attrib[1] };
            for (uint32_t n{ 0 }; n < attrib_int_size; ++n)
                attrib_int_next[n] = (int32_t)(attrib[n + attrib_int_offset] * w);
            for (uint32_t n{ 0 }; n < attrib_int_size; ++n)
                dx_int[n] = (attrib_int_next[n] - attrib_int[n]) >> interp_span_block_size_shift;
        }
        else
        {
            depth = attrib[0];
            float count_float{ (float)count };
            for (uint32_t n{ 0 }; n < size; ++n)
                attrib[n] += interp_base<size>::g[n].dx * count_float;
            for (uint32_t n{ 0 }; n < attrib_int_size; ++n)
                attrib_int[n] = attrib_int_next[n];
            float w{ (float)0x10000 / attrib[1] };
            for (uint32_t n{ 0 }; n < attrib_int_size; ++n)
                attrib_int_next[n] = (int32_t)(attrib[n + attrib_int_offset] * w);
            for (uint32_t n{ 0 }; n < attrib_int_size; ++n)
                dx_int[n] = (attrib_int_next[n] - attrib_int[n]) / count;
        }
    }

    force_inline void advance()
    {
        depth += interp_base<size>::g[0].dx;
        for (uint32_t n{ 0 }; n < attrib_int_size; ++n)
            attrib_int[n] += dx_int[n];
    }
};

template<>
struct interp<1> : public interp_base<1>
{
    float attrib;
    float depth;

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        attrib = x0f * g[0].dx + y0f * g[0].dy + g[0].d;
    }

    force_inline void setup_subspan(int32_t count)
    {
        depth = attrib;
        attrib += g[0].dx * (float)count;
    }

    force_inline void advance()
    {
        depth += g[0].dx;
    }
};

template<>
struct interp<2>
{
};

} // namespace lib3d::raster
