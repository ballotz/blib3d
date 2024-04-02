#include "raster.hpp"
#include "raster_interp.hpp"
#include "raster_fill.hpp"
#include <new>
#include <cassert>
#include <cfloat>

namespace lib3d::raster
{

//------------------------------------------------------------------------------

struct abstract_raster
{
    bool is_clockwise;

    virtual bool setup_face(const float* pv[], uint32_t vertex_count) = 0;
    virtual void process_span(int32_t y, int32_t x0, int32_t x1) = 0;
};

//------------------------------------------------------------------------------

force_inline int32_t real_to_raster(float v)
{
    return math::ceil(v - 0.5f);
}

force_inline float raster_to_real(int32_t v)
{
    return (float)v + 0.5f;
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
void scan_face(const float* v[], int32_t num_vertices, abstract_raster* r)
{
    struct edge
    {
        float x;
        float dx_dy;

        force_inline void setup(const float* v0, const float* v1, float y0)
        {
            dx_dy = (v1[0] - v0[0]) / (v1[1] - v0[1]);
            x = v0[0] + (y0 - v0[1]) * dx_dy;
        }

        force_inline void advance()
        {
            x += dx_dy;
        }
    };

    struct util
    {
        static force_inline int32_t wrap(int32_t v, int32_t last)
        {
            if (v < 0)
                return last;
            if (v > last)
                return 0;
            return v;
        };
    };

    int32_t vi{ r->is_clockwise ? -1 : 1 }; // vertex index increment

    int32_t vt{ 0 }; // top vertex index
    int32_t vb{ 0 }; // bottom vertex index
    for (int32_t n{ 1 }; n < num_vertices; ++n)
    {
        if (v[vt][1] > v[n][1])
            vt = n;
        if (v[vb][1] < v[n][1])
            vb = n;
    }

    int32_t ylimit[2]
    {
        real_to_raster(v[vt][1]),
        real_to_raster(v[vb][1])
    };

    if (ylimit[0] == ylimit[1])
        return;

    int32_t last_vertex{ num_vertices - 1 };

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
        int32_t x[2]
        {
            real_to_raster(e[0].x),
            real_to_raster(e[1].x)
        };
        if (x[0] < x[1]) // float error guard (1)
            r->process_span(y, x[0], x[1]);
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

template<typename raster_type>
force_inline void span_process_algo(int32_t y, int32_t x0, int32_t x1, raster_type* r)
{
    //assert(x0 < x1);
    raster_type::span_data s;
    r->setup_span(y, x0, s);
    int32_t n{ x1 - x0 };
    while (n)
    {
        int32_t c{ math::min(n, span_block_size) };
        n -= c;
        raster_type::setup_subspan(c, s);
        while (c--)
            raster_type::fill(s);
    }
}

//template<typename raster_type>
//force_inline void span_process_algo(raster_type& raster, int32_t y, int32_t x0, int32_t x1)
//{
//    //assert(x0 < x1);
//    raster.setup_span(y, x0);
//    while (x0 != x1)
//    {
//        int32_t n{ (x0 + span_block_size) & ~(span_block_size - 1) };
//        int32_t c{ math::min(n, x1) - x0 };
//        x0 += c;
//        raster.setup_subspan(c);
//        while (c--)
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

struct raster_outline : public abstract_raster
{
    raster_outline(const config* c)
    {
        frame_stride = c->frame_stride;
        frame_buffer = c->frame_buffer;
    }

    // abstract_raster

    bool setup_face(const float* /*pv*/[], uint32_t /*vertex_count*/) override
    {
        is_clockwise = true;
        return true;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        //assert(x0 < x1);
        uint32_t* line_addr = reinterpret_cast<uint32_t*>(&frame_buffer[frame_stride * y]);
        int32_t n = x1 - x0;
        if (n == 1)
            line_addr[x0] = 0xFFFFFF00u;
        else if (n > 1)
        {
            line_addr[x0] = 0xFF00FF00u;
            line_addr[x1 - 1] = 0xFFFF0000u;
        }
    }

    // raster

    int32_t frame_stride;
    ARGB* frame_buffer;
};

//------------------------------------------------------------------------------

struct raster_depth : public abstract_raster
{
    raster_depth(const config* c)
    {
        back_cull = c->back_cull;

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
    }

    // abstract_raster

    bool back_cull;

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp_setup_face(pv, vertex_count, back_cull,
            is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        span_process_algo(y, x0, x1, this);
    }

    // raster

    gradient g[1];

    int32_t frame_stride;
    float* depth_buffer;

    struct span_data
    {
        float gdx;

        float attrib;
        float depth;

        float* depth_addr;
    };

    force_inline void setup_span(int32_t y, int32_t x0, span_data& s)
    {
        s.gdx = g[0].dx;

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        s.attrib = x0f * g[0].dx + y0f * g[0].dy + g[0].d;

        s.depth_addr = &depth_buffer[frame_stride * y + x0];
    }

    force_inline static void setup_subspan(int32_t count, span_data& s)
    {
        s.depth = s.attrib;
        s.attrib += s.gdx * (float)count;
    }

    force_inline static void fill(span_data& s)
    {
        *s.depth_addr = math::min(*s.depth_addr, s.depth);

        s.depth += s.gdx;

        s.depth_addr++;
    }
};

//------------------------------------------------------------------------------

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_solid_shade_none : public abstract_raster
{
    raster_solid_shade_none(const config* c)
    {
        back_cull = c->back_cull;

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        fill_color = reinterpret_cast<const uint32_t&>(c->fill_color);
    }

    // abstract_raster

    bool back_cull;

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp_setup_face(pv, vertex_count, back_cull,
            is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        span_process_algo(y, x0, x1, this);
    }

    // raster

    gradient g[1];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    uint32_t fill_color;

    struct span_data
    {
        float gdx;

        uint32_t fill_color;

        float attrib;
        float depth;

        float* depth_addr;
        uint32_t* frame_addr;
    };

    force_inline void setup_span(int32_t y, int32_t x0, span_data& s)
    {
        s.gdx = g[0].dx;

        s.fill_color = fill_color;

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        s.attrib = x0f * g[0].dx + y0f * g[0].dy + g[0].d;

        int32_t start{ frame_stride * y + x0 };
        s.depth_addr = &depth_buffer[start];
        s.frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline static void setup_subspan(int32_t count, span_data& s)
    {
        s.depth = s.attrib;
        s.attrib += s.gdx * (float)count;
    }

    force_inline static void fill(span_data& s)
    {
        if (depth_type::process_test(s.depth_addr, s.depth))
        {
            blend_type::process(s.frame_addr, s.fill_color);
            depth_type::process_write(s.depth_addr, s.depth);
        }

        s.depth += s.gdx;

        s.depth_addr++;
        s.frame_addr++;
    }
};

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_solid_shade_vertex : public abstract_raster
{
    raster_solid_shade_vertex(const config* c)
    {
        back_cull = c->back_cull;

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        fill_color[0] = (uint32_t)c->fill_color.a << 24u;
        fill_color[1] = (uint32_t)c->fill_color.r;
        fill_color[2] = (uint32_t)c->fill_color.g;
        fill_color[3] = (uint32_t)c->fill_color.b;
    }

    // abstract_raster

    bool back_cull;

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp_setup_face(pv, vertex_count, back_cull,
            is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        span_process_algo(y, x0, x1, this);
    }

    // raster

    gradient g[5];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    uint32_t fill_color[4];

    struct span_data
    {
        float gdx[5];

        uint32_t fill_color[4];

        float attrib[5];
        float depth;

        float* depth_addr;
        uint32_t* frame_addr;
        int32_t attrib_int_dx[3]; // 16.16
        int32_t attrib_int[3]; // 16.16
        int32_t attrib_int_next[3]; // 16.16
    };

    force_inline void setup_span(int32_t y, int32_t x0, span_data& s)
    {
        s.gdx[0] = g[0].dx;
        s.gdx[1] = g[1].dx;
        s.gdx[2] = g[2].dx;
        s.gdx[3] = g[3].dx;
        s.gdx[4] = g[4].dx;

        s.fill_color[0] = fill_color[0];
        s.fill_color[1] = fill_color[1];
        s.fill_color[2] = fill_color[2];
        s.fill_color[3] = fill_color[3];

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        s.attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        s.attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        s.attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        s.attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        s.attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int_next[0] = math::clamp((int32_t)(s.attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[1] = math::clamp((int32_t)(s.attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[2] = math::clamp((int32_t)(s.attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);

        int32_t start{ frame_stride * y + x0 };
        s.depth_addr = &depth_buffer[start];
        s.frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline static void setup_subspan(int32_t count, span_data& s)
    {
        float count_float{ (float)count };
        s.depth = s.attrib[0];
        s.attrib[0] += s.gdx[0] * count_float;
        s.attrib[1] += s.gdx[1] * count_float;
        s.attrib[2] += s.gdx[2] * count_float;
        s.attrib[3] += s.gdx[3] * count_float;
        s.attrib[4] += s.gdx[4] * count_float;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int[0] = s.attrib_int_next[0];
        s.attrib_int[1] = s.attrib_int_next[1];
        s.attrib_int[2] = s.attrib_int_next[2];
        s.attrib_int_next[0] = math::clamp((int32_t)(s.attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[1] = math::clamp((int32_t)(s.attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[2] = math::clamp((int32_t)(s.attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        if (count == span_block_size)
        {
            s.attrib_int_dx[0] = (s.attrib_int_next[0] - s.attrib_int[0]) >> span_block_size_shift;
            s.attrib_int_dx[1] = (s.attrib_int_next[1] - s.attrib_int[1]) >> span_block_size_shift;
            s.attrib_int_dx[2] = (s.attrib_int_next[2] - s.attrib_int[2]) >> span_block_size_shift;
        }
        else
        {
            float scale{ subspan_scale[count] };
            s.attrib_int_dx[0] = (int32_t)((float)(s.attrib_int_next[0] - s.attrib_int[0]) * scale);
            s.attrib_int_dx[1] = (int32_t)((float)(s.attrib_int_next[1] - s.attrib_int[1]) * scale);
            s.attrib_int_dx[2] = (int32_t)((float)(s.attrib_int_next[2] - s.attrib_int[2]) * scale);
        }
    }

    force_inline static void fill(span_data& s)
    {
        if (depth_type::process_test(s.depth_addr, s.depth))
        {
            uint32_t color
            {
                (((s.fill_color[0]                            )              )       ) +
                (((s.fill_color[1] * (uint32_t)s.attrib_int[0]) & 0xFF000000u) >>  8u) +
                (((s.fill_color[2] * (uint32_t)s.attrib_int[1]) & 0xFF000000u) >> 16u) +
                (((s.fill_color[3] * (uint32_t)s.attrib_int[2])              ) >> 24u)
            };
            blend_type::process(s.frame_addr, color);
            depth_type::process_write(s.depth_addr, s.depth);
        }

        s.depth += s.gdx[0];
        s.attrib_int[0] += s.attrib_int_dx[0];
        s.attrib_int[1] += s.attrib_int_dx[1];
        s.attrib_int[2] += s.attrib_int_dx[2];

        s.depth_addr++;
        s.frame_addr++;
    }
};

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_solid_shade_lightmap : public abstract_raster
{
    raster_solid_shade_lightmap(const config* c)
    {
        back_cull = c->back_cull;

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

    // abstract_raster

    bool back_cull;

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp_setup_face(pv, vertex_count, back_cull,
            is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        span_process_algo(y, x0, x1, this);
    }

    // raster

    gradient g[4];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    uint32_t fill_color[4];
    int32_t umax;
    int32_t vmax;
    int32_t vshift;
    const uint32_t* lightmap;

    struct span_data
    {
        float gdx[4];

        uint32_t fill_color[4];
        int32_t umax;
        int32_t vmax;
        int32_t vshift;
        const uint32_t* lightmap;

        float attrib[4];
        float depth;

        float* depth_addr;
        uint32_t* frame_addr;
        int32_t attrib_int_dx[2]; // 16.16
        int32_t attrib_int[2]; // 16.16
        int32_t attrib_int_next[2]; // 16.16

        uint32_t shade_counter;
        uint32_t shade_trigger;
        uint32_t shade[3];
    };

    force_inline void setup_span(int32_t y, int32_t x0, span_data& s)
    {
        s.gdx[0] = g[0].dx;
        s.gdx[1] = g[1].dx;
        s.gdx[2] = g[2].dx;
        s.gdx[3] = g[3].dx;

        s.fill_color[0] = fill_color[0];
        s.fill_color[1] = fill_color[1];
        s.fill_color[2] = fill_color[2];
        s.fill_color[3] = fill_color[3];
        s.umax = umax;
        s.vmax = vmax;
        s.vshift = vshift;
        s.lightmap = lightmap;

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        s.attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        s.attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        s.attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        s.attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int_next[0] = math::clamp((int32_t)(s.attrib[2] * w), (int32_t)0, umax);
        s.attrib_int_next[1] = math::clamp((int32_t)(s.attrib[3] * w), (int32_t)0, vmax);

        int32_t start{ frame_stride * y + x0 };
        s.depth_addr = &depth_buffer[start];
        s.frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);

        s.shade_counter = ((y & 1 ? shade_hold >> 1u : 0u) + x0) & shade_mask;
        s.shade_trigger = 1;
    }

    force_inline static void setup_subspan(int32_t count, span_data& s)
    {
        float count_float{ (float)count };
        s.depth = s.attrib[0];
        s.attrib[0] += s.gdx[0] * count_float;
        s.attrib[1] += s.gdx[1] * count_float;
        s.attrib[2] += s.gdx[2] * count_float;
        s.attrib[3] += s.gdx[3] * count_float;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int[0] = s.attrib_int_next[0];
        s.attrib_int[1] = s.attrib_int_next[1];
        s.attrib_int_next[0] = math::clamp((int32_t)(s.attrib[2] * w), (int32_t)0, s.umax);
        s.attrib_int_next[1] = math::clamp((int32_t)(s.attrib[3] * w), (int32_t)0, s.vmax);
        if (count == span_block_size)
        {
            s.attrib_int_dx[0] = (s.attrib_int_next[0] - s.attrib_int[0]) >> span_block_size_shift;
            s.attrib_int_dx[1] = (s.attrib_int_next[1] - s.attrib_int[1]) >> span_block_size_shift;
        }
        else
        {
            float scale{ subspan_scale[count] };
            s.attrib_int_dx[0] = (int32_t)((float)(s.attrib_int_next[0] - s.attrib_int[0]) * scale);
            s.attrib_int_dx[1] = (int32_t)((float)(s.attrib_int_next[1] - s.attrib_int[1]) * scale);
        }
    }

    force_inline static void fill(span_data& s)
    {
        if (depth_type::process_test(s.depth_addr, s.depth))
        {
            if (((s.shade_counter & shade_mask) == 0) | s.shade_trigger)
            {
                s.shade_trigger = 0;
                uint32_t shade_color{ sample_lightmap(
                    s.attrib_int[0],
                    s.attrib_int[1],
                    s.vshift, s.lightmap) };
                s.shade[0] = (shade_color & 0x00FF0000u) >> 16u;
                s.shade[1] = (shade_color & 0x0000FF00u) >>  8u;
                s.shade[2] = (shade_color & 0x000000FFu)       ;
            }
            uint32_t color
            {
                (((s.fill_color[0]             )              )      ) +
                (((s.fill_color[1] * s.shade[0]) & 0x00FF0000u)      ) +
                (((s.fill_color[2] * s.shade[1]) & 0x0000FF00u)      ) +
                (((s.fill_color[3] * s.shade[2])              ) >> 8u)
            };
            blend_type::process(s.frame_addr, color);
            depth_type::process_write(s.depth_addr, s.depth);
        }
        else
        {
            s.shade_trigger = 1;
        }

        s.depth += s.gdx[0];
        s.attrib_int[0] += s.attrib_int_dx[0];
        s.attrib_int[1] += s.attrib_int_dx[1];

        s.depth_addr++;
        s.frame_addr++;

        s.shade_counter++;
    }
};

//------------------------------------------------------------------------------

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_vertex_shade_none : public abstract_raster
{
    raster_vertex_shade_none(const config* c)
    {
        back_cull = c->back_cull;

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
    }

    // abstract_raster

    bool back_cull;

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp_setup_face(pv, vertex_count, back_cull,
            is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        span_process_algo(y, x0, x1, this);
    }

    // raster

    gradient g[6];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;

    struct span_data
    {
        float gdx[6];

        float attrib[6];
        float depth;

        float* depth_addr;
        uint32_t* frame_addr;
        int32_t attrib_int_dx[4]; // 16.16
        int32_t attrib_int[4]; // 16.16
        int32_t attrib_int_next[4]; // 16.16
    };

    force_inline void setup_span(int32_t y, int32_t x0, span_data& s)
    {
        s.gdx[0] = g[0].dx;
        s.gdx[1] = g[1].dx;
        s.gdx[2] = g[2].dx;
        s.gdx[3] = g[3].dx;
        s.gdx[4] = g[4].dx;
        s.gdx[5] = g[5].dx;

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        s.attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        s.attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        s.attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        s.attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        s.attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        s.attrib[5] = g[5].dx * x0f + g[5].dy * y0f + g[5].d;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int_next[0] = math::clamp((int32_t)(s.attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[1] = math::clamp((int32_t)(s.attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[2] = math::clamp((int32_t)(s.attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[3] = math::clamp((int32_t)(s.attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);

        int32_t start{ frame_stride * y + x0 };
        s.depth_addr = &depth_buffer[start];
        s.frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline static void setup_subspan(int32_t count, span_data& s)
    {
        float count_float{ (float)count };
        s.depth = s.attrib[0];
        s.attrib[0] += s.gdx[0] * count_float;
        s.attrib[1] += s.gdx[1] * count_float;
        s.attrib[2] += s.gdx[2] * count_float;
        s.attrib[3] += s.gdx[3] * count_float;
        s.attrib[4] += s.gdx[4] * count_float;
        s.attrib[5] += s.gdx[5] * count_float;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int[0] = s.attrib_int_next[0];
        s.attrib_int[1] = s.attrib_int_next[1];
        s.attrib_int[2] = s.attrib_int_next[2];
        s.attrib_int[3] = s.attrib_int_next[3];
        s.attrib_int_next[0] = math::clamp((int32_t)(s.attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[1] = math::clamp((int32_t)(s.attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[2] = math::clamp((int32_t)(s.attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[3] = math::clamp((int32_t)(s.attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        if (count == span_block_size)
        {
            s.attrib_int_dx[0] = (s.attrib_int_next[0] - s.attrib_int[0]) >> span_block_size_shift;
            s.attrib_int_dx[1] = (s.attrib_int_next[1] - s.attrib_int[1]) >> span_block_size_shift;
            s.attrib_int_dx[2] = (s.attrib_int_next[2] - s.attrib_int[2]) >> span_block_size_shift;
            s.attrib_int_dx[3] = (s.attrib_int_next[3] - s.attrib_int[3]) >> span_block_size_shift;
        }
        else
        {
            float scale{ subspan_scale[count] };
            s.attrib_int_dx[0] = (int32_t)((float)(s.attrib_int_next[0] - s.attrib_int[0]) * scale);
            s.attrib_int_dx[1] = (int32_t)((float)(s.attrib_int_next[1] - s.attrib_int[1]) * scale);
            s.attrib_int_dx[2] = (int32_t)((float)(s.attrib_int_next[2] - s.attrib_int[2]) * scale);
            s.attrib_int_dx[3] = (int32_t)((float)(s.attrib_int_next[3] - s.attrib_int[3]) * scale);
        }
    }

    force_inline static void fill(span_data& s)
    {
        if (depth_type::process_test(s.depth_addr, s.depth))
        {
            uint32_t color
            {
                (((uint32_t)s.attrib_int[3] & 0x00FF0000u) <<  8u) +
                (((uint32_t)s.attrib_int[0] & 0x00FF0000u)       ) +
                (((uint32_t)s.attrib_int[1] & 0x00FF0000u) >>  8u) +
                (((uint32_t)s.attrib_int[2] & 0x00FF0000u) >> 16u)
            };
            blend_type::process(s.frame_addr, color);
            depth_type::process_write(s.depth_addr, s.depth);
        }

        s.depth += s.gdx[0];
        s.attrib_int[0] += s.attrib_int_dx[0];
        s.attrib_int[1] += s.attrib_int_dx[1];
        s.attrib_int[2] += s.attrib_int_dx[2];
        s.attrib_int[3] += s.attrib_int_dx[3];

        s.depth_addr++;
        s.frame_addr++;
    }
};

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_vertex_shade_vertex : public abstract_raster
{
    raster_vertex_shade_vertex(const config* c)
    {
        back_cull = c->back_cull;

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
    }

    // abstract_raster

    bool back_cull;

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp_setup_face(pv, vertex_count, back_cull,
            is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        span_process_algo(y, x0, x1, this);
    }

    // raster

    gradient g[9];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;

    struct span_data
    {
        float gdx[9];

        float attrib[9];
        float depth;

        float* depth_addr;
        uint32_t* frame_addr;
        int32_t attrib_int_dx[7]; // 16.16
        int32_t attrib_int[7]; // 16.16
        int32_t attrib_int_next[7]; // 16.16
    };

    force_inline void setup_span(int32_t y, int32_t x0, span_data& s)
    {
        s.gdx[0] = g[0].dx;
        s.gdx[1] = g[1].dx;
        s.gdx[2] = g[2].dx;
        s.gdx[3] = g[3].dx;
        s.gdx[4] = g[4].dx;
        s.gdx[5] = g[5].dx;
        s.gdx[6] = g[6].dx;
        s.gdx[7] = g[7].dx;
        s.gdx[8] = g[8].dx;

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        s.attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        s.attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        s.attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        s.attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        s.attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        s.attrib[5] = g[5].dx * x0f + g[5].dy * y0f + g[5].d;
        s.attrib[6] = g[6].dx * x0f + g[6].dy * y0f + g[6].d;
        s.attrib[7] = g[7].dx * x0f + g[7].dy * y0f + g[7].d;
        s.attrib[8] = g[8].dx * x0f + g[8].dy * y0f + g[8].d;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int_next[0] = math::clamp((int32_t)(s.attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[1] = math::clamp((int32_t)(s.attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[2] = math::clamp((int32_t)(s.attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[3] = math::clamp((int32_t)(s.attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[4] = math::clamp((int32_t)(s.attrib[6] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[5] = math::clamp((int32_t)(s.attrib[7] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[6] = math::clamp((int32_t)(s.attrib[8] * w), (int32_t)0, (int32_t)0x00FFFFFF);

        int32_t start{ frame_stride * y + x0 };
        s.depth_addr = &depth_buffer[start];
        s.frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline static void setup_subspan(int32_t count, span_data& s)
    {
        float count_float{ (float)count };
        s.depth = s.attrib[0];
        s.attrib[0] += s.gdx[0] * count_float;
        s.attrib[1] += s.gdx[1] * count_float;
        s.attrib[2] += s.gdx[2] * count_float;
        s.attrib[3] += s.gdx[3] * count_float;
        s.attrib[4] += s.gdx[4] * count_float;
        s.attrib[5] += s.gdx[5] * count_float;
        s.attrib[6] += s.gdx[6] * count_float;
        s.attrib[7] += s.gdx[7] * count_float;
        s.attrib[8] += s.gdx[8] * count_float;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int[0] = s.attrib_int_next[0];
        s.attrib_int[1] = s.attrib_int_next[1];
        s.attrib_int[2] = s.attrib_int_next[2];
        s.attrib_int[3] = s.attrib_int_next[3];
        s.attrib_int[4] = s.attrib_int_next[4];
        s.attrib_int[5] = s.attrib_int_next[5];
        s.attrib_int[6] = s.attrib_int_next[6];
        s.attrib_int_next[0] = math::clamp((int32_t)(s.attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[1] = math::clamp((int32_t)(s.attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[2] = math::clamp((int32_t)(s.attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[3] = math::clamp((int32_t)(s.attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[4] = math::clamp((int32_t)(s.attrib[6] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[5] = math::clamp((int32_t)(s.attrib[7] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[6] = math::clamp((int32_t)(s.attrib[8] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        if (count == span_block_size)
        {
            s.attrib_int_dx[0] = (s.attrib_int_next[0] - s.attrib_int[0]) >> span_block_size_shift;
            s.attrib_int_dx[1] = (s.attrib_int_next[1] - s.attrib_int[1]) >> span_block_size_shift;
            s.attrib_int_dx[2] = (s.attrib_int_next[2] - s.attrib_int[2]) >> span_block_size_shift;
            s.attrib_int_dx[3] = (s.attrib_int_next[3] - s.attrib_int[3]) >> span_block_size_shift;
            s.attrib_int_dx[4] = (s.attrib_int_next[4] - s.attrib_int[4]) >> span_block_size_shift;
            s.attrib_int_dx[5] = (s.attrib_int_next[5] - s.attrib_int[5]) >> span_block_size_shift;
            s.attrib_int_dx[6] = (s.attrib_int_next[6] - s.attrib_int[6]) >> span_block_size_shift;
        }
        else
        {
            float scale{ subspan_scale[count] };
            s.attrib_int_dx[0] = (int32_t)((float)(s.attrib_int_next[0] - s.attrib_int[0]) * scale);
            s.attrib_int_dx[1] = (int32_t)((float)(s.attrib_int_next[1] - s.attrib_int[1]) * scale);
            s.attrib_int_dx[2] = (int32_t)((float)(s.attrib_int_next[2] - s.attrib_int[2]) * scale);
            s.attrib_int_dx[3] = (int32_t)((float)(s.attrib_int_next[3] - s.attrib_int[3]) * scale);
            s.attrib_int_dx[4] = (int32_t)((float)(s.attrib_int_next[4] - s.attrib_int[4]) * scale);
            s.attrib_int_dx[5] = (int32_t)((float)(s.attrib_int_next[5] - s.attrib_int[5]) * scale);
            s.attrib_int_dx[6] = (int32_t)((float)(s.attrib_int_next[6] - s.attrib_int[6]) * scale);
        }
    }

    force_inline static void fill(span_data& s)
    {
        if (depth_type::process_test(s.depth_addr, s.depth))
        {
            uint32_t color
            {
                (((((uint32_t)s.attrib_int[3] <<  8u)                                     ) & 0xFF000000u)       ) +
                (((((uint32_t)s.attrib_int[0] >> 16u) * ((uint32_t)s.attrib_int[4] >>  8u)) & 0x00FF0000u)       ) +
                (((((uint32_t)s.attrib_int[1] >> 16u) * ((uint32_t)s.attrib_int[5] >> 16u)) & 0x0000FF00u)       ) +
                (((((uint32_t)s.attrib_int[2] >>  8u) * ((uint32_t)s.attrib_int[6] >>  8u))              ) >> 24u)
            };
            blend_type::process(s.frame_addr, color);
            depth_type::process_write(s.depth_addr, s.depth);
        }

        s.depth += s.gdx[0];
        s.attrib_int[0] += s.attrib_int_dx[0];
        s.attrib_int[1] += s.attrib_int_dx[1];
        s.attrib_int[2] += s.attrib_int_dx[2];
        s.attrib_int[3] += s.attrib_int_dx[3];
        s.attrib_int[4] += s.attrib_int_dx[4];
        s.attrib_int[5] += s.attrib_int_dx[5];
        s.attrib_int[6] += s.attrib_int_dx[6];

        s.depth_addr++;
        s.frame_addr++;
    }
};

template<typename blend_type = blend_none, typename depth_type = depth_test_write>
struct raster_vertex_shade_lightmap : public abstract_raster
{
    raster_vertex_shade_lightmap(const config* c)
    {
        back_cull = c->back_cull;

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        umax = (c->lightmap_width - 1) << 16;
        vmax = (c->lightmap_height - 1) << 16;
        vshift = math::log2(c->lightmap_width);
        lightmap = (const uint32_t*)(c->lightmap);
    }

    // abstract_raster

    bool back_cull;

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp_setup_face(pv, vertex_count, back_cull,
            is_clockwise, g);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        span_process_algo(y, x0, x1, this);
    }

    // raster

    gradient g[8];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    int32_t umax;
    int32_t vmax;
    int32_t vshift;
    const uint32_t* lightmap;

    struct span_data
    {
        float gdx[8];

        int32_t umax;
        int32_t vmax;
        int32_t vshift;
        const uint32_t* lightmap;

        float attrib[8];
        float depth;

        float* depth_addr;
        uint32_t* frame_addr;
        int32_t attrib_int_dx[6]; // 16.16
        int32_t attrib_int[6]; // 16.16
        int32_t attrib_int_next[6]; // 16.16

        uint32_t shade_counter;
        uint32_t shade_trigger;
        uint32_t shade[3];
    };

    force_inline void setup_span(int32_t y, int32_t x0, span_data& s)
    {
        s.gdx[0] = g[0].dx;
        s.gdx[1] = g[1].dx;
        s.gdx[2] = g[2].dx;
        s.gdx[3] = g[3].dx;
        s.gdx[4] = g[4].dx;
        s.gdx[5] = g[5].dx;
        s.gdx[6] = g[6].dx;
        s.gdx[7] = g[7].dx;

        s.umax = umax;
        s.vmax = vmax;
        s.vshift = vshift;
        s.lightmap = lightmap;

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        s.attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        s.attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        s.attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        s.attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        s.attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        s.attrib[5] = g[5].dx * x0f + g[5].dy * y0f + g[5].d;
        s.attrib[6] = g[6].dx * x0f + g[6].dy * y0f + g[6].d;
        s.attrib[7] = g[7].dx * x0f + g[7].dy * y0f + g[7].d;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int_next[0] = math::clamp((int32_t)(s.attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[1] = math::clamp((int32_t)(s.attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[2] = math::clamp((int32_t)(s.attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[3] = math::clamp((int32_t)(s.attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[4] = math::clamp((int32_t)(s.attrib[6] * w), (int32_t)0, s.umax);
        s.attrib_int_next[5] = math::clamp((int32_t)(s.attrib[7] * w), (int32_t)0, s.vmax);

        int32_t start{ frame_stride * y + x0 };
        s.depth_addr = &depth_buffer[start];
        s.frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);

        s.shade_counter = ((y & 1 ? shade_hold >> 1u : 0u) + x0) & shade_mask;
        s.shade_trigger = 1;
    }

    force_inline static void setup_subspan(int32_t count, span_data& s)
    {
        float count_float{ (float)count };
        s.depth = s.attrib[0];
        s.attrib[0] += s.gdx[0] * count_float;
        s.attrib[1] += s.gdx[1] * count_float;
        s.attrib[2] += s.gdx[2] * count_float;
        s.attrib[3] += s.gdx[3] * count_float;
        s.attrib[4] += s.gdx[4] * count_float;
        s.attrib[5] += s.gdx[5] * count_float;
        s.attrib[6] += s.gdx[6] * count_float;
        s.attrib[7] += s.gdx[7] * count_float;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int[0] = s.attrib_int_next[0];
        s.attrib_int[1] = s.attrib_int_next[1];
        s.attrib_int[2] = s.attrib_int_next[2];
        s.attrib_int[3] = s.attrib_int_next[3];
        s.attrib_int[4] = s.attrib_int_next[4];
        s.attrib_int[5] = s.attrib_int_next[5];
        s.attrib_int_next[0] = math::clamp((int32_t)(s.attrib[2] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[1] = math::clamp((int32_t)(s.attrib[3] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[2] = math::clamp((int32_t)(s.attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[3] = math::clamp((int32_t)(s.attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[4] = math::clamp((int32_t)(s.attrib[6] * w), (int32_t)0, s.umax);
        s.attrib_int_next[5] = math::clamp((int32_t)(s.attrib[7] * w), (int32_t)0, s.vmax);
        if (count == span_block_size)
        {
            s.attrib_int_dx[0] = (s.attrib_int_next[0] - s.attrib_int[0]) >> span_block_size_shift;
            s.attrib_int_dx[1] = (s.attrib_int_next[1] - s.attrib_int[1]) >> span_block_size_shift;
            s.attrib_int_dx[2] = (s.attrib_int_next[2] - s.attrib_int[2]) >> span_block_size_shift;
            s.attrib_int_dx[3] = (s.attrib_int_next[3] - s.attrib_int[3]) >> span_block_size_shift;
            s.attrib_int_dx[4] = (s.attrib_int_next[4] - s.attrib_int[4]) >> span_block_size_shift;
            s.attrib_int_dx[5] = (s.attrib_int_next[5] - s.attrib_int[5]) >> span_block_size_shift;
        }
        else
        {
            float scale{ subspan_scale[count] };
            s.attrib_int_dx[0] = (int32_t)((float)(s.attrib_int_next[0] - s.attrib_int[0]) * scale);
            s.attrib_int_dx[1] = (int32_t)((float)(s.attrib_int_next[1] - s.attrib_int[1]) * scale);
            s.attrib_int_dx[2] = (int32_t)((float)(s.attrib_int_next[2] - s.attrib_int[2]) * scale);
            s.attrib_int_dx[3] = (int32_t)((float)(s.attrib_int_next[3] - s.attrib_int[3]) * scale);
            s.attrib_int_dx[4] = (int32_t)((float)(s.attrib_int_next[4] - s.attrib_int[4]) * scale);
            s.attrib_int_dx[5] = (int32_t)((float)(s.attrib_int_next[5] - s.attrib_int[5]) * scale);
        }
    }

    force_inline static void fill(span_data& s)
    {
        if (depth_type::process_test(s.depth_addr, s.depth))
        {
            if (((s.shade_counter & shade_mask) == 0) | s.shade_trigger)
            {
                s.shade_trigger = 0;
                uint32_t shade_color{ sample_lightmap(
                    s.attrib_int[4],
                    s.attrib_int[5],
                    s.vshift, s.lightmap) };
                s.shade[0] = (shade_color & 0x00FF0000u) >> 16u;
                s.shade[1] = (shade_color & 0x0000FF00u) >>  8u;
                s.shade[2] = (shade_color & 0x000000FFu)       ;
            }
            uint32_t color
            {
                ((((uint32_t)s.attrib_int[3]             ) & 0x00FF0000u) <<  8u) +
                ((((uint32_t)s.attrib_int[0] * s.shade[0]) & 0xFF000000u) >>  8u) +
                ((((uint32_t)s.attrib_int[1] * s.shade[1]) & 0xFF000000u) >> 16u) +
                ((((uint32_t)s.attrib_int[2] * s.shade[2])              ) >> 24u)
            };
            blend_type::process(s.frame_addr, color);
            depth_type::process_write(s.depth_addr, s.depth);
        }
        else
        {
            s.shade_trigger = 1;
        }

        s.depth += s.gdx[0];
        s.attrib_int[0] += s.attrib_int_dx[0];
        s.attrib_int[1] += s.attrib_int_dx[1];
        s.attrib_int[2] += s.attrib_int_dx[2];
        s.attrib_int[3] += s.attrib_int_dx[3];
        s.attrib_int[4] += s.attrib_int_dx[4];
        s.attrib_int[5] += s.attrib_int_dx[5];

        s.depth_addr++;
        s.frame_addr++;

        s.shade_counter++;
    }
};

//------------------------------------------------------------------------------

template<
    typename sample_type = sample_nearest,
    typename blend_type = blend_none,
    typename depth_type = depth_test_write,
    typename mask_type = mask_texture_off>
struct raster_texture_shade_none : public abstract_raster
{
    raster_texture_shade_none(const config* c)
    {
        back_cull = c->back_cull;

        texture_width = (float)c->texture_width;
        texture_height = (float)c->texture_height;

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        smask = (c->texture_width - 1) << 16;
        tmask = (c->texture_height - 1) << 16;
        tshift = 16 - math::log2(c->texture_width);
        texture_lut = (uint32_t*)(c->texture_lut);
        texture_data = c->texture_data;
    }

    // abstract_raster

    bool back_cull;

    float texture_width;
    float texture_height;

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        if (interp_setup_face(pv, vertex_count, back_cull,
            is_clockwise, g))
        {
            g[2].dx *= texture_width;
            g[2].dy *= texture_width;
            g[2].d *= texture_width;
            g[3].dx *= texture_height;
            g[3].dy *= texture_height;
            g[3].d *= texture_height;
            return true;
        }
        return false;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        span_process_algo(y, x0, x1, this);
    }

    // raster

    gradient g[4];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    int32_t smask;
    int32_t tmask;
    int32_t tshift;
    const uint32_t* texture_lut;
    const uint8_t* texture_data;

    struct span_data
    {
        float gdx[4];

        int32_t smask;
        int32_t tmask;
        int32_t tshift;
        const uint32_t* texture_lut;
        const uint8_t* texture_data;

        float attrib[4];
        float depth;

        float* depth_addr;
        uint32_t* frame_addr;
        int32_t attrib_int_dx[2]; // 16.16
        int32_t attrib_int[2]; // 16.16
        int32_t attrib_int_next[2]; // 16.16
    };

    force_inline void setup_span(int32_t y, int32_t x0, span_data& s)
    {
        s.gdx[0] = g[0].dx;
        s.gdx[1] = g[1].dx;
        s.gdx[2] = g[2].dx;
        s.gdx[3] = g[3].dx;

        s.smask = smask;
        s.tmask = tmask;
        s.tshift = tshift;
        s.texture_lut = texture_lut;
        s.texture_data = texture_data;

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        s.attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        s.attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        s.attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        s.attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int_next[0] = sample_type::process_coord((int32_t)(s.attrib[2] * w));
        s.attrib_int_next[1] = sample_type::process_coord((int32_t)(s.attrib[3] * w));

        int32_t start{ frame_stride * y + x0 };
        s.depth_addr = &depth_buffer[start];
        s.frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline static void setup_subspan(int32_t count, span_data& s)
    {
        float count_float{ (float)count };
        s.depth = s.attrib[0];
        s.attrib[0] += s.gdx[0] * count_float;
        s.attrib[1] += s.gdx[1] * count_float;
        s.attrib[2] += s.gdx[2] * count_float;
        s.attrib[3] += s.gdx[3] * count_float;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int[0] = s.attrib_int_next[0];
        s.attrib_int[1] = s.attrib_int_next[1];
        s.attrib_int_next[0] = sample_type::process_coord((int32_t)(s.attrib[2] * w));
        s.attrib_int_next[1] = sample_type::process_coord((int32_t)(s.attrib[3] * w));
        if (count == span_block_size)
        {
            s.attrib_int_dx[0] = (s.attrib_int_next[0] - s.attrib_int[0]) >> span_block_size_shift;
            s.attrib_int_dx[1] = (s.attrib_int_next[1] - s.attrib_int[1]) >> span_block_size_shift;
        }
        else
        {
            float scale{ subspan_scale[count] };
            s.attrib_int_dx[0] = (int32_t)((float)(s.attrib_int_next[0] - s.attrib_int[0]) * scale);
            s.attrib_int_dx[1] = (int32_t)((float)(s.attrib_int_next[1] - s.attrib_int[1]) * scale);
        }
    }

    force_inline static void fill(span_data& s)
    {
        if (depth_type::process_test(s.depth_addr, s.depth))
        {
            uint32_t color{ sample_type::process_texel(
                s.attrib_int[0],
                s.attrib_int[1],
                s.smask, s.tmask, s.tshift, s.texture_lut, s.texture_data) };
            if (mask_type::process(color))
            {
                blend_type::process(s.frame_addr, color);
                depth_type::process_write(s.depth_addr, s.depth);
            }
        }

        s.depth += s.gdx[0];
        s.attrib_int[0] += s.attrib_int_dx[0];
        s.attrib_int[1] += s.attrib_int_dx[1];

        s.depth_addr++;
        s.frame_addr++;
    }
};


template<
    typename sample_type = sample_nearest,
    typename blend_type = blend_none,
    typename depth_type = depth_test_write,
    typename mask_type = mask_texture_off>
struct raster_texture_shade_vertex : public abstract_raster
{
    raster_texture_shade_vertex(const config* c)
    {
        back_cull = c->back_cull;

        texture_width = (float)c->texture_width;
        texture_height = (float)c->texture_height;

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        smask = (c->texture_width - 1) << 16;
        tmask = (c->texture_height - 1) << 16;
        tshift = 16 - math::log2(c->texture_width);
        texture_lut = (uint32_t*)(c->texture_lut);
        texture_data = c->texture_data;
    }

    // abstract_raster

    bool back_cull;

    float texture_width;
    float texture_height;

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        if (interp_setup_face(pv, vertex_count, back_cull,
            is_clockwise, g))
        {
            g[2].dx *= texture_width;
            g[2].dy *= texture_width;
            g[2].d *= texture_width;
            g[3].dx *= texture_height;
            g[3].dy *= texture_height;
            g[3].d *= texture_height;
            return true;
        }
        return false;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        span_process_algo(y, x0, x1, this);
    }

    // raster

    gradient g[7];

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    int32_t smask;
    int32_t tmask;
    int32_t tshift;
    const uint32_t* texture_lut;
    const uint8_t* texture_data;

    struct span_data
    {
        float gdx[7];

        int32_t smask;
        int32_t tmask;
        int32_t tshift;
        const uint32_t* texture_lut;
        const uint8_t* texture_data;

        float attrib[7];
        float depth;

        float* depth_addr;
        uint32_t* frame_addr;
        int32_t attrib_int_dx[5]; // 16.16
        int32_t attrib_int[5]; // 16.16
        int32_t attrib_int_next[5]; // 16.16
    };

    force_inline void setup_span(int32_t y, int32_t x0, span_data& s)
    {
        s.gdx[0] = g[0].dx;
        s.gdx[1] = g[1].dx;
        s.gdx[2] = g[2].dx;
        s.gdx[3] = g[3].dx;
        s.gdx[4] = g[4].dx;
        s.gdx[5] = g[5].dx;
        s.gdx[6] = g[6].dx;

        s.smask = smask;
        s.tmask = tmask;
        s.tshift = tshift;
        s.texture_lut = texture_lut;
        s.texture_data = texture_data;

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        s.attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        s.attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        s.attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        s.attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        s.attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        s.attrib[5] = g[5].dx * x0f + g[5].dy * y0f + g[5].d;
        s.attrib[6] = g[6].dx * x0f + g[6].dy * y0f + g[6].d;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int_next[0] = sample_type::process_coord((int32_t)(s.attrib[2] * w));
        s.attrib_int_next[1] = sample_type::process_coord((int32_t)(s.attrib[3] * w));
        s.attrib_int_next[2] = math::clamp((int32_t)(s.attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[3] = math::clamp((int32_t)(s.attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[4] = math::clamp((int32_t)(s.attrib[6] * w), (int32_t)0, (int32_t)0x00FFFFFF);

        int32_t start{ frame_stride * y + x0 };
        s.depth_addr = &depth_buffer[start];
        s.frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline static void setup_subspan(int32_t count, span_data& s)
    {
        float count_float{ (float)count };
        s.depth = s.attrib[0];
        s.attrib[0] += s.gdx[0] * count_float;
        s.attrib[1] += s.gdx[1] * count_float;
        s.attrib[2] += s.gdx[2] * count_float;
        s.attrib[3] += s.gdx[3] * count_float;
        s.attrib[4] += s.gdx[4] * count_float;
        s.attrib[5] += s.gdx[5] * count_float;
        s.attrib[6] += s.gdx[6] * count_float;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int[0] = s.attrib_int_next[0];
        s.attrib_int[1] = s.attrib_int_next[1];
        s.attrib_int[2] = s.attrib_int_next[2];
        s.attrib_int[3] = s.attrib_int_next[3];
        s.attrib_int[4] = s.attrib_int_next[4];
        s.attrib_int_next[0] = sample_type::process_coord((int32_t)(s.attrib[2] * w));
        s.attrib_int_next[1] = sample_type::process_coord((int32_t)(s.attrib[3] * w));
        s.attrib_int_next[2] = math::clamp((int32_t)(s.attrib[4] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[3] = math::clamp((int32_t)(s.attrib[5] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        s.attrib_int_next[4] = math::clamp((int32_t)(s.attrib[6] * w), (int32_t)0, (int32_t)0x00FFFFFF);
        if (count == span_block_size)
        {
            s.attrib_int_dx[0] = (s.attrib_int_next[0] - s.attrib_int[0]) >> span_block_size_shift;
            s.attrib_int_dx[1] = (s.attrib_int_next[1] - s.attrib_int[1]) >> span_block_size_shift;
            s.attrib_int_dx[2] = (s.attrib_int_next[2] - s.attrib_int[2]) >> span_block_size_shift;
            s.attrib_int_dx[3] = (s.attrib_int_next[3] - s.attrib_int[3]) >> span_block_size_shift;
            s.attrib_int_dx[4] = (s.attrib_int_next[4] - s.attrib_int[4]) >> span_block_size_shift;
        }
        else
        {
            float scale{ subspan_scale[count] };
            s.attrib_int_dx[0] = (int32_t)((float)(s.attrib_int_next[0] - s.attrib_int[0]) * scale);
            s.attrib_int_dx[1] = (int32_t)((float)(s.attrib_int_next[1] - s.attrib_int[1]) * scale);
            s.attrib_int_dx[2] = (int32_t)((float)(s.attrib_int_next[2] - s.attrib_int[2]) * scale);
            s.attrib_int_dx[3] = (int32_t)((float)(s.attrib_int_next[3] - s.attrib_int[3]) * scale);
            s.attrib_int_dx[4] = (int32_t)((float)(s.attrib_int_next[4] - s.attrib_int[4]) * scale);
        }
    }

    force_inline static void fill(span_data& s)
    {
        if (depth_type::process_test(s.depth_addr, s.depth))
        {
            uint32_t texel{ sample_type::process_texel(
                s.attrib_int[0],
                s.attrib_int[1],
                s.smask, s.tmask, s.tshift, s.texture_lut, s.texture_data) };
            if (mask_type::process(texel))
            {
                uint32_t color
                {
                    (((((texel & 0xFF000000u)        )                            )              )       ) +
                    (((((texel & 0x00FF0000u) >> 16u ) * (uint32_t)s.attrib_int[2]) & 0xFF000000u) >>  8u) +
                    (((((texel & 0x0000FF00u) >>  8u ) * (uint32_t)s.attrib_int[3]) & 0xFF000000u) >> 16u) +
                    (((((texel & 0x000000FFu)        ) * (uint32_t)s.attrib_int[4])              ) >> 24u)
                };
                blend_type::process(s.frame_addr, color);
                depth_type::process_write(s.depth_addr, s.depth);
            }
        }

        s.depth += s.gdx[0];
        s.attrib_int[0] += s.attrib_int_dx[0];
        s.attrib_int[1] += s.attrib_int_dx[1];
        s.attrib_int[2] += s.attrib_int_dx[2];
        s.attrib_int[3] += s.attrib_int_dx[3];
        s.attrib_int[4] += s.attrib_int_dx[4];

        s.depth_addr++;
        s.frame_addr++;
    }
};

template<
    typename sample_type = sample_nearest,
    typename blend_type = blend_none,
    typename depth_type = depth_test_write,
    typename mask_type = mask_texture_off>
struct raster_texture_shade_lightmap : public abstract_raster
{
    raster_texture_shade_lightmap(const config* c)
    {
        back_cull = c->back_cull;

        texture_width = (float)c->texture_width;
        texture_height = (float)c->texture_height;

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        smask = (c->texture_width - 1) << 16;
        tmask = (c->texture_height - 1) << 16;
        tshift = 16 - math::log2(c->texture_width);
        texture_lut = (uint32_t*)(c->texture_lut);
        texture_data = c->texture_data;
        umax = (c->lightmap_width - 1) << 16;
        vmax = (c->lightmap_height - 1) << 16;
        vshift = math::log2(c->lightmap_width);
        lightmap = (const uint32_t*)(c->lightmap);
    }

    // abstract_raster

    bool back_cull;

    float texture_width;
    float texture_height;

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        if (interp_setup_face(pv, vertex_count, back_cull,
            is_clockwise, g))
        {
            g[2].dx *= texture_width;
            g[2].dy *= texture_width;
            g[2].d *= texture_width;
            g[3].dx *= texture_height;
            g[3].dy *= texture_height;
            g[3].d *= texture_height;
            return true;
        }
        return false;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        span_process_algo(y, x0, x1, this);
    }

    // raster

    gradient g[6];

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

    struct span_data
    {
        float gdx[6];

        int32_t smask;
        int32_t tmask;
        int32_t tshift;
        const uint32_t* texture_lut;
        const uint8_t* texture_data;
        int32_t umax;
        int32_t vmax;
        int32_t vshift;
        const uint32_t* lightmap;

        float attrib[6];
        float depth;

        float* depth_addr;
        uint32_t* frame_addr;
        int32_t attrib_int_dx[4]; // 16.16
        int32_t attrib_int[4]; // 16.16
        int32_t attrib_int_next[4]; // 16.16

        uint32_t shade_counter;
        uint32_t shade_trigger;
        uint32_t shade[3];
    };

    force_inline void setup_span(int32_t y, int32_t x0, span_data& s)
    {
        s.gdx[0] = g[0].dx;
        s.gdx[1] = g[1].dx;
        s.gdx[2] = g[2].dx;
        s.gdx[3] = g[3].dx;
        s.gdx[4] = g[4].dx;
        s.gdx[5] = g[5].dx;

        s.smask = smask;
        s.tmask = tmask;
        s.tshift = tshift;
        s.texture_lut = texture_lut;
        s.texture_data = texture_data;
        s.umax = umax;
        s.vmax = vmax;
        s.vshift = vshift;
        s.lightmap = lightmap;

        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        s.attrib[0] = g[0].dx * x0f + g[0].dy * y0f + g[0].d;
        s.attrib[1] = g[1].dx * x0f + g[1].dy * y0f + g[1].d;
        s.attrib[2] = g[2].dx * x0f + g[2].dy * y0f + g[2].d;
        s.attrib[3] = g[3].dx * x0f + g[3].dy * y0f + g[3].d;
        s.attrib[4] = g[4].dx * x0f + g[4].dy * y0f + g[4].d;
        s.attrib[5] = g[5].dx * x0f + g[5].dy * y0f + g[5].d;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int_next[0] = sample_type::process_coord((int32_t)(s.attrib[2] * w));
        s.attrib_int_next[1] = sample_type::process_coord((int32_t)(s.attrib[3] * w));
        s.attrib_int_next[2] = math::clamp((int32_t)(s.attrib[4] * w), (int32_t)0, s.umax);
        s.attrib_int_next[3] = math::clamp((int32_t)(s.attrib[5] * w), (int32_t)0, s.vmax);

        int32_t start{ frame_stride * y + x0 };
        s.depth_addr = &depth_buffer[start];
        s.frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);

        s.shade_counter = ((y & 1 ? shade_hold >> 1u : 0u) + x0) & shade_mask;
        s.shade_trigger = 1;
    }

    force_inline static void setup_subspan(int32_t count, span_data& s)
    {
        float count_float{ (float)count };
        s.depth = s.attrib[0];
        s.attrib[0] += s.gdx[0] * count_float;
        s.attrib[1] += s.gdx[1] * count_float;
        s.attrib[2] += s.gdx[2] * count_float;
        s.attrib[3] += s.gdx[3] * count_float;
        s.attrib[4] += s.gdx[4] * count_float;
        s.attrib[5] += s.gdx[5] * count_float;
        float w{ (float)0x10000 / s.attrib[1] };
        s.attrib_int[0] = s.attrib_int_next[0];
        s.attrib_int[1] = s.attrib_int_next[1];
        s.attrib_int[2] = s.attrib_int_next[2];
        s.attrib_int[3] = s.attrib_int_next[3];
        s.attrib_int_next[0] = sample_type::process_coord((int32_t)(s.attrib[2] * w));
        s.attrib_int_next[1] = sample_type::process_coord((int32_t)(s.attrib[3] * w));
        s.attrib_int_next[2] = math::clamp((int32_t)(s.attrib[4] * w), (int32_t)0, s.umax);
        s.attrib_int_next[3] = math::clamp((int32_t)(s.attrib[5] * w), (int32_t)0, s.vmax);
        if (count == span_block_size)
        {
            s.attrib_int_dx[0] = (s.attrib_int_next[0] - s.attrib_int[0]) >> span_block_size_shift;
            s.attrib_int_dx[1] = (s.attrib_int_next[1] - s.attrib_int[1]) >> span_block_size_shift;
            s.attrib_int_dx[2] = (s.attrib_int_next[2] - s.attrib_int[2]) >> span_block_size_shift;
            s.attrib_int_dx[3] = (s.attrib_int_next[3] - s.attrib_int[3]) >> span_block_size_shift;
        }
        else
        {
            float scale{ subspan_scale[count] };
            s.attrib_int_dx[0] = (int32_t)((float)(s.attrib_int_next[0] - s.attrib_int[0]) * scale);
            s.attrib_int_dx[1] = (int32_t)((float)(s.attrib_int_next[1] - s.attrib_int[1]) * scale);
            s.attrib_int_dx[2] = (int32_t)((float)(s.attrib_int_next[2] - s.attrib_int[2]) * scale);
            s.attrib_int_dx[3] = (int32_t)((float)(s.attrib_int_next[3] - s.attrib_int[3]) * scale);
        }
    }

    force_inline static void fill(span_data& s)
    {
        if (depth_type::process_test(s.depth_addr, s.depth))
        {
            uint32_t texel{ sample_type::process_texel(
                s.attrib_int[0],
                s.attrib_int[1],
                s.smask, s.tmask, s.tshift, s.texture_lut, s.texture_data) };
            if (mask_type::process(texel))
            {
                if (((s.shade_counter & shade_mask) == 0) | s.shade_trigger)
                {
                    s.shade_trigger = 0;
                    uint32_t shade_color{ sample_lightmap(
                        s.attrib_int[2],
                        s.attrib_int[3],
                        s.vshift, s.lightmap) };
                    s.shade[0] = (shade_color & 0x00FF0000u) >> 16u;
                    s.shade[1] = (shade_color & 0x0000FF00u) >>  8u;
                    s.shade[2] = (shade_color & 0x000000FFu)       ;
                }
                uint32_t color
                {
                    ((((texel & 0xFF000000u)             )              )      ) +
                    ((((texel & 0x00FF0000u) * s.shade[0]) & 0xFF000000u) >> 8u) +
                    ((((texel & 0x0000FF00u) * s.shade[1]) & 0x00FF0000u) >> 8u) +
                    ((((texel & 0x000000FFu) * s.shade[2])              ) >> 8u)
                };
                blend_type::process(s.frame_addr, color);
                depth_type::process_write(s.depth_addr, s.depth);
            }
        }
        else
        {
            s.shade_trigger = 1;
        }

        s.depth += s.gdx[0];
        s.attrib_int[0] += s.attrib_int_dx[0];
        s.attrib_int[1] += s.attrib_int_dx[1];
        s.attrib_int[2] += s.attrib_int_dx[2];
        s.attrib_int[3] += s.attrib_int_dx[3];

        s.depth_addr++;
        s.frame_addr++;

        s.shade_counter++;
    }
};

//------------------------------------------------------------------------------

void scan_faces(const config* c)
{
    abstract_raster* r{};

    //------------------------------------//

    union raster_pool
    {
        raster_depth r0;
        raster_solid_shade_none<> r1;
        raster_solid_shade_vertex<> r2;
        raster_solid_shade_lightmap<> r3;
        raster_vertex_shade_none<> r4;
        raster_vertex_shade_vertex<> r5;
        raster_vertex_shade_lightmap<> r6;
        raster_texture_shade_none<> r7;
        raster_texture_shade_vertex<> r8;
        raster_texture_shade_lightmap<> r9;
    };
    alignas(alignof(raster_pool)) uint8_t raster[sizeof(raster_pool)];

    switch (c->flags & FILL_BIT_MASK)
    {
    case FILL_DEPTH:
        r = new (raster) raster_depth(c);
        break;
    case FILL_OUTLINE:
        r = new (raster) raster_outline(c);
        break;
    case FILL_SOLID:
        switch (c->flags & (SHADE_BIT_MASK | BLEND_BIT_MASK))
        {
        case (SHADE_NONE | BLEND_NONE):
            r = new (raster) raster_solid_shade_none(c);
            break;
        case (SHADE_NONE | BLEND_ADD):
            r = new (raster) raster_solid_shade_none<blend_add, depth_test>(c);
            break;
        case (SHADE_NONE | BLEND_MUL):
            r = new (raster) raster_solid_shade_none<blend_mul, depth_test>(c);
            break;
        case (SHADE_NONE | BLEND_ALPHA):
            r = new (raster) raster_solid_shade_none<blend_alpha, depth_test>(c);
            break;
        case (SHADE_VERTEX | BLEND_NONE):
            r = new (raster) raster_solid_shade_vertex(c);
            break;
        case (SHADE_VERTEX | BLEND_ADD):
            r = new (raster) raster_solid_shade_vertex<blend_add, depth_test>(c);
            break;
        case (SHADE_VERTEX | BLEND_MUL):
            r = new (raster) raster_solid_shade_vertex<blend_mul, depth_test>(c);
            break;
        case (SHADE_VERTEX | BLEND_ALPHA):
            r = new (raster) raster_solid_shade_vertex<blend_alpha, depth_test>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_NONE):
            r = new (raster) raster_solid_shade_lightmap(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_ADD):
            r = new (raster) raster_solid_shade_lightmap<blend_add, depth_test>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_MUL):
            r = new (raster) raster_solid_shade_lightmap<blend_mul, depth_test>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_ALPHA):
            r = new (raster) raster_solid_shade_lightmap<blend_alpha, depth_test>(c);
            break;
        }
        break;
    case FILL_VERTEX:
        switch (c->flags & (SHADE_BIT_MASK | BLEND_BIT_MASK))
        {
        case (SHADE_NONE | BLEND_NONE):
            r = new (raster) raster_vertex_shade_none(c);
            break;
        case (SHADE_NONE | BLEND_ADD):
            r = new (raster) raster_vertex_shade_none<blend_add, depth_test>(c);
            break;
        case (SHADE_NONE | BLEND_MUL):
            r = new (raster) raster_vertex_shade_none<blend_mul, depth_test>(c);
            break;
        case (SHADE_NONE | BLEND_ALPHA):
            r = new (raster) raster_vertex_shade_none<blend_alpha, depth_test>(c);
            break;
        case (SHADE_VERTEX | BLEND_NONE):
            r = new (raster) raster_vertex_shade_vertex(c);
            break;
        case (SHADE_VERTEX | BLEND_ADD):
            r = new (raster) raster_vertex_shade_vertex<blend_add, depth_test>(c);
            break;
        case (SHADE_VERTEX | BLEND_MUL):
            r = new (raster) raster_vertex_shade_vertex<blend_mul, depth_test>(c);
            break;
        case (SHADE_VERTEX | BLEND_ALPHA):
            r = new (raster) raster_vertex_shade_vertex<blend_alpha, depth_test>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_NONE):
            r = new (raster) raster_vertex_shade_lightmap(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_ADD):
            r = new (raster) raster_vertex_shade_lightmap<blend_add, depth_test>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_MUL):
            r = new (raster) raster_vertex_shade_lightmap<blend_mul, depth_test>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_ALPHA):
            r = new (raster) raster_vertex_shade_lightmap<blend_alpha, depth_test>(c);
            break;
        }
        break;
    case FILL_TEXTURE:
        switch (c->flags & (SHADE_BIT_MASK | BLEND_BIT_MASK | FILTER_BIT_MASK))
        {
        case (SHADE_NONE | BLEND_NONE | FILTER_NONE):
            r = new (raster) raster_texture_shade_none<sample_nearest>(c);
            break;
        case (SHADE_NONE | BLEND_MASK | FILTER_NONE):
            r = new (raster) raster_texture_shade_none<sample_nearest, blend_none, depth_test_write, mask_texture_on>(c);
            break;
        case (SHADE_NONE | BLEND_ADD | FILTER_NONE):
            r = new (raster) raster_texture_shade_none<sample_nearest, blend_add, depth_test>(c);
            break;
        case (SHADE_NONE | BLEND_MUL | FILTER_NONE):
            r = new (raster) raster_texture_shade_none<sample_nearest, blend_mul, depth_test>(c);
            break;
        case (SHADE_NONE | BLEND_ALPHA | FILTER_NONE):
            r = new (raster) raster_texture_shade_none<sample_nearest, blend_alpha, depth_test>(c);
            break;
        case (SHADE_VERTEX | BLEND_NONE | FILTER_NONE):
            r = new (raster) raster_texture_shade_vertex<sample_nearest>(c);
            break;
        case (SHADE_VERTEX | BLEND_MASK | FILTER_NONE):
            r = new (raster) raster_texture_shade_vertex<sample_nearest, blend_none, depth_test_write, mask_texture_on>(c);
            break;
        case (SHADE_VERTEX | BLEND_ADD | FILTER_NONE):
            r = new (raster) raster_texture_shade_vertex<sample_nearest, blend_add, depth_test>(c);
            break;
        case (SHADE_VERTEX | BLEND_MUL | FILTER_NONE):
            r = new (raster) raster_texture_shade_vertex<sample_nearest, blend_mul, depth_test>(c);
            break;
        case (SHADE_VERTEX | BLEND_ALPHA | FILTER_NONE):
            r = new (raster) raster_texture_shade_vertex<sample_nearest, blend_alpha, depth_test>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_NONE | FILTER_NONE):
            r = new (raster) raster_texture_shade_lightmap<sample_nearest>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_MASK | FILTER_NONE):
            r = new (raster) raster_texture_shade_lightmap<sample_nearest, blend_none, depth_test_write, mask_texture_on>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_ADD | FILTER_NONE):
            r = new (raster) raster_texture_shade_lightmap<sample_nearest, blend_add, depth_test>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_MUL | FILTER_NONE):
            r = new (raster) raster_texture_shade_lightmap<sample_nearest, blend_mul, depth_test>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_ALPHA | FILTER_NONE):
            r = new (raster) raster_texture_shade_lightmap<sample_nearest, blend_alpha, depth_test>(c);
            break;
        case (SHADE_NONE | BLEND_NONE | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_none<sample_bilinear>(c);
            break;
        case (SHADE_NONE | BLEND_MASK | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_none<sample_bilinear, blend_none, depth_test_write, mask_texture_on>(c);
            break;
        case (SHADE_NONE | BLEND_ADD | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_none<sample_bilinear, blend_add, depth_test>(c);
            break;
        case (SHADE_NONE | BLEND_MUL | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_none<sample_bilinear, blend_mul, depth_test>(c);
            break;
        case (SHADE_NONE | BLEND_ALPHA | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_none<sample_bilinear, blend_alpha, depth_test>(c);
            break;
        case (SHADE_VERTEX | BLEND_NONE | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_vertex<sample_bilinear>(c);
            break;
        case (SHADE_VERTEX | BLEND_MASK | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_vertex<sample_bilinear, blend_none, depth_test_write, mask_texture_on>(c);
            break;
        case (SHADE_VERTEX | BLEND_ADD | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_vertex<sample_bilinear, blend_add, depth_test>(c);
            break;
        case (SHADE_VERTEX | BLEND_MUL | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_vertex<sample_bilinear, blend_mul, depth_test>(c);
            break;
        case (SHADE_VERTEX | BLEND_ALPHA | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_vertex<sample_bilinear, blend_alpha, depth_test>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_NONE | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_lightmap<sample_bilinear>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_MASK | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_lightmap<sample_bilinear, blend_none, depth_test_write, mask_texture_on>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_ADD | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_lightmap<sample_bilinear, blend_add, depth_test>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_MUL | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_lightmap<sample_bilinear, blend_mul, depth_test>(c);
            break;
        case (SHADE_LIGHTMAP | BLEND_ALPHA | FILTER_LINEAR):
            r = new (raster) raster_texture_shade_lightmap<sample_bilinear, blend_alpha, depth_test>(c);
            break;
        }
        break;
    }

    //------------------------------------//

    if (r == nullptr)
        return;

    uint32_t num_faces{ c->num_faces };
    const uint32_t* num_vertices{ c->vertex_count_data };
    const float* vertex_data{ c->vertex_data };
    uint32_t vertex_stride{ c->vertex_stride };

    while (num_faces--)
    {
        uint32_t vertex_count{ *num_vertices++ };

        const float* pv[num_max_vertices];
        for (uint32_t nv{}; nv < vertex_count; ++nv)
        {
            pv[nv] = vertex_data;
            vertex_data += vertex_stride;
        }

        if (r->setup_face(pv, vertex_count))
            scan_face(pv, vertex_count, r);
    }
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

} // namespace lib3d::raster
