#include "raster.hpp"
#include "raster_interp.hpp"
#include "raster_fill.hpp"
//#include <cassert>

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
force_inline void process_span_algo(raster_type& raster, int32_t y, int32_t x0, int32_t x1)
{
    //assert(x0 < x1);
    raster.setup_span(y, x0);
    int32_t n{ x1 - x0 };
    while (n)
    {
        int32_t c{ math::min(n, span_block_size) };
        n -= c;
        raster.setup_subspan(c);
        while (c--)
            raster.fill();
    }
}

//------------------------------------------------------------------------------

struct raster_depth : public abstract_raster
{
    raster_depth(const config* c)
    {
        interp.setup(c);

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
    }

    // abstract_raster

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp.setup_face(pv, vertex_count, is_clockwise);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // raster

    interp<1> interp;

    int32_t frame_stride;
    float* depth_buffer;

    float attrib;
    float depth;

    float* depth_addr;

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        attrib = x0f * interp.g[0].dx + y0f * interp.g[0].dy + interp.g[0].d;

        depth_addr = &depth_buffer[frame_stride * y + x0];
    }

    force_inline void setup_subspan(int32_t count)
    {
        depth = attrib;
        attrib += interp.g[0].dx * (float)count;
    }

    force_inline void fill()
    {
        *depth_addr = math::min(*depth_addr, depth);

        depth += interp.g[0].dx;

        depth_addr++;
    }
};

//------------------------------------------------------------------------------

template<typename blend_type>
struct raster_solid_shade_none : public abstract_raster
{
    raster_solid_shade_none(const config* c)
    {
        interp.setup(c);

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        fill_color = reinterpret_cast<const uint32_t&>(c->fill_color);
    }

    // abstract_raster

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp.setup_face(pv, vertex_count, is_clockwise);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // raster

    interp<1> interp;

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    uint32_t fill_color;

    float attrib;
    float depth;

    float* depth_addr;
    uint32_t* frame_addr;

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        attrib = x0f * interp.g[0].dx + y0f * interp.g[0].dy + interp.g[0].d;

        int32_t start{ frame_stride * y + x0 };
        depth_addr = &depth_buffer[start];
        frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline void setup_subspan(int32_t count)
    {
        depth = attrib;
        attrib += interp.g[0].dx * (float)count;
    }

    force_inline void fill()
    {
        if (*depth_addr > depth)
        {
            *frame_addr = blend_type::process(*frame_addr, fill_color);
            *depth_addr = depth;
        }

        depth += interp.g[0].dx;

        depth_addr++;
        frame_addr++;
    }
};

template<typename blend_type>
struct raster_solid_shade_vertex : public abstract_raster
{
    raster_solid_shade_vertex(const config* c)
    {
        interp.setup(c);

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        fill_color[0] = (uint32_t)c->fill_color.a << 24u;
        fill_color[1] = (uint32_t)c->fill_color.r;
        fill_color[2] = (uint32_t)c->fill_color.g;
        fill_color[3] = (uint32_t)c->fill_color.b;
    }

    // abstract_raster

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp.setup_face(pv, vertex_count, is_clockwise);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // raster

    interp<5> interp;

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    uint32_t fill_color[4];

    float attrib[5];
    float depth;

    float* depth_addr;
    uint32_t* frame_addr;
    int32_t attrib_int_dx[3]; // 16.16
    int32_t attrib_int[3]; // 16.16
    int32_t attrib_int_next[3]; // 16.16

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        attrib[0] = interp.g[0].dx * x0f + interp.g[0].dy * y0f + interp.g[0].d;
        attrib[1] = interp.g[1].dx * x0f + interp.g[1].dy * y0f + interp.g[1].d;
        attrib[2] = interp.g[2].dx * x0f + interp.g[2].dy * y0f + interp.g[2].d;
        attrib[3] = interp.g[3].dx * x0f + interp.g[3].dy * y0f + interp.g[3].d;
        attrib[4] = interp.g[4].dx * x0f + interp.g[4].dy * y0f + interp.g[4].d;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int_next[0] = math::min((int32_t)(attrib[2] * w), 0x00FFFFFF); // saturate
        attrib_int_next[1] = math::min((int32_t)(attrib[3] * w), 0x00FFFFFF); // saturate
        attrib_int_next[2] = math::min((int32_t)(attrib[4] * w), 0x00FFFFFF); // saturate

        int32_t start{ frame_stride * y + x0 };
        depth_addr = &depth_buffer[start];
        frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline void setup_subspan(int32_t count)
    {
        float count_float{ (float)count };
        depth = attrib[0];
        attrib[0] += interp.g[0].dx * count_float;
        attrib[1] += interp.g[1].dx * count_float;
        attrib[2] += interp.g[2].dx * count_float;
        attrib[3] += interp.g[3].dx * count_float;
        attrib[4] += interp.g[4].dx * count_float;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int[0] = attrib_int_next[0];
        attrib_int[1] = attrib_int_next[1];
        attrib_int[2] = attrib_int_next[2];
        attrib_int_next[0] = math::min((int32_t)(attrib[2] * w), 0x00FFFFFF); // saturate
        attrib_int_next[1] = math::min((int32_t)(attrib[3] * w), 0x00FFFFFF); // saturate
        attrib_int_next[2] = math::min((int32_t)(attrib[4] * w), 0x00FFFFFF); // saturate
        if (count == span_block_size)
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) >> span_block_size_shift;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) >> span_block_size_shift;
            attrib_int_dx[2] = (attrib_int_next[2] - attrib_int[2]) >> span_block_size_shift;
        }
        else
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) / count;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) / count;
            attrib_int_dx[2] = (attrib_int_next[2] - attrib_int[2]) / count;
        }
    }

    force_inline void fill()
    {
        if (*depth_addr > depth)
        {
            uint32_t color
            {
                (((fill_color[0]                          )              )       ) +
                (((fill_color[1] * (uint32_t)attrib_int[0]) & 0xFF000000u) >>  8u) +
                (((fill_color[2] * (uint32_t)attrib_int[1]) & 0xFF000000u) >> 16u) +
                (((fill_color[3] * (uint32_t)attrib_int[2])              ) >> 24u)
            };
            *frame_addr = blend_type::process(*frame_addr, color);
            *depth_addr = depth;
        }

        depth += interp.g[0].dx;
        attrib_int[0] += attrib_int_dx[0];
        attrib_int[1] += attrib_int_dx[1];
        attrib_int[2] += attrib_int_dx[2];

        depth_addr++;
        frame_addr++;
    }
};

template<typename blend_type>
struct raster_solid_shade_lightmap : public abstract_raster
{
    raster_solid_shade_lightmap(const config* c)
    {
        interp.setup(c);

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        fill_color[0] = (uint32_t)c->fill_color.a << 24u;
        fill_color[1] = (uint32_t)c->fill_color.r;
        fill_color[2] = (uint32_t)c->fill_color.g;
        fill_color[3] = (uint32_t)c->fill_color.b;
        umax = (c->lightmap_width - 1) << 16;
        vmax = (c->lightmap_height - 1) << 16;
        vshift = math::log2(c->lightmap_width);
        lightmap = (uint32_t*)(c->lightmap);
    }

    // abstract_raster

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp.setup_face(pv, vertex_count, is_clockwise);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // raster

    interp<4> interp;

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    uint32_t fill_color[4];
    int32_t umax;
    int32_t vmax;
    int32_t vshift;
    uint32_t* lightmap;

    float attrib[4];
    float depth;

    float* depth_addr;
    uint32_t* frame_addr;
    int32_t shade_int_dx[3]; // 16.16
    int32_t shade_int[3]; // 16.16
    int32_t shade_int_next[3]; // 16.16

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        attrib[0] = interp.g[0].dx * x0f + interp.g[0].dy * y0f + interp.g[0].d;
        attrib[1] = interp.g[1].dx * x0f + interp.g[1].dy * y0f + interp.g[1].d;
        attrib[2] = interp.g[2].dx * x0f + interp.g[2].dy * y0f + interp.g[2].d;
        attrib[3] = interp.g[3].dx * x0f + interp.g[3].dy * y0f + interp.g[3].d;
        float w{ (float)0x10000 / attrib[1] };
        int32_t attrib_shade_int[2]
        {
            math::clamp((int32_t)(attrib[2] * w), 0, umax), // clamp
            math::clamp((int32_t)(attrib[3] * w), 0, vmax), // clamp
        };
        uint32_t shade{ sample_bilinear(attrib_shade_int[0], attrib_shade_int[1], vshift, lightmap) };
        shade_int_next[0] = (shade & 0x00FF0000u)       ;
        shade_int_next[1] = (shade & 0x0000FF00u) <<  8u;
        shade_int_next[2] = (shade & 0x000000FFu) << 16u;

        int32_t start{ frame_stride * y + x0 };
        depth_addr = &depth_buffer[start];
        frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline void setup_subspan(int32_t count)
    {
        float count_float{ (float)count };
        depth = attrib[0];
        attrib[0] += interp.g[0].dx * count_float;
        attrib[1] += interp.g[1].dx * count_float;
        attrib[2] += interp.g[2].dx * count_float;
        attrib[3] += interp.g[3].dx * count_float;
        float w{ (float)0x10000 / attrib[1] };
        int32_t attrib_int[2]
        {
            math::clamp((int32_t)(attrib[2] * w), 0, umax), // clamp
            math::clamp((int32_t)(attrib[3] * w), 0, vmax), // clamp
        };
        shade_int[0] = shade_int_next[0];
        shade_int[1] = shade_int_next[1];
        shade_int[2] = shade_int_next[2];
        uint32_t shade{ sample_bilinear(attrib_int[0], attrib_int[1], vshift, lightmap) };
        shade_int_next[0] = (shade & 0x00FF0000u)       ;
        shade_int_next[1] = (shade & 0x0000FF00u) <<  8u;
        shade_int_next[2] = (shade & 0x000000FFu) << 16u;
        if (count == span_block_size)
        {
            shade_int_dx[0] = (shade_int_next[0] - shade_int[0]) >> span_block_size_shift;
            shade_int_dx[1] = (shade_int_next[1] - shade_int[1]) >> span_block_size_shift;
            shade_int_dx[2] = (shade_int_next[2] - shade_int[2]) >> span_block_size_shift;
        }
        else
        {
            shade_int_dx[0] = (shade_int_next[0] - shade_int[0]) / count;
            shade_int_dx[1] = (shade_int_next[1] - shade_int[1]) / count;
            shade_int_dx[2] = (shade_int_next[2] - shade_int[2]) / count;
        }
    }

    force_inline void fill()
    {
        if (*depth_addr > depth)
        {
            uint32_t color
            {
                (((fill_color[0]                         )              )       ) +
                (((fill_color[1] * (uint32_t)shade_int[0]) & 0xFF000000u) >>  8u) +
                (((fill_color[2] * (uint32_t)shade_int[1]) & 0xFF000000u) >> 16u) +
                (((fill_color[3] * (uint32_t)shade_int[2])              ) >> 24u)
            };
            *frame_addr = blend_type::process(*frame_addr, color);
            *depth_addr = depth;
        }

        depth += interp.g[0].dx;
        shade_int[0] += shade_int_dx[0];
        shade_int[1] += shade_int_dx[1];
        shade_int[2] += shade_int_dx[2];

        depth_addr++;
        frame_addr++;
    }
};

//------------------------------------------------------------------------------

template<typename blend_type>
struct raster_vertex_shade_none : public abstract_raster
{
    raster_vertex_shade_none(const config* c)
    {
        interp.setup(c);

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
    }

    // abstract_raster

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp.setup_face(pv, vertex_count, is_clockwise);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // raster

    interp<6> interp;

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;

    float attrib[6];
    float depth;

    float* depth_addr;
    uint32_t* frame_addr;
    int32_t attrib_int_dx[4]; // 16.16
    int32_t attrib_int[4]; // 16.16
    int32_t attrib_int_next[4]; // 16.16

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        attrib[0] = interp.g[0].dx * x0f + interp.g[0].dy * y0f + interp.g[0].d;
        attrib[1] = interp.g[1].dx * x0f + interp.g[1].dy * y0f + interp.g[1].d;
        attrib[2] = interp.g[2].dx * x0f + interp.g[2].dy * y0f + interp.g[2].d;
        attrib[3] = interp.g[3].dx * x0f + interp.g[3].dy * y0f + interp.g[3].d;
        attrib[4] = interp.g[4].dx * x0f + interp.g[4].dy * y0f + interp.g[4].d;
        attrib[5] = interp.g[5].dx * x0f + interp.g[5].dy * y0f + interp.g[5].d;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int_next[0] = math::min((int32_t)(attrib[2] * w), 0x00FFFFFF); // saturate
        attrib_int_next[1] = math::min((int32_t)(attrib[3] * w), 0x00FFFFFF); // saturate
        attrib_int_next[2] = math::min((int32_t)(attrib[4] * w), 0x00FFFFFF); // saturate
        attrib_int_next[3] = math::min((int32_t)(attrib[5] * w), 0x00FFFFFF); // saturate

        int32_t start{ frame_stride * y + x0 };
        depth_addr = &depth_buffer[start];
        frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline void setup_subspan(int32_t count)
    {
        float count_float{ (float)count };
        depth = attrib[0];
        attrib[0] += interp.g[0].dx * count_float;
        attrib[1] += interp.g[1].dx * count_float;
        attrib[2] += interp.g[2].dx * count_float;
        attrib[3] += interp.g[3].dx * count_float;
        attrib[4] += interp.g[4].dx * count_float;
        attrib[5] += interp.g[5].dx * count_float;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int[0] = attrib_int_next[0];
        attrib_int[1] = attrib_int_next[1];
        attrib_int[2] = attrib_int_next[2];
        attrib_int[3] = attrib_int_next[3];
        attrib_int_next[0] = math::min((int32_t)(attrib[2] * w), 0x00FFFFFF); // saturate
        attrib_int_next[1] = math::min((int32_t)(attrib[3] * w), 0x00FFFFFF); // saturate
        attrib_int_next[2] = math::min((int32_t)(attrib[4] * w), 0x00FFFFFF); // saturate
        attrib_int_next[3] = math::min((int32_t)(attrib[5] * w), 0x00FFFFFF); // saturate
        if (count == span_block_size)
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) >> span_block_size_shift;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) >> span_block_size_shift;
            attrib_int_dx[2] = (attrib_int_next[2] - attrib_int[2]) >> span_block_size_shift;
            attrib_int_dx[3] = (attrib_int_next[3] - attrib_int[3]) >> span_block_size_shift;
        }
        else
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) / count;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) / count;
            attrib_int_dx[2] = (attrib_int_next[2] - attrib_int[2]) / count;
            attrib_int_dx[3] = (attrib_int_next[3] - attrib_int[3]) / count;
        }
    }

    force_inline void fill()
    {
        if (*depth_addr > depth)
        {
            uint32_t color
            {
                (((uint32_t)attrib_int[3] & 0x00FF0000u) <<  8u) +
                (((uint32_t)attrib_int[0] & 0x00FF0000u)       ) +
                (((uint32_t)attrib_int[1] & 0x00FF0000u) >>  8u) +
                (((uint32_t)attrib_int[2] & 0x00FF0000u) >> 16u)
            };
            *frame_addr = blend_type::process(*frame_addr, color);
            *depth_addr = depth;
        }

        depth += interp.g[0].dx;
        attrib_int[0] += attrib_int_dx[0];
        attrib_int[1] += attrib_int_dx[1];
        attrib_int[2] += attrib_int_dx[2];
        attrib_int[3] += attrib_int_dx[3];

        depth_addr++;
        frame_addr++;
    }
};

template<typename blend_type>
struct raster_vertex_shade_vertex : public abstract_raster
{
    raster_vertex_shade_vertex(const config* c)
    {
        interp.setup(c);

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
    }

    // abstract_raster

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp.setup_face(pv, vertex_count, is_clockwise);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // raster

    interp<9> interp;

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;

    float attrib[9];
    float depth;

    float* depth_addr;
    uint32_t* frame_addr;
    int32_t attrib_int_dx[7]; // 16.16
    int32_t attrib_int[7]; // 16.16
    int32_t attrib_int_next[7]; // 16.16

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        attrib[0] = interp.g[0].dx * x0f + interp.g[0].dy * y0f + interp.g[0].d;
        attrib[1] = interp.g[1].dx * x0f + interp.g[1].dy * y0f + interp.g[1].d;
        attrib[2] = interp.g[2].dx * x0f + interp.g[2].dy * y0f + interp.g[2].d;
        attrib[3] = interp.g[3].dx * x0f + interp.g[3].dy * y0f + interp.g[3].d;
        attrib[4] = interp.g[4].dx * x0f + interp.g[4].dy * y0f + interp.g[4].d;
        attrib[5] = interp.g[5].dx * x0f + interp.g[5].dy * y0f + interp.g[5].d;
        attrib[6] = interp.g[6].dx * x0f + interp.g[6].dy * y0f + interp.g[6].d;
        attrib[7] = interp.g[7].dx * x0f + interp.g[7].dy * y0f + interp.g[7].d;
        attrib[8] = interp.g[8].dx * x0f + interp.g[8].dy * y0f + interp.g[8].d;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int_next[0] = math::min((int32_t)(attrib[2] * w), 0x00FFFFFF); // saturate
        attrib_int_next[1] = math::min((int32_t)(attrib[3] * w), 0x00FFFFFF); // saturate
        attrib_int_next[2] = math::min((int32_t)(attrib[4] * w), 0x00FFFFFF); // saturate
        attrib_int_next[3] = math::min((int32_t)(attrib[5] * w), 0x00FFFFFF); // saturate
        attrib_int_next[4] = math::min((int32_t)(attrib[6] * w), 0x00FFFFFF); // saturate
        attrib_int_next[5] = math::min((int32_t)(attrib[7] * w), 0x00FFFFFF); // saturate
        attrib_int_next[6] = math::min((int32_t)(attrib[8] * w), 0x00FFFFFF); // saturate

        int32_t start{ frame_stride * y + x0 };
        depth_addr = &depth_buffer[start];
        frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline void setup_subspan(int32_t count)
    {
        float count_float{ (float)count };
        depth = attrib[0];
        attrib[0] += interp.g[0].dx * count_float;
        attrib[1] += interp.g[1].dx * count_float;
        attrib[2] += interp.g[2].dx * count_float;
        attrib[3] += interp.g[3].dx * count_float;
        attrib[4] += interp.g[4].dx * count_float;
        attrib[5] += interp.g[5].dx * count_float;
        attrib[6] += interp.g[6].dx * count_float;
        attrib[7] += interp.g[7].dx * count_float;
        attrib[8] += interp.g[8].dx * count_float;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int[0] = attrib_int_next[0];
        attrib_int[1] = attrib_int_next[1];
        attrib_int[2] = attrib_int_next[2];
        attrib_int[3] = attrib_int_next[3];
        attrib_int[4] = attrib_int_next[4];
        attrib_int[5] = attrib_int_next[5];
        attrib_int[6] = attrib_int_next[6];
        attrib_int_next[0] = math::min((int32_t)(attrib[2] * w), 0x00FFFFFF); // saturate
        attrib_int_next[1] = math::min((int32_t)(attrib[3] * w), 0x00FFFFFF); // saturate
        attrib_int_next[2] = math::min((int32_t)(attrib[4] * w), 0x00FFFFFF); // saturate
        attrib_int_next[3] = math::min((int32_t)(attrib[5] * w), 0x00FFFFFF); // saturate
        attrib_int_next[4] = math::min((int32_t)(attrib[6] * w), 0x00FFFFFF); // saturate
        attrib_int_next[5] = math::min((int32_t)(attrib[7] * w), 0x00FFFFFF); // saturate
        attrib_int_next[6] = math::min((int32_t)(attrib[8] * w), 0x00FFFFFF); // saturate
        if (count == span_block_size)
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) >> span_block_size_shift;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) >> span_block_size_shift;
            attrib_int_dx[2] = (attrib_int_next[2] - attrib_int[2]) >> span_block_size_shift;
            attrib_int_dx[3] = (attrib_int_next[3] - attrib_int[3]) >> span_block_size_shift;
            attrib_int_dx[4] = (attrib_int_next[4] - attrib_int[4]) >> span_block_size_shift;
            attrib_int_dx[5] = (attrib_int_next[5] - attrib_int[5]) >> span_block_size_shift;
            attrib_int_dx[6] = (attrib_int_next[6] - attrib_int[6]) >> span_block_size_shift;
        }
        else
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) / count;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) / count;
            attrib_int_dx[2] = (attrib_int_next[2] - attrib_int[2]) / count;
            attrib_int_dx[3] = (attrib_int_next[3] - attrib_int[3]) / count;
            attrib_int_dx[4] = (attrib_int_next[4] - attrib_int[4]) / count;
            attrib_int_dx[5] = (attrib_int_next[5] - attrib_int[5]) / count;
            attrib_int_dx[6] = (attrib_int_next[6] - attrib_int[6]) / count;
        }
    }

    force_inline void fill()
    {
        if (*depth_addr > depth)
        {
            uint32_t color
            {
                (((((uint32_t)attrib_int[3] << 8u)                                  ) & 0xFF000000u)       ) +
                (((((uint32_t)attrib_int[0] >> 8u) * ((uint32_t)attrib_int[4] >> 8u)) & 0xFF000000u) >>  8u) +
                (((((uint32_t)attrib_int[1] >> 8u) * ((uint32_t)attrib_int[5] >> 8u)) & 0xFF000000u) >> 16u) +
                (((((uint32_t)attrib_int[2] >> 8u) * ((uint32_t)attrib_int[6] >> 8u))              ) >> 24u)
            };
            *frame_addr = blend_type::process(*frame_addr, color);
            *depth_addr = depth;
        }

        depth += interp.g[0].dx;
        attrib_int[0] += attrib_int_dx[0];
        attrib_int[1] += attrib_int_dx[1];
        attrib_int[2] += attrib_int_dx[2];
        attrib_int[3] += attrib_int_dx[3];
        attrib_int[4] += attrib_int_dx[4];
        attrib_int[5] += attrib_int_dx[5];
        attrib_int[6] += attrib_int_dx[6];

        depth_addr++;
        frame_addr++;
    }
};

template<typename blend_type>
struct raster_vertex_shade_lightmap : public abstract_raster
{
    raster_vertex_shade_lightmap(const config* c)
    {
        interp.setup(c);

        frame_stride = c->frame_stride;
        depth_buffer = c->depth_buffer;
        frame_buffer = c->frame_buffer;
        umax = (c->lightmap_width - 1) << 16;
        vmax = (c->lightmap_height - 1) << 16;
        vshift = math::log2(c->lightmap_width);
        lightmap = (uint32_t*)(c->lightmap);
    }

    // abstract_raster

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        return interp.setup_face(pv, vertex_count, is_clockwise);
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // raster

    interp<8> interp;

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    int32_t umax;
    int32_t vmax;
    int32_t vshift;
    uint32_t* lightmap;

    float attrib[8];
    float depth;

    float* depth_addr;
    uint32_t* frame_addr;
    int32_t attrib_int_dx[4]; // 16.16
    int32_t attrib_int[4]; // 16.16
    int32_t attrib_int_next[4]; // 16.16
    int32_t shade_int_dx[3]; // 16.16
    int32_t shade_int[3]; // 16.16
    int32_t shade_int_next[3]; // 16.16

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        attrib[0] = interp.g[0].dx * x0f + interp.g[0].dy * y0f + interp.g[0].d;
        attrib[1] = interp.g[1].dx * x0f + interp.g[1].dy * y0f + interp.g[1].d;
        attrib[2] = interp.g[2].dx * x0f + interp.g[2].dy * y0f + interp.g[2].d;
        attrib[3] = interp.g[3].dx * x0f + interp.g[3].dy * y0f + interp.g[3].d;
        attrib[4] = interp.g[4].dx * x0f + interp.g[4].dy * y0f + interp.g[4].d;
        attrib[5] = interp.g[5].dx * x0f + interp.g[5].dy * y0f + interp.g[5].d;
        attrib[6] = interp.g[6].dx * x0f + interp.g[6].dy * y0f + interp.g[6].d;
        attrib[7] = interp.g[7].dx * x0f + interp.g[7].dy * y0f + interp.g[7].d;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int_next[0] = math::min((int32_t)(attrib[2] * w), 0x00FFFFFF); // saturate
        attrib_int_next[1] = math::min((int32_t)(attrib[3] * w), 0x00FFFFFF); // saturate
        attrib_int_next[2] = math::min((int32_t)(attrib[4] * w), 0x00FFFFFF); // saturate
        attrib_int_next[3] = math::min((int32_t)(attrib[5] * w), 0x00FFFFFF); // saturate
        int32_t attrib_shade_int[2]
        {
            math::clamp((int32_t)(attrib[6] * w), 0, umax), // clamp
            math::clamp((int32_t)(attrib[7] * w), 0, vmax), // clamp
        };
        uint32_t shade{ sample_bilinear(attrib_shade_int[0], attrib_shade_int[1], vshift, lightmap) };
        shade_int_next[0] = (shade & 0x00FF0000u)       ;
        shade_int_next[1] = (shade & 0x0000FF00u) <<  8u;
        shade_int_next[2] = (shade & 0x000000FFu) << 16u;

        int32_t start{ frame_stride * y + x0 };
        depth_addr = &depth_buffer[start];
        frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline void setup_subspan(int32_t count)
    {
        float count_float{ (float)count };
        depth = attrib[0];
        attrib[0] += interp.g[0].dx * count_float;
        attrib[1] += interp.g[1].dx * count_float;
        attrib[2] += interp.g[2].dx * count_float;
        attrib[3] += interp.g[3].dx * count_float;
        attrib[4] += interp.g[4].dx * count_float;
        attrib[5] += interp.g[5].dx * count_float;
        attrib[6] += interp.g[6].dx * count_float;
        attrib[7] += interp.g[7].dx * count_float;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int[0] = attrib_int_next[0];
        attrib_int[1] = attrib_int_next[1];
        attrib_int[2] = attrib_int_next[2];
        attrib_int[3] = attrib_int_next[3];
        shade_int[0] = shade_int_next[0];
        shade_int[1] = shade_int_next[1];
        shade_int[2] = shade_int_next[2];
        attrib_int_next[0] = math::min((int32_t)(attrib[2] * w), 0x00FFFFFF); // saturate
        attrib_int_next[1] = math::min((int32_t)(attrib[3] * w), 0x00FFFFFF); // saturate
        attrib_int_next[2] = math::min((int32_t)(attrib[4] * w), 0x00FFFFFF); // saturate
        attrib_int_next[3] = math::min((int32_t)(attrib[5] * w), 0x00FFFFFF); // saturate
        int32_t attrib_shade_int[2]
        {
            math::clamp((int32_t)(attrib[6] * w), 0, umax), // clamp
            math::clamp((int32_t)(attrib[7] * w), 0, vmax), // clamp
        };
        uint32_t shade{ sample_bilinear(attrib_shade_int[0], attrib_shade_int[1], vshift, lightmap) };
        shade_int_next[0] = (shade & 0x00FF0000u)       ;
        shade_int_next[1] = (shade & 0x0000FF00u) <<  8u;
        shade_int_next[2] = (shade & 0x000000FFu) << 16u;
        if (count == span_block_size)
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) >> span_block_size_shift;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) >> span_block_size_shift;
            attrib_int_dx[2] = (attrib_int_next[2] - attrib_int[2]) >> span_block_size_shift;
            attrib_int_dx[3] = (attrib_int_next[3] - attrib_int[3]) >> span_block_size_shift;
            shade_int_dx[0] = (shade_int_next[0] - shade_int[0]) >> span_block_size_shift;
            shade_int_dx[1] = (shade_int_next[1] - shade_int[1]) >> span_block_size_shift;
            shade_int_dx[2] = (shade_int_next[2] - shade_int[2]) >> span_block_size_shift;
        }
        else
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) / count;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) / count;
            attrib_int_dx[2] = (attrib_int_next[2] - attrib_int[2]) / count;
            attrib_int_dx[3] = (attrib_int_next[3] - attrib_int[3]) / count;
            shade_int_dx[0] = (shade_int_next[0] - shade_int[0]) / count;
            shade_int_dx[1] = (shade_int_next[1] - shade_int[1]) / count;
            shade_int_dx[2] = (shade_int_next[2] - shade_int[2]) / count;
        }
    }

    force_inline void fill()
    {
        if (*depth_addr > depth)
        {
            uint32_t color
            {
                (((((uint32_t)attrib_int[3] << 8u)                                 ) & 0xFF000000u)       ) +
                (((((uint32_t)attrib_int[0] >> 8u) * ((uint32_t)shade_int[0] >> 8u)) & 0xFF000000u) >>  8u) +
                (((((uint32_t)attrib_int[1] >> 8u) * ((uint32_t)shade_int[1] >> 8u)) & 0xFF000000u) >> 16u) +
                (((((uint32_t)attrib_int[2] >> 8u) * ((uint32_t)shade_int[2] >> 8u))              ) >> 24u)
            };
            *frame_addr = blend_type::process(*frame_addr, color);
            *depth_addr = depth;
        }

        depth += interp.g[0].dx;
        attrib_int[0] += attrib_int_dx[0];
        attrib_int[1] += attrib_int_dx[1];
        attrib_int[2] += attrib_int_dx[2];
        attrib_int[3] += attrib_int_dx[3];
        shade_int[0] += shade_int_dx[0];
        shade_int[1] += shade_int_dx[1];
        shade_int[2] += shade_int_dx[2];

        depth_addr++;
        frame_addr++;
    }
};

//------------------------------------------------------------------------------

template<typename blend_type>
struct raster_texture_shade_none : public abstract_raster
{
    raster_texture_shade_none(const config* c)
    {
        interp.setup(c);

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

    float texture_width;
    float texture_height;

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        if (interp.setup_face(pv, vertex_count, is_clockwise))
        {
            interp.g[2].dx *= texture_width;
            interp.g[2].dy *= texture_width;
            interp.g[2].d *= texture_width;
            interp.g[3].dx *= texture_height;
            interp.g[3].dy *= texture_height;
            interp.g[3].d *= texture_height;
            return true;
        }
        return false;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // raster

    interp<4> interp;

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    int32_t smask;
    int32_t tmask;
    int32_t tshift;
    uint32_t* texture_lut;
    uint8_t* texture_data;

    float attrib[4];
    float depth;

    float* depth_addr;
    uint32_t* frame_addr;
    int32_t attrib_int_dx[2]; // 16.16
    int32_t attrib_int[2]; // 16.16
    int32_t attrib_int_next[2]; // 16.16

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        attrib[0] = interp.g[0].dx * x0f + interp.g[0].dy * y0f + interp.g[0].d;
        attrib[1] = interp.g[1].dx * x0f + interp.g[1].dy * y0f + interp.g[1].d;
        attrib[2] = interp.g[2].dx * x0f + interp.g[2].dy * y0f + interp.g[2].d;
        attrib[3] = interp.g[3].dx * x0f + interp.g[3].dy * y0f + interp.g[3].d;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int_next[0] = (int32_t)(attrib[2] * w);
        attrib_int_next[1] = (int32_t)(attrib[3] * w);

        int32_t start{ frame_stride * y + x0 };
        depth_addr = &depth_buffer[start];
        frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline void setup_subspan(int32_t count)
    {
        float count_float{ (float)count };
        depth = attrib[0];
        attrib[0] += interp.g[0].dx * count_float;
        attrib[1] += interp.g[1].dx * count_float;
        attrib[2] += interp.g[2].dx * count_float;
        attrib[3] += interp.g[3].dx * count_float;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int[0] = attrib_int_next[0];
        attrib_int[1] = attrib_int_next[1];
        attrib_int_next[0] = (int32_t)(attrib[2] * w);
        attrib_int_next[1] = (int32_t)(attrib[3] * w);
        if (count == span_block_size)
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) >> span_block_size_shift;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) >> span_block_size_shift;
        }
        else
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) / count;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) / count;
        }
    }

    force_inline void fill()
    {
        if (*depth_addr > depth)
        {
            uint8_t index{ sample_nearest(
                attrib_int[0] & smask,
                attrib_int[1] & tmask,
                tshift,
                texture_data) };
            //if (index != 255u)
            {
                uint32_t color{ texture_lut[index] };
                *frame_addr = blend_type::process(*frame_addr, color);
                *depth_addr = depth;
            }
        }

        depth += interp.g[0].dx;
        attrib_int[0] += attrib_int_dx[0];
        attrib_int[1] += attrib_int_dx[1];

        depth_addr++;
        frame_addr++;
    }
};

template<typename blend_type>
struct raster_texture_shade_vertex : public abstract_raster
{
    raster_texture_shade_vertex(const config* c)
    {
        interp.setup(c);

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

    float texture_width;
    float texture_height;

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        if (interp.setup_face(pv, vertex_count, is_clockwise))
        {
            interp.g[2].dx *= texture_width;
            interp.g[2].dy *= texture_width;
            interp.g[2].d *= texture_width;
            interp.g[3].dx *= texture_height;
            interp.g[3].dy *= texture_height;
            interp.g[3].d *= texture_height;
            return true;
        }
        return false;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // raster

    interp<7> interp;

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    int32_t smask;
    int32_t tmask;
    int32_t tshift;
    uint32_t* texture_lut;
    uint8_t* texture_data;

    float attrib[7];
    float depth;

    float* depth_addr;
    uint32_t* frame_addr;
    int32_t attrib_int_dx[5]; // 16.16
    int32_t attrib_int[5]; // 16.16
    int32_t attrib_int_next[5]; // 16.16

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        attrib[0] = interp.g[0].dx * x0f + interp.g[0].dy * y0f + interp.g[0].d;
        attrib[1] = interp.g[1].dx * x0f + interp.g[1].dy * y0f + interp.g[1].d;
        attrib[2] = interp.g[2].dx * x0f + interp.g[2].dy * y0f + interp.g[2].d;
        attrib[3] = interp.g[3].dx * x0f + interp.g[3].dy * y0f + interp.g[3].d;
        attrib[4] = interp.g[4].dx * x0f + interp.g[4].dy * y0f + interp.g[4].d;
        attrib[5] = interp.g[5].dx * x0f + interp.g[5].dy * y0f + interp.g[5].d;
        attrib[6] = interp.g[6].dx * x0f + interp.g[6].dy * y0f + interp.g[6].d;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int_next[0] = (int32_t)(attrib[2] * w);
        attrib_int_next[1] = (int32_t)(attrib[3] * w);
        attrib_int_next[2] = math::min((int32_t)(attrib[4] * w), 0x00FFFFFF); // saturate
        attrib_int_next[3] = math::min((int32_t)(attrib[5] * w), 0x00FFFFFF); // saturate
        attrib_int_next[4] = math::min((int32_t)(attrib[6] * w), 0x00FFFFFF); // saturate

        int32_t start{ frame_stride * y + x0 };
        depth_addr = &depth_buffer[start];
        frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline void setup_subspan(int32_t count)
    {
        float count_float{ (float)count };
        depth = attrib[0];
        attrib[0] += interp.g[0].dx * count_float;
        attrib[1] += interp.g[1].dx * count_float;
        attrib[2] += interp.g[2].dx * count_float;
        attrib[3] += interp.g[3].dx * count_float;
        attrib[4] += interp.g[4].dx * count_float;
        attrib[5] += interp.g[5].dx * count_float;
        attrib[6] += interp.g[6].dx * count_float;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int[0] = attrib_int_next[0];
        attrib_int[1] = attrib_int_next[1];
        attrib_int[2] = attrib_int_next[2];
        attrib_int[3] = attrib_int_next[3];
        attrib_int[4] = attrib_int_next[4];
        attrib_int_next[0] = (int32_t)(attrib[2] * w);
        attrib_int_next[1] = (int32_t)(attrib[3] * w);
        attrib_int_next[2] = math::min((int32_t)(attrib[4] * w), 0x00FFFFFF); // saturate
        attrib_int_next[3] = math::min((int32_t)(attrib[5] * w), 0x00FFFFFF); // saturate
        attrib_int_next[4] = math::min((int32_t)(attrib[6] * w), 0x00FFFFFF); // saturate

        if (count == span_block_size)
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) >> span_block_size_shift;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) >> span_block_size_shift;
            attrib_int_dx[2] = (attrib_int_next[2] - attrib_int[2]) >> span_block_size_shift;
            attrib_int_dx[3] = (attrib_int_next[3] - attrib_int[3]) >> span_block_size_shift;
            attrib_int_dx[4] = (attrib_int_next[4] - attrib_int[4]) >> span_block_size_shift;
        }
        else
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) / count;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) / count;
            attrib_int_dx[2] = (attrib_int_next[2] - attrib_int[2]) / count;
            attrib_int_dx[3] = (attrib_int_next[3] - attrib_int[3]) / count;
            attrib_int_dx[4] = (attrib_int_next[4] - attrib_int[4]) / count;
        }
    }

    force_inline void fill()
    {
        if (*depth_addr > depth)
        {
            uint8_t index{ sample_nearest(
                attrib_int[0] & smask,
                attrib_int[1] & tmask,
                tshift,
                texture_data) };
            //if (index != 255u)
            {
                uint32_t texel{ texture_lut[index] };
                uint32_t color
                {
                    (((((texel & 0xFF000000u)        )                          )              )       ) +
                    (((((texel & 0x00FF0000u) >> 16u ) * (uint32_t)attrib_int[2]) & 0xFF000000u) >>  8u) +
                    (((((texel & 0x0000FF00u) >>  8u ) * (uint32_t)attrib_int[3]) & 0xFF000000u) >> 16u) +
                    (((((texel & 0x000000FFu)        ) * (uint32_t)attrib_int[4])              ) >> 24u)
                };
                *frame_addr = blend_type::process(*frame_addr, color);
                *depth_addr = depth;
            }
        }

        depth += interp.g[0].dx;
        attrib_int[0] += attrib_int_dx[0];
        attrib_int[1] += attrib_int_dx[1];
        attrib_int[2] += attrib_int_dx[2];
        attrib_int[3] += attrib_int_dx[3];
        attrib_int[4] += attrib_int_dx[4];

        depth_addr++;
        frame_addr++;
    }
};

template<typename blend_type>
struct raster_texture_shade_lightmap : public abstract_raster
{
    raster_texture_shade_lightmap(const config* c)
    {
        interp.setup(c);

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
        lightmap = (uint32_t*)(c->lightmap);
    }

    // abstract_raster

    float texture_width;
    float texture_height;

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        if (interp.setup_face(pv, vertex_count, is_clockwise))
        {
            interp.g[2].dx *= texture_width;
            interp.g[2].dy *= texture_width;
            interp.g[2].d *= texture_width;
            interp.g[3].dx *= texture_height;
            interp.g[3].dy *= texture_height;
            interp.g[3].d *= texture_height;
            return true;
        }
        return false;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // raster

    interp<6> interp;

    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    int32_t smask;
    int32_t tmask;
    int32_t tshift;
    uint32_t* texture_lut;
    uint8_t* texture_data;
    int32_t umax;
    int32_t vmax;
    int32_t vshift;
    uint32_t* lightmap;

    float attrib[6];
    float depth;

    float* depth_addr;
    uint32_t* frame_addr;
    int32_t attrib_int_dx[2]; // 16.16
    int32_t attrib_int[2]; // 16.16
    int32_t attrib_int_next[2]; // 16.16
    int32_t shade_int_dx[3]; // 16.16
    int32_t shade_int[3]; // 16.16
    int32_t shade_int_next[3]; // 16.16

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        float x0f{ raster_to_real(x0) };
        float y0f{ raster_to_real(y) };
        attrib[0] = interp.g[0].dx * x0f + interp.g[0].dy * y0f + interp.g[0].d;
        attrib[1] = interp.g[1].dx * x0f + interp.g[1].dy * y0f + interp.g[1].d;
        attrib[2] = interp.g[2].dx * x0f + interp.g[2].dy * y0f + interp.g[2].d;
        attrib[3] = interp.g[3].dx * x0f + interp.g[3].dy * y0f + interp.g[3].d;
        attrib[4] = interp.g[4].dx * x0f + interp.g[4].dy * y0f + interp.g[4].d;
        attrib[5] = interp.g[5].dx * x0f + interp.g[5].dy * y0f + interp.g[5].d;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int_next[0] = (int32_t)(attrib[2] * w);
        attrib_int_next[1] = (int32_t)(attrib[3] * w);
        int32_t attrib_shade_int[2]
        {
            math::clamp((int32_t)(attrib[4] * w), 0, umax), // clamp
            math::clamp((int32_t)(attrib[5] * w), 0, vmax), // clamp
        };
        uint32_t shade{ sample_bilinear(attrib_shade_int[0], attrib_shade_int[1], vshift, lightmap) };
        shade_int_next[0] = (shade & 0x00FF0000u)       ;
        shade_int_next[1] = (shade & 0x0000FF00u) <<  8u;
        shade_int_next[2] = (shade & 0x000000FFu) << 16u;

        int32_t start{ frame_stride * y + x0 };
        depth_addr = &depth_buffer[start];
        frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline void setup_subspan(int32_t count)
    {
        float count_float{ (float)count };
        depth = attrib[0];
        attrib[0] += interp.g[0].dx * count_float;
        attrib[1] += interp.g[1].dx * count_float;
        attrib[2] += interp.g[2].dx * count_float;
        attrib[3] += interp.g[3].dx * count_float;
        attrib[4] += interp.g[4].dx * count_float;
        attrib[5] += interp.g[5].dx * count_float;
        float w{ (float)0x10000 / attrib[1] };
        attrib_int[0] = attrib_int_next[0];
        attrib_int[1] = attrib_int_next[1];
        shade_int[0] = shade_int_next[0];
        shade_int[1] = shade_int_next[1];
        shade_int[2] = shade_int_next[2];
        attrib_int_next[0] = (int32_t)(attrib[2] * w);
        attrib_int_next[1] = (int32_t)(attrib[3] * w);
        int32_t attrib_shade_int[2]
        {
            math::clamp((int32_t)(attrib[4] * w), 0, umax), // clamp
            math::clamp((int32_t)(attrib[5] * w), 0, vmax), // clamp
        };
        uint32_t shade{ sample_bilinear(attrib_shade_int[0], attrib_shade_int[1], vshift, lightmap) };
        shade_int_next[0] = (shade & 0x00FF0000u)       ;
        shade_int_next[1] = (shade & 0x0000FF00u) <<  8u;
        shade_int_next[2] = (shade & 0x000000FFu) << 16u;

        if (count == span_block_size)
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) >> span_block_size_shift;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) >> span_block_size_shift;
            shade_int_dx[0] = (shade_int_next[0] - shade_int[0]) >> span_block_size_shift;
            shade_int_dx[1] = (shade_int_next[1] - shade_int[1]) >> span_block_size_shift;
            shade_int_dx[2] = (shade_int_next[2] - shade_int[2]) >> span_block_size_shift;
        }
        else
        {
            attrib_int_dx[0] = (attrib_int_next[0] - attrib_int[0]) / count;
            attrib_int_dx[1] = (attrib_int_next[1] - attrib_int[1]) / count;
            shade_int_dx[0] = (shade_int_next[0] - shade_int[0]) / count;
            shade_int_dx[1] = (shade_int_next[1] - shade_int[1]) / count;
            shade_int_dx[2] = (shade_int_next[2] - shade_int[2]) / count;
        }
    }

    force_inline void fill()
    {
        if (*depth_addr > depth)
        {
            uint8_t index{ sample_nearest(
                attrib_int[0] & smask,
                attrib_int[1] & tmask,
                tshift,
                texture_data) };
            //if (index != 255u)
            {
                uint32_t texel{ texture_lut[index] };
                uint32_t color
                {
                    (((((texel & 0xFF000000u)        )                         )              )       ) +
                    (((((texel & 0x00FF0000u) >> 16u ) * (uint32_t)shade_int[0]) & 0xFF000000u) >>  8u) +
                    (((((texel & 0x0000FF00u) >>  8u ) * (uint32_t)shade_int[1]) & 0xFF000000u) >> 16u) +
                    (((((texel & 0x000000FFu)        ) * (uint32_t)shade_int[2])              ) >> 24u)
                };
                *frame_addr = blend_type::process(*frame_addr, color);
                *depth_addr = depth;
            }
        }

        depth += interp.g[0].dx;
        attrib_int[0] += attrib_int_dx[0];
        attrib_int[1] += attrib_int_dx[1];
        shade_int[0] += shade_int_dx[0];
        shade_int[1] += shade_int_dx[1];
        shade_int[2] += shade_int_dx[2];

        depth_addr++;
        frame_addr++;
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
        raster_solid_shade_none<blend_none> r1;
        raster_solid_shade_none<blend_add> r2;
        raster_solid_shade_none<blend_mul> r3;
        raster_solid_shade_none<blend_alpha> r4;
        raster_solid_shade_vertex<blend_none> r5;
        raster_solid_shade_vertex<blend_add> r6;
        raster_solid_shade_vertex<blend_mul> r7;
        raster_solid_shade_vertex<blend_alpha> r8;
        raster_solid_shade_lightmap<blend_none> r9;
        raster_solid_shade_lightmap<blend_add> r10;
        raster_solid_shade_lightmap<blend_mul> r11;
        raster_solid_shade_lightmap<blend_alpha> r12;
        raster_vertex_shade_none<blend_none> r13;
        raster_vertex_shade_none<blend_add> r14;
        raster_vertex_shade_none<blend_mul> r15;
        raster_vertex_shade_none<blend_alpha> r16;
        raster_vertex_shade_vertex<blend_none> r17;
        raster_vertex_shade_vertex<blend_add> r18;
        raster_vertex_shade_vertex<blend_mul> r19;
        raster_vertex_shade_vertex<blend_alpha> r20;
        raster_vertex_shade_lightmap<blend_none> r21;
        raster_vertex_shade_lightmap<blend_add> r22;
        raster_vertex_shade_lightmap<blend_mul> r23;
        raster_vertex_shade_lightmap<blend_alpha> r24;
        raster_texture_shade_none<blend_none> r25;
        raster_texture_shade_none<blend_add> r26;
        raster_texture_shade_none<blend_mul> r27;
        raster_texture_shade_none<blend_alpha> r28;
        raster_texture_shade_vertex<blend_none> r29;
        raster_texture_shade_vertex<blend_add> r30;
        raster_texture_shade_vertex<blend_mul> r31;
        raster_texture_shade_vertex<blend_alpha> r32;
        raster_texture_shade_lightmap<blend_none> r33;
        raster_texture_shade_lightmap<blend_add> r34;
        raster_texture_shade_lightmap<blend_mul> r35;
        raster_texture_shade_lightmap<blend_alpha> r36;
    };
    alignas(alignof(raster_pool)) uint8_t raster[sizeof(raster_pool)];

    switch (c->flags)
    {
    case (FILL_DEPTH):
        r = new (raster) raster_depth(c);
        break;
    case (FILL_SOLID):
        r = new (raster) raster_solid_shade_none<blend_none>(c);
        break;
    case (FILL_SOLID | BLEND_ADD):
        r = new (raster) raster_solid_shade_none<blend_add>(c);
        break;
    case (FILL_SOLID | BLEND_MUL):
        r = new (raster) raster_solid_shade_none<blend_mul>(c);
        break;
    case (FILL_SOLID | BLEND_ALPHA):
        r = new (raster) raster_solid_shade_none<blend_alpha>(c);
        break;
    case (FILL_SOLID | SHADE_VERTEX):
        r = new (raster) raster_solid_shade_vertex<blend_none>(c);
        break;
    case (FILL_SOLID | SHADE_VERTEX | BLEND_ADD):
        r = new (raster) raster_solid_shade_vertex<blend_add>(c);
        break;
    case (FILL_SOLID | SHADE_VERTEX | BLEND_MUL):
        r = new (raster) raster_solid_shade_vertex<blend_mul>(c);
        break;
    case (FILL_SOLID | SHADE_VERTEX | BLEND_ALPHA):
        r = new (raster) raster_solid_shade_vertex<blend_alpha>(c);
        break;
    case (FILL_SOLID | SHADE_LIGHTMAP):
        r = new (raster) raster_solid_shade_lightmap<blend_none>(c);
        break;
    case (FILL_SOLID | SHADE_LIGHTMAP | BLEND_ADD):
        r = new (raster) raster_solid_shade_lightmap<blend_add>(c);
        break;
    case (FILL_SOLID | SHADE_LIGHTMAP | BLEND_MUL):
        r = new (raster) raster_solid_shade_lightmap<blend_mul>(c);
        break;
    case (FILL_SOLID | SHADE_LIGHTMAP | BLEND_ALPHA):
        r = new (raster) raster_solid_shade_lightmap<blend_alpha>(c);
        break;
    case (FILL_VERTEX):
        r = new (raster) raster_vertex_shade_none<blend_none>(c);
        break;
    case (FILL_VERTEX | BLEND_ADD):
        r = new (raster) raster_vertex_shade_none<blend_add>(c);
        break;
    case (FILL_VERTEX | BLEND_MUL):
        r = new (raster) raster_vertex_shade_none<blend_mul>(c);
        break;
    case (FILL_VERTEX | BLEND_ALPHA):
        r = new (raster) raster_vertex_shade_none<blend_alpha>(c);
        break;
    case (FILL_VERTEX | SHADE_VERTEX):
        r = new (raster) raster_vertex_shade_vertex<blend_none>(c);
        break;
    case (FILL_VERTEX | SHADE_VERTEX | BLEND_ADD):
        r = new (raster) raster_vertex_shade_vertex<blend_add>(c);
        break;
    case (FILL_VERTEX | SHADE_VERTEX | BLEND_MUL):
        r = new (raster) raster_vertex_shade_vertex<blend_mul>(c);
        break;
    case (FILL_VERTEX | SHADE_VERTEX | BLEND_ALPHA):
        r = new (raster) raster_vertex_shade_vertex<blend_alpha>(c);
        break;
    case (FILL_VERTEX | SHADE_LIGHTMAP):
        r = new (raster) raster_vertex_shade_lightmap<blend_none>(c);
        break;
    case (FILL_VERTEX | SHADE_LIGHTMAP | BLEND_ADD):
        r = new (raster) raster_vertex_shade_lightmap<blend_add>(c);
        break;
    case (FILL_VERTEX | SHADE_LIGHTMAP | BLEND_MUL):
        r = new (raster) raster_vertex_shade_lightmap<blend_mul>(c);
        break;
    case (FILL_VERTEX | SHADE_LIGHTMAP | BLEND_ALPHA):
        r = new (raster) raster_vertex_shade_lightmap<blend_alpha>(c);
        break;
    case (FILL_TEXTURE):
        r = new (raster) raster_texture_shade_none<blend_none>(c);
        break;
    case (FILL_TEXTURE | BLEND_ADD):
        r = new (raster) raster_texture_shade_none<blend_add>(c);
        break;
    case (FILL_TEXTURE | BLEND_MUL):
        r = new (raster) raster_texture_shade_none<blend_mul>(c);
        break;
    case (FILL_TEXTURE | BLEND_ALPHA):
        r = new (raster) raster_texture_shade_none<blend_alpha>(c);
        break;
    case (FILL_TEXTURE | SHADE_VERTEX):
        r = new (raster) raster_texture_shade_vertex<blend_none>(c);
        break;
    case (FILL_TEXTURE | SHADE_VERTEX | BLEND_ADD):
        r = new (raster) raster_texture_shade_vertex<blend_add>(c);
        break;
    case (FILL_TEXTURE | SHADE_VERTEX | BLEND_MUL):
        r = new (raster) raster_texture_shade_vertex<blend_mul>(c);
        break;
    case (FILL_TEXTURE | SHADE_VERTEX | BLEND_ALPHA):
        r = new (raster) raster_texture_shade_vertex<blend_alpha>(c);
        break;
    case (FILL_TEXTURE | SHADE_LIGHTMAP):
        r = new (raster) raster_texture_shade_lightmap<blend_none>(c);
        break;
    case (FILL_TEXTURE | SHADE_LIGHTMAP | BLEND_ADD):
        r = new (raster) raster_texture_shade_lightmap<blend_add>(c);
        break;
    case (FILL_TEXTURE | SHADE_LIGHTMAP | BLEND_MUL):
        r = new (raster) raster_texture_shade_lightmap<blend_mul>(c);
        break;
    case (FILL_TEXTURE | SHADE_LIGHTMAP | BLEND_ALPHA):
        r = new (raster) raster_texture_shade_lightmap<blend_alpha>(c);
        break;

    default:
        break;
    }

    //------------------------------------//

    if (r == nullptr)
        return;

    uint32_t num_faces{ c->num_faces };
    const face* faces{ c->faces };
    const float* vertex_data{ c->vertex_data };
    uint32_t vertex_stride{ c->vertex_stride };

    while (num_faces)
    {
        uint32_t vertex_index{ faces->index };
        uint32_t vertex_count{ faces->count };
        faces++;

        const float* pv[num_max_vertices];
        const float* p{ &vertex_data[vertex_stride * vertex_index ] };
        for (uint32_t nv{}; nv < vertex_count; ++nv)
        {
            pv[nv] = p;
            p += vertex_stride;
        }

        if (r->setup_face(pv, vertex_count))
            scan_face(pv, vertex_count, r);

        num_faces--;
    }
}

} // namespace lib3d::raster
