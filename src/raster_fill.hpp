#pragma once

namespace blib3d::raster
{

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

//------------------------------------------------------------------------------

struct blend_none
{
    static force_inline void process(uint32_t* buffer, uint32_t color)
    {
        *buffer = color;
    }
};

struct blend_add
{
    static force_inline void process(uint32_t* buffer, uint32_t color)
    {
        *buffer = adds_X888(*buffer, color);
    }
};

struct blend_mul
{
    static force_inline void process(uint32_t* buffer, uint32_t color)
    {
        *buffer = mul_X888(*buffer, color);
    }
};

struct blend_alpha
{
    static force_inline void process(uint32_t* buffer, uint32_t color)
    {
        *buffer = alpha_8888(*buffer, color);
    }
};

//------------------------------------------------------------------------------

struct depth_off
{
    static force_inline bool process_test(float* /*buffer*/, float /*depth*/)
    {
        return true;
    }

    static force_inline void process_write(float* /*buffer*/, float /*depth*/)
    {
    }
};

struct depth_test
{
    static force_inline bool process_test(float* buffer, float depth)
    {
        return *buffer > depth;
    }

    static force_inline void process_write(float* /*buffer*/, float /*depth*/)
    {
    }
};

struct depth_test_write
{
    static force_inline bool process_test(float* buffer, float depth)
    {
        return *buffer > depth;
    }

    static force_inline void process_write(float* buffer, float depth)
    {
        *buffer = depth;
    }
};

//------------------------------------------------------------------------------

force_inline uint32_t bilinear88(
    uint32_t v0, uint32_t v1,
    uint32_t v2, uint32_t v3,
    uint32_t a0, uint32_t a1)
{
    uint32_t w3{ a0 * a1 >> 8u };
    uint32_t w2{ a1 - w3 };
    uint32_t w1{ a0 - w3 };
    uint32_t w0{ 256u + w3 - a0 - a1 };
    uint32_t r0
    {
        (v0 & 0x00FF00FFu) * w0 +
        (v1 & 0x00FF00FFu) * w1 +
        (v2 & 0x00FF00FFu) * w2 +
        (v3 & 0x00FF00FFu) * w3
    };
    v0 >>= 8u;
    v1 >>= 8u;
    v2 >>= 8u;
    v3 >>= 8u;
    uint32_t r1
    {
        (v0 & 0x00FF00FFu) * w0 +
        (v1 & 0x00FF00FFu) * w1 +
        (v2 & 0x00FF00FFu) * w2 +
        (v3 & 0x00FF00FFu) * w3
    };
    return ((r0 >> 8u) & 0x00FF00FFu) + (r1 & 0xFF00FF00u);
}

force_inline uint32_t bilinear44(
    uint32_t v0, uint32_t v1,
    uint32_t v2, uint32_t v3,
    uint32_t a0, uint32_t a1)
{
    uint32_t w3{ a0 * a1 >> 4u };
    uint32_t w2{ a1 - w3 };
    uint32_t w1{ a0 - w3 };
    uint32_t w0{ 16u + w3 - a0 - a1 };
    return
        ((v0 & 0xF0F0F0F0u) >> 4u) * w0 +
        ((v1 & 0xF0F0F0F0u) >> 4u) * w1 +
        ((v2 & 0xF0F0F0F0u) >> 4u) * w2 +
        ((v3 & 0xF0F0F0F0u) >> 4u) * w3;
}

force_inline uint32_t bilinear53(
    uint32_t v0, uint32_t v1,
    uint32_t v2, uint32_t v3,
    uint32_t a0, uint32_t a1)
{
    uint32_t w3{ a0 * a1 >> 3u };
    uint32_t w2{ a1 - w3 };
    uint32_t w1{ a0 - w3 };
    uint32_t w0{ 8u + w3 - a0 - a1 };
    return 
        ((v0 & 0xF8F8F8F8u) >> 3u) * w0 +
        ((v1 & 0xF8F8F8F8u) >> 3u) * w1 +
        ((v2 & 0xF8F8F8F8u) >> 3u) * w2 +
        ((v3 & 0xF8F8F8F8u) >> 3u) * w3;
}

force_inline uint32_t bilinear62(
    uint32_t v0, uint32_t v1,
    uint32_t v2, uint32_t v3,
    uint32_t a0, uint32_t a1)
{
    uint32_t w3{ a0 * a1 >> 2u };
    uint32_t w2{ a1 - w3 };
    uint32_t w1{ a0 - w3 };
    uint32_t w0{ 4u + w3 - a0 - a1 };
    return
        ((v0 & 0xFCFCFCFCu) >> 2u) * w0 +
        ((v1 & 0xFCFCFCFCu) >> 2u) * w1 +
        ((v2 & 0xFCFCFCFCu) >> 2u) * w2 +
        ((v3 & 0xFCFCFCFCu) >> 2u) * w3;
}

//------------------------------------------------------------------------------

force_inline uint32_t sample_lightmap(int32_t u, int32_t v, int32_t vshift, const uint32_t* pt)
{
    int32_t r0{ (u >> 16) + (v >> 16 << vshift) };
    int32_t r1{ r0 + (1 << vshift) };
    return bilinear88(
        pt[r0],
        pt[r0 + 1],
        pt[r1],
        pt[r1 + 1],
        (u >> 8) & 0xFF,
        (v >> 8) & 0xFF);
}

//------------------------------------------------------------------------------

struct sample_nearest
{
    static force_inline int32_t process_coord(int32_t attribute)
    {
        return attribute;
    }

    static force_inline uint32_t process_texel(int32_t s, int32_t t, int32_t smask, int32_t tmask, int32_t tshift, const uint32_t* plut, const uint8_t* pdata)
    {
        return plut[pdata[((s & smask) >> 16) + ((t & tmask) >> tshift)]];
    }
};

struct sample_bilinear
{
    static force_inline int32_t process_coord(int32_t attribute)
    {
        return attribute - 0x8000;
    }

    static force_inline uint32_t process_texel(int32_t s, int32_t t, int32_t smask, int32_t tmask, int32_t tshift, const uint32_t* plut, const uint8_t* pdata)
    {
        int32_t s0{ s };
        int32_t s1{ s + 0x10000 };
        int32_t t0{ t };
        int32_t t1{ t + 0x10000 };
        s0 = (s0 & smask) >> 16;
        s1 = (s1 & smask) >> 16;
        t0 = (t0 & tmask) >> tshift;
        t1 = (t1 & tmask) >> tshift;
        //return bilinear88(
        //    plut[pdata[s0 + t0]],
        //    plut[pdata[s1 + t0]],
        //    plut[pdata[s0 + t1]],
        //    plut[pdata[s1 + t1]],
        //    (s >> 16) & 0xFF,
        //    (t >> 16) & 0xFF);
        //return bilinear44(
        //    plut[pdata[s0 + t0]],
        //    plut[pdata[s1 + t0]],
        //    plut[pdata[s0 + t1]],
        //    plut[pdata[s1 + t1]],
        //    (s >> 12) & 0xF,
        //    (t >> 12) & 0xF);
        //return bilinear53(
        //    plut[pdata[s0 + t0]],
        //    plut[pdata[s1 + t0]],
        //    plut[pdata[s0 + t1]],
        //    plut[pdata[s1 + t1]],
        //    (s >> 13) & 0x7,
        //    (t >> 13) & 0x7);
        return bilinear62(
            plut[pdata[s0 + t0]],
            plut[pdata[s1 + t0]],
            plut[pdata[s0 + t1]],
            plut[pdata[s1 + t1]],
            (s >> 14) & 0x3,
            (t >> 14) & 0x3);
    }
};

//------------------------------------------------------------------------------

struct mask_texture_on
{
    static force_inline bool process(uint32_t v)
    {
        return v > 0x7FFFFFFFu;
    }
};

struct mask_texture_off
{
    static force_inline bool process(uint32_t /*v*/)
    {
        return true;
    }
};

//------------------------------------------------------------------------------

//constexpr uint32_t dither_size{ 2 };
//constexpr uint32_t dither_mask{ dither_size - 1 };
//constexpr uint32_t dither_offset[dither_size * dither_size * 2]
//{
//    0 << 14, 2 << 14,
//    3 << 14, 1 << 14,
//    0 << 14, 2 << 14,
//    3 << 14, 1 << 14
//};

//constexpr uint32_t dither_size{ 4 };
//constexpr uint32_t dither_mask{ dither_size - 1 };
//constexpr uint32_t dither_offset[dither_size * dither_size * 2]
//{
//     0 << 12,  8 << 12,  2 << 12, 10 << 12,
//    12 << 12,  4 << 12, 14 << 12,  6 << 12,
//     3 << 12, 11 << 12,  1 << 12,  9 << 12,
//    15 << 12,  7 << 12, 13 << 12,  5 << 12,
//     0 << 12,  8 << 12,  2 << 12, 10 << 12,
//    12 << 12,  4 << 12, 14 << 12,  6 << 12,
//     3 << 12, 11 << 12,  1 << 12,  9 << 12,
//    15 << 12,  7 << 12, 13 << 12,  5 << 12,
//};

} // namespace blib3d::raster
