/* Copyright 2019 Alessio Ballotti <alessioballotti@tiscali.it> */

#include "raster.hpp"
#include "raster_interp.hpp"
#include "raster_fill.hpp"
#include <new>
#include <cassert>
#include <cfloat>
#include <algorithm>

namespace blib3d::raster
{

//------------------------------------------------------------------------------

blib3d_force_inline int32_t real_to_raster(float v)
{
    return math::ceil(v - 0.5f);
}

blib3d_force_inline float raster_to_real(int32_t v)
{
    return (float)v + 0.5f;
}

//------------------------------------------------------------------------------

void batch_draw_wireframe(const config* c)
{
    uint32_t num_faces{ c->num_faces };
    uint32_t vertex_stride{ c->vertex_stride };
    const uint32_t* num_vertices{ c->vertex_count_data };
    const float* vertex_data{ c->vertex_data };
    int32_t frame_width{ c->frame_width };
    int32_t frame_height{ c->frame_height };
    int32_t frame_stride{ c->frame_stride };
    ARGB* frame_buffer{ c->frame_buffer };
    uint32_t* frame_addr{ reinterpret_cast<uint32_t*>(frame_buffer) };

    while (num_faces--)
    {
        uint32_t vertex_count{ *num_vertices++ };

        for (uint32_t v0{ vertex_count - 1 }, v1{ 0 }; v1 < vertex_count; v0 = v1++)
        {
            float x0{ vertex_data[vertex_stride * v0 + 0] };
            float y0{ vertex_data[vertex_stride * v0 + 1] };
            float x1{ vertex_data[vertex_stride * v1 + 0] };
            float y1{ vertex_data[vertex_stride * v1 + 1] };

            float dx{ x1 - x0 };
            float dy{ y1 - y0 };
            if (y1 < y0)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
            }
            if (std::abs(dx) <= std::abs(dy))
            {
                float slope{ dx / dy };
                int32_t y{ math::max((int32_t)0, real_to_raster(y0)) };
                int32_t yend{ math::min(frame_height, real_to_raster(y1)) };
                float xf{ x0 + ((float)y - y0) * slope };
                for (; y < yend; ++y)
                {
                    int32_t x{ real_to_raster(xf) };
                    if (x >= 0 && x < frame_width)
                        frame_addr[x + frame_stride * y] = 0xFFFFFFFFu;
                    xf += slope;
                }
            }
            else
            {
                float slope{ dy / dx };
                int32_t x{ math::max((int32_t)0, real_to_raster(x0)) };
                int32_t xend{ math::min(frame_width, real_to_raster(x1)) };
                float yf{ y0 + ((float)x - x0) * slope };
                if (x0 <= x1)
                {
                    for (; x < xend; ++x)
                    {
                        int32_t y{ real_to_raster(yf) };
                        if (y >= 0 && y < frame_height)
                            frame_addr[x + frame_stride * y] = 0xFFFFFFFFu;
                        yf += slope;
                    }
                }
                else
                {
                    for (; x > xend; --x)
                    {
                        int32_t y{ real_to_raster(yf) };
                        if (y >= 0 && y < frame_height)
                            frame_addr[x + frame_stride * y] = 0xFFFFFFFFu;
                        yf -= slope;
                    }
                }
            }
        }

        vertex_data += vertex_stride * vertex_count;
    }
}

//------------------------------------------------------------------------------

struct scan
{
    void batch_draw(const config* c);

    void scan_face(const float* v[], const int32_t num_vertices, const bool is_clockwise);

    virtual void setup(const config* c) = 0;

    virtual bool setup_face(const float* pv[], uint32_t vertex_count,
        const bool back_cull, bool& is_clockwise) = 0;

    virtual void process_span(int32_t y, int32_t x0, int32_t x1) = 0;
};

void scan::batch_draw(const config* c)
{
    uint32_t num_faces{ c->num_faces };
    uint32_t vertex_stride{ c->vertex_stride };
    const uint32_t* num_vertices{ c->vertex_count_data };
    const float* vertex_data{ c->vertex_data };
    const bool back_cull{ c->back_cull };

    setup(c);

    while (num_faces--)
    {
        const uint32_t vertex_count{ *num_vertices++ };

        const float* v[num_max_vertices];
        for (uint32_t nv{}; nv < vertex_count; ++nv)
        {
            v[nv] = vertex_data;
            vertex_data += vertex_stride;
        }

        bool is_clockwise;
        if (setup_face(v, vertex_count, back_cull, is_clockwise))
            scan_face(v, vertex_count, is_clockwise);
    }
}

/*
          v0
          *
         *  *
        *     * v1
       *  *
   v2 *

    (1)
    exact edge rasterization could be done using
        * fixed point integer x and y coordinates with subpixel precision
        * integer edge walking with an integer error term
    this avoids floating point error to cause, in some nearly degenerate triangles
        * sign changes in the computation of the cross product (winding test, not here anymore)
        * error accumulation to make some edges intersect before the endpoint
    but using integer coordinates along with integer line interpolation for edges
    is slower than using float with the x[0] < x[1] check

    (2)
    having surfaces with more than 3 vertices introduced the issue of handling concave shapes
    this should not happen, but if it does for numerical precision or whatever
    this could cause the scanning loop to not terminate
    to avoid this situation backward tilted edges are treated as horizontal and skipped
*/
void scan::scan_face(const float* v[], const int32_t vertex_count, const bool is_clockwise)
{
    struct edge
    {
        float x;
        float dx_dy;

        blib3d_force_inline void setup(const float* v0, const float* v1, const float y0)
        {
            dx_dy = (v1[0] - v0[0]) / (v1[1] - v0[1]);
            x = v0[0] + (y0 - v0[1]) * dx_dy;
        }

        blib3d_force_inline void advance()
        {
            x += dx_dy;
        }
    };

    struct util
    {
        static blib3d_force_inline int32_t wrap(const int32_t v, const int32_t last)
        {
            if (v < 0)
                return last;
            if (v > last)
                return 0;
            return v;
        };
    };

    const int32_t vi{ is_clockwise ? -1 : 1 }; // vertex index increment

    int32_t vt{ 0 }; // top vertex index
    int32_t vb{ 0 }; // bottom vertex index
    for (int32_t n{ 1 }; n < vertex_count; ++n)
    {
        if (v[vt][1] > v[n][1])
            vt = n;
        if (v[vb][1] < v[n][1])
            vb = n;
    }

    const int32_t ylimit[2]
    {
        real_to_raster(v[vt][1]),
        real_to_raster(v[vb][1])
    };

    if (ylimit[0] == ylimit[1])
        return;

    int32_t last_vertex{ vertex_count - 1 };

    int32_t vl[2]{ vt, util::wrap(vt + vi, last_vertex) }; // left edge vertex indexes
    int32_t vr[2]{ vt, util::wrap(vt - vi, last_vertex) }; // right edge vertex indexes

    int32_t yl[2]{ ylimit[0], real_to_raster(v[vl[1]][1]) }; // left edge y
    int32_t yr[2]{ ylimit[0], real_to_raster(v[vr[1]][1]) }; // right edge y

    edge e[2]; // e[0] left, e[1] right

    int32_t y{ ylimit[0] };
    int32_t yend{ y };
    for (;;)
    {
        if (y == yend)
        {
            if (y >= yl[1]) // advance left edge (2)
            {
                vl[0] = util::wrap(vl[0] + vi, last_vertex);
                vl[1] = util::wrap(vl[1] + vi, last_vertex);
                if (yl[1] == ylimit[1]) // next top is polygon bottom
                    break;
                yl[0] = yl[1];
                yl[1] = real_to_raster(v[vl[1]][1]);
                continue;
            }
            if (y >= yr[1]) // advance right edge (2)
            {
                vr[0] = util::wrap(vr[0] - vi, last_vertex);
                vr[1] = util::wrap(vr[1] - vi, last_vertex);
                if (yr[1] == ylimit[1]) // next top is polygon bottom
                    break;
                yr[0] = yr[1];
                yr[1] = real_to_raster(v[vr[1]][1]);
                continue;
            }
            if (y >= yl[0]) // setup left edge (2)
                e[0].setup(v[vl[0]], v[vl[1]], raster_to_real(y));
            if (y >= yr[0]) // setup right edge (2)
                e[1].setup(v[vr[0]], v[vr[1]], raster_to_real(y));
            yend = yl[1] < yr[1] ? yl[1] : yr[1]; // next nearest end point
        }
        //assert(y < ylimit[1]);
        const int32_t x[2]
        {
            real_to_raster(e[0].x),
            real_to_raster(e[1].x)
        };
        if (x[0] < x[1]) // float error guard (1)
            process_span(y, x[0], x[1]);
        e[0].advance();
        e[1].advance();
        ++y;
    }
}

//------------------------------------------------------------------------------

/*
    span fill algorithm
*/

constexpr int32_t span_block_size{ 16 };
constexpr int32_t span_block_size_shift{ 4 };

//template<typename raster_type>
//blib3d_force_inline void span_process_algo(int32_t y, int32_t x0, int32_t x1, raster_type* r)
//{
//    //assert(x0 < x1);
//    typename raster_type::span_data s;
//    r->setup_span(y, x0, s);
//    int32_t n{ x1 - x0 };
//    while (n)
//    {
//        int32_t c{ math::min(n, span_block_size) };
//        n -= c;
//        raster_type::setup_subspan(c, s);
//        while (c--)
//            raster_type::fill(s);
//    }
//}

//template<typename raster_type>
//blib3d_force_inline void span_process_algo(raster_type& raster, int32_t y, int32_t x0, int32_t x1)
//{
//    //assert(x0 < x1);
//    raster.setup_span(y, x0);
//    while (x0 != x1)
//    {
//        int32_t n{ (x0 + span_block_size) & ~(span_block_size - 1) };
//        int32_t c{ math::min(n, x1) - x0 };
//        x0 += c;
//        raster.setup_subspan(c);
//        while (count--)
//            raster.fill();
//    }
//}

constexpr float subspan_scale[span_block_size]
{
           0, 1.f /  1, 1.f /  2, 1.f /  3,
    1.f /  4, 1.f /  5, 1.f /  6, 1.f /  7,
    1.f /  8, 1.f /  9, 1.f / 10, 1.f / 11,
    1.f / 12, 1.f / 13, 1.f / 14, 1.f / 15
};

static constexpr uint32_t shade_hold{ 4 };
static constexpr uint32_t shade_mask{ shade_hold - 1 };

//------------------------------------------------------------------------------

void sample_light(
    const math::vec3 pos, const math::vec3 norm,
    const light lights[], uint32_t num_lights,
    const math::powfast_table* table,
    uint32_t res[3])
{
    math::vec3 resf{};
    for (uint32_t n{}; n < num_lights; n++)
    {
        const light& l{ lights[n] };
        switch (l.type)
        {
        case light::type_ambient:
            light_ambient(l, resf);
            break;
        case light::type_directional:
            light_directional(l, norm, resf);
            break;
        case light::type_point:
            light_point(l, pos, norm, resf);
            break;
        case light::type_spot:
            light_spot(l, pos, norm, resf);
            break;
        }
    }
    math::min3(resf, resf, 1.f);
    resf[0] = math::powfast(resf[0], *table);
    resf[1] = math::powfast(resf[1], *table);
    resf[2] = math::powfast(resf[2], *table);
    math::mul3(resf, (float)0xFF);
    res[0] = (uint32_t)resf[0];
    res[1] = (uint32_t)resf[1];
    res[2] = (uint32_t)resf[2];
}

//------------------------------------------------------------------------------

template<typename depth_type = depth_test_write>
struct raster_depth : public scan
{
    // scan

    void setup(const config* c) override
    {
        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
    }

    enum
    {
        attrib_z,
        attrib_count
    };

    bool setup_face(const float* pv[], uint32_t vertex_count, bool back_cull, bool& is_clockwise) override
    {
        return interp_setup_face(pv, vertex_count, back_cull, is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        float l_gdx;
        float l_attrib;
        float l_depth;
        float* l_depth_addr;

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        int32_t start{ frame_stride * y + x0 };
        l_gdx = g[0].dx;
        l_attrib = x0f * g[0].dx + y0f * g[0].dy + g[0].d;
        l_depth_addr = &depth_buffer[start];

        int32_t n{ x1 - x0 };
        while (n)
        {
            int32_t count{ math::min(n, span_block_size) };
            n -= count;

            l_depth = l_attrib;
            l_attrib += l_gdx * (float)count;

            while (count--)
            {
                if (depth_type::process_test(l_depth_addr, l_depth))
                    depth_type::process_write(l_depth_addr, l_depth);

                l_depth += l_gdx;

                l_depth_addr++;
            }
        }
    }

    // raster

    gradient g[attrib_count];

    int32_t frame_stride;
    float* depth_buffer;
};

//------------------------------------------------------------------------------

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_solid_shade_none : public scan
{
    // scan

    void setup(const config* c) override
    {
        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        fill_color = reinterpret_cast<const uint32_t&>(c->fill_color);
    }

    enum
    {
        attrib_z,
        attrib_count
    };

    bool setup_face(const float* pv[], uint32_t vertex_count, bool back_cull, bool& is_clockwise) override
    {
        return interp_setup_face(pv, vertex_count, back_cull, is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        float l_gdx;
        uint32_t l_fill_color;
        float l_attrib;
        float l_depth;
        float* l_depth_addr;
        uint32_t* l_frame_addr;

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        int32_t start{ frame_stride * y + x0 };
        l_gdx = g[0].dx;
        l_fill_color = fill_color;
        l_attrib = x0f * g[0].dx + y0f * g[0].dy + g[0].d;
        l_depth_addr = &depth_buffer[start];
        l_frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);

        int32_t n{ x1 - x0 };
        while (n)
        {
            int32_t count{ math::min(n, span_block_size) };
            n -= count;

            l_depth = l_attrib;
            l_attrib += l_gdx * (float)count;

            while (count--)
            {
                if (depth_type::process_test(l_depth_addr, l_depth))
                {
                    blend_type::process(l_frame_addr, l_fill_color);
                    depth_type::process_write(l_depth_addr, l_depth);
                }

                l_depth += l_gdx;

                l_depth_addr++;
                l_frame_addr++;
            }
        }
    }

    // raster

    gradient g[attrib_count];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    uint32_t fill_color;
};

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_solid_shade_vertex : public scan
{
    // scan

    void setup(const config* c) override
    {
        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        fill_color[0] = (uint32_t)c->fill_color.a << 24u;
        fill_color[1] = (uint32_t)c->fill_color.r;
        fill_color[2] = (uint32_t)c->fill_color.g;
        fill_color[3] = (uint32_t)c->fill_color.b;
    }

    enum
    {
        attrib_z,
        attrib_w,
        attrib_sr,
        attrib_sg,
        attrib_sb,
        attrib_count
    };

    bool setup_face(const float* pv[], uint32_t vertex_count, bool back_cull, bool& is_clockwise) override
    {
        return interp_setup_face(pv, vertex_count, back_cull, is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        float l_gdx[attrib_count];
        uint32_t l_fill_color[4];
        float l_attrib[attrib_count];
        float l_depth;
        float* l_depth_addr;
        uint32_t* l_frame_addr;
        int32_t l_attrib_int_dx[attrib_count - 2]; // 16.16
        int32_t l_attrib_int[attrib_count - 2]; // 16.16
        int32_t l_attrib_int_next[attrib_count - 2]; // 16.16

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        int32_t start{ frame_stride * y + x0 };
        l_gdx[0] = g[0].dx;
        l_gdx[1] = g[1].dx;
        l_gdx[2] = g[2].dx;
        l_gdx[3] = g[3].dx;
        l_gdx[4] = g[4].dx;
        l_fill_color[0] = fill_color[0];
        l_fill_color[1] = fill_color[1];
        l_fill_color[2] = fill_color[2];
        l_fill_color[3] = fill_color[3];
        l_attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        l_attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        l_attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        l_attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        l_attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        float w{ (float)0x10000 / l_attrib[1] };
        l_attrib_int_next[0] = math::clamp((int32_t)(l_attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[1] = math::clamp((int32_t)(l_attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[2] = math::clamp((int32_t)(l_attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_depth_addr = &depth_buffer[start];
        l_frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);

        int32_t n{ x1 - x0 };
        while (n)
        {
            int32_t count{ math::min(n, span_block_size) };
            n -= count;

            float count_float{ (float)count };
            l_depth = l_attrib[0];
            l_attrib[0] += l_gdx[0] * count_float;
            l_attrib[1] += l_gdx[1] * count_float;
            l_attrib[2] += l_gdx[2] * count_float;
            l_attrib[3] += l_gdx[3] * count_float;
            l_attrib[4] += l_gdx[4] * count_float;
            float w{ (float)0x10000 / l_attrib[1] };
            l_attrib_int[0] = l_attrib_int_next[0];
            l_attrib_int[1] = l_attrib_int_next[1];
            l_attrib_int[2] = l_attrib_int_next[2];
            l_attrib_int_next[0] = math::clamp((int32_t)(l_attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[1] = math::clamp((int32_t)(l_attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[2] = math::clamp((int32_t)(l_attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            if (count == span_block_size)
            {
                l_attrib_int_dx[0] = (l_attrib_int_next[0] - l_attrib_int[0]) >> span_block_size_shift;
                l_attrib_int_dx[1] = (l_attrib_int_next[1] - l_attrib_int[1]) >> span_block_size_shift;
                l_attrib_int_dx[2] = (l_attrib_int_next[2] - l_attrib_int[2]) >> span_block_size_shift;
            }
            else
            {
                float scale{ subspan_scale[count] };
                l_attrib_int_dx[0] = (int32_t)((float)(l_attrib_int_next[0] - l_attrib_int[0]) * scale);
                l_attrib_int_dx[1] = (int32_t)((float)(l_attrib_int_next[1] - l_attrib_int[1]) * scale);
                l_attrib_int_dx[2] = (int32_t)((float)(l_attrib_int_next[2] - l_attrib_int[2]) * scale);
            }

            while (count--)
            {
                if (depth_type::process_test(l_depth_addr, l_depth))
                {
                    uint32_t color
                    {
                        (((l_fill_color[0]                            )              )       ) +
                        (((l_fill_color[1] * (uint32_t)l_attrib_int[0]) & 0xFF000000u) >>  8u) +
                        (((l_fill_color[2] * (uint32_t)l_attrib_int[1]) & 0xFF000000u) >> 16u) +
                        (((l_fill_color[3] * (uint32_t)l_attrib_int[2])              ) >> 24u) + 0x00010101u
                    };
                    blend_type::process(l_frame_addr, color);
                    depth_type::process_write(l_depth_addr, l_depth);
                }

                l_depth += l_gdx[0];
                l_attrib_int[0] += l_attrib_int_dx[0];
                l_attrib_int[1] += l_attrib_int_dx[1];
                l_attrib_int[2] += l_attrib_int_dx[2];

                l_depth_addr++;
                l_frame_addr++;
            }
        }
    }

    // raster

    gradient g[attrib_count];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    uint32_t fill_color[4];
};

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_solid_shade_lightmap : public scan
{
    // scan

    void setup(const config* c) override
    {
        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        fill_color[0] = (uint32_t)c->fill_color.a << 24u;
        fill_color[1] = (uint32_t)c->fill_color.r <<  8u;
        fill_color[2] = (uint32_t)c->fill_color.g;
        fill_color[3] = (uint32_t)c->fill_color.b;
        umax = (c->lightmap_width - 1) << 16;
        vmax = (c->lightmap_height - 1) << 16;
        vshift = math::log2(c->lightmap_width);
        lightmap = (const uint32_t*)(c->lightmap);
    }

    enum
    {
        attrib_z,
        attrib_w,
        attrib_su,
        attrib_sv,
        attrib_count
    };

    bool setup_face(const float* pv[], uint32_t vertex_count, bool back_cull, bool& is_clockwise) override
    {
        return interp_setup_face(pv, vertex_count, back_cull, is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        float l_gdx[attrib_count];
        uint32_t l_fill_color[attrib_count];
        int32_t l_umax;
        int32_t l_vmax;
        int32_t l_vshift;
        const uint32_t* l_lightmap;
        float l_attrib[attrib_count];
        float l_depth;
        float* l_depth_addr;
        uint32_t* l_frame_addr;
        int32_t l_attrib_int_dx[attrib_count - 2]; // 16.16
        int32_t l_attrib_int[attrib_count - 2]; // 16.16
        int32_t l_attrib_int_next[attrib_count - 2]; // 16.16
        uint32_t l_shade_counter;
        uint32_t l_shade_trigger;
        uint32_t l_shade[3];

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        int32_t start{ frame_stride * y + x0 };
        l_gdx[0] = g[0].dx;
        l_gdx[1] = g[1].dx;
        l_gdx[2] = g[2].dx;
        l_gdx[3] = g[3].dx;
        l_fill_color[0] = fill_color[0];
        l_fill_color[1] = fill_color[1];
        l_fill_color[2] = fill_color[2];
        l_fill_color[3] = fill_color[3];
        l_umax = umax;
        l_vmax = vmax;
        l_vshift = vshift;
        l_lightmap = lightmap;
        l_attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        l_attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        l_attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        l_attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        float w{ (float)0x10000 / l_attrib[1] };
        l_attrib_int_next[0] = math::clamp((int32_t)(l_attrib[2] * w), (int32_t)0, umax);
        l_attrib_int_next[1] = math::clamp((int32_t)(l_attrib[3] * w), (int32_t)0, vmax);
        l_depth_addr = &depth_buffer[start];
        l_frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
        l_shade_counter = ((y & 1 ? shade_hold >> 1u : 0u) + x0) & shade_mask;
        l_shade_trigger = 1;

        int32_t n{ x1 - x0 };
        while (n)
        {
            int32_t count{ math::min(n, span_block_size) };
            n -= count;

            float count_float{ (float)count };
            l_depth = l_attrib[0];
            l_attrib[0] += l_gdx[0] * count_float;
            l_attrib[1] += l_gdx[1] * count_float;
            l_attrib[2] += l_gdx[2] * count_float;
            l_attrib[3] += l_gdx[3] * count_float;
            float w{ (float)0x10000 / l_attrib[1] };
            l_attrib_int[0] = l_attrib_int_next[0];
            l_attrib_int[1] = l_attrib_int_next[1];
            l_attrib_int_next[0] = math::clamp((int32_t)(l_attrib[2] * w), (int32_t)0, l_umax);
            l_attrib_int_next[1] = math::clamp((int32_t)(l_attrib[3] * w), (int32_t)0, l_vmax);
            if (count == span_block_size)
            {
                l_attrib_int_dx[0] = (l_attrib_int_next[0] - l_attrib_int[0]) >> span_block_size_shift;
                l_attrib_int_dx[1] = (l_attrib_int_next[1] - l_attrib_int[1]) >> span_block_size_shift;
            }
            else
            {
                float scale{ subspan_scale[count] };
                l_attrib_int_dx[0] = (int32_t)((float)(l_attrib_int_next[0] - l_attrib_int[0]) * scale);
                l_attrib_int_dx[1] = (int32_t)((float)(l_attrib_int_next[1] - l_attrib_int[1]) * scale);
            }

            while (count--)
            {
                if (depth_type::process_test(l_depth_addr, l_depth))
                {
                    if (((l_shade_counter & shade_mask) == 0) | l_shade_trigger)
                    {
                        l_shade_trigger = 0;
                        uint32_t shade_color{ sample_lightmap(
                            l_attrib_int[0],
                            l_attrib_int[1],
                            l_vshift, l_lightmap) };
                        l_shade[0] = (shade_color & 0x00FF0000u) >> 16u;
                        l_shade[1] = (shade_color & 0x0000FF00u) >>  8u;
                        l_shade[2] = (shade_color & 0x000000FFu)       ;
                    }
                    uint32_t color
                    {
                        (((l_fill_color[0]             )              )      ) +
                        (((l_fill_color[1] * l_shade[0]) & 0x00FF0000u)      ) +
                        (((l_fill_color[2] * l_shade[1]) & 0x0000FF00u)      ) +
                        (((l_fill_color[3] * l_shade[2])              ) >> 8u) + 0x00010101u
                    };
                    blend_type::process(l_frame_addr, color);
                    depth_type::process_write(l_depth_addr, l_depth);
                }
                else
                {
                    l_shade_trigger = 1;
                }

                l_depth += l_gdx[0];
                l_attrib_int[0] += l_attrib_int_dx[0];
                l_attrib_int[1] += l_attrib_int_dx[1];

                l_depth_addr++;
                l_frame_addr++;

                l_shade_counter++;
            }
        }
    }

    // raster

    gradient g[attrib_count];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    uint32_t fill_color[4];
    int32_t umax;
    int32_t vmax;
    int32_t vshift;
    const uint32_t* lightmap;
};

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_solid_shade_light : public scan
{
    // scan

    void setup(const config* c) override
    {
        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        fill_color[0] = (uint32_t)c->fill_color.a << 24u;
        fill_color[1] = (uint32_t)c->fill_color.r <<  8u;
        fill_color[2] = (uint32_t)c->fill_color.g;
        fill_color[3] = (uint32_t)c->fill_color.b;
        num_lights = c->num_lights;
        light_data = c->light_data;
        light_table = c->light_table;
    }

    enum
    {
        attrib_z,
        attrib_w,
        attrib_px,
        attrib_py,
        attrib_pz,
        attrib_nx,
        attrib_ny,
        attrib_nz,
        attrib_count
    };

    bool setup_face(const float* pv[], uint32_t vertex_count, bool back_cull, bool& is_clockwise) override
    {
        return interp_setup_face(pv, vertex_count, back_cull, is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        float l_gdx[attrib_count];
        uint32_t l_fill_color[4];
        uint32_t l_num_lights;
        const light* l_light_data;
        const math::powfast_table* l_light_table;
        float l_attrib[attrib_count];
        float l_depth;
        float* l_depth_addr;
        uint32_t* l_frame_addr;
        float l_attrib_int_dx[attrib_count - 2];
        float l_attrib_int[attrib_count - 2];
        float l_attrib_int_next[attrib_count - 2];
        uint32_t l_shade_counter;
        uint32_t l_shade_trigger;
        uint32_t l_shade[3];

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        int32_t start{ frame_stride * y + x0 };
        l_gdx[0] = g[0].dx;
        l_gdx[1] = g[1].dx;
        l_gdx[2] = g[2].dx;
        l_gdx[3] = g[3].dx;
        l_gdx[4] = g[4].dx;
        l_gdx[5] = g[5].dx;
        l_gdx[6] = g[6].dx;
        l_gdx[7] = g[7].dx;
        l_fill_color[0] = fill_color[0];
        l_fill_color[1] = fill_color[1];
        l_fill_color[2] = fill_color[2];
        l_fill_color[3] = fill_color[3];
        l_num_lights = num_lights;
        l_light_data = light_data;
        l_light_table = light_table;
        l_attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        l_attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        l_attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        l_attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        l_attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        l_attrib[5] = g[5].dx * x0f + g[5].dy * y0f + g[5].d;
        l_attrib[6] = g[6].dx * x0f + g[6].dy * y0f + g[6].d;
        l_attrib[7] = g[7].dx * x0f + g[7].dy * y0f + g[7].d;
        float w{ 1.f / l_attrib[1] };
        l_attrib_int_next[0] = l_attrib[2] * w;
        l_attrib_int_next[1] = l_attrib[3] * w;
        l_attrib_int_next[2] = l_attrib[4] * w;
        l_attrib_int_next[3] = l_attrib[5] * w;
        l_attrib_int_next[4] = l_attrib[6] * w;
        l_attrib_int_next[5] = l_attrib[7] * w;
        l_depth_addr = &depth_buffer[start];
        l_frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
        l_shade_counter = ((y & 1 ? shade_hold >> 1u : 0u) + x0) & shade_mask;
        l_shade_trigger = 1;

        int32_t n{ x1 - x0 };
        while (n)
        {
            int32_t count{ math::min(n, span_block_size) };
            n -= count;

            float count_float{ (float)count };
            l_depth = l_attrib[0];
            l_attrib[0] += l_gdx[0] * count_float;
            l_attrib[1] += l_gdx[1] * count_float;
            l_attrib[2] += l_gdx[2] * count_float;
            l_attrib[3] += l_gdx[3] * count_float;
            l_attrib[4] += l_gdx[4] * count_float;
            l_attrib[5] += l_gdx[5] * count_float;
            l_attrib[6] += l_gdx[6] * count_float;
            l_attrib[7] += l_gdx[7] * count_float;
            float w{ 1.f / l_attrib[1] };
            l_attrib_int[0] = l_attrib_int_next[0];
            l_attrib_int[1] = l_attrib_int_next[1];
            l_attrib_int[2] = l_attrib_int_next[2];
            l_attrib_int[3] = l_attrib_int_next[3];
            l_attrib_int[4] = l_attrib_int_next[4];
            l_attrib_int[5] = l_attrib_int_next[5];
            l_attrib_int_next[0] = l_attrib[2] * w;
            l_attrib_int_next[1] = l_attrib[3] * w;
            l_attrib_int_next[2] = l_attrib[4] * w;
            l_attrib_int_next[3] = l_attrib[5] * w;
            l_attrib_int_next[4] = l_attrib[6] * w;
            l_attrib_int_next[5] = l_attrib[7] * w;
            float scale{ 1.f / count_float };
            l_attrib_int_dx[0] = (l_attrib_int_next[0] - l_attrib_int[0]) * scale;
            l_attrib_int_dx[1] = (l_attrib_int_next[1] - l_attrib_int[1]) * scale;
            l_attrib_int_dx[2] = (l_attrib_int_next[2] - l_attrib_int[2]) * scale;
            l_attrib_int_dx[3] = (l_attrib_int_next[3] - l_attrib_int[3]) * scale;
            l_attrib_int_dx[4] = (l_attrib_int_next[4] - l_attrib_int[4]) * scale;
            l_attrib_int_dx[5] = (l_attrib_int_next[5] - l_attrib_int[5]) * scale;

            while (count--)
            {
                if (depth_type::process_test(l_depth_addr, l_depth))
                {
                    if (((l_shade_counter & shade_mask) == 0) | l_shade_trigger)
                    {
                        l_shade_trigger = 0;
                        sample_light(
                            &l_attrib_int[0], &l_attrib_int[3],
                            l_light_data, l_num_lights,
                            l_light_table,
                            l_shade);
                    }
                    uint32_t color
                    {
                        (((l_fill_color[0]             )              )      ) +
                        (((l_fill_color[1] * l_shade[0]) & 0x00FF0000u)      ) +
                        (((l_fill_color[2] * l_shade[1]) & 0x0000FF00u)      ) +
                        (((l_fill_color[3] * l_shade[2])              ) >> 8u) + 0x00010101u
                    };
                    blend_type::process(l_frame_addr, color);
                    depth_type::process_write(l_depth_addr, l_depth);
                }
                else
                {
                    l_shade_trigger = 1;
                }

                l_depth += l_gdx[0];
                l_attrib_int[0] += l_attrib_int_dx[0];
                l_attrib_int[1] += l_attrib_int_dx[1];
                l_attrib_int[2] += l_attrib_int_dx[2];
                l_attrib_int[3] += l_attrib_int_dx[3];
                l_attrib_int[4] += l_attrib_int_dx[4];
                l_attrib_int[5] += l_attrib_int_dx[5];

                l_depth_addr++;
                l_frame_addr++;

                l_shade_counter++;
            }
        }
    }

    // raster

    gradient g[attrib_count];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    uint32_t fill_color[4];
    uint32_t num_lights;
    const light* light_data;
    const math::powfast_table* light_table;
};

//------------------------------------------------------------------------------

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_vertex_shade_none : public scan
{
    // scan

    void setup(const config* c) override
    {
        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
    }

    enum
    {
        attrib_z,
        attrib_w,
        attrib_fr,
        attrib_fg,
        attrib_fb,
        attrib_fa,
        attrib_count
    };

    bool setup_face(const float* pv[], uint32_t vertex_count, bool back_cull, bool& is_clockwise) override
    {
        return interp_setup_face(pv, vertex_count, back_cull, is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        float l_gdx[attrib_count];
        float l_attrib[attrib_count];
        float l_depth;
        float* l_depth_addr;
        uint32_t* l_frame_addr;
        int32_t l_attrib_int_dx[attrib_count - 2]; // 16.16
        int32_t l_attrib_int[attrib_count - 2]; // 16.16
        int32_t l_attrib_int_next[attrib_count - 2]; // 16.16

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        int32_t start{ frame_stride * y + x0 };
        l_gdx[0] = g[0].dx;
        l_gdx[1] = g[1].dx;
        l_gdx[2] = g[2].dx;
        l_gdx[3] = g[3].dx;
        l_gdx[4] = g[4].dx;
        l_gdx[5] = g[5].dx;
        l_attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        l_attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        l_attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        l_attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        l_attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        l_attrib[5] = g[5].dx * x0f + g[5].dy * y0f + g[5].d;
        float w{ (float)0x10000 / l_attrib[1] };
        l_attrib_int_next[0] = math::clamp((int32_t)(l_attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[1] = math::clamp((int32_t)(l_attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[2] = math::clamp((int32_t)(l_attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[3] = math::clamp((int32_t)(l_attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_depth_addr = &depth_buffer[start];
        l_frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);

        int32_t n{ x1 - x0 };
        while (n)
        {
            int32_t count{ math::min(n, span_block_size) };
            n -= count;

            float count_float{ (float)count };
            l_depth = l_attrib[0];
            l_attrib[0] += l_gdx[0] * count_float;
            l_attrib[1] += l_gdx[1] * count_float;
            l_attrib[2] += l_gdx[2] * count_float;
            l_attrib[3] += l_gdx[3] * count_float;
            l_attrib[4] += l_gdx[4] * count_float;
            l_attrib[5] += l_gdx[5] * count_float;
            float w{ (float)0x10000 / l_attrib[1] };
            l_attrib_int[0] = l_attrib_int_next[0];
            l_attrib_int[1] = l_attrib_int_next[1];
            l_attrib_int[2] = l_attrib_int_next[2];
            l_attrib_int[3] = l_attrib_int_next[3];
            l_attrib_int_next[0] = math::clamp((int32_t)(l_attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[1] = math::clamp((int32_t)(l_attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[2] = math::clamp((int32_t)(l_attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[3] = math::clamp((int32_t)(l_attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            if (count == span_block_size)
            {
                l_attrib_int_dx[0] = (l_attrib_int_next[0] - l_attrib_int[0]) >> span_block_size_shift;
                l_attrib_int_dx[1] = (l_attrib_int_next[1] - l_attrib_int[1]) >> span_block_size_shift;
                l_attrib_int_dx[2] = (l_attrib_int_next[2] - l_attrib_int[2]) >> span_block_size_shift;
                l_attrib_int_dx[3] = (l_attrib_int_next[3] - l_attrib_int[3]) >> span_block_size_shift;
            }
            else
            {
                float scale{ subspan_scale[count] };
                l_attrib_int_dx[0] = (int32_t)((float)(l_attrib_int_next[0] - l_attrib_int[0]) * scale);
                l_attrib_int_dx[1] = (int32_t)((float)(l_attrib_int_next[1] - l_attrib_int[1]) * scale);
                l_attrib_int_dx[2] = (int32_t)((float)(l_attrib_int_next[2] - l_attrib_int[2]) * scale);
                l_attrib_int_dx[3] = (int32_t)((float)(l_attrib_int_next[3] - l_attrib_int[3]) * scale);
            }

            while (count--)
            {
                if (depth_type::process_test(l_depth_addr, l_depth))
                {
                    uint32_t color
                    {
                        (((uint32_t)l_attrib_int[3] & 0x00FF0000u) <<  8u) +
                        (((uint32_t)l_attrib_int[0] & 0x00FF0000u)       ) +
                        (((uint32_t)l_attrib_int[1] & 0x00FF0000u) >>  8u) +
                        (((uint32_t)l_attrib_int[2] & 0x00FF0000u) >> 16u)
                    };
                    blend_type::process(l_frame_addr, color);
                    depth_type::process_write(l_depth_addr, l_depth);
                }

                l_depth += l_gdx[0];
                l_attrib_int[0] += l_attrib_int_dx[0];
                l_attrib_int[1] += l_attrib_int_dx[1];
                l_attrib_int[2] += l_attrib_int_dx[2];
                l_attrib_int[3] += l_attrib_int_dx[3];

                l_depth_addr++;
                l_frame_addr++;
            }
        }
    }

    // raster

    gradient g[attrib_count];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
};

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_vertex_shade_vertex : public scan
{
    // scan

    void setup(const config* c) override
    {
        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
    }

    enum
    {
        attrib_z,
        attrib_w,
        attrib_fr,
        attrib_fg,
        attrib_fb,
        attrib_fa,
        attrib_sr,
        attrib_sg,
        attrib_sb,
        attrib_count
    };

    bool setup_face(const float* pv[], uint32_t vertex_count, bool back_cull, bool& is_clockwise) override
    {
        return interp_setup_face(pv, vertex_count, back_cull, is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        float l_gdx[attrib_count];
        float l_attrib[attrib_count];
        float l_depth;
        float* l_depth_addr;
        uint32_t* l_frame_addr;
        int32_t l_attrib_int_dx[attrib_count - 2]; // 16.16
        int32_t l_attrib_int[attrib_count - 2]; // 16.16
        int32_t l_attrib_int_next[attrib_count - 2]; // 16.16

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        int32_t start{ frame_stride * y + x0 };
        l_gdx[0] = g[0].dx;
        l_gdx[1] = g[1].dx;
        l_gdx[2] = g[2].dx;
        l_gdx[3] = g[3].dx;
        l_gdx[4] = g[4].dx;
        l_gdx[5] = g[5].dx;
        l_gdx[6] = g[6].dx;
        l_gdx[7] = g[7].dx;
        l_gdx[8] = g[8].dx;
        l_attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        l_attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        l_attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        l_attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        l_attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        l_attrib[5] = g[5].dx * x0f + g[5].dy * y0f + g[5].d;
        l_attrib[6] = g[6].dx * x0f + g[6].dy * y0f + g[6].d;
        l_attrib[7] = g[7].dx * x0f + g[7].dy * y0f + g[7].d;
        l_attrib[8] = g[8].dx * x0f + g[8].dy * y0f + g[8].d;
        float w{ (float)0x10000 / l_attrib[1] };
        l_attrib_int_next[0] = math::clamp((int32_t)(l_attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[1] = math::clamp((int32_t)(l_attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[2] = math::clamp((int32_t)(l_attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[3] = math::clamp((int32_t)(l_attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[4] = math::clamp((int32_t)(l_attrib[6] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[5] = math::clamp((int32_t)(l_attrib[7] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[6] = math::clamp((int32_t)(l_attrib[8] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_depth_addr = &depth_buffer[start];
        l_frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);

        int32_t n{ x1 - x0 };
        while (n)
        {
            int32_t count{ math::min(n, span_block_size) };
            n -= count;

            float count_float{ (float)count };
            l_depth = l_attrib[0];
            l_attrib[0] += l_gdx[0] * count_float;
            l_attrib[1] += l_gdx[1] * count_float;
            l_attrib[2] += l_gdx[2] * count_float;
            l_attrib[3] += l_gdx[3] * count_float;
            l_attrib[4] += l_gdx[4] * count_float;
            l_attrib[5] += l_gdx[5] * count_float;
            l_attrib[6] += l_gdx[6] * count_float;
            l_attrib[7] += l_gdx[7] * count_float;
            l_attrib[8] += l_gdx[8] * count_float;
            float w{ (float)0x10000 / l_attrib[1] };
            l_attrib_int[0] = l_attrib_int_next[0];
            l_attrib_int[1] = l_attrib_int_next[1];
            l_attrib_int[2] = l_attrib_int_next[2];
            l_attrib_int[3] = l_attrib_int_next[3];
            l_attrib_int[4] = l_attrib_int_next[4];
            l_attrib_int[5] = l_attrib_int_next[5];
            l_attrib_int[6] = l_attrib_int_next[6];
            l_attrib_int_next[0] = math::clamp((int32_t)(l_attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[1] = math::clamp((int32_t)(l_attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[2] = math::clamp((int32_t)(l_attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[3] = math::clamp((int32_t)(l_attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[4] = math::clamp((int32_t)(l_attrib[6] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[5] = math::clamp((int32_t)(l_attrib[7] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[6] = math::clamp((int32_t)(l_attrib[8] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            if (count == span_block_size)
            {
                l_attrib_int_dx[0] = (l_attrib_int_next[0] - l_attrib_int[0]) >> span_block_size_shift;
                l_attrib_int_dx[1] = (l_attrib_int_next[1] - l_attrib_int[1]) >> span_block_size_shift;
                l_attrib_int_dx[2] = (l_attrib_int_next[2] - l_attrib_int[2]) >> span_block_size_shift;
                l_attrib_int_dx[3] = (l_attrib_int_next[3] - l_attrib_int[3]) >> span_block_size_shift;
                l_attrib_int_dx[4] = (l_attrib_int_next[4] - l_attrib_int[4]) >> span_block_size_shift;
                l_attrib_int_dx[5] = (l_attrib_int_next[5] - l_attrib_int[5]) >> span_block_size_shift;
                l_attrib_int_dx[6] = (l_attrib_int_next[6] - l_attrib_int[6]) >> span_block_size_shift;
            }
            else
            {
                float scale{ subspan_scale[count] };
                l_attrib_int_dx[0] = (int32_t)((float)(l_attrib_int_next[0] - l_attrib_int[0]) * scale);
                l_attrib_int_dx[1] = (int32_t)((float)(l_attrib_int_next[1] - l_attrib_int[1]) * scale);
                l_attrib_int_dx[2] = (int32_t)((float)(l_attrib_int_next[2] - l_attrib_int[2]) * scale);
                l_attrib_int_dx[3] = (int32_t)((float)(l_attrib_int_next[3] - l_attrib_int[3]) * scale);
                l_attrib_int_dx[4] = (int32_t)((float)(l_attrib_int_next[4] - l_attrib_int[4]) * scale);
                l_attrib_int_dx[5] = (int32_t)((float)(l_attrib_int_next[5] - l_attrib_int[5]) * scale);
                l_attrib_int_dx[6] = (int32_t)((float)(l_attrib_int_next[6] - l_attrib_int[6]) * scale);
            }

            while (count--)
            {
                if (depth_type::process_test(l_depth_addr, l_depth))
                {
                    uint32_t color
                    {
                        (((((uint32_t)l_attrib_int[3] <<  8u)                                     ) & 0xFF000000u)       ) +
                        (((((uint32_t)l_attrib_int[0] >> 16u) * ((uint32_t)l_attrib_int[4] >>  8u)) & 0x00FF0000u)       ) +
                        (((((uint32_t)l_attrib_int[1] >> 16u) * ((uint32_t)l_attrib_int[5] >> 16u)) & 0x0000FF00u)       ) +
                        (((((uint32_t)l_attrib_int[2] >>  8u) * ((uint32_t)l_attrib_int[6] >>  8u))              ) >> 24u) + 0x00010101u
                    };
                    blend_type::process(l_frame_addr, color);
                    depth_type::process_write(l_depth_addr, l_depth);
                }

                l_depth += l_gdx[0];
                l_attrib_int[0] += l_attrib_int_dx[0];
                l_attrib_int[1] += l_attrib_int_dx[1];
                l_attrib_int[2] += l_attrib_int_dx[2];
                l_attrib_int[3] += l_attrib_int_dx[3];
                l_attrib_int[4] += l_attrib_int_dx[4];
                l_attrib_int[5] += l_attrib_int_dx[5];
                l_attrib_int[6] += l_attrib_int_dx[6];

                l_depth_addr++;
                l_frame_addr++;
            }
        }
    }

    // raster

    gradient g[attrib_count];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
};

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_vertex_shade_lightmap : public scan
{
    // scan

    void setup(const config* c) override
    {
        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        umax = (c->lightmap_width - 1) << 16;
        vmax = (c->lightmap_height - 1) << 16;
        vshift = math::log2(c->lightmap_width);
        lightmap = (const uint32_t*)(c->lightmap);
    }

    enum
    {
        attrib_z,
        attrib_w,
        attrib_fr,
        attrib_fg,
        attrib_fb,
        attrib_fa,
        attrib_su,
        attrib_sv,
        attrib_count
    };

    bool setup_face(const float* pv[], uint32_t vertex_count, bool back_cull, bool& is_clockwise) override
    {
        return interp_setup_face(pv, vertex_count, back_cull, is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        float l_gdx[attrib_count];
        int32_t l_umax;
        int32_t l_vmax;
        int32_t l_vshift;
        const uint32_t* l_lightmap;
        float l_attrib[attrib_count];
        float l_depth;
        float* l_depth_addr;
        uint32_t* l_frame_addr;
        int32_t l_attrib_int_dx[attrib_count - 2]; // 16.16
        int32_t l_attrib_int[attrib_count - 2]; // 16.16
        int32_t l_attrib_int_next[attrib_count - 2]; // 16.16
        uint32_t l_shade_counter;
        uint32_t l_shade_trigger;
        uint32_t l_shade[3];

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        int32_t start{ frame_stride * y + x0 };
        l_gdx[0] = g[0].dx;
        l_gdx[1] = g[1].dx;
        l_gdx[2] = g[2].dx;
        l_gdx[3] = g[3].dx;
        l_gdx[4] = g[4].dx;
        l_gdx[5] = g[5].dx;
        l_gdx[6] = g[6].dx;
        l_gdx[7] = g[7].dx;
        l_umax = umax;
        l_vmax = vmax;
        l_vshift = vshift;
        l_lightmap = lightmap;
        l_attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        l_attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        l_attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        l_attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        l_attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        l_attrib[5] = g[5].dx * x0f + g[5].dy * y0f + g[5].d;
        l_attrib[6] = g[6].dx * x0f + g[6].dy * y0f + g[6].d;
        l_attrib[7] = g[7].dx * x0f + g[7].dy * y0f + g[7].d;
        float w{ (float)0x10000 / l_attrib[1] };
        l_attrib_int_next[0] = math::clamp((int32_t)(l_attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[1] = math::clamp((int32_t)(l_attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[2] = math::clamp((int32_t)(l_attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[3] = math::clamp((int32_t)(l_attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[4] = math::clamp((int32_t)(l_attrib[6] * w), (int32_t)0, l_umax);
        l_attrib_int_next[5] = math::clamp((int32_t)(l_attrib[7] * w), (int32_t)0, l_vmax);
        l_depth_addr = &depth_buffer[start];
        l_frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
        l_shade_counter = ((y & 1 ? shade_hold >> 1u : 0u) + x0) & shade_mask;
        l_shade_trigger = 1;

        int32_t n{ x1 - x0 };
        while (n)
        {
            int32_t count{ math::min(n, span_block_size) };
            n -= count;

            float count_float{ (float)count };
            l_depth = l_attrib[0];
            l_attrib[0] += l_gdx[0] * count_float;
            l_attrib[1] += l_gdx[1] * count_float;
            l_attrib[2] += l_gdx[2] * count_float;
            l_attrib[3] += l_gdx[3] * count_float;
            l_attrib[4] += l_gdx[4] * count_float;
            l_attrib[5] += l_gdx[5] * count_float;
            l_attrib[6] += l_gdx[6] * count_float;
            l_attrib[7] += l_gdx[7] * count_float;
            float w{ (float)0x10000 / l_attrib[1] };
            l_attrib_int[0] = l_attrib_int_next[0];
            l_attrib_int[1] = l_attrib_int_next[1];
            l_attrib_int[2] = l_attrib_int_next[2];
            l_attrib_int[3] = l_attrib_int_next[3];
            l_attrib_int[4] = l_attrib_int_next[4];
            l_attrib_int[5] = l_attrib_int_next[5];
            l_attrib_int_next[0] = math::clamp((int32_t)(l_attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[1] = math::clamp((int32_t)(l_attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[2] = math::clamp((int32_t)(l_attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[3] = math::clamp((int32_t)(l_attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[4] = math::clamp((int32_t)(l_attrib[6] * w), (int32_t)0, l_umax);
            l_attrib_int_next[5] = math::clamp((int32_t)(l_attrib[7] * w), (int32_t)0, l_vmax);
            if (count == span_block_size)
            {
                l_attrib_int_dx[0] = (l_attrib_int_next[0] - l_attrib_int[0]) >> span_block_size_shift;
                l_attrib_int_dx[1] = (l_attrib_int_next[1] - l_attrib_int[1]) >> span_block_size_shift;
                l_attrib_int_dx[2] = (l_attrib_int_next[2] - l_attrib_int[2]) >> span_block_size_shift;
                l_attrib_int_dx[3] = (l_attrib_int_next[3] - l_attrib_int[3]) >> span_block_size_shift;
                l_attrib_int_dx[4] = (l_attrib_int_next[4] - l_attrib_int[4]) >> span_block_size_shift;
                l_attrib_int_dx[5] = (l_attrib_int_next[5] - l_attrib_int[5]) >> span_block_size_shift;
            }
            else
            {
                float scale{ subspan_scale[count] };
                l_attrib_int_dx[0] = (int32_t)((float)(l_attrib_int_next[0] - l_attrib_int[0]) * scale);
                l_attrib_int_dx[1] = (int32_t)((float)(l_attrib_int_next[1] - l_attrib_int[1]) * scale);
                l_attrib_int_dx[2] = (int32_t)((float)(l_attrib_int_next[2] - l_attrib_int[2]) * scale);
                l_attrib_int_dx[3] = (int32_t)((float)(l_attrib_int_next[3] - l_attrib_int[3]) * scale);
                l_attrib_int_dx[4] = (int32_t)((float)(l_attrib_int_next[4] - l_attrib_int[4]) * scale);
                l_attrib_int_dx[5] = (int32_t)((float)(l_attrib_int_next[5] - l_attrib_int[5]) * scale);
            }

            while (count--)
            {
                if (depth_type::process_test(l_depth_addr, l_depth))
                {
                    if (((l_shade_counter & shade_mask) == 0) | l_shade_trigger)
                    {
                        l_shade_trigger = 0;
                        uint32_t shade_color{ sample_lightmap(
                            l_attrib_int[4],
                            l_attrib_int[5],
                            l_vshift, l_lightmap) };
                        l_shade[0] = (shade_color & 0x00FF0000u) >> 16u;
                        l_shade[1] = (shade_color & 0x0000FF00u) >>  8u;
                        l_shade[2] = (shade_color & 0x000000FFu)       ;
                    }
                    uint32_t color
                    {
                        ((((uint32_t)l_attrib_int[3]             ) & 0x00FF0000u) <<  8u) +
                        ((((uint32_t)l_attrib_int[0] * l_shade[0]) & 0xFF000000u) >>  8u) +
                        ((((uint32_t)l_attrib_int[1] * l_shade[1]) & 0xFF000000u) >> 16u) +
                        ((((uint32_t)l_attrib_int[2] * l_shade[2])              ) >> 24u) + 0x00010101u
                    };
                    blend_type::process(l_frame_addr, color);
                    depth_type::process_write(l_depth_addr, l_depth);
                }
                else
                {
                    l_shade_trigger = 1;
                }

                l_depth += l_gdx[0];
                l_attrib_int[0] += l_attrib_int_dx[0];
                l_attrib_int[1] += l_attrib_int_dx[1];
                l_attrib_int[2] += l_attrib_int_dx[2];
                l_attrib_int[3] += l_attrib_int_dx[3];
                l_attrib_int[4] += l_attrib_int_dx[4];
                l_attrib_int[5] += l_attrib_int_dx[5];

                l_depth_addr++;
                l_frame_addr++;

                l_shade_counter++;
            }
        }
    }

    // raster

    gradient g[attrib_count];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    int32_t umax;
    int32_t vmax;
    int32_t vshift;
    const uint32_t* lightmap;
};

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_vertex_shade_light : public scan
{
    // scan

    void setup(const config* c) override
    {
        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        num_lights = c->num_lights;
        light_data = c->light_data;
        light_table = c->light_table;
    }

    enum
    {
        attrib_z,
        attrib_w,
        attrib_fr,
        attrib_fg,
        attrib_fb,
        attrib_fa,
        attrib_px,
        attrib_py,
        attrib_pz,
        attrib_nx,
        attrib_ny,
        attrib_nz,
        attrib_count
    };

    bool setup_face(const float* pv[], uint32_t vertex_count, bool back_cull, bool& is_clockwise) override
    {
        return interp_setup_face(pv, vertex_count, back_cull, is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        float l_gdx[attrib_count];
        uint32_t l_num_lights;
        const light* l_light_data;
        const math::powfast_table* l_light_table;
        float l_attrib[attrib_count];
        float l_depth;
        float* l_depth_addr;
        uint32_t* l_frame_addr;
        int32_t l_attrib_inti_dx[4]; // 16.16
        float l_attrib_intf_dx[6];
        int32_t l_attrib_inti[4]; // 16.16
        float l_attrib_intf[6];
        int32_t l_attrib_inti_next[4]; // 16.16
        float l_attrib_intf_next[6];
        uint32_t l_shade_counter;
        uint32_t l_shade_trigger;
        uint32_t l_shade[3];

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        int32_t start{ frame_stride * y + x0 };
        l_gdx[0x0] = g[0x0].dx;
        l_gdx[0x1] = g[0x1].dx;
        l_gdx[0x2] = g[0x2].dx;
        l_gdx[0x3] = g[0x3].dx;
        l_gdx[0x4] = g[0x4].dx;
        l_gdx[0x5] = g[0x5].dx;
        l_gdx[0x6] = g[0x6].dx;
        l_gdx[0x7] = g[0x7].dx;
        l_gdx[0x8] = g[0x8].dx;
        l_gdx[0x9] = g[0x9].dx;
        l_gdx[0xA] = g[0xA].dx;
        l_gdx[0xB] = g[0xB].dx;
        l_num_lights = num_lights;
        l_light_data = light_data;
        l_light_table = light_table;
        l_attrib[0x0] = g[0x0].dx * x0f + g[0x0].dy * y0f + g[0x0].d;
        l_attrib[0x1] = g[0x1].dx * x0f + g[0x1].dy * y0f + g[0x1].d;
        l_attrib[0x2] = g[0x2].dx * x0f + g[0x2].dy * y0f + g[0x2].d;
        l_attrib[0x3] = g[0x3].dx * x0f + g[0x3].dy * y0f + g[0x3].d;
        l_attrib[0x4] = g[0x4].dx * x0f + g[0x4].dy * y0f + g[0x4].d;
        l_attrib[0x5] = g[0x5].dx * x0f + g[0x5].dy * y0f + g[0x5].d;
        l_attrib[0x6] = g[0x6].dx * x0f + g[0x6].dy * y0f + g[0x6].d;
        l_attrib[0x7] = g[0x7].dx * x0f + g[0x7].dy * y0f + g[0x7].d;
        l_attrib[0x8] = g[0x8].dx * x0f + g[0x8].dy * y0f + g[0x8].d;
        l_attrib[0x9] = g[0x9].dx * x0f + g[0x9].dy * y0f + g[0x9].d;
        l_attrib[0xA] = g[0xA].dx * x0f + g[0xA].dy * y0f + g[0xA].d;
        l_attrib[0xB] = g[0xB].dx * x0f + g[0xB].dy * y0f + g[0xB].d;
        float wf{ 1.f / l_attrib[1] };
        float wi{ (float)0x10000 * wf };
        l_attrib_inti_next[0] = math::clamp((int32_t)(l_attrib[0x2] * wi), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_inti_next[1] = math::clamp((int32_t)(l_attrib[0x3] * wi), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_inti_next[2] = math::clamp((int32_t)(l_attrib[0x4] * wi), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_inti_next[3] = math::clamp((int32_t)(l_attrib[0x5] * wi), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_intf_next[0] = l_attrib[0x6] * wf;
        l_attrib_intf_next[1] = l_attrib[0x7] * wf;
        l_attrib_intf_next[2] = l_attrib[0x8] * wf;
        l_attrib_intf_next[3] = l_attrib[0x9] * wf;
        l_attrib_intf_next[4] = l_attrib[0xA] * wf;
        l_attrib_intf_next[5] = l_attrib[0xB] * wf;
        l_depth_addr = &depth_buffer[start];
        l_frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
        l_shade_counter = ((y & 1 ? shade_hold >> 1u : 0u) + x0) & shade_mask;
        l_shade_trigger = 1;

        int32_t n{ x1 - x0 };
        while (n)
        {
            int32_t count{ math::min(n, span_block_size) };
            n -= count;

            float count_float{ (float)count };
            l_depth = l_attrib[0];
            l_attrib[0x0] += l_gdx[0x0] * count_float;
            l_attrib[0x1] += l_gdx[0x1] * count_float;
            l_attrib[0x2] += l_gdx[0x2] * count_float;
            l_attrib[0x3] += l_gdx[0x3] * count_float;
            l_attrib[0x4] += l_gdx[0x4] * count_float;
            l_attrib[0x5] += l_gdx[0x5] * count_float;
            l_attrib[0x6] += l_gdx[0x6] * count_float;
            l_attrib[0x7] += l_gdx[0x7] * count_float;
            l_attrib[0x8] += l_gdx[0x8] * count_float;
            l_attrib[0x9] += l_gdx[0x9] * count_float;
            l_attrib[0xA] += l_gdx[0xA] * count_float;
            l_attrib[0xB] += l_gdx[0xB] * count_float;
            float wf{ 1.f / l_attrib[1] };
            float wi{ (float)0x10000 * wf };
            l_attrib_inti[0] = l_attrib_inti_next[0];
            l_attrib_inti[1] = l_attrib_inti_next[1];
            l_attrib_inti[2] = l_attrib_inti_next[2];
            l_attrib_inti[3] = l_attrib_inti_next[3];
            l_attrib_intf[0] = l_attrib_intf_next[0];
            l_attrib_intf[1] = l_attrib_intf_next[1];
            l_attrib_intf[2] = l_attrib_intf_next[2];
            l_attrib_intf[3] = l_attrib_intf_next[3];
            l_attrib_intf[4] = l_attrib_intf_next[4];
            l_attrib_intf[5] = l_attrib_intf_next[5];
            l_attrib_inti_next[0] = math::clamp((int32_t)(l_attrib[0x2] * wi), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_inti_next[1] = math::clamp((int32_t)(l_attrib[0x3] * wi), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_inti_next[2] = math::clamp((int32_t)(l_attrib[0x4] * wi), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_inti_next[3] = math::clamp((int32_t)(l_attrib[0x5] * wi), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_intf_next[0] = l_attrib[0x6] * wf;
            l_attrib_intf_next[1] = l_attrib[0x7] * wf;
            l_attrib_intf_next[2] = l_attrib[0x8] * wf;
            l_attrib_intf_next[3] = l_attrib[0x9] * wf;
            l_attrib_intf_next[4] = l_attrib[0xA] * wf;
            l_attrib_intf_next[5] = l_attrib[0xB] * wf;
            if (count == span_block_size)
            {
                l_attrib_inti_dx[0] = (l_attrib_inti_next[0] - l_attrib_inti[0]) >> span_block_size_shift;
                l_attrib_inti_dx[1] = (l_attrib_inti_next[1] - l_attrib_inti[1]) >> span_block_size_shift;
                l_attrib_inti_dx[2] = (l_attrib_inti_next[2] - l_attrib_inti[2]) >> span_block_size_shift;
                l_attrib_inti_dx[3] = (l_attrib_inti_next[3] - l_attrib_inti[3]) >> span_block_size_shift;
            }
            else
            {
                float scale{ subspan_scale[count] };
                l_attrib_inti_dx[0] = (int32_t)((float)(l_attrib_inti_next[0] - l_attrib_inti[0]) * scale);
                l_attrib_inti_dx[1] = (int32_t)((float)(l_attrib_inti_next[1] - l_attrib_inti[1]) * scale);
                l_attrib_inti_dx[2] = (int32_t)((float)(l_attrib_inti_next[2] - l_attrib_inti[2]) * scale);
                l_attrib_inti_dx[3] = (int32_t)((float)(l_attrib_inti_next[3] - l_attrib_inti[3]) * scale);
            }
            float scale{ 1.f / count_float };
            l_attrib_intf_dx[0] = (l_attrib_intf_next[0] - l_attrib_intf[0]) * scale;
            l_attrib_intf_dx[1] = (l_attrib_intf_next[1] - l_attrib_intf[1]) * scale;
            l_attrib_intf_dx[2] = (l_attrib_intf_next[2] - l_attrib_intf[2]) * scale;
            l_attrib_intf_dx[3] = (l_attrib_intf_next[3] - l_attrib_intf[3]) * scale;
            l_attrib_intf_dx[4] = (l_attrib_intf_next[4] - l_attrib_intf[4]) * scale;
            l_attrib_intf_dx[5] = (l_attrib_intf_next[5] - l_attrib_intf[5]) * scale;

            while (count--)
            {
                if (depth_type::process_test(l_depth_addr, l_depth))
                {
                    if (((l_shade_counter & shade_mask) == 0) | l_shade_trigger)
                    {
                        l_shade_trigger = 0;
                        sample_light(
                            &l_attrib_intf[0], &l_attrib_intf[3],
                            l_light_data, l_num_lights,
                            l_light_table,
                            l_shade);
                    }
                    uint32_t color
                    {
                        ((((uint32_t)l_attrib_inti[3]             ) & 0x00FF0000u) <<  8u) +
                        ((((uint32_t)l_attrib_inti[0] * l_shade[0]) & 0xFF000000u) >>  8u) +
                        ((((uint32_t)l_attrib_inti[1] * l_shade[1]) & 0xFF000000u) >> 16u) +
                        ((((uint32_t)l_attrib_inti[2] * l_shade[2])              ) >> 24u) + 0x00010101u
                    };
                    blend_type::process(l_frame_addr, color);
                    depth_type::process_write(l_depth_addr, l_depth);
                }
                else
                {
                    l_shade_trigger = 1;
                }

                l_depth += l_gdx[0];
                l_attrib_inti[0] += l_attrib_inti_dx[0];
                l_attrib_inti[1] += l_attrib_inti_dx[1];
                l_attrib_inti[2] += l_attrib_inti_dx[2];
                l_attrib_inti[3] += l_attrib_inti_dx[3];
                l_attrib_intf[0] += l_attrib_intf_dx[0];
                l_attrib_intf[1] += l_attrib_intf_dx[1];
                l_attrib_intf[2] += l_attrib_intf_dx[2];
                l_attrib_intf[3] += l_attrib_intf_dx[3];
                l_attrib_intf[4] += l_attrib_intf_dx[4];
                l_attrib_intf[5] += l_attrib_intf_dx[5];

                l_depth_addr++;
                l_frame_addr++;

                l_shade_counter++;
            }
        }
    }

    // raster

    gradient g[attrib_count];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    uint32_t num_lights;
    const light* light_data;
    const math::powfast_table* light_table;
};

//------------------------------------------------------------------------------

static int32_t mip_level_calc(
    const float* v[],
    uint32_t num_vertices,
    float texture_width,
    float texture_height)
{
    enum { x, y, zdivw, winv, udivw, vdivw };

    float xy[2][2]; // v0->v1, v0->v2
    float uv[2][2]; // v0->v1, v0->v2
    float w0{ 1.f / v[0][winv] };
    float w1{ 1.f / v[1][winv] };
    float x0{ v[0][x] };
    float y0{ v[0][y] };
    float u0{ v[0][udivw] * w0 };
    float v0{ v[0][vdivw] * w0 };
    xy[0][0] = v[1][x] - x0;
    xy[0][1] = v[1][y] - y0;
    uv[0][0] = v[1][udivw] * w1 - u0;
    uv[0][1] = v[1][vdivw] * w1 - v0;

    float a2xy{ 0.f };
    float a2uv{ 0.f };

    for (uint32_t nv2{ 2 }; nv2 < num_vertices; ++nv2)
    {
        float w2{ 1.f / v[nv2][winv] };
        xy[1][0] = v[nv2][x] - x0;
        xy[1][1] = v[nv2][y] - y0;
        uv[1][0] = v[nv2][udivw] * w2 - u0;
        uv[1][1] = v[nv2][vdivw] * w2 - v0;

        a2xy += math::cross2(xy[0][0], xy[0][1], xy[1][0], xy[1][1]);
        a2uv += math::cross2(uv[0][0], uv[0][1], uv[1][0], uv[1][1]);

        xy[0][0] = xy[1][0];
        xy[0][1] = xy[1][1];
        uv[0][0] = uv[1][0];
        uv[0][1] = uv[1][1];
    }

    a2uv *= texture_width * texture_height;

    float l{ math::sqrt(std::abs(a2uv / a2xy)) };

    return math::log2ceil(l);
}

static int32_t mip_table_build(
    const uint8_t* texture,
    int32_t texture_width,
    int32_t texture_height,
    uint8_t const* mip_table[])
{
    int32_t n{ 0 };
    for (;;)
    {
        mip_table[n++] = texture;
        if (texture_width != 1 && texture_height != 1 && n < mip_table_max_size)
        {
            texture += texture_width * texture_height;
            texture_width >>= 1;
            texture_height >>= 1;
        }
        else
            break;
    }
    for (int32_t i{ n }; i < mip_table_max_size; ++i)
        mip_table[i] = nullptr;
    return n;
}

//------------------------------------------------------------------------------

template<
    typename sample_type = sample_nearest,
    typename blend_type = blend_none,
    typename depth_type = depth_test_write,
    typename mask_type = mask_texture_off>
struct raster_texture_shade_none : public scan
{
    // scan

    bool mip_enable;

    int32_t texture_width;
    int32_t texture_height;

    const uint8_t* mip_table[mip_table_max_size];
    int32_t mip_max_level;

    void setup(const config* c) override
    {
        mip_enable = (c->flags & TEXMIP_FACE) != 0;

        texture_width = c->texture_width;
        texture_height = c->texture_height;

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;

        if (!mip_enable)
        {
            smask = (c->texture_width - 1) << 16;
            tmask = (c->texture_height - 1) << 16;
            tshift = 16 - math::log2(c->texture_width);
            texture_lut = (uint32_t*)(c->texture_lut);
            texture_data = c->texture_data;
        }
        else
        {
            texture_lut = (uint32_t*)(c->texture_lut);
            mip_max_level = mip_table_build(c->texture_data, c->texture_width, c->texture_height, mip_table) - 1;
        }
    }

    enum
    {
        attrib_z,
        attrib_w,
        attrib_fs,
        attrib_ft,
        attrib_count
    };

    bool setup_face(const float* pv[], uint32_t vertex_count, bool back_cull, bool& is_clockwise) override
    {
        if (interp_setup_face(pv, vertex_count, back_cull, is_clockwise, g))
        {
            float texture_width_f{ (float)texture_width };
            float texture_height_f{ (float)texture_height };
            if (!mip_enable)
            {
                g[attrib_fs].dx *= texture_width_f;
                g[attrib_fs].dy *= texture_width_f;
                g[attrib_fs].d *= texture_width_f;
                g[attrib_ft].dx *= texture_height_f;
                g[attrib_ft].dy *= texture_height_f;
                g[attrib_ft].d *= texture_height_f;
            }
            else
            {
                int32_t mip_level{ mip_level_calc(pv, vertex_count, texture_width_f, texture_height_f) };
                mip_level = math::clamp(mip_level, (int32_t)0, mip_max_level);
                int32_t mip_texture_width{ texture_width >> mip_level };
                int32_t mip_texture_height{ texture_height >> mip_level };
                float mip_texture_width_f{ (float)mip_texture_width };
                float mip_texture_height_f{ (float)mip_texture_height };
                g[attrib_fs].dx *= mip_texture_width_f;
                g[attrib_fs].dy *= mip_texture_width_f;
                g[attrib_fs].d *= mip_texture_width_f;
                g[attrib_ft].dx *= mip_texture_height_f;
                g[attrib_ft].dy *= mip_texture_height_f;
                g[attrib_ft].d *= mip_texture_height_f;
                smask = (mip_texture_width - 1) << 16;
                tmask = (mip_texture_height - 1) << 16;
                tshift = 16 - math::log2(mip_texture_width);
                texture_data = mip_table[mip_level];
            }
            return true;
        }
        return false;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        float l_gdx[attrib_count];
        int32_t l_smask;
        int32_t l_tmask;
        int32_t l_tshift;
        const uint32_t* l_texture_lut;
        const uint8_t* l_texture_data;
        float l_attrib[attrib_count];
        float l_depth;
        float* l_depth_addr;
        uint32_t* l_frame_addr;
        int32_t l_attrib_int_dx[attrib_count - 2]; // 16.16
        int32_t l_attrib_int[attrib_count - 2]; // 16.16
        int32_t l_attrib_int_next[attrib_count - 2]; // 16.16

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        int32_t start{ frame_stride * y + x0 };
        l_gdx[0] = g[0].dx;
        l_gdx[1] = g[1].dx;
        l_gdx[2] = g[2].dx;
        l_gdx[3] = g[3].dx;
        l_smask = smask;
        l_tmask = tmask;
        l_tshift = tshift;
        l_texture_lut = texture_lut;
        l_texture_data = texture_data;
        l_attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        l_attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        l_attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        l_attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        float w{ (float)0x10000 / l_attrib[1] };
        l_attrib_int_next[0] = sample_type::process_coord((int32_t)(l_attrib[2] * w));
        l_attrib_int_next[1] = sample_type::process_coord((int32_t)(l_attrib[3] * w));
        l_depth_addr = &depth_buffer[start];
        l_frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);

        int32_t n{ x1 - x0 };
        while (n)
        {
            int32_t count{ math::min(n, span_block_size) };
            n -= count;

            float count_float{ (float)count };
            l_depth = l_attrib[0];
            l_attrib[0] += l_gdx[0] * count_float;
            l_attrib[1] += l_gdx[1] * count_float;
            l_attrib[2] += l_gdx[2] * count_float;
            l_attrib[3] += l_gdx[3] * count_float;
            float w{ (float)0x10000 / l_attrib[1] };
            l_attrib_int[0] = l_attrib_int_next[0];
            l_attrib_int[1] = l_attrib_int_next[1];
            l_attrib_int_next[0] = sample_type::process_coord((int32_t)(l_attrib[2] * w));
            l_attrib_int_next[1] = sample_type::process_coord((int32_t)(l_attrib[3] * w));
            if (count == span_block_size)
            {
                l_attrib_int_dx[0] = (l_attrib_int_next[0] - l_attrib_int[0]) >> span_block_size_shift;
                l_attrib_int_dx[1] = (l_attrib_int_next[1] - l_attrib_int[1]) >> span_block_size_shift;
            }
            else
            {
                float scale{ subspan_scale[count] };
                l_attrib_int_dx[0] = (int32_t)((float)(l_attrib_int_next[0] - l_attrib_int[0]) * scale);
                l_attrib_int_dx[1] = (int32_t)((float)(l_attrib_int_next[1] - l_attrib_int[1]) * scale);
            }

            while (count--)
            {
                if (depth_type::process_test(l_depth_addr, l_depth))
                {
                    uint32_t color{ sample_type::process_texel(
                        l_attrib_int[0],
                        l_attrib_int[1],
                        l_smask, l_tmask, l_tshift, l_texture_lut, l_texture_data) };
                    if (mask_type::process(color))
                    {
                        blend_type::process(l_frame_addr, color);
                        depth_type::process_write(l_depth_addr, l_depth);
                    }
                }

                l_depth += l_gdx[0];
                l_attrib_int[0] += l_attrib_int_dx[0];
                l_attrib_int[1] += l_attrib_int_dx[1];

                l_depth_addr++;
                l_frame_addr++;
            }
        }
    }

    // raster

    gradient g[attrib_count];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    int32_t smask;
    int32_t tmask;
    int32_t tshift;
    const uint32_t* texture_lut;
    const uint8_t* texture_data;
};

template<
    typename sample_type = sample_nearest,
    typename blend_type = blend_none,
    typename depth_type = depth_test_write,
    typename mask_type = mask_texture_off>
struct raster_texture_shade_vertex : public scan
{
    // scan

    bool mip_enable;

    int32_t texture_width;
    int32_t texture_height;

    const uint8_t* mip_table[mip_table_max_size];
    int32_t mip_max_level;

    void setup(const config* c) override
    {
        mip_enable = (c->flags & TEXMIP_FACE) != 0;

        texture_width = c->texture_width;
        texture_height = c->texture_height;

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;

        if (!mip_enable)
        {
            smask = (c->texture_width - 1) << 16;
            tmask = (c->texture_height - 1) << 16;
            tshift = 16 - math::log2(c->texture_width);
            texture_lut = (uint32_t*)(c->texture_lut);
            texture_data = c->texture_data;
        }
        else
        {
            texture_lut = (uint32_t*)(c->texture_lut);
            mip_max_level = mip_table_build(c->texture_data, c->texture_width, c->texture_height, mip_table) - 1;
        }
    }

    enum
    {
        attrib_z,
        attrib_w,
        attrib_fs,
        attrib_ft,
        attrib_sr,
        attrib_sg,
        attrib_sb,
        attrib_count
    };

    bool setup_face(const float* pv[], uint32_t vertex_count, bool back_cull, bool& is_clockwise) override
    {
        if (interp_setup_face(pv, vertex_count, back_cull, is_clockwise, g))
        {
            float texture_width_f{ (float)texture_width };
            float texture_height_f{ (float)texture_height };
            if (!mip_enable)
            {
                g[attrib_fs].dx *= texture_width_f;
                g[attrib_fs].dy *= texture_width_f;
                g[attrib_fs].d *= texture_width_f;
                g[attrib_ft].dx *= texture_height_f;
                g[attrib_ft].dy *= texture_height_f;
                g[attrib_ft].d *= texture_height_f;
            }
            else
            {
                int32_t mip_level{ mip_level_calc(pv, vertex_count, texture_width_f, texture_height_f) };
                mip_level = math::clamp(mip_level, (int32_t)0, mip_max_level);
                int32_t mip_texture_width{ texture_width >> mip_level };
                int32_t mip_texture_height{ texture_height >> mip_level };
                float mip_texture_width_f{ (float)mip_texture_width };
                float mip_texture_height_f{ (float)mip_texture_height };
                g[attrib_fs].dx *= mip_texture_width_f;
                g[attrib_fs].dy *= mip_texture_width_f;
                g[attrib_fs].d *= mip_texture_width_f;
                g[attrib_ft].dx *= mip_texture_height_f;
                g[attrib_ft].dy *= mip_texture_height_f;
                g[attrib_ft].d *= mip_texture_height_f;
                smask = (mip_texture_width - 1) << 16;
                tmask = (mip_texture_height - 1) << 16;
                tshift = 16 - math::log2(mip_texture_width);
                texture_data = mip_table[mip_level];
            }
            return true;
        }
        return false;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        float l_gdx[attrib_count];
        int32_t l_smask;
        int32_t l_tmask;
        int32_t l_tshift;
        const uint32_t* l_texture_lut;
        const uint8_t* l_texture_data;
        float l_attrib[attrib_count];
        float l_depth;
        float* l_depth_addr;
        uint32_t* l_frame_addr;
        int32_t l_attrib_int_dx[attrib_count - 2]; // 16.16
        int32_t l_attrib_int[attrib_count - 2]; // 16.16
        int32_t l_attrib_int_next[attrib_count - 2]; // 16.16

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        int32_t start{ frame_stride * y + x0 };
        l_gdx[0] = g[0].dx;
        l_gdx[1] = g[1].dx;
        l_gdx[2] = g[2].dx;
        l_gdx[3] = g[3].dx;
        l_gdx[4] = g[4].dx;
        l_gdx[5] = g[5].dx;
        l_gdx[6] = g[6].dx;
        l_smask = smask;
        l_tmask = tmask;
        l_tshift = tshift;
        l_texture_lut = texture_lut;
        l_texture_data = texture_data;
        l_attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        l_attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        l_attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        l_attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        l_attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        l_attrib[5] = g[5].dx * x0f + g[5].dy * y0f + g[5].d;
        l_attrib[6] = g[6].dx * x0f + g[6].dy * y0f + g[6].d;
        float w{ (float)0x10000 / l_attrib[1] };
        l_attrib_int_next[0] = sample_type::process_coord((int32_t)(l_attrib[2] * w));
        l_attrib_int_next[1] = sample_type::process_coord((int32_t)(l_attrib[3] * w));
        l_attrib_int_next[2] = math::clamp((int32_t)(l_attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[3] = math::clamp((int32_t)(l_attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_attrib_int_next[4] = math::clamp((int32_t)(l_attrib[6] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        l_depth_addr = &depth_buffer[start];
        l_frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);

        int32_t n{ x1 - x0 };
        while (n)
        {
            int32_t count{ math::min(n, span_block_size) };
            n -= count;

            float count_float{ (float)count };
            l_depth = l_attrib[0];
            l_attrib[0] += l_gdx[0] * count_float;
            l_attrib[1] += l_gdx[1] * count_float;
            l_attrib[2] += l_gdx[2] * count_float;
            l_attrib[3] += l_gdx[3] * count_float;
            l_attrib[4] += l_gdx[4] * count_float;
            l_attrib[5] += l_gdx[5] * count_float;
            l_attrib[6] += l_gdx[6] * count_float;
            float w{ (float)0x10000 / l_attrib[1] };
            l_attrib_int[0] = l_attrib_int_next[0];
            l_attrib_int[1] = l_attrib_int_next[1];
            l_attrib_int[2] = l_attrib_int_next[2];
            l_attrib_int[3] = l_attrib_int_next[3];
            l_attrib_int[4] = l_attrib_int_next[4];
            l_attrib_int_next[0] = sample_type::process_coord((int32_t)(l_attrib[2] * w));
            l_attrib_int_next[1] = sample_type::process_coord((int32_t)(l_attrib[3] * w));
            l_attrib_int_next[2] = math::clamp((int32_t)(l_attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[3] = math::clamp((int32_t)(l_attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            l_attrib_int_next[4] = math::clamp((int32_t)(l_attrib[6] * w), (int32_t)0, (int32_t)0x00FFFFFF);
            if (count == span_block_size)
            {
                l_attrib_int_dx[0] = (l_attrib_int_next[0] - l_attrib_int[0]) >> span_block_size_shift;
                l_attrib_int_dx[1] = (l_attrib_int_next[1] - l_attrib_int[1]) >> span_block_size_shift;
                l_attrib_int_dx[2] = (l_attrib_int_next[2] - l_attrib_int[2]) >> span_block_size_shift;
                l_attrib_int_dx[3] = (l_attrib_int_next[3] - l_attrib_int[3]) >> span_block_size_shift;
                l_attrib_int_dx[4] = (l_attrib_int_next[4] - l_attrib_int[4]) >> span_block_size_shift;
            }
            else
            {
                float scale{ subspan_scale[count] };
                l_attrib_int_dx[0] = (int32_t)((float)(l_attrib_int_next[0] - l_attrib_int[0]) * scale);
                l_attrib_int_dx[1] = (int32_t)((float)(l_attrib_int_next[1] - l_attrib_int[1]) * scale);
                l_attrib_int_dx[2] = (int32_t)((float)(l_attrib_int_next[2] - l_attrib_int[2]) * scale);
                l_attrib_int_dx[3] = (int32_t)((float)(l_attrib_int_next[3] - l_attrib_int[3]) * scale);
                l_attrib_int_dx[4] = (int32_t)((float)(l_attrib_int_next[4] - l_attrib_int[4]) * scale);
            }

            while (count--)
            {
                if (depth_type::process_test(l_depth_addr, l_depth))
                {
                    uint32_t texel{ sample_type::process_texel(
                        l_attrib_int[0],
                        l_attrib_int[1],
                        l_smask, l_tmask, l_tshift, l_texture_lut, l_texture_data) };
                    if (mask_type::process(texel))
                    {
                        uint32_t color
                        {
                            (((((texel & 0xFF000000u)        )                            )              )       ) +
                            (((((texel & 0x00FF0000u) >> 16u ) * (uint32_t)l_attrib_int[2]) & 0xFF000000u) >>  8u) +
                            (((((texel & 0x0000FF00u) >>  8u ) * (uint32_t)l_attrib_int[3]) & 0xFF000000u) >> 16u) +
                            (((((texel & 0x000000FFu)        ) * (uint32_t)l_attrib_int[4])              ) >> 24u) + 0x00010101u
                        };
                        blend_type::process(l_frame_addr, color);
                        depth_type::process_write(l_depth_addr, l_depth);
                    }
                }

                l_depth += l_gdx[0];
                l_attrib_int[0] += l_attrib_int_dx[0];
                l_attrib_int[1] += l_attrib_int_dx[1];
                l_attrib_int[2] += l_attrib_int_dx[2];
                l_attrib_int[3] += l_attrib_int_dx[3];
                l_attrib_int[4] += l_attrib_int_dx[4];

                l_depth_addr++;
                l_frame_addr++;
            }
        }
    }

    // raster

    gradient g[attrib_count];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    int32_t smask;
    int32_t tmask;
    int32_t tshift;
    const uint32_t* texture_lut;
    const uint8_t* texture_data;
};

template<
    typename sample_type = sample_nearest,
    typename blend_type = blend_none,
    typename depth_type = depth_test_write,
    typename mask_type = mask_texture_off>
struct raster_texture_shade_lightmap : public scan
{
    // scan

    bool mip_enable;

    int32_t texture_width;
    int32_t texture_height;

    const uint8_t* mip_table[mip_table_max_size];
    int32_t mip_max_level;

    void setup(const config* c) override
    {
        mip_enable = (c->flags & TEXMIP_FACE) != 0;

        texture_width = c->texture_width;
        texture_height = c->texture_height;

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;

        if (!mip_enable)
        {
            smask = (c->texture_width - 1) << 16;
            tmask = (c->texture_height - 1) << 16;
            tshift = 16 - math::log2(c->texture_width);
            texture_lut = (uint32_t*)(c->texture_lut);
            texture_data = c->texture_data;
        }
        else
        {
            texture_lut = (uint32_t*)(c->texture_lut);
            mip_max_level = mip_table_build(c->texture_data, c->texture_width, c->texture_height, mip_table) - 1;
        }

        umax = (c->lightmap_width - 1) << 16;
        vmax = (c->lightmap_height - 1) << 16;
        vshift = math::log2(c->lightmap_width);
        lightmap = (const uint32_t*)(c->lightmap);
    }

    enum
    {
        attrib_z,
        attrib_w,
        attrib_fs,
        attrib_ft,
        attrib_su,
        attrib_sv,
        attrib_count
    };

    bool setup_face(const float* pv[], uint32_t vertex_count, bool back_cull, bool& is_clockwise) override
    {
        if (interp_setup_face(pv, vertex_count, back_cull, is_clockwise, g))
        {
            float texture_width_f{ (float)texture_width };
            float texture_height_f{ (float)texture_height };
            if (!mip_enable)
            {
                g[attrib_fs].dx *= texture_width_f;
                g[attrib_fs].dy *= texture_width_f;
                g[attrib_fs].d *= texture_width_f;
                g[attrib_ft].dx *= texture_height_f;
                g[attrib_ft].dy *= texture_height_f;
                g[attrib_ft].d *= texture_height_f;
            }
            else
            {
                int32_t mip_level{ mip_level_calc(pv, vertex_count, texture_width_f, texture_height_f) };
                mip_level = math::clamp(mip_level, (int32_t)0, mip_max_level);
                int32_t mip_texture_width{ texture_width >> mip_level };
                int32_t mip_texture_height{ texture_height >> mip_level };
                float mip_texture_width_f{ (float)mip_texture_width };
                float mip_texture_height_f{ (float)mip_texture_height };
                g[attrib_fs].dx *= mip_texture_width_f;
                g[attrib_fs].dy *= mip_texture_width_f;
                g[attrib_fs].d *= mip_texture_width_f;
                g[attrib_ft].dx *= mip_texture_height_f;
                g[attrib_ft].dy *= mip_texture_height_f;
                g[attrib_ft].d *= mip_texture_height_f;
                smask = (mip_texture_width - 1) << 16;
                tmask = (mip_texture_height - 1) << 16;
                tshift = 16 - math::log2(mip_texture_width);
                texture_data = mip_table[mip_level];
            }
            return true;
        }
        return false;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        float l_gdx[attrib_count];
        int32_t l_smask;
        int32_t l_tmask;
        int32_t l_tshift;
        const uint32_t* l_texture_lut;
        const uint8_t* l_texture_data;
        int32_t l_umax;
        int32_t l_vmax;
        int32_t l_vshift;
        const uint32_t* l_lightmap;
        float l_attrib[attrib_count];
        float l_depth;
        float* l_depth_addr;
        uint32_t* l_frame_addr;
        int32_t l_attrib_int_dx[attrib_count - 2]; // 16.16
        int32_t l_attrib_int[attrib_count - 2]; // 16.16
        int32_t l_attrib_int_next[attrib_count - 2]; // 16.16
        uint32_t l_shade_counter;
        uint32_t l_shade_trigger;
        uint32_t l_shade[3];

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        int32_t start{ frame_stride * y + x0 };
        l_gdx[0] = g[0].dx;
        l_gdx[1] = g[1].dx;
        l_gdx[2] = g[2].dx;
        l_gdx[3] = g[3].dx;
        l_gdx[4] = g[4].dx;
        l_gdx[5] = g[5].dx;
        l_smask = smask;
        l_tmask = tmask;
        l_tshift = tshift;
        l_texture_lut = texture_lut;
        l_texture_data = texture_data;
        l_umax = umax;
        l_vmax = vmax;
        l_vshift = vshift;
        l_lightmap = lightmap;
        l_attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        l_attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        l_attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        l_attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        l_attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        l_attrib[5] = g[5].dx * x0f + g[5].dy * y0f + g[5].d;
        float w{ (float)0x10000 / l_attrib[1] };
        l_attrib_int_next[0] = sample_type::process_coord((int32_t)(l_attrib[2] * w));
        l_attrib_int_next[1] = sample_type::process_coord((int32_t)(l_attrib[3] * w));
        l_attrib_int_next[2] = math::clamp((int32_t)(l_attrib[4] * w), (int32_t)0, l_umax);
        l_attrib_int_next[3] = math::clamp((int32_t)(l_attrib[5] * w), (int32_t)0, l_vmax);
        l_depth_addr = &depth_buffer[start];
        l_frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
        l_shade_counter = ((y & 1 ? shade_hold >> 1u : 0u) + x0) & shade_mask;
        l_shade_trigger = 1;

        int32_t n{ x1 - x0 };
        while (n)
        {
            int32_t count{ math::min(n, span_block_size) };
            n -= count;

            float count_float{ (float)count };
            l_depth = l_attrib[0];
            l_attrib[0] += l_gdx[0] * count_float;
            l_attrib[1] += l_gdx[1] * count_float;
            l_attrib[2] += l_gdx[2] * count_float;
            l_attrib[3] += l_gdx[3] * count_float;
            l_attrib[4] += l_gdx[4] * count_float;
            l_attrib[5] += l_gdx[5] * count_float;
            float w{ (float)0x10000 / l_attrib[1] };
            l_attrib_int[0] = l_attrib_int_next[0];
            l_attrib_int[1] = l_attrib_int_next[1];
            l_attrib_int[2] = l_attrib_int_next[2];
            l_attrib_int[3] = l_attrib_int_next[3];
            l_attrib_int_next[0] = sample_type::process_coord((int32_t)(l_attrib[2] * w));
            l_attrib_int_next[1] = sample_type::process_coord((int32_t)(l_attrib[3] * w));
            l_attrib_int_next[2] = math::clamp((int32_t)(l_attrib[4] * w), (int32_t)0, l_umax);
            l_attrib_int_next[3] = math::clamp((int32_t)(l_attrib[5] * w), (int32_t)0, l_vmax);
            if (count == span_block_size)
            {
                l_attrib_int_dx[0] = (l_attrib_int_next[0] - l_attrib_int[0]) >> span_block_size_shift;
                l_attrib_int_dx[1] = (l_attrib_int_next[1] - l_attrib_int[1]) >> span_block_size_shift;
                l_attrib_int_dx[2] = (l_attrib_int_next[2] - l_attrib_int[2]) >> span_block_size_shift;
                l_attrib_int_dx[3] = (l_attrib_int_next[3] - l_attrib_int[3]) >> span_block_size_shift;
            }
            else
            {
                float scale{ subspan_scale[count] };
                l_attrib_int_dx[0] = (int32_t)((float)(l_attrib_int_next[0] - l_attrib_int[0]) * scale);
                l_attrib_int_dx[1] = (int32_t)((float)(l_attrib_int_next[1] - l_attrib_int[1]) * scale);
                l_attrib_int_dx[2] = (int32_t)((float)(l_attrib_int_next[2] - l_attrib_int[2]) * scale);
                l_attrib_int_dx[3] = (int32_t)((float)(l_attrib_int_next[3] - l_attrib_int[3]) * scale);
            }

            while (count--)
            {
                if (depth_type::process_test(l_depth_addr, l_depth))
                {
                    uint32_t texel{ sample_type::process_texel(
                        l_attrib_int[0],
                        l_attrib_int[1],
                        l_smask, l_tmask, l_tshift, l_texture_lut, l_texture_data) };
                    if (mask_type::process(texel))
                    {
                        if (((l_shade_counter & shade_mask) == 0) | l_shade_trigger)
                        {
                            l_shade_trigger = 0;
                            uint32_t shade_color{ sample_lightmap(
                                l_attrib_int[2],
                                l_attrib_int[3],
                                l_vshift, l_lightmap) };
                            l_shade[0] = (shade_color & 0x00FF0000u) >> 16u;
                            l_shade[1] = (shade_color & 0x0000FF00u) >>  8u;
                            l_shade[2] = (shade_color & 0x000000FFu)       ;
                        }
                        uint32_t color
                        {
                            ((((texel & 0xFF000000u)             )              )      ) +
                            ((((texel & 0x00FF0000u) * l_shade[0]) & 0xFF000000u) >> 8u) +
                            ((((texel & 0x0000FF00u) * l_shade[1]) & 0x00FF0000u) >> 8u) +
                            ((((texel & 0x000000FFu) * l_shade[2])              ) >> 8u) + 0x00010101u
                        };
                        blend_type::process(l_frame_addr, color);
                        depth_type::process_write(l_depth_addr, l_depth);
                    }
                }
                else
                {
                    l_shade_trigger = 1;
                }

                l_depth += l_gdx[0];
                l_attrib_int[0] += l_attrib_int_dx[0];
                l_attrib_int[1] += l_attrib_int_dx[1];
                l_attrib_int[2] += l_attrib_int_dx[2];
                l_attrib_int[3] += l_attrib_int_dx[3];

                l_depth_addr++;
                l_frame_addr++;

                l_shade_counter++;
            }
        }
    }

    // raster

    gradient g[attrib_count];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    int32_t smask;
    int32_t tmask;
    int32_t tshift;
    const uint32_t* texture_lut;
    const uint8_t* texture_data;
    int32_t umax;
    int32_t vmax;
    int32_t vshift;
    const uint32_t* lightmap;
};

template<
    typename sample_type = sample_nearest,
    typename blend_type = blend_none,
    typename depth_type = depth_test_write,
    typename mask_type = mask_texture_off>
struct raster_texture_shade_light : public scan
{
    // scan

    bool mip_enable;

    int32_t texture_width;
    int32_t texture_height;

    const uint8_t* mip_table[mip_table_max_size];
    int32_t mip_max_level;

    void setup(const config* c) override
    {
        mip_enable = (c->flags & TEXMIP_FACE) != 0;

        texture_width = c->texture_width;
        texture_height = c->texture_height;

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;

        if (!mip_enable)
        {
            smask = (c->texture_width - 1) << 16;
            tmask = (c->texture_height - 1) << 16;
            tshift = 16 - math::log2(c->texture_width);
            texture_lut = (uint32_t*)(c->texture_lut);
            texture_data = c->texture_data;
        }
        else
        {
            texture_lut = (uint32_t*)(c->texture_lut);
            mip_max_level = mip_table_build(c->texture_data, c->texture_width, c->texture_height, mip_table) - 1;
        }

        num_lights = c->num_lights;
        light_data = c->light_data;
        light_table = c->light_table;
    }

    enum
    {
        attrib_z,
        attrib_w,
        attrib_fs,
        attrib_ft,
        attrib_px,
        attrib_py,
        attrib_pz,
        attrib_nx,
        attrib_ny,
        attrib_nz,
        attrib_count
    };

    bool setup_face(const float* pv[], uint32_t vertex_count, bool back_cull, bool& is_clockwise) override
    {
        if (interp_setup_face(pv, vertex_count, back_cull, is_clockwise, g))
        {
            float texture_width_f{ (float)texture_width };
            float texture_height_f{ (float)texture_height };
            if (!mip_enable)
            {
                g[attrib_fs].dx *= texture_width_f;
                g[attrib_fs].dy *= texture_width_f;
                g[attrib_fs].d *= texture_width_f;
                g[attrib_ft].dx *= texture_height_f;
                g[attrib_ft].dy *= texture_height_f;
                g[attrib_ft].d *= texture_height_f;
            }
            else
            {
                int32_t mip_level{ mip_level_calc(pv, vertex_count, texture_width_f, texture_height_f) };
                mip_level = math::clamp(mip_level, (int32_t)0, mip_max_level);
                int32_t mip_texture_width{ texture_width >> mip_level };
                int32_t mip_texture_height{ texture_height >> mip_level };
                float mip_texture_width_f{ (float)mip_texture_width };
                float mip_texture_height_f{ (float)mip_texture_height };
                g[attrib_fs].dx *= mip_texture_width_f;
                g[attrib_fs].dy *= mip_texture_width_f;
                g[attrib_fs].d *= mip_texture_width_f;
                g[attrib_ft].dx *= mip_texture_height_f;
                g[attrib_ft].dy *= mip_texture_height_f;
                g[attrib_ft].d *= mip_texture_height_f;
                smask = (mip_texture_width - 1) << 16;
                tmask = (mip_texture_height - 1) << 16;
                tshift = 16 - math::log2(mip_texture_width);
                texture_data = mip_table[mip_level];
            }
            return true;
        }
        return false;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        float l_gdx[attrib_count];
        int32_t l_smask;
        int32_t l_tmask;
        int32_t l_tshift;
        const uint32_t* l_texture_lut;
        const uint8_t* l_texture_data;
        uint32_t l_num_lights;
        const light* l_light_data;
        const math::powfast_table* l_light_table;
        float l_attrib[attrib_count];
        float l_depth;
        float* l_depth_addr;
        uint32_t* l_frame_addr;
        int32_t l_attrib_inti_dx[2]; // 16.16
        float l_attrib_intf_dx[6];
        int32_t l_attrib_inti[2]; // 16.16
        float l_attrib_intf[6];
        int32_t l_attrib_inti_next[2]; // 16.16
        float l_attrib_intf_next[6];
        uint32_t l_shade_counter;
        uint32_t l_shade_trigger;
        uint32_t l_shade[3];

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        int32_t start{ frame_stride * y + x0 };
        l_gdx[0] = g[0].dx;
        l_gdx[1] = g[1].dx;
        l_gdx[2] = g[2].dx;
        l_gdx[3] = g[3].dx;
        l_gdx[4] = g[4].dx;
        l_gdx[5] = g[5].dx;
        l_gdx[6] = g[6].dx;
        l_gdx[7] = g[7].dx;
        l_gdx[8] = g[8].dx;
        l_gdx[9] = g[9].dx;
        l_smask = smask;
        l_tmask = tmask;
        l_tshift = tshift;
        l_texture_lut = texture_lut;
        l_texture_data = texture_data;
        l_num_lights = num_lights;
        l_light_data = light_data;
        l_light_table = light_table;
        l_attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        l_attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        l_attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        l_attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        l_attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        l_attrib[5] = g[5].dx * x0f + g[5].dy * y0f + g[5].d;
        l_attrib[6] = g[6].dx * x0f + g[6].dy * y0f + g[6].d;
        l_attrib[7] = g[7].dx * x0f + g[7].dy * y0f + g[7].d;
        l_attrib[8] = g[8].dx * x0f + g[8].dy * y0f + g[8].d;
        l_attrib[9] = g[9].dx * x0f + g[9].dy * y0f + g[9].d;
        float wf{ 1.f / l_attrib[1] };
        float wi{ (float)0x10000 * wf };
        l_attrib_inti_next[0] = sample_type::process_coord((int32_t)(l_attrib[2] * wi));
        l_attrib_inti_next[1] = sample_type::process_coord((int32_t)(l_attrib[3] * wi));
        l_attrib_intf_next[0] = l_attrib[4] * wf;
        l_attrib_intf_next[1] = l_attrib[5] * wf;
        l_attrib_intf_next[2] = l_attrib[6] * wf;
        l_attrib_intf_next[3] = l_attrib[7] * wf;
        l_attrib_intf_next[4] = l_attrib[8] * wf;
        l_attrib_intf_next[5] = l_attrib[9] * wf;
        l_depth_addr = &depth_buffer[start];
        l_frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
        l_shade_counter = ((y & 1 ? shade_hold >> 1u : 0u) + x0) & shade_mask;
        l_shade_trigger = 1;

        int32_t n{ x1 - x0 };
        while (n)
        {
            int32_t count{ math::min(n, span_block_size) };
            n -= count;

            float count_float{ (float)count };
            l_depth = l_attrib[0];
            l_attrib[0] += l_gdx[0] * count_float;
            l_attrib[1] += l_gdx[1] * count_float;
            l_attrib[2] += l_gdx[2] * count_float;
            l_attrib[3] += l_gdx[3] * count_float;
            l_attrib[4] += l_gdx[4] * count_float;
            l_attrib[5] += l_gdx[5] * count_float;
            l_attrib[6] += l_gdx[6] * count_float;
            l_attrib[7] += l_gdx[7] * count_float;
            l_attrib[8] += l_gdx[8] * count_float;
            l_attrib[9] += l_gdx[9] * count_float;
            float wf{ 1.f / l_attrib[1] };
            float wi{ (float)0x10000 * wf };
            l_attrib_inti[0] = l_attrib_inti_next[0];
            l_attrib_inti[1] = l_attrib_inti_next[1];
            l_attrib_intf[0] = l_attrib_intf_next[0];
            l_attrib_intf[1] = l_attrib_intf_next[1];
            l_attrib_intf[2] = l_attrib_intf_next[2];
            l_attrib_intf[3] = l_attrib_intf_next[3];
            l_attrib_intf[4] = l_attrib_intf_next[4];
            l_attrib_intf[5] = l_attrib_intf_next[5];
            l_attrib_inti_next[0] = sample_type::process_coord((int32_t)(l_attrib[2] * wi));
            l_attrib_inti_next[1] = sample_type::process_coord((int32_t)(l_attrib[3] * wi));
            l_attrib_intf_next[0] = l_attrib[4] * wf;
            l_attrib_intf_next[1] = l_attrib[5] * wf;
            l_attrib_intf_next[2] = l_attrib[6] * wf;
            l_attrib_intf_next[3] = l_attrib[7] * wf;
            l_attrib_intf_next[4] = l_attrib[8] * wf;
            l_attrib_intf_next[5] = l_attrib[9] * wf;
            if (count == span_block_size)
            {
                l_attrib_inti_dx[0] = (l_attrib_inti_next[0] - l_attrib_inti[0]) >> span_block_size_shift;
                l_attrib_inti_dx[1] = (l_attrib_inti_next[1] - l_attrib_inti[1]) >> span_block_size_shift;
            }
            else
            {
                float scale{ subspan_scale[count] };
                l_attrib_inti_dx[0] = (int32_t)((float)(l_attrib_inti_next[0] - l_attrib_inti[0]) * scale);
                l_attrib_inti_dx[1] = (int32_t)((float)(l_attrib_inti_next[1] - l_attrib_inti[1]) * scale);
            }
            float scale{ 1.f / count_float };
            l_attrib_intf_dx[0] = (l_attrib_intf_next[0] - l_attrib_intf[0]) * scale;
            l_attrib_intf_dx[1] = (l_attrib_intf_next[1] - l_attrib_intf[1]) * scale;
            l_attrib_intf_dx[2] = (l_attrib_intf_next[2] - l_attrib_intf[2]) * scale;
            l_attrib_intf_dx[3] = (l_attrib_intf_next[3] - l_attrib_intf[3]) * scale;
            l_attrib_intf_dx[4] = (l_attrib_intf_next[4] - l_attrib_intf[4]) * scale;
            l_attrib_intf_dx[5] = (l_attrib_intf_next[5] - l_attrib_intf[5]) * scale;

            while (count--)
            {
                if (depth_type::process_test(l_depth_addr, l_depth))
                {
                    uint32_t texel{ sample_type::process_texel(
                        l_attrib_inti[0],
                        l_attrib_inti[1],
                        l_smask, l_tmask, l_tshift, l_texture_lut, l_texture_data) };
                    if (mask_type::process(texel))
                    {
                        if (((l_shade_counter & shade_mask) == 0) | l_shade_trigger)
                        {
                            l_shade_trigger = 0;
                            sample_light(
                                &l_attrib_intf[0], &l_attrib_intf[3],
                                l_light_data, l_num_lights,
                                l_light_table,
                                l_shade);
                        }
                        uint32_t color
                        {
                            ((((texel & 0xFF000000u)             )              )      ) +
                            ((((texel & 0x00FF0000u) * l_shade[0]) & 0xFF000000u) >> 8u) +
                            ((((texel & 0x0000FF00u) * l_shade[1]) & 0x00FF0000u) >> 8u) +
                            ((((texel & 0x000000FFu) * l_shade[2])              ) >> 8u) + 0x00010101u
                        };
                        blend_type::process(l_frame_addr, color);
                        depth_type::process_write(l_depth_addr, l_depth);
                    }
                }
                else
                {
                    l_shade_trigger = 1;
                }

                l_depth += l_gdx[0];
                l_attrib_inti[0] += l_attrib_inti_dx[0];
                l_attrib_inti[1] += l_attrib_inti_dx[1];
                l_attrib_intf[0] += l_attrib_intf_dx[0];
                l_attrib_intf[1] += l_attrib_intf_dx[1];
                l_attrib_intf[2] += l_attrib_intf_dx[2];
                l_attrib_intf[3] += l_attrib_intf_dx[3];
                l_attrib_intf[4] += l_attrib_intf_dx[4];
                l_attrib_intf[5] += l_attrib_intf_dx[5];

                l_depth_addr++;
                l_frame_addr++;

                l_shade_counter++;
            }
        }
    }

    // raster

    gradient g[attrib_count];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    int32_t smask;
    int32_t tmask;
    int32_t tshift;
    const uint32_t* texture_lut;
    const uint8_t* texture_data;
    uint32_t num_lights;
    const light* light_data;
    const math::powfast_table* light_table;
};

//------------------------------------------------------------------------------

struct fill_solid_tag {};
struct fill_vertex_tag {};
struct fill_texture_tag {};

struct shade_none_tag {};
struct shade_vertex_tag {};
struct shade_lightmap_tag {};
struct shade_light_tag {};

template<typename depth_type, typename fill_tag, typename shade_tag, typename blend_type, typename mask_type, typename sample_type>
blib3d_force_inline scan* create_raster_texfilter(void* addr, const config* /*c*/)
{
    if constexpr (std::is_same_v<fill_tag, fill_texture_tag>)
    {
        if constexpr (std::is_same_v<shade_tag, shade_none_tag>)
            return new (addr) raster_texture_shade_none<sample_type, blend_type, depth_type, mask_type>();
        if constexpr (std::is_same_v<shade_tag, shade_vertex_tag>)
            return new (addr) raster_texture_shade_vertex<sample_type, blend_type, depth_type, mask_type>();
        if constexpr (std::is_same_v<shade_tag, shade_lightmap_tag>)
            return new (addr) raster_texture_shade_lightmap<sample_type, blend_type, depth_type, mask_type>();
        if constexpr (std::is_same_v<shade_tag, shade_light_tag>)
            return new (addr) raster_texture_shade_light<sample_type, blend_type, depth_type, mask_type>();
    }
}

template<typename depth_type, typename fill_tag, typename shade_tag, typename blend_type, typename mask_type>
blib3d_force_inline scan* create_raster_texmask(void* addr, const config* c)
{
    switch (c->flags & TEXFILTER_MASK)
    {
    case TEXFILTER_NONE: return create_raster_texfilter<depth_type, fill_tag, shade_tag, blend_type, mask_type, sample_nearest>(addr, c);
    case TEXFILTER_LINEAR: return create_raster_texfilter<depth_type, fill_tag, shade_tag, blend_type, mask_type, sample_bilinear>(addr, c);
    default: return nullptr;
    }
}

template<typename depth_type, typename fill_tag, typename shade_tag, typename blend_type>
blib3d_force_inline scan* create_raster_blend(void* addr, const config* c)
{
    if constexpr (std::is_same_v<fill_tag, fill_solid_tag>)
    {
        if constexpr (std::is_same_v<shade_tag, shade_none_tag>)
            return new (addr) raster_solid_shade_none<blend_type, depth_type>();
        if constexpr (std::is_same_v<shade_tag, shade_vertex_tag>)
            return new (addr) raster_solid_shade_vertex<blend_type, depth_type>();
        if constexpr (std::is_same_v<shade_tag, shade_lightmap_tag>)
            return new (addr) raster_solid_shade_lightmap<blend_type, depth_type>();
        if constexpr (std::is_same_v<shade_tag, shade_light_tag>)
            return new (addr) raster_solid_shade_light<blend_type, depth_type>();
    }
    if constexpr (std::is_same_v<fill_tag, fill_vertex_tag>)
    {
        if constexpr (std::is_same_v<shade_tag, shade_none_tag>)
            return new (addr) raster_vertex_shade_none<blend_type, depth_type>();
        if constexpr (std::is_same_v<shade_tag, shade_vertex_tag>)
            return new (addr) raster_vertex_shade_vertex<blend_type, depth_type>();
        if constexpr (std::is_same_v<shade_tag, shade_lightmap_tag>)
            return new (addr) raster_vertex_shade_lightmap<blend_type, depth_type>();
        if constexpr (std::is_same_v<shade_tag, shade_light_tag>)
            return new (addr) raster_vertex_shade_light<blend_type, depth_type>();
    }
    if constexpr (std::is_same_v<fill_tag, fill_texture_tag>)
    {
        switch (c->flags & TEXMASK_MASK)
        {
        case TEXMASK_OFF: return create_raster_texmask<depth_type, fill_tag, shade_tag, blend_type, mask_texture_off>(addr, c);
        case TEXMASK_ON: return create_raster_texmask<depth_type, fill_tag, shade_tag, blend_type, mask_texture_on>(addr, c);
        default: return nullptr;
        }
    }
}

template<typename depth_type, typename fill_tag, typename shade_tag>
blib3d_force_inline scan* create_raster_shade(void* addr, const config* c)
{
    switch (c->flags & BLEND_MASK)
    {
    case BLEND_NONE: return create_raster_blend<depth_type, fill_tag, shade_tag, blend_none>(addr, c);
    case BLEND_ADD: return create_raster_blend<depth_type, fill_tag, shade_tag, blend_add>(addr, c);
    case BLEND_MUL: return create_raster_blend<depth_type, fill_tag, shade_tag, blend_mul>(addr, c);
    case BLEND_ALPHA: return create_raster_blend<depth_type, fill_tag, shade_tag, blend_alpha>(addr, c);
    default: return nullptr;
    }
}

template<typename depth_type, typename fill_tag>
blib3d_force_inline scan* create_raster_fill(void* addr, const config* c)
{
    switch (c->flags & SHADE_MASK)
    {
    case SHADE_NONE: return create_raster_shade<depth_type, fill_tag, shade_none_tag>(addr, c);
    case SHADE_VERTEX: return create_raster_shade<depth_type, fill_tag, shade_vertex_tag>(addr, c);
    case SHADE_LIGHTMAP: return create_raster_shade<depth_type, fill_tag, shade_lightmap_tag>(addr, c);
    case SHADE_LIGHT: return create_raster_shade<depth_type, fill_tag, shade_light_tag>(addr, c);
    default: return nullptr;
    }
}

template<typename depth_type>
blib3d_force_inline scan* create_raster_depth(void* addr, const config* c)
{
    switch (c->flags & FILL_MASK)
    {
    case FILL_NONE: return new (addr) raster_depth<depth_type>();
    case FILL_SOLID: return create_raster_fill<depth_type, fill_solid_tag>(addr, c);
    case FILL_VERTEX: return create_raster_fill<depth_type, fill_vertex_tag>(addr, c);
    case FILL_TEXTURE: return create_raster_fill<depth_type, fill_texture_tag>(addr, c);
    default: return nullptr;
    }
}

blib3d_force_inline scan* create_raster(void* addr, const config* c)
{
    switch (c->flags & DEPTH_MASK)
    {
    case DEPTH_OFF: return create_raster_depth<depth_notest_nowrite>(addr, c);
    case DEPTH_WRITE: return create_raster_depth<depth_notest_write>(addr, c);
    case DEPTH_TEST: return create_raster_depth<depth_test_nowrite>(addr, c);
    case DEPTH_TEST_WRITE: return create_raster_depth<depth_test_write>(addr, c);
    default: return nullptr;
    }
}

//------------------------------------------------------------------------------

void scan_faces_wireframe(const config* c)
{
    batch_draw_wireframe(c);

    switch (c->flags & DEPTH_MASK)
    {
    case DEPTH_OFF:
    {
        raster_depth<depth_notest_nowrite> r;
        r.batch_draw(c);
        break;
    }
    case DEPTH_WRITE:
    {
        raster_depth<depth_notest_write> r;
        r.batch_draw(c);
        break;
    }
    case DEPTH_TEST:
    {
        raster_depth<depth_test_nowrite> r;
        r.batch_draw(c);
        break;
    }
    case DEPTH_TEST_WRITE:
    {
        raster_depth<depth_test_write> r;
        r.batch_draw(c);
        break;
    }
    default:
        break;
    }
}

void scan_faces(const config* c)
{
    union raster_pool
    {
        raster_depth<> r0;
        raster_solid_shade_none<> r1;
        raster_solid_shade_vertex<> r2;
        raster_solid_shade_lightmap<> r3;
        raster_solid_shade_light<> r4;
        raster_vertex_shade_none<> r5;
        raster_vertex_shade_vertex<> r6;
        raster_vertex_shade_lightmap<> r7;
        raster_vertex_shade_light<> r8;
        raster_texture_shade_none<> r9;
        raster_texture_shade_vertex<> r10;
        raster_texture_shade_lightmap<> r11;
        raster_texture_shade_light<> r12;
    };
    alignas(alignof(raster_pool)) uint8_t buffer[sizeof(raster_pool)];

    scan* r{ create_raster(buffer, c) };

    if (r)
        r->batch_draw(c);
}

//------------------------------------------------------------------------------

void occlusion_build_mipchain(occlusion_config& cfg, occlusion_data& data)
{
    uint32_t depth_hi_w;
    uint32_t depth_hi_h;
    float* pdepth_hi;
    uint32_t depth_lo_w;
    uint32_t depth_lo_h;
    float* pdepth_lo;

    depth_hi_w = cfg.frame_width;
    depth_hi_h = cfg.frame_height;
    pdepth_hi = cfg.depth_buffer;
    depth_lo_w = (depth_hi_w + 1) >> 1;
    depth_lo_h = (depth_hi_h + 1) >> 1;
    pdepth_lo = pdepth_hi + depth_hi_w * depth_hi_h;

    data.level_count = 0;

#if 1
    // level 0 from depth buffer
    // use min to ignore 1 pixel cracks
    assert(data.level_count < data.level_max_count);
    {
        {
            uint32_t count = depth_lo_w * depth_lo_h;
            float* pdepth = pdepth_lo;
            while (count--)
                *pdepth++ = +FLT_MAX;
        }

        for (uint32_t y_hi = 0; y_hi < depth_hi_h; ++y_hi)
        {
            uint32_t index_row_hi = depth_hi_w * y_hi;
            uint32_t index_row_lo = depth_lo_w * (y_hi >> 1);
            for (uint32_t x_hi = 0; x_hi < depth_hi_w; ++x_hi)
            {
                uint32_t index_hi = index_row_hi + x_hi;
                uint32_t index_lo = index_row_lo + (x_hi >> 1);
                assert(index_hi >= 0);
                assert(index_hi < depth_hi_w* depth_hi_h);
                assert(index_lo >= 0);
                assert(index_lo < depth_lo_w* depth_lo_h);
                pdepth_lo[index_lo] = math::min(pdepth_lo[index_lo], pdepth_hi[index_hi]);
            }
        }

        uint32_t level = data.level_count;
        data.levels[level].depth = pdepth_lo;
        data.levels[level].w = depth_lo_w;
        data.levels[level].h = depth_lo_h;
        data.level_count++;

        depth_hi_w = depth_lo_w;
        depth_hi_h = depth_lo_h;
        pdepth_hi = pdepth_lo;
        depth_lo_w = (depth_hi_w + 1) >> 1;
        depth_lo_h = (depth_hi_h + 1) >> 1;
        pdepth_lo = pdepth_hi + depth_hi_w * depth_hi_h;
    }
#endif

    // other levels, use max
    while ((depth_lo_w != 1 || depth_lo_h != 1) && data.level_count < data.level_max_count)
    {
        {
            uint32_t count = depth_lo_w * depth_lo_h;
            float* pdepth = pdepth_lo;
            while (count--)
                *pdepth++ = -FLT_MAX;
        }

        for (uint32_t y_hi = 0; y_hi < depth_hi_h; ++y_hi)
        {
            uint32_t index_row_hi = depth_hi_w * y_hi;
            uint32_t index_row_lo = depth_lo_w * (y_hi >> 1);
            for (uint32_t x_hi = 0; x_hi < depth_hi_w; ++x_hi)
            {
                uint32_t index_hi = index_row_hi + x_hi;
                uint32_t index_lo = index_row_lo + (x_hi >> 1);
                assert(index_hi >= 0);
                assert(index_hi < depth_hi_w* depth_hi_h);
                assert(index_lo >= 0);
                assert(index_lo < depth_lo_w* depth_lo_h);
                pdepth_lo[index_lo] = math::max(pdepth_lo[index_lo], pdepth_hi[index_hi]);
            }
        }

        uint32_t level = data.level_count;
        data.levels[level].depth = pdepth_lo;
        data.levels[level].w = depth_lo_w;
        data.levels[level].h = depth_lo_h;
        data.level_count++;

        depth_hi_w = depth_lo_w;
        depth_hi_h = depth_lo_h;
        pdepth_hi = pdepth_lo;
        depth_lo_w = (depth_hi_w + 1) >> 1;
        depth_lo_h = (depth_hi_h + 1) >> 1;
        pdepth_lo = pdepth_hi + depth_hi_w * depth_hi_h;
    }
}

bool occlusion_test_rect(
    occlusion_config& cfg,
    occlusion_data& data,
    float screen_min[2], float screen_max[2], float depth_min)
{
    int32_t rect_min[2]{ real_to_raster(screen_min[0]), real_to_raster(screen_min[1]) };
    int32_t rect_max[2]{ real_to_raster(screen_max[0]), real_to_raster(screen_max[1]) };
    if (rect_min[0] == rect_max[0])
        return true;
    if (rect_min[1] == rect_max[1])
        return true;
    assert(rect_min[0] < rect_max[0]);
    assert(rect_min[1] < rect_max[1]);
    if (rect_min[0] < 0)
        rect_min[0] = 0;
    if (rect_min[1] < 0)
        rect_min[1] = 0;
    if (rect_max[0] > cfg.frame_width)
        rect_max[0] = cfg.frame_width;
    if (rect_max[1] > cfg.frame_height)
        rect_max[1] = cfg.frame_height;
    // tight bounds
    rect_max[0]--;
    rect_max[1]--;
    // level zero is half size
    rect_min[0] >>= 1;
    rect_min[1] >>= 1;
    rect_max[0] >>= 1;
    rect_max[1] >>= 1;
    uint32_t level = 0;
    while (
        //rect_min[0] != rect_max[0] &&
        //rect_min[1] != rect_max[1] &&
        rect_max[0] - rect_min[0] > 1 &&
        rect_max[1] - rect_min[1] > 1 &&
        level != data.level_count - 1)
    {
        rect_min[0] >>= 1;
        rect_min[1] >>= 1;
        rect_max[0] >>= 1;
        rect_max[1] >>= 1;
        level++;
    }
    float depth_max = -FLT_MAX;
    occlusion_data::level& ol = data.levels[level];
    for (int32_t y = rect_min[1]; y <= rect_max[1]; ++y)
    {
        assert(y >= 0);
        assert(y < ol.h);
        for (int32_t x = rect_min[0]; x <= rect_max[0]; ++x)
        {
            assert(x >= 0);
            assert(x < ol.w);
            depth_max = math::max(depth_max, ol.depth[x + ol.w * y]);
        }
    }
    return depth_min > depth_max;
}

//------------------------------------------------------------------------------

} // namespace blib3d::raster
