// cpu_dispatch.cpp — PolyAVX 运行时 CPU 调度与 SIMD 复数乘法实现
//
// 本文件包含：
//   1. 基于 SSE3 的复数乘法实现（通用回退方案）
//   2. 基于 AVX 的复数乘法实现（需要 AVX 和 FMA3 支持）
//   3. 基于 AVX-512 的复数乘法实现（需要 AVX-512F 支持）
//   4. 运行时 CPU 特性检测和调度器
//
// 作者: yuzheng2026 (与 DeepSeek AI 协作开发)
// 许可证: GNU GPLv3 or any later version

#include "poly_avx.hpp"
#include <emmintrin.h>
#include <pmmintrin.h>
#ifdef __AVX__
#include <immintrin.h>
#endif

namespace poly_avx {

// ==================== SSE3 版本 ====================
// 该版本使用 SSE3 的 _mm_addsub_pd 和 FMA3（如果可用）。
// 一次处理 4 个复数（即 8 个双精度浮点数），并进行循环展开。
// 它是所有 x86-64 CPU 都支持的回退实现。

static void pointwise_mul_sse3(cd* A, const cd* B, int len) {
    const int step = 4;   // 每次循环处理 4 个复数
    int i = 0;

    // 主循环：每次处理 4 个复数
    for (; i <= len - step; i += step) {
        // 预取下一批数据到 L1 缓存，减少缓存未命中
        _mm_prefetch((const char*)&A[i + step], _MM_HINT_T0);
        _mm_prefetch((const char*)&B[i + step], _MM_HINT_T0);

        // 加载 4 个复数（每个复数占一个 __m128d：实部、虚部）
        __m128d a0 = _mm_loadu_pd((double*)&A[i]);
        __m128d b0 = _mm_loadu_pd((double*)&B[i]);
        __m128d a1 = _mm_loadu_pd((double*)&A[i + 1]);
        __m128d b1 = _mm_loadu_pd((double*)&B[i + 1]);
        __m128d a2 = _mm_loadu_pd((double*)&A[i + 2]);
        __m128d b2 = _mm_loadu_pd((double*)&B[i + 2]);
        __m128d a3 = _mm_loadu_pd((double*)&A[i + 3]);
        __m128d b3 = _mm_loadu_pd((double*)&B[i + 3]);

#if defined(__FMA__)
        // 使用 FMA3 指令，减少乘加次数，提高精度和速度。
        // 复数乘法：(a0r + i*a0i) * (b0r + i*b0i)
        //   real = a0r*b0r - a0i*b0i
        //   imag = a0r*b0i + a0i*b0r
        // 这里通过 shuffle 和 fused multiply-add 实现。

        // 第 1 个复数
        __m128d a0_ar = _mm_movedup_pd(a0);          // [a0r, a0r]
        __m128d a0_ai = _mm_unpackhi_pd(a0, a0);     // [a0i, a0i]
        __m128d b0_sw = _mm_shuffle_pd(b0, b0, 1);   // [b0i, b0r]
        __m128d real0 = _mm_fnmadd_pd(a0_ai, b0_sw, _mm_mul_pd(a0_ar, b0));
        __m128d imag0 = _mm_fmadd_pd(a0_ar, b0_sw, _mm_mul_pd(a0_ai, b0));
        _mm_storeu_pd((double*)&A[i], _mm_shuffle_pd(real0, imag0, 0));

        // 第 2 个复数
        __m128d a1_ar = _mm_movedup_pd(a1);
        __m128d a1_ai = _mm_unpackhi_pd(a1, a1);
        __m128d b1_sw = _mm_shuffle_pd(b1, b1, 1);
        __m128d real1 = _mm_fnmadd_pd(a1_ai, b1_sw, _mm_mul_pd(a1_ar, b1));
        __m128d imag1 = _mm_fmadd_pd(a1_ar, b1_sw, _mm_mul_pd(a1_ai, b1));
        _mm_storeu_pd((double*)&A[i + 1], _mm_shuffle_pd(real1, imag1, 0));

        // 第 3 个复数
        __m128d a2_ar = _mm_movedup_pd(a2);
        __m128d a2_ai = _mm_unpackhi_pd(a2, a2);
        __m128d b2_sw = _mm_shuffle_pd(b2, b2, 1);
        __m128d real2 = _mm_fnmadd_pd(a2_ai, b2_sw, _mm_mul_pd(a2_ar, b2));
        __m128d imag2 = _mm_fmadd_pd(a2_ar, b2_sw, _mm_mul_pd(a2_ai, b2));
        _mm_storeu_pd((double*)&A[i + 2], _mm_shuffle_pd(real2, imag2, 0));

        // 第 4 个复数
        __m128d a3_ar = _mm_movedup_pd(a3);
        __m128d a3_ai = _mm_unpackhi_pd(a3, a3);
        __m128d b3_sw = _mm_shuffle_pd(b3, b3, 1);
        __m128d real3 = _mm_fnmadd_pd(a3_ai, b3_sw, _mm_mul_pd(a3_ar, b3));
        __m128d imag3 = _mm_fmadd_pd(a3_ar, b3_sw, _mm_mul_pd(a3_ai, b3));
        _mm_storeu_pd((double*)&A[i + 3], _mm_shuffle_pd(real3, imag3, 0));
#else
        // 无 FMA 时使用 SSE3 的 _mm_addsub_pd 指令
        for (int j = 0; j < step; ++j) {
            __m128d a = _mm_loadu_pd((double*)&A[i + j]);
            __m128d b = _mm_loadu_pd((double*)&B[i + j]);
            __m128d b_sw = _mm_shuffle_pd(b, b, 1);   // [bi, br]
            __m128d m1 = _mm_mul_pd(a, b);
            __m128d m2 = _mm_mul_pd(a, b_sw);
            __m128d X = _mm_unpacklo_pd(m1, m2);
            __m128d Y = _mm_unpackhi_pd(m1, m2);
            __m128d res = _mm_addsub_pd(X, Y);         // [real, imag]
            _mm_storeu_pd((double*)&A[i + j], res);
        }
#endif
    }

    // 剩余不足 4 个的复数用标量处理
    for (; i < len; ++i) {
        A[i] *= B[i];
    }
}

// ==================== AVX 版本 ====================
// 使用 AVX 指令集，一次处理 8 个复数（利用 256 位寄存器）。
// 仅当编译时定义了 __AVX__ 且运行时 CPU 支持 AVX 时才会被调用。

#ifdef __AVX__
static void pointwise_mul_avx(cd* A, const cd* B, int len) {
    const int step = 8;   // 每次循环处理 8 个复数
    int i = 0;

    // 主循环：处理 8 个复数
    for (; i <= len - step; i += step) {
        // 预取下一批数据
        _mm_prefetch((const char*)&A[i + step], _MM_HINT_T0);
        _mm_prefetch((const char*)&B[i + step], _MM_HINT_T0);

        // 加载 4 组，每组 2 个复数（一个 __m256d 包含 4 个 double）
        __m256d a0 = _mm256_loadu_pd((double*)&A[i]);
        __m256d b0 = _mm256_loadu_pd((double*)&B[i]);
        __m256d a1 = _mm256_loadu_pd((double*)&A[i + 2]);
        __m256d b1 = _mm256_loadu_pd((double*)&B[i + 2]);
        __m256d a2 = _mm256_loadu_pd((double*)&A[i + 4]);
        __m256d b2 = _mm256_loadu_pd((double*)&B[i + 4]);
        __m256d a3 = _mm256_loadu_pd((double*)&A[i + 6]);
        __m256d b3 = _mm256_loadu_pd((double*)&B[i + 6]);

        // 第 1 组
        __m256d b0_sw = _mm256_permute_pd(b0, 0x5);
        __m256d a0_ar = _mm256_permute_pd(a0, 0x0);   // [ar0, ar0, ar1, ar1]
        __m256d a0_ai = _mm256_permute_pd(a0, 0xF);   // [ai0, ai0, ai1, ai1]
        __m256d real0 = _mm256_fnmadd_pd(a0_ai, b0_sw, _mm256_mul_pd(a0_ar, b0));
        __m256d imag0 = _mm256_fmadd_pd(a0_ar, b0_sw, _mm256_mul_pd(a0_ai, b0));
        _mm256_storeu_pd((double*)&A[i], _mm256_shuffle_pd(real0, imag0, 0));

        // 第 2 组
        __m256d b1_sw = _mm256_permute_pd(b1, 0x5);
        __m256d a1_ar = _mm256_permute_pd(a1, 0x0);
        __m256d a1_ai = _mm256_permute_pd(a1, 0xF);
        __m256d real1 = _mm256_fnmadd_pd(a1_ai, b1_sw, _mm256_mul_pd(a1_ar, b1));
        __m256d imag1 = _mm256_fmadd_pd(a1_ar, b1_sw, _mm256_mul_pd(a1_ai, b1));
        _mm256_storeu_pd((double*)&A[i + 2], _mm256_shuffle_pd(real1, imag1, 0));

        // 第 3 组
        __m256d b2_sw = _mm256_permute_pd(b2, 0x5);
        __m256d a2_ar = _mm256_permute_pd(a2, 0x0);
        __m256d a2_ai = _mm256_permute_pd(a2, 0xF);
        __m256d real2 = _mm256_fnmadd_pd(a2_ai, b2_sw, _mm256_mul_pd(a2_ar, b2));
        __m256d imag2 = _mm256_fmadd_pd(a2_ar, b2_sw, _mm256_mul_pd(a2_ai, b2));
        _mm256_storeu_pd((double*)&A[i + 4], _mm256_shuffle_pd(real2, imag2, 0));

        // 第 4 组
        __m256d b3_sw = _mm256_permute_pd(b3, 0x5);
        __m256d a3_ar = _mm256_permute_pd(a3, 0x0);
        __m256d a3_ai = _mm256_permute_pd(a3, 0xF);
        __m256d real3 = _mm256_fnmadd_pd(a3_ai, b3_sw, _mm256_mul_pd(a3_ar, b3));
        __m256d imag3 = _mm256_fmadd_pd(a3_ar, b3_sw, _mm256_mul_pd(a3_ai, b3));
        _mm256_storeu_pd((double*)&A[i + 6], _mm256_shuffle_pd(real3, imag3, 0));
    }

    // 剩余不足 8 个的复数，逐个调用 SSE3 版本处理（或标量）
    for (; i < len; ++i) {
        pointwise_mul_sse3(A + i, B + i, 1);
    }
}
#else
static void pointwise_mul_avx(cd*, const cd*, int) { /* 未编译 */ }
#endif

// ==================== AVX-512 版本 ====================
// 使用 AVX-512 指令集，一次处理 16 个复数。
// 仅当编译时定义了 __AVX512F__ 且运行时 CPU 支持 AVX-512F 时才会被调用。
// 注意：当前代码假定同时支持 FMA3 和 AVX-512F。

#if defined(__AVX512F__)
static void pointwise_mul_avx512(cd* A, const cd* B, int len) {
    const int step = 16;   // 每次循环处理 16 个复数
    int i = 0;

    for (; i <= len - step; i += step) {
        // 预取下一批数据
        _mm_prefetch((const char*)&A[i + step], _MM_HINT_T0);
        _mm_prefetch((const char*)&B[i + step], _MM_HINT_T0);

        // 加载 4 组，每组 4 个复数（一个 __m512d 包含 8 个 double）
        __m512d a0 = _mm512_loadu_pd((double*)&A[i]);
        __m512d b0 = _mm512_loadu_pd((double*)&B[i]);
        __m512d a1 = _mm512_loadu_pd((double*)&A[i + 4]);
        __m512d b1 = _mm512_loadu_pd((double*)&B[i + 4]);
        __m512d a2 = _mm512_loadu_pd((double*)&A[i + 8]);
        __m512d b2 = _mm512_loadu_pd((double*)&B[i + 8]);
        __m512d a3 = _mm512_loadu_pd((double*)&A[i + 12]);
        __m512d b3 = _mm512_loadu_pd((double*)&B[i + 12]);

        // 第一组 4 个复数
        __m512d b0_sw = _mm512_permute_pd(b0, 0x55);
        __m512d real0 = _mm512_fnmadd_pd(_mm512_permute_pd(a0, 0xFF), b0_sw,
                                         _mm512_mul_pd(_mm512_permute_pd(a0, 0x00), b0));
        __m512d imag0 = _mm512_fmadd_pd(_mm512_permute_pd(a0, 0x00), b0_sw,
                                         _mm512_mul_pd(_mm512_permute_pd(a0, 0xFF), b0));
        _mm512_storeu_pd((double*)&A[i], _mm512_shuffle_pd(real0, imag0, 0));

        // 第二组 4 个复数
        __m512d b1_sw = _mm512_permute_pd(b1, 0x55);
        __m512d real1 = _mm512_fnmadd_pd(_mm512_permute_pd(a1, 0xFF), b1_sw,
                                         _mm512_mul_pd(_mm512_permute_pd(a1, 0x00), b1));
        __m512d imag1 = _mm512_fmadd_pd(_mm512_permute_pd(a1, 0x00), b1_sw,
                                         _mm512_mul_pd(_mm512_permute_pd(a1, 0xFF), b1));
        _mm512_storeu_pd((double*)&A[i + 4], _mm512_shuffle_pd(real1, imag1, 0));

        // 第三组 4 个复数
        __m512d b2_sw = _mm512_permute_pd(b2, 0x55);
        __m512d real2 = _mm512_fnmadd_pd(_mm512_permute_pd(a2, 0xFF), b2_sw,
                                         _mm512_mul_pd(_mm512_permute_pd(a2, 0x00), b2));
        __m512d imag2 = _mm512_fmadd_pd(_mm512_permute_pd(a2, 0x00), b2_sw,
                                         _mm512_mul_pd(_mm512_permute_pd(a2, 0xFF), b2));
        _mm512_storeu_pd((double*)&A[i + 8], _mm512_shuffle_pd(real2, imag2, 0));

        // 第四组 4 个复数
        __m512d b3_sw = _mm512_permute_pd(b3, 0x55);
        __m512d real3 = _mm512_fnmadd_pd(_mm512_permute_pd(a3, 0xFF), b3_sw,
                                         _mm512_mul_pd(_mm512_permute_pd(a3, 0x00), b3));
        __m512d imag3 = _mm512_fmadd_pd(_mm512_permute_pd(a3, 0x00), b3_sw,
                                         _mm512_mul_pd(_mm512_permute_pd(a3, 0xFF), b3));
        _mm512_storeu_pd((double*)&A[i + 12], _mm512_shuffle_pd(real3, imag3, 0));
    }

    // 剩余不足 16 个的复数，调用 SSE3 版本处理
    for (int j = len & ~15; j < len; ++j) {
        pointwise_mul_sse3(A + j, B + j, 1);
    }
}
#else
static void pointwise_mul_avx512(cd*, const cd*, int) { /* 未编译 */ }
#endif

// ==================== 运行时调度 ====================
// 全局函数指针，默认为 SSE3 实现，在启动时根据 CPU 特性自动调整。

pointwise_mul_func pointwise_mul = pointwise_mul_sse3;

void init_cpu_dispatch() {
    // 优先尝试 AVX-512
#ifdef __AVX512F__
    if (__builtin_cpu_supports("avx512f")) {
        pointwise_mul = pointwise_mul_avx512;
        return;
    }
#endif
    // 其次尝试 AVX
#ifdef __AVX__
    if (__builtin_cpu_supports("avx")) {
        pointwise_mul = pointwise_mul_avx;
        return;
    }
#endif
    // 默认使用 SSE3（所有 x86-64 CPU 都支持）
    pointwise_mul = pointwise_mul_sse3;
}

// 自动初始化对象：在 main() 之前执行一次 CPU 调度初始化
namespace {
    struct AutoInit {
        AutoInit() { init_cpu_dispatch(); }
    } auto_init;
}

} // namespace poly_avx
