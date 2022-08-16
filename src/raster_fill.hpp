#pragma once

namespace lib3d::raster
{

//------------------------------------------------------------------------------

struct fill_span_depth
{
    int32_t frame_stride;
    float* depth_buffer;

    float* depth_addr;

    void setup(const config* cfg)
    {
        frame_stride = cfg->frame_stride;
        depth_buffer = cfg->depth_buffer;
    }

    void setup_face() {}

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        depth_addr = &depth_buffer[frame_stride * y + x0];
    }

    force_inline void write(const interp<1>& interp)
    {
        *depth_addr = math::min(*depth_addr, interp.depth);
    }

    force_inline void advance()
    {
        depth_addr++;
    }
};

//------------------------------------------------------------------------------

force_inline uint32_t adds_X888(uint32_t v0, uint32_t v1)
{
    uint32_t r{ v0 + v1 };
    uint32_t m{ (v0 ^ v1 ^ r) & 0x01010100u };
    r |= m - (m >> 8u) + 0xFF000000u;
    return r - m;
}

force_inline uint32_t mul_X888(uint32_t v0, uint32_t v1)
{
    uint32_t r0{ (v0 & 0x000000FFu) *  (v1 & 0x000000FFu)         };
    uint32_t r1{ (v0 & 0x0000FF00u) * ((v1 & 0x0000FF00u) >>  8u) };
    uint32_t r2{ (v0 & 0x00FF0000u) * ((v1 & 0x00FF0000u) >> 16u) };
    return ((r0 + (r1 & 0x00FF0000u) + (r2 & 0xFF000000u)) >> 8u) + 0xFF000000u;
}

force_inline uint32_t alpha_8888(uint32_t b, uint32_t f)
{
#if 1
    uint32_t a1{ f >> 24u };
    uint32_t a0{ 0x100u - a1 };
    uint32_t v0{ (b & 0x00FF00FFu) * a0 + (f & 0x00FF00FFu) * a1 };
    uint32_t v1{ (b & 0x0000FF00u) * a0 + (f & 0x0000FF00u) * a1 };
    return (((v0 & 0xFF00FF00u) + (v1 & 0x00FF0000u)) >> 8u) + 0xFF000000u;
#else
    uint32_t a{ 0xFFu - (f >> 24u) };
    uint32_t v0{ (b & 0x00FF00FFu) * a };
    uint32_t v1{ (b & 0x0000FF00u) * a };
    uint32_t r{ (a << 24u) + (((v0 & 0xFF00FF00u) + (v1 & 0x00FF0000u)) >> 8u) };
    return r + f;
#endif
}

struct blend_none
{
    static force_inline uint32_t process(uint32_t /*b*/, uint32_t f)
    {
        return f;
    }
};

struct blend_add
{
    static force_inline uint32_t process(uint32_t v0, uint32_t v1)
    {
        return adds_X888(v0, v1);
    }
};

struct blend_mul
{
    static force_inline uint32_t process(uint32_t v0, uint32_t v1)
    {
        return mul_X888(v0, v1);
    }
};

struct blend_alpha
{
    static force_inline uint32_t process(uint32_t b, uint32_t f)
    {
        return alpha_8888(b, f);
    }
};

//------------------------------------------------------------------------------

template<typename blend_type>
struct fill_span_solid_shade_none
{
    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    uint32_t fill_color;

    float* depth_addr;
    uint32_t* frame_addr;

    void setup(const config* cfg)
    {
        frame_stride = cfg->frame_stride;
        depth_buffer = cfg->depth_buffer;
        frame_buffer = cfg->frame_buffer;
        fill_color = reinterpret_cast<const uint32_t&>(cfg->fill_color);
    }

    void setup_face() {}

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        int32_t start{ frame_stride * y + x0 };
        depth_addr = &depth_buffer[start];
        frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline void write(const interp<1>& interp)
    {
        if (*depth_addr > interp.depth)
        {
            *frame_addr = blend_type::process(*frame_addr, fill_color);
            *depth_addr = interp.depth;
        }
    }

    force_inline void advance()
    {
        depth_addr++;
        frame_addr++;
    }
};

using fill_span_solid_shade_none_blend_none = fill_span_solid_shade_none<blend_none>;
using fill_span_solid_shade_none_blend_add = fill_span_solid_shade_none<blend_add>;
using fill_span_solid_shade_none_blend_mul = fill_span_solid_shade_none<blend_mul>;
using fill_span_solid_shade_none_blend_alpha = fill_span_solid_shade_none<blend_alpha>;

template<typename blend_type>
struct fill_span_solid_shade_vertex
{
    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;
    uint32_t fill_color[4];

    float* depth_addr;
    uint32_t* frame_addr;

    void setup(const config* cfg)
    {
        frame_stride = cfg->frame_stride;
        depth_buffer = cfg->depth_buffer;
        frame_buffer = cfg->frame_buffer;
        fill_color[0] = (uint32_t)cfg->fill_color.a << 24u;
        fill_color[1] = (uint32_t)cfg->fill_color.r;
        fill_color[2] = (uint32_t)cfg->fill_color.g;
        fill_color[3] = (uint32_t)cfg->fill_color.b;
    }

    void setup_face() {}

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        int32_t start{ frame_stride * y + x0 };
        depth_addr = &depth_buffer[start];
        frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline void write(const interp<5>& interp)
    {
        enum { shade_r, shade_g, shade_b };
        if (*depth_addr > interp.depth)
        {
            uint32_t color
            {
                (((fill_color[0]                                       )              )       ) +
                (((fill_color[1] * (uint32_t)interp.attrib_int[shade_r]) & 0xFF000000u) >>  8u) +
                (((fill_color[2] * (uint32_t)interp.attrib_int[shade_g]) & 0xFF000000u) >> 16u) +
                (((fill_color[3] * (uint32_t)interp.attrib_int[shade_b])              ) >> 24u)
            };
            *frame_addr = blend_type::process(*frame_addr, color);
            *depth_addr = interp.depth;
        }
    }

    force_inline void advance()
    {
        depth_addr++;
        frame_addr++;
    }
};

using fill_span_solid_shade_vertex_blend_none = fill_span_solid_shade_vertex<blend_none>;
using fill_span_solid_shade_vertex_blend_add = fill_span_solid_shade_vertex<blend_add>;
using fill_span_solid_shade_vertex_blend_mul = fill_span_solid_shade_vertex<blend_mul>;
using fill_span_solid_shade_vertex_blend_alpha = fill_span_solid_shade_vertex<blend_alpha>;

//------------------------------------------------------------------------------

template<typename blend_type>
struct fill_span_vertex_shade_none
{
    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;

    float* depth_addr;
    uint32_t* frame_addr;

    void setup(const config* cfg)
    {
        frame_stride = cfg->frame_stride;
        depth_buffer = cfg->depth_buffer;
        frame_buffer = cfg->frame_buffer;
    }

    void setup_face() {}

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        int32_t start{ frame_stride * y + x0 };
        depth_addr = &depth_buffer[start];
        frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline void write(const interp<6>& interp)
    {
        enum { color_r, color_g, color_b, color_a };
        if (*depth_addr > interp.depth)
        {
            uint32_t color
            {
                (((uint32_t)interp.attrib_int[color_a] & 0x00FF0000u) <<  8u) +
                (((uint32_t)interp.attrib_int[color_r] & 0x00FF0000u)       ) +
                (((uint32_t)interp.attrib_int[color_g] & 0x00FF0000u) >>  8u) +
                (((uint32_t)interp.attrib_int[color_b] & 0x00FF0000u) >> 16u)
            };
            *frame_addr = blend_type::process(*frame_addr, color);
            *depth_addr = interp.depth;
        }
    }

    force_inline void advance()
    {
        depth_addr++;
        frame_addr++;
    }
};

using fill_span_vertex_shade_none_blend_none = fill_span_vertex_shade_none<blend_none>;
using fill_span_vertex_shade_none_blend_add = fill_span_vertex_shade_none<blend_add>;
using fill_span_vertex_shade_none_blend_mul = fill_span_vertex_shade_none<blend_mul>;
using fill_span_vertex_shade_none_blend_alpha = fill_span_vertex_shade_none<blend_alpha>;

template<typename blend_type>
struct fill_span_vertex_shade_vertex
{
    int32_t frame_stride;
    float* depth_buffer;
    ARGB* frame_buffer;

    float* depth_addr;
    uint32_t* frame_addr;

    void setup(const config* cfg)
    {
        frame_stride = cfg->frame_stride;
        depth_buffer = cfg->depth_buffer;
        frame_buffer = cfg->frame_buffer;
    }

    void setup_face() {}

    force_inline void setup_span(int32_t y, int32_t x0)
    {
        int32_t start{ frame_stride * y + x0 };
        depth_addr = &depth_buffer[start];
        frame_addr = reinterpret_cast<uint32_t*>(&frame_buffer[start]);
    }

    force_inline void write(const interp<9>& interp)
    {
        enum { color_r, color_g, color_b, color_a, shade_r, shade_g, shade_b };
        if (*depth_addr > interp.depth)
        {
            uint32_t color
            {
                (((((uint32_t)interp.attrib_int[color_a] << 8u)                                               ) & 0xFF000000u)       ) +
                (((((uint32_t)interp.attrib_int[color_r] >> 8u) * ((uint32_t)interp.attrib_int[shade_r] >> 8u)) & 0xFF000000u) >>  8u) +
                (((((uint32_t)interp.attrib_int[color_g] >> 8u) * ((uint32_t)interp.attrib_int[shade_g] >> 8u)) & 0xFF000000u) >> 16u) +
                (((((uint32_t)interp.attrib_int[color_b] >> 8u) * ((uint32_t)interp.attrib_int[shade_b] >> 8u))              ) >> 24u)
            };
            *frame_addr = blend_type::process(*frame_addr, color);
            *depth_addr = interp.depth;
        }
    }

    force_inline void advance()
    {
        depth_addr++;
        frame_addr++;
    }
};

using fill_span_vertex_shade_vertex_blend_none = fill_span_vertex_shade_vertex<blend_none>;
using fill_span_vertex_shade_vertex_blend_add = fill_span_vertex_shade_vertex<blend_add>;
using fill_span_vertex_shade_vertex_blend_mul = fill_span_vertex_shade_vertex<blend_mul>;
using fill_span_vertex_shade_vertex_blend_alpha = fill_span_vertex_shade_vertex<blend_alpha>;

} // namespace lib3d::raster
