#pragma once

namespace lib3d::raster
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

force_inline uint32_t bilinear(
    uint32_t v0, uint32_t v1,
    uint32_t v2, uint32_t v3,
    uint32_t a0,
    uint32_t a1)
{
    uint32_t w3{ a0 * a1 >> 8u };
    uint32_t w2{ a1 - w3 };
    uint32_t w1{ a0 - w3 };
    uint32_t w0{ 0x100u - a0 - w2 };
    uint32_t r0
    {
        (v0 & 0x00FF00FFu) * w0 +
        (v1 & 0x00FF00FFu) * w1 +
        (v2 & 0x00FF00FFu) * w2 +
        (v3 & 0x00FF00FFu) * w3
    };
    r0 >>= 8u;
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
    return (r0 & 0x00FF00FFu) + (r1 & 0xFF00FF00u);
}

force_inline uint8_t sample_nearest(int32_t u, int32_t v, int32_t vshift, uint8_t* pt)
{
    return pt[(u >> 16) + (v >> vshift)];
}

force_inline uint32_t sample_bilinear(int32_t u, int32_t v, int32_t vshift, uint32_t* pt)
{
    int32_t ua{ (u >> 8) & 0xFF };
    int32_t va{ (v >> 8) & 0xFF };
    int32_t u0{ u >> 16 };
    int32_t u1{ (u + 0xFFFF) >> 16 };
    int32_t v0{ v >> 16 };
    int32_t v1{ (v + 0xFFFF) >> 16 };
    v0 <<= vshift;
    v1 <<= vshift;
    return bilinear(
        pt[u0 + v0],
        pt[u1 + v0],
        pt[u0 + v1],
        pt[u1 + v1],
        ua,
        va);
}

} // namespace lib3d::raster
