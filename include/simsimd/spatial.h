/**
 *  @brief Collection of Similarity Measures, SIMD-accelerated with SSE, AVX, NEON, SVE.
 *
 *  @author Ash Vardanian
 *  @date March 14, 2023
 *
 *  x86 intrinsics: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/
 *  Arm intrinsics: https://developer.arm.com/architectures/instruction-sets/intrinsics/
 *  Detecting target CPU features at compile time: https://stackoverflow.com/a/28939692/2766161
 */

#pragma once
#include "types.h"

#ifndef SIMSIMD_RSQRT
#include <math.h>
#define SIMSIMD_RSQRT sqrtf
#endif

#define MAKE_L2SQ(name, input_type, accumulator_type)                                                                  \
    inline static simsimd_f32_t simsimd_##name##_##input_type##_l2sq(                                                  \
        simsimd_##input_type##_t const* a, simsimd_##input_type##_t const* b, simsimd_size_t d) {                      \
        simsimd_##accumulator_type##_t d2 = 0;                                                                         \
        for (simsimd_size_t i = 0; i != d; ++i) {                                                                      \
            simsimd_##accumulator_type##_t ai = a[i];                                                                  \
            simsimd_##accumulator_type##_t bi = b[i];                                                                  \
            d2 += (ai - bi) * (ai - bi);                                                                               \
        }                                                                                                              \
        return d2;                                                                                                     \
    }

#define MAKE_IP(name, input_type, accumulator_type)                                                                    \
    inline static simsimd_f32_t simsimd_##name##_##input_type##_ip(                                                    \
        simsimd_##input_type##_t const* a, simsimd_##input_type##_t const* b, simsimd_size_t d) {                      \
        simsimd_##accumulator_type##_t ab = 0;                                                                         \
        for (simsimd_size_t i = 0; i != d; ++i) {                                                                      \
            simsimd_##accumulator_type##_t ai = a[i];                                                                  \
            simsimd_##accumulator_type##_t bi = b[i];                                                                  \
            ab += ai * bi;                                                                                             \
        }                                                                                                              \
        return 1 - ab;                                                                                                 \
    }

#define MAKE_COS(name, input_type, accumulator_type)                                                                   \
    inline static simsimd_f32_t simsimd_##name##_##input_type##_cos(                                                   \
        simsimd_##input_type##_t const* a, simsimd_##input_type##_t const* b, simsimd_size_t d) {                      \
        simsimd_##accumulator_type##_t ab = 0, a2 = 0, b2 = 0;                                                         \
        for (simsimd_size_t i = 0; i != d; ++i) {                                                                      \
            simsimd_##accumulator_type##_t ai = a[i];                                                                  \
            simsimd_##accumulator_type##_t bi = b[i];                                                                  \
            ab += ai * bi;                                                                                             \
            a2 += ai * ai;                                                                                             \
            b2 += bi * bi;                                                                                             \
        }                                                                                                              \
        double result = ab;                                                                                            \
        result *= SIMSIMD_RSQRT(a2);                                                                                   \
        result *= SIMSIMD_RSQRT(b2);                                                                                   \
        return 1 - result;                                                                                             \
    }

#ifdef __cplusplus
extern "C" {
#endif

MAKE_L2SQ(auto, f32, f32) // simsimd_auto_f32_l2sq
MAKE_IP(auto, f32, f32)   // simsimd_auto_f32_ip
MAKE_COS(auto, f32, f32)  // simsimd_auto_f32_cos

MAKE_L2SQ(auto, f16, f32) // simsimd_auto_f16_l2sq
MAKE_IP(auto, f16, f32)   // simsimd_auto_f16_ip
MAKE_COS(auto, f16, f32)  // simsimd_auto_f16_cos

MAKE_L2SQ(auto, i8, i32) // simsimd_auto_i8_l2sq
MAKE_COS(auto, i8, i32)  // simsimd_auto_i8_cos

inline static simsimd_f32_t simsimd_auto_i8_ip(simsimd_i8_t const* a, simsimd_i8_t const* b, simsimd_size_t d) {
    return simsimd_auto_i8_cos(a, b, d);
}

MAKE_L2SQ(accurate, f32, f64) // simsimd_accurate_f32_l2sq
MAKE_IP(accurate, f32, f64)   // simsimd_accurate_f32_ip
MAKE_COS(accurate, f32, f64)  // simsimd_accurate_f32_cos

MAKE_L2SQ(accurate, f16, f64) // simsimd_accurate_f16_l2sq
MAKE_IP(accurate, f16, f64)   // simsimd_accurate_f16_ip
MAKE_COS(accurate, f16, f64)  // simsimd_accurate_f16_cos

MAKE_L2SQ(accurate, i8, i32) // simsimd_accurate_i8_l2sq
MAKE_COS(accurate, i8, i32)  // simsimd_accurate_i8_cos

inline static simsimd_f32_t simsimd_accurate_i8_ip(simsimd_i8_t const* a, simsimd_i8_t const* b, simsimd_size_t d) {
    return simsimd_accurate_i8_cos(a, b, d);
}

#undef MAKE_L2SQ
#undef MAKE_IP
#undef MAKE_COS

#if SIMSIMD_TARGET_ARM
#if SIMSIMD_TARGET_ARM_NEON

/*
 *  @file   arm_neon_f32.h
 *  @brief  Arm NEON implementation of the most common similarity metrics for 32-bit floating point numbers.
 *  @author Ash Vardanian
 *
 *  - Implements: L2 squared, inner product, cosine similarity.
 *  - Uses `f32` for storage and `f32` for accumulation.
 *  - Requires compiler capabilities: +simd.
 */

__attribute__((target("+simd"))) //
inline static simsimd_f32_t
simsimd_neon_f32_l2sq(simsimd_f32_t const* a, simsimd_f32_t const* b, simsimd_size_t d) {
    float32x4_t sum_vec = vdupq_n_f32(0);
    simsimd_size_t i = 0;
    for (; i + 4 <= d; i += 4) {
        float32x4_t a_vec = vld1q_f32(a + i);
        float32x4_t b_vec = vld1q_f32(b + i);
        float32x4_t diff_vec = vsubq_f32(a_vec, b_vec);
        sum_vec = vfmaq_f32(sum_vec, diff_vec, diff_vec);
    }
    simsimd_f32_t sum = vaddvq_f32(sum_vec);
    for (; i < d; ++i) {
        simsimd_f32_t diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum;
}

__attribute__((target("+simd"))) //
inline static simsimd_f32_t
simsimd_neon_f32_ip(simsimd_f32_t const* a, simsimd_f32_t const* b, simsimd_size_t d) {
    float32x4_t ab_vec = vdupq_n_f32(0);
    simsimd_size_t i = 0;
    for (; i + 4 <= d; i += 4) {
        float32x4_t a_vec = vld1q_f32(a + i);
        float32x4_t b_vec = vld1q_f32(b + i);
        ab_vec = vfmaq_f32(ab_vec, a_vec, b_vec);
    }
    simsimd_f32_t ab = vaddvq_f32(ab_vec);
    for (; i < d; ++i)
        ab += a[i] * b[i];
    return 1 - ab;
}

__attribute__((target("+simd"))) //
inline static simsimd_f32_t
simsimd_neon_f32_cos(simsimd_f32_t const* a, simsimd_f32_t const* b, simsimd_size_t d) {
    float32x4_t ab_vec = vdupq_n_f32(0), a2_vec = vdupq_n_f32(0), b2_vec = vdupq_n_f32(0);
    simsimd_size_t i = 0;
    for (; i + 4 <= d; i += 4) {
        float32x4_t a_vec = vld1q_f32(a + i);
        float32x4_t b_vec = vld1q_f32(b + i);
        ab_vec = vfmaq_f32(ab_vec, a_vec, b_vec);
        a2_vec = vfmaq_f32(a2_vec, a_vec, a_vec);
        b2_vec = vfmaq_f32(b2_vec, b_vec, b_vec);
    }
    simsimd_f32_t ab = vaddvq_f32(ab_vec), a2 = vaddvq_f32(a2_vec), b2 = vaddvq_f32(b2_vec);
    for (; i < d; ++i) {
        simsimd_f32_t ai = a[i], bi = b[i];
        ab += ai * bi, a2 += ai * ai, b2 += bi * bi;
    }

    // Avoid `simsimd_approximate_inverse_square_root` on Arm NEON
    simsimd_f32_t a2_b2_arr[2] = {a2, b2};
    vst1_f32(a2_b2_arr, vrsqrte_f32(vld1_f32(a2_b2_arr)));
    return 1 - ab * a2_b2_arr[0] * a2_b2_arr[1];
}

/*
 *  @file   arm_neon_f16.h
 *  @brief  Arm NEON implementation of the most common similarity metrics for 16-bit floating point numbers.
 *  @author Ash Vardanian
 *
 *  - Implements: L2 squared, inner product, cosine similarity.
 *  - Uses `f16` for storage and `f32` for accumulation, as the 16-bit FMA may not always be available.
 *  - Requires compiler capabilities: +simd+fp16.
 */

__attribute__((target("+simd+fp16"))) //
inline static simsimd_f32_t
simsimd_neon_f16_l2sq(simsimd_f16_t const* a, simsimd_f16_t const* b, simsimd_size_t d) {
    float32x4_t sum_vec = vdupq_n_f32(0);
    simsimd_size_t i = 0;
    for (; i + 4 <= d; i += 4) {
        float32x4_t a_vec = vcvt_f32_f16(vld1_f16((float16_t const*)a + i));
        float32x4_t b_vec = vcvt_f32_f16(vld1_f16((float16_t const*)b + i));
        float32x4_t diff_vec = vsubq_f32(a_vec, b_vec);
        sum_vec = vfmaq_f32(sum_vec, diff_vec, diff_vec);
    }
    simsimd_f32_t sum = vaddvq_f32(sum_vec);
    for (; i < d; ++i) {
        simsimd_f32_t diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum;
}

__attribute__((target("+simd+fp16"))) //
inline static simsimd_f32_t
simsimd_neon_f16_ip(simsimd_f16_t const* a, simsimd_f16_t const* b, simsimd_size_t d) {
    float32x4_t ab_vec = vdupq_n_f32(0);
    simsimd_size_t i = 0;
    for (; i + 4 <= d; i += 4) {
        float32x4_t a_vec = vcvt_f32_f16(vld1_f16((float16_t const*)a + i));
        float32x4_t b_vec = vcvt_f32_f16(vld1_f16((float16_t const*)b + i));
        ab_vec = vfmaq_f32(ab_vec, a_vec, b_vec);
    }
    simsimd_f32_t ab = vaddvq_f32(ab_vec);
    for (; i < d; ++i)
        ab += a[i] * b[i];
    return 1 - ab;
}

__attribute__((target("+simd+fp16"))) //
inline static simsimd_f32_t
simsimd_neon_f16_cos(simsimd_f16_t const* a, simsimd_f16_t const* b, simsimd_size_t d) {
    float32x4_t ab_vec = vdupq_n_f32(0), a2_vec = vdupq_n_f32(0), b2_vec = vdupq_n_f32(0);
    simsimd_size_t i = 0;
    for (; i + 4 <= d; i += 4) {
        float32x4_t a_vec = vcvt_f32_f16(vld1_f16((float16_t const*)a + i));
        float32x4_t b_vec = vcvt_f32_f16(vld1_f16((float16_t const*)b + i));
        ab_vec = vfmaq_f32(ab_vec, a_vec, b_vec);
        a2_vec = vfmaq_f32(a2_vec, a_vec, a_vec);
        b2_vec = vfmaq_f32(b2_vec, b_vec, b_vec);
    }
    simsimd_f32_t ab = vaddvq_f32(ab_vec), a2 = vaddvq_f32(a2_vec), b2 = vaddvq_f32(b2_vec);
    for (; i < d; ++i) {
        simsimd_f32_t ai = a[i], bi = b[i];
        ab += ai * bi, a2 += ai * ai, b2 += bi * bi;
    }

    // Avoid `simsimd_approximate_inverse_square_root` on Arm NEON
    simsimd_f32_t a2_b2_arr[2] = {a2, b2};
    float32x2_t a2_b2 = vld1_f32(a2_b2_arr);
    a2_b2 = vrsqrte_f32(a2_b2);
    vst1_f32(a2_b2_arr, a2_b2);
    return 1 - ab * a2_b2_arr[0] * a2_b2_arr[1];
}

/*
 *  @file   arm_neon_i8.h
 *  @brief  Arm NEON implementation of the most common similarity metrics for 8-bit signed integral numbers.
 *  @author Ash Vardanian
 *
 *  - Implements: L2 squared, cosine similarity, inner product (same as cosine).
 *  - Uses `i8` for storage, `i16` for multiplication, and `i32` for accumulation, if no better option is available.
 *  - Requires compiler capabilities: +simd+dotprod.
 */

__attribute__((target("+simd"))) //
inline static simsimd_f32_t
simsimd_neon_i8_l2sq(simsimd_i8_t const* a, simsimd_i8_t const* b, simsimd_size_t d) {
    int32x4_t d2_vec = vdupq_n_s32(0);
    simsimd_size_t i = 0;
    for (; i + 7 < d; i += 8) {
        int8x8_t a_vec = vld1_s8(a + i);
        int8x8_t b_vec = vld1_s8(b + i);
        int16x8_t a_vec16 = vmovl_s8(a_vec);
        int16x8_t b_vec16 = vmovl_s8(b_vec);
        int16x8_t d_vec = vsubq_s16(a_vec16, b_vec16);
        int32x4_t d_low = vmull_s16(vget_low_s16(d_vec), vget_low_s16(d_vec));
        int32x4_t d_high = vmull_s16(vget_high_s16(d_vec), vget_high_s16(d_vec));
        d2_vec = vaddq_s32(d2_vec, vaddq_s32(d_low, d_high));
    }
    int32_t d2 = vaddvq_s32(d2_vec);
    for (; i < d; ++i) {
        int32_t d = a[i] - b[i];
        d2 += d * d;
    }
    return d2;
}

__attribute__((target("+simd+dotprod"))) //
inline static simsimd_f32_t
simsimd_neon_i8_cos(simsimd_i8_t const* a, simsimd_i8_t const* b, simsimd_size_t d) {

    int32x4_t ab_vec = vdupq_n_s32(0);
    int32x4_t a2_vec = vdupq_n_s32(0);
    int32x4_t b2_vec = vdupq_n_s32(0);
    simsimd_size_t i = 0;

    // If the 128-bit `vdot_s32` intrinsic is unavailable, we can use the 64-bit `vdot_s32`.
    // for (simsimd_size_t i = 0; i != d; i += 8) {
    //     int16x8_t a_vec = vmovl_s8(vld1_s8(a + i));
    //     int16x8_t b_vec = vmovl_s8(vld1_s8(b + i));
    //     int16x8_t ab_part_vec = vmulq_s16(a_vec, b_vec);
    //     int16x8_t a2_part_vec = vmulq_s16(a_vec, a_vec);
    //     int16x8_t b2_part_vec = vmulq_s16(b_vec, b_vec);
    //     ab_vec = vaddq_s32(ab_vec, vaddq_s32(vmovl_s16(vget_high_s16(ab_part_vec)), //
    //                                          vmovl_s16(vget_low_s16(ab_part_vec))));
    //     a2_vec = vaddq_s32(a2_vec, vaddq_s32(vmovl_s16(vget_high_s16(a2_part_vec)), //
    //                                          vmovl_s16(vget_low_s16(a2_part_vec))));
    //     b2_vec = vaddq_s32(b2_vec, vaddq_s32(vmovl_s16(vget_high_s16(b2_part_vec)), //
    //                                          vmovl_s16(vget_low_s16(b2_part_vec))));
    // }
    for (; i + 15 < d; i += 16) {
        int8x16_t a_vec = vld1q_s8(a + i);
        int8x16_t b_vec = vld1q_s8(b + i);
        ab_vec = vdotq_s32(ab_vec, a_vec, b_vec);
        a2_vec = vdotq_s32(a2_vec, a_vec, a_vec);
        b2_vec = vdotq_s32(b2_vec, b_vec, b_vec);
    }

    int32_t ab = vaddvq_s32(ab_vec);
    int32_t a2 = vaddvq_s32(a2_vec);
    int32_t b2 = vaddvq_s32(b2_vec);

    // Take care of the tail:
    for (; i < d; ++i) {
        int32_t ai = a[i], bi = b[i];
        ab += ai * bi, a2 += ai * ai, b2 += bi * bi;
    }

    // Avoid `simsimd_approximate_inverse_square_root` on Arm NEON
    simsimd_f32_t a2_b2_arr[2] = {(simsimd_f32_t)a2, (simsimd_f32_t)b2};
    float32x2_t a2_b2 = vld1_f32(a2_b2_arr);
    a2_b2 = vrsqrte_f32(a2_b2);
    vst1_f32(a2_b2_arr, a2_b2);
    return 1 - ab * a2_b2_arr[0] * a2_b2_arr[1];
}

__attribute__((target("+simd"))) //
inline static simsimd_f32_t
simsimd_neon_i8_ip(simsimd_i8_t const* a, simsimd_i8_t const* b, simsimd_size_t d) {
    return simsimd_neon_i8_cos(a, b, d);
}

#endif // SIMSIMD_TARGET_ARM_NEON

#if SIMSIMD_TARGET_ARM_SVE

/*
 *  @file   arm_sve_f32.h
 *  @brief  Arm SVE implementation of the most common similarity metrics for 32-bit floating point numbers.
 *  @author Ash Vardanian
 *
 *  - Implements: L2 squared, inner product, cosine similarity.
 *  - Uses `f16` for both storage and `f32` for accumulation.
 *  - Requires compiler capabilities: +sve.
 */

__attribute__((target("+sve"))) //
inline static simsimd_f32_t
simsimd_sve_f32_l2sq(simsimd_f32_t const* a, simsimd_f32_t const* b, simsimd_size_t d) {
    simsimd_size_t i = 0;
    svfloat32_t d2_vec = svdupq_n_f32(0.f, 0.f, 0.f, 0.f);
    svbool_t pg_vec = svwhilelt_b32(i, d);
    do {
        svfloat32_t a_vec = svld1_f32(pg_vec, a + i);
        svfloat32_t b_vec = svld1_f32(pg_vec, b + i);
        svfloat32_t a_minus_b_vec = svsub_f32_x(pg_vec, a_vec, b_vec);
        d2_vec = svmla_f32_x(pg_vec, d2_vec, a_minus_b_vec, a_minus_b_vec);
        i += svcntw();
        pg_vec = svwhilelt_b32(i, d);
    } while (svptest_any(svptrue_b32(), pg_vec));
    simsimd_f32_t d2 = svaddv_f32(svptrue_b32(), d2_vec);
    return d2;
}
__attribute__((target("+sve"))) //
inline static simsimd_f32_t
simsimd_sve_f32_ip(simsimd_f32_t const* a, simsimd_f32_t const* b, simsimd_size_t d) {
    simsimd_size_t i = 0;
    svfloat32_t ab_vec = svdupq_n_f32(0.f, 0.f, 0.f, 0.f);
    svbool_t pg_vec = svwhilelt_b32(i, d);
    do {
        svfloat32_t a_vec = svld1_f32(pg_vec, a + i);
        svfloat32_t b_vec = svld1_f32(pg_vec, b + i);
        ab_vec = svmla_f32_x(pg_vec, ab_vec, a_vec, b_vec);
        i += svcntw();
        pg_vec = svwhilelt_b32(i, d);
    } while (svptest_any(svptrue_b32(), pg_vec));
    return 1 - svaddv_f32(svptrue_b32(), ab_vec);
}

__attribute__((target("+sve"))) //
inline static simsimd_f32_t
simsimd_sve_f32_cos(simsimd_f32_t const* a, simsimd_f32_t const* b, simsimd_size_t d) {
    simsimd_size_t i = 0;
    svfloat32_t ab_vec = svdupq_n_f32(0.f, 0.f, 0.f, 0.f);
    svfloat32_t a2_vec = svdupq_n_f32(0.f, 0.f, 0.f, 0.f);
    svfloat32_t b2_vec = svdupq_n_f32(0.f, 0.f, 0.f, 0.f);
    svbool_t pg_vec = svwhilelt_b32(i, d);
    do {
        svfloat32_t a_vec = svld1_f32(pg_vec, a + i);
        svfloat32_t b_vec = svld1_f32(pg_vec, b + i);
        ab_vec = svmla_f32_x(pg_vec, ab_vec, a_vec, b_vec);
        a2_vec = svmla_f32_x(pg_vec, a2_vec, a_vec, a_vec);
        b2_vec = svmla_f32_x(pg_vec, b2_vec, b_vec, b_vec);
        i += svcntw();
        pg_vec = svwhilelt_b32(i, d);
    } while (svptest_any(svptrue_b32(), pg_vec));

    simsimd_f32_t ab = svaddv_f32(svptrue_b32(), ab_vec);
    simsimd_f32_t a2 = svaddv_f32(svptrue_b32(), a2_vec);
    simsimd_f32_t b2 = svaddv_f32(svptrue_b32(), b2_vec);

    // Avoid `simsimd_approximate_inverse_square_root` on Arm NEON
    simsimd_f32_t a2_b2_arr[2] = {a2, b2};
    float32x2_t a2_b2 = vld1_f32(a2_b2_arr);
    a2_b2 = vrsqrte_f32(a2_b2);
    vst1_f32(a2_b2_arr, a2_b2);
    return 1 - ab * a2_b2_arr[0] * a2_b2_arr[1];
}

/*
 *  @file   arm_sve_f16.h
 *  @brief  Arm SVE implementation of the most common similarity metrics for 16-bit floating point numbers.
 *  @author Ash Vardanian
 *
 *  - Implements: L2 squared, inner product, cosine similarity.
 *  - Uses `f16` for both storage and `f32` for accumulation.
 *  - Requires compiler capabilities: +sve+fp16.
 */

__attribute__((target("+sve+fp16"))) //
inline static simsimd_f32_t
simsimd_sve_f16_l2sq(simsimd_f16_t const* a_enum, simsimd_f16_t const* b_enum, simsimd_size_t d) {
    simsimd_size_t i = 0;
    svfloat16_t d2_vec = svdupq_n_f16(0, 0, 0, 0, 0, 0, 0, 0);
    svbool_t pg_vec = svwhilelt_b16(i, d);
    simsimd_f16_t const* a = (simsimd_f16_t const*)(a_enum);
    simsimd_f16_t const* b = (simsimd_f16_t const*)(b_enum);
    do {
        svfloat16_t a_vec = svld1_f16(pg_vec, (float16_t const*)a + i);
        svfloat16_t b_vec = svld1_f16(pg_vec, (float16_t const*)b + i);
        svfloat16_t a_minus_b_vec = svsub_f16_x(pg_vec, a_vec, b_vec);
        d2_vec = svmla_f16_x(pg_vec, d2_vec, a_minus_b_vec, a_minus_b_vec);
        i += svcnth();
        pg_vec = svwhilelt_b16(i, d);
    } while (svptest_any(svptrue_b16(), pg_vec));
    float16_t d2_f16 = svaddv_f16(svptrue_b16(), d2_vec);
    return 1 - d2_f16;
}
__attribute__((target("+sve+fp16"))) //
inline static simsimd_f32_t
simsimd_sve_f16_ip(simsimd_f16_t const* a_enum, simsimd_f16_t const* b_enum, simsimd_size_t d) {
    simsimd_size_t i = 0;
    svfloat16_t ab_vec = svdupq_n_f16(0, 0, 0, 0, 0, 0, 0, 0);
    svbool_t pg_vec = svwhilelt_b16(i, d);
    simsimd_f16_t const* a = (simsimd_f16_t const*)(a_enum);
    simsimd_f16_t const* b = (simsimd_f16_t const*)(b_enum);
    do {
        svfloat16_t a_vec = svld1_f16(pg_vec, (float16_t const*)a + i);
        svfloat16_t b_vec = svld1_f16(pg_vec, (float16_t const*)b + i);
        ab_vec = svmla_f16_x(pg_vec, ab_vec, a_vec, b_vec);
        i += svcnth();
        pg_vec = svwhilelt_b16(i, d);
    } while (svptest_any(svptrue_b16(), pg_vec));
    simsimd_f16_t ab = svaddv_f16(svptrue_b16(), ab_vec);
    return 1 - ab;
}

__attribute__((target("+sve+fp16"))) //
inline static simsimd_f32_t
simsimd_sve_f16_cos(simsimd_f16_t const* a_enum, simsimd_f16_t const* b_enum, simsimd_size_t d) {
    simsimd_size_t i = 0;
    svfloat16_t ab_vec = svdupq_n_f16(0, 0, 0, 0, 0, 0, 0, 0);
    svfloat16_t a2_vec = svdupq_n_f16(0, 0, 0, 0, 0, 0, 0, 0);
    svfloat16_t b2_vec = svdupq_n_f16(0, 0, 0, 0, 0, 0, 0, 0);
    svbool_t pg_vec = svwhilelt_b16(i, d);
    simsimd_f16_t const* a = (simsimd_f16_t const*)(a_enum);
    simsimd_f16_t const* b = (simsimd_f16_t const*)(b_enum);
    do {
        svfloat16_t a_vec = svld1_f16(pg_vec, (float16_t const*)a + i);
        svfloat16_t b_vec = svld1_f16(pg_vec, (float16_t const*)b + i);
        ab_vec = svmla_f16_x(pg_vec, ab_vec, a_vec, b_vec);
        a2_vec = svmla_f16_x(pg_vec, a2_vec, a_vec, a_vec);
        b2_vec = svmla_f16_x(pg_vec, b2_vec, b_vec, b_vec);
        i += svcnth();
        pg_vec = svwhilelt_b16(i, d);
    } while (svptest_any(svptrue_b16(), pg_vec));

    simsimd_f16_t ab = svaddv_f16(svptrue_b16(), ab_vec);
    simsimd_f16_t a2 = svaddv_f16(svptrue_b16(), a2_vec);
    simsimd_f16_t b2 = svaddv_f16(svptrue_b16(), b2_vec);

    // Avoid `simsimd_approximate_inverse_square_root` on Arm NEON
    simsimd_f32_t a2_b2_arr[2] = {a2, b2};
    float32x2_t a2_b2 = vld1_f32(a2_b2_arr);
    a2_b2 = vrsqrte_f32(a2_b2);
    vst1_f32(a2_b2_arr, a2_b2);
    return 1 - ab * a2_b2_arr[0] * a2_b2_arr[1];
}

#endif // SIMSIMD_TARGET_ARM_SVE
#endif // SIMSIMD_TARGET_ARM

#if SIMSIMD_TARGET_X86
#if SIMSIMD_TARGET_X86_AVX2

/*
 *  @file   x86_avx2_f16.h
 *  @brief  x86 AVX2 implementation of the most common similarity metrics for 16-bit floating point numbers.
 *  @author Ash Vardanian
 *
 *  - Implements: L2 squared, inner product, cosine similarity.
 *  - As AVX2 doesn't support masked loads of 16-bit words, implementations have a separate `for`-loop for tails.
 *  - Uses `f16` for both storage and `f32` for accumulation.
 *  - Requires compiler capabilities: avx2, f16c, fma.
 */

__attribute__((target("avx2,f16c,fma"))) //
inline static simsimd_f32_t
simsimd_avx2_f16_l2sq(simsimd_f16_t const* a, simsimd_f16_t const* b, simsimd_size_t n) {
    __m256 d2_vec = _mm256_set1_ps(0);
    simsimd_size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 a_vec = _mm256_cvtph_ps(_mm_loadu_si128((__m128i const*)(a + i)));
        __m256 b_vec = _mm256_cvtph_ps(_mm_loadu_si128((__m128i const*)(b + i)));
        __m256 d_vec = _mm256_sub_ps(a_vec, b_vec);
        d2_vec = _mm256_fmadd_ps(d_vec, d_vec, d2_vec);
    }

    d2_vec = _mm256_add_ps(_mm256_permute2f128_ps(d2_vec, d2_vec, 1), d2_vec);
    d2_vec = _mm256_hadd_ps(d2_vec, d2_vec);
    d2_vec = _mm256_hadd_ps(d2_vec, d2_vec);

    simsimd_f32_t result[1];
    _mm_store_ss(result, _mm256_castps256_ps128(d2_vec));

    // Accumulate the tail:
    for (; i < n; ++i)
        result[0] += (a[i] - b[i]) * (a[i] - b[i]);
    return result[0];
}

__attribute__((target("avx2,f16c,fma"))) //
inline static simsimd_f32_t
simsimd_avx2_f16_ip(simsimd_f16_t const* a, simsimd_f16_t const* b, simsimd_size_t n) {
    __m256 ab_vec = _mm256_set1_ps(0);
    simsimd_size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 a_vec = _mm256_cvtph_ps(_mm_loadu_si128((__m128i const*)(a + i)));
        __m256 b_vec = _mm256_cvtph_ps(_mm_loadu_si128((__m128i const*)(b + i)));
        ab_vec = _mm256_fmadd_ps(a_vec, b_vec, ab_vec);
    }

    ab_vec = _mm256_add_ps(_mm256_permute2f128_ps(ab_vec, ab_vec, 1), ab_vec);
    ab_vec = _mm256_hadd_ps(ab_vec, ab_vec);
    ab_vec = _mm256_hadd_ps(ab_vec, ab_vec);

    simsimd_f32_t result[1];
    _mm_store_ss(result, _mm256_castps256_ps128(ab_vec));

    // Accumulate the tail:
    for (; i < n; ++i)
        result[0] += a[i] * b[i];
    return 1 - result[0];
}
__attribute__((target("avx2,f16c,fma"))) //
inline static simsimd_f32_t
simsimd_avx2_f16_cos(simsimd_f16_t const* a, simsimd_f16_t const* b, simsimd_size_t n) {
    __m256 ab_vec = _mm256_set1_ps(0), a2_vec = _mm256_set1_ps(0), b2_vec = _mm256_set1_ps(0);
    simsimd_size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 a_vec = _mm256_cvtph_ps(_mm_loadu_si128((__m128i const*)(a + i)));
        __m256 b_vec = _mm256_cvtph_ps(_mm_loadu_si128((__m128i const*)(b + i)));
        ab_vec = _mm256_fmadd_ps(a_vec, b_vec, ab_vec);
        a2_vec = _mm256_fmadd_ps(a_vec, a_vec, a2_vec);
        b2_vec = _mm256_fmadd_ps(b_vec, b_vec, b2_vec);
    }

    ab_vec = _mm256_add_ps(_mm256_permute2f128_ps(ab_vec, ab_vec, 1), ab_vec);
    ab_vec = _mm256_hadd_ps(ab_vec, ab_vec);
    ab_vec = _mm256_hadd_ps(ab_vec, ab_vec);

    a2_vec = _mm256_add_ps(_mm256_permute2f128_ps(a2_vec, a2_vec, 1), a2_vec);
    a2_vec = _mm256_hadd_ps(a2_vec, a2_vec);
    a2_vec = _mm256_hadd_ps(a2_vec, a2_vec);

    b2_vec = _mm256_add_ps(_mm256_permute2f128_ps(b2_vec, b2_vec, 1), b2_vec);
    b2_vec = _mm256_hadd_ps(b2_vec, b2_vec);
    b2_vec = _mm256_hadd_ps(b2_vec, b2_vec);

    simsimd_f32_t ab, a2, b2;
    _mm_store_ss(&ab, _mm256_castps256_ps128(ab_vec));
    _mm_store_ss(&a2, _mm256_castps256_ps128(a2_vec));
    _mm_store_ss(&b2, _mm256_castps256_ps128(b2_vec));

    // Accumulate the tail:
    for (; i < n; ++i)
        ab += a[i] * b[i], a2 += a[i] * a[i], b2 += b[i] * b[i];

    // Replace simsimd_approximate_inverse_square_root with `rsqrtss`
    __m128 a2_sqrt_recip = _mm_rsqrt_ss(_mm_set_ss((float)a2));
    __m128 b2_sqrt_recip = _mm_rsqrt_ss(_mm_set_ss((float)b2));
    __m128 result = _mm_mul_ss(a2_sqrt_recip, b2_sqrt_recip); // Multiply the reciprocal square roots
    result = _mm_mul_ss(result, _mm_set_ss((float)ab));       // Multiply by ab
    result = _mm_sub_ss(_mm_set_ss(1.0f), result);            // Subtract from 1
    return _mm_cvtss_f32(result);                             // Extract the final result
}

/*
 *  @file   x86_avx2_i8.h
 *  @brief  x86 AVX2 implementation of the most common similarity metrics for 8-bit signed integral numbers.
 *  @author Ash Vardanian
 *
 *  - Implements: L2 squared, cosine similarity, inner product (same as cosine).
 *  - As AVX2 doesn't support masked loads of 16-bit words, implementations have a separate `for`-loop for tails.
 *  - Uses `i8` for storage, `i16` for multiplication, and `i32` for accumulation, if no better option is available.
 *  - Requires compiler capabilities: avx2.
 */

__attribute__((target("avx2"))) //
inline static simsimd_f32_t
simsimd_avx2_i8_l2sq(simsimd_i8_t const* a, simsimd_i8_t const* b, simsimd_size_t d) {

    __m256i d2_high_vec = _mm256_setzero_si256();
    __m256i d2_low_vec = _mm256_setzero_si256();

    simsimd_size_t i = 0;
    for (; i + 31 < d; i += 32) {
        __m256i a_vec = _mm256_loadu_si256((__m256i const*)(a + i));
        __m256i b_vec = _mm256_loadu_si256((__m256i const*)(b + i));

        // Sign extend int8 to int16
        __m256i a_low = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(a_vec));
        __m256i a_high = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(a_vec, 1));
        __m256i b_low = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(b_vec));
        __m256i b_high = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(b_vec, 1));

        // Subtract and multiply
        __m256i d_low = _mm256_sub_epi16(a_low, b_low);
        __m256i d_high = _mm256_sub_epi16(a_high, b_high);
        __m256i d2_low_part = _mm256_madd_epi16(d_low, d_low);
        __m256i d2_high_part = _mm256_madd_epi16(d_high, d_high);

        // Accumulate into int32 vectors
        d2_low_vec = _mm256_add_epi32(d2_low_vec, d2_low_part);
        d2_high_vec = _mm256_add_epi32(d2_high_vec, d2_high_part);
    }

    // Accumulate the 32-bit integers from `d2_high_vec` and `d2_low_vec`
    __m256i d2_vec = _mm256_add_epi32(d2_low_vec, d2_high_vec);
    __m128i d2_sum = _mm_add_epi32(_mm256_extracti128_si256(d2_vec, 0), _mm256_extracti128_si256(d2_vec, 1));
    d2_sum = _mm_hadd_epi32(d2_sum, d2_sum);
    d2_sum = _mm_hadd_epi32(d2_sum, d2_sum);
    int d2 = _mm_extract_epi32(d2_sum, 0);

    // Take care of the tail:
    for (; i < d; ++i) {
        int d = a[i] - b[i];
        d2 += d * d;
    }

    return (simsimd_f32_t)d2;
}
__attribute__((target("avx2"))) //
inline static simsimd_f32_t
simsimd_avx2_i8_cos(simsimd_i8_t const* a, simsimd_i8_t const* b, simsimd_size_t d) {

    __m256i ab_high_vec = _mm256_setzero_si256();
    __m256i ab_low_vec = _mm256_setzero_si256();
    __m256i a2_high_vec = _mm256_setzero_si256();
    __m256i a2_low_vec = _mm256_setzero_si256();
    __m256i b2_high_vec = _mm256_setzero_si256();
    __m256i b2_low_vec = _mm256_setzero_si256();

    simsimd_size_t i = 0;
    for (; i + 31 < d; i += 32) {
        __m256i a_vec = _mm256_loadu_si256((__m256i const*)(a + i));
        __m256i b_vec = _mm256_loadu_si256((__m256i const*)(b + i));

        // Unpack int8 to int32
        __m256i a_low = _mm256_cvtepi8_epi32(_mm256_castsi256_si128(a_vec));
        __m256i a_high = _mm256_cvtepi8_epi32(_mm256_extracti128_si256(a_vec, 1));
        __m256i b_low = _mm256_cvtepi8_epi32(_mm256_castsi256_si128(b_vec));
        __m256i b_high = _mm256_cvtepi8_epi32(_mm256_extracti128_si256(b_vec, 1));

        // Multiply and accumulate
        ab_low_vec = _mm256_add_epi32(ab_low_vec, _mm256_mullo_epi32(a_low, b_low));
        ab_high_vec = _mm256_add_epi32(ab_high_vec, _mm256_mullo_epi32(a_high, b_high));
        a2_low_vec = _mm256_add_epi32(a2_low_vec, _mm256_mullo_epi32(a_low, a_low));
        a2_high_vec = _mm256_add_epi32(a2_high_vec, _mm256_mullo_epi32(a_high, a_high));
        b2_low_vec = _mm256_add_epi32(b2_low_vec, _mm256_mullo_epi32(b_low, b_low));
        b2_high_vec = _mm256_add_epi32(b2_high_vec, _mm256_mullo_epi32(b_high, b_high));
    }

    // Horizontal sum across the 256-bit register
    __m256i ab_vec = _mm256_add_epi32(ab_low_vec, ab_high_vec);
    __m128i ab_sum = _mm_add_epi32(_mm256_extracti128_si256(ab_vec, 0), _mm256_extracti128_si256(ab_vec, 1));
    ab_sum = _mm_hadd_epi32(ab_sum, ab_sum);
    ab_sum = _mm_hadd_epi32(ab_sum, ab_sum);

    __m256i a2_vec = _mm256_add_epi32(a2_low_vec, a2_high_vec);
    __m128i a2_sum = _mm_add_epi32(_mm256_extracti128_si256(a2_vec, 0), _mm256_extracti128_si256(a2_vec, 1));
    a2_sum = _mm_hadd_epi32(a2_sum, a2_sum);
    a2_sum = _mm_hadd_epi32(a2_sum, a2_sum);

    __m256i b2_vec = _mm256_add_epi32(b2_low_vec, b2_high_vec);
    __m128i b2_sum = _mm_add_epi32(_mm256_extracti128_si256(b2_vec, 0), _mm256_extracti128_si256(b2_vec, 1));
    b2_sum = _mm_hadd_epi32(b2_sum, b2_sum);
    b2_sum = _mm_hadd_epi32(b2_sum, b2_sum);

    // Further reduce to a single sum for each vector
    int ab = _mm_extract_epi32(ab_sum, 0);
    int a2 = _mm_extract_epi32(a2_sum, 0);
    int b2 = _mm_extract_epi32(b2_sum, 0);

    // Take care of the tail:
    for (; i < d; ++i) {
        int ai = a[i], bi = b[i];
        ab += ai * bi, a2 += ai * ai, b2 += bi * bi;
    }

    // Replace simsimd_approximate_inverse_square_root with `rsqrtss`
    __m128 a2_sqrt_recip = _mm_rsqrt_ss(_mm_set_ss((float)a2));
    __m128 b2_sqrt_recip = _mm_rsqrt_ss(_mm_set_ss((float)b2));
    __m128 result = _mm_mul_ss(a2_sqrt_recip, b2_sqrt_recip); // Multiply the reciprocal square roots
    result = _mm_mul_ss(result, _mm_set_ss((float)ab));       // Multiply by ab
    result = _mm_sub_ss(_mm_set_ss(1.0f), result);            // Subtract from 1
    return _mm_cvtss_f32(result);                             // Extract the final result
}

__attribute__((target("avx2"))) //
inline static simsimd_f32_t
simsimd_avx2_i8_ip(simsimd_i8_t const* a, simsimd_i8_t const* b, simsimd_size_t d) {
    return simsimd_avx2_i8_cos(a, b, d);
}

#endif // SIMSIMD_TARGET_X86_AVX2

#if SIMSIMD_TARGET_X86_AVX512

/*
 *  @file   x86_avx512_f16.h
 *  @brief  x86 AVX-512 implementation of the most common similarity metrics for 16-bit floating point numbers.
 *  @author Ash Vardanian
 *
 *  - Implements: L2 squared, inner product, cosine similarity.
 *  - Uses `_mm512_maskz_loadu_epi16` intrinsics to perform masked unaligned loads.
 *  - Uses `f16` for both storage and accumulation, assuming it's resolution is enough for average case.
 *  - Requires compiler capabilities: avx512fp16, avx512f, avx512vl.
 */

__attribute__((target("avx512fp16,avx512vl,avx512f"))) //
inline static simsimd_f32_t
simsimd_avx512_f16_l2sq(simsimd_f16_t const* a, simsimd_f16_t const* b, simsimd_size_t d) {
    __m512h d2_vec = _mm512_set1_ph(0);
    simsimd_size_t i = 0;

    do {
        __mmask32 mask = d - i >= 32 ? 0xFFFFFFFF : ((1u << (d - i)) - 1u);
        __m512i a_vec = _mm512_maskz_loadu_epi16(mask, a + i);
        __m512i b_vec = _mm512_maskz_loadu_epi16(mask, b + i);
        __m512h d_vec = _mm512_sub_ph(_mm512_castsi512_ph(a_vec), _mm512_castsi512_ph(b_vec));
        d2_vec = _mm512_fmadd_ph(d_vec, d_vec, d2_vec);

        i += 32;
    } while (i < d);

    return _mm512_reduce_add_ph(d2_vec);
}

__attribute__((target("avx512fp16,avx512vl,avx512f"))) //
inline static simsimd_f32_t
simsimd_avx512_f16_ip(simsimd_f16_t const* a, simsimd_f16_t const* b, simsimd_size_t d) {
    __m512h ab_vec = _mm512_set1_ph(0);
    simsimd_size_t i = 0;

    do {
        __mmask32 mask = d - i >= 32 ? 0xFFFFFFFF : ((1u << (d - i)) - 1u);
        __m512i a_vec = _mm512_maskz_loadu_epi16(mask, a + i);
        __m512i b_vec = _mm512_maskz_loadu_epi16(mask, b + i);
        ab_vec = _mm512_fmadd_ph(_mm512_castsi512_ph(a_vec), _mm512_castsi512_ph(b_vec), ab_vec);

        i += 32;
    } while (i < d);

    return 1 - _mm512_reduce_add_ph(ab_vec);
}

__attribute__((target("avx512fp16,avx512vl,avx512f"))) //
inline static simsimd_f32_t
simsimd_avx512_f16_cos(simsimd_f16_t const* a, simsimd_f16_t const* b, simsimd_size_t d) {
    __m512h ab_vec = _mm512_set1_ph(0);
    __m512h a2_vec = _mm512_set1_ph(0);
    __m512h b2_vec = _mm512_set1_ph(0);
    simsimd_size_t i = 0;

    do {
        __mmask32 mask = d - i >= 32 ? 0xFFFFFFFF : ((1u << (d - i)) - 1u);
        __m512i a_vec = _mm512_maskz_loadu_epi16(mask, a + i);
        __m512i b_vec = _mm512_maskz_loadu_epi16(mask, b + i);
        ab_vec = _mm512_fmadd_ph(_mm512_castsi512_ph(a_vec), _mm512_castsi512_ph(b_vec), ab_vec);
        a2_vec = _mm512_fmadd_ph(_mm512_castsi512_ph(a_vec), _mm512_castsi512_ph(a_vec), a2_vec);
        b2_vec = _mm512_fmadd_ph(_mm512_castsi512_ph(b_vec), _mm512_castsi512_ph(b_vec), b2_vec);

        i += 32;
    } while (i < d);

    simsimd_f32_t ab = _mm512_reduce_add_ph(ab_vec);
    simsimd_f32_t a2 = _mm512_reduce_add_ph(a2_vec);
    simsimd_f32_t b2 = _mm512_reduce_add_ph(b2_vec);

    __m128d a2_b2 = _mm_set_pd((double)a2, (double)b2);
    __m128d rsqrts = _mm_mask_rsqrt14_pd(_mm_setzero_pd(), 0xFF, a2_b2);
    double rsqrts_array[2];
    _mm_storeu_pd(rsqrts_array, rsqrts);
    return 1 - ab * rsqrts_array[0] * rsqrts_array[1];
}

/*
 *  @file   x86_avx512_i8.h
 *  @brief  x86 AVX-512 implementation of the most common similarity metrics for 8-bit integers.
 *  @author Ash Vardanian
 *
 *  - Implements: L2 squared, cosine similarity, inner product (same as cosine).
 *  - Uses `_mm512_maskz_loadu_epi16` intrinsics to perform masked unaligned loads.
 *  - Uses `i8` for storage, `i16` for multiplication, and `i32` for accumulation, if no better option is available.
 *  - Requires compiler capabilities: avx512f, avx512vl.
 */

__attribute__((target("avx512vl,avx512f"))) //
inline static simsimd_f32_t
simsimd_avx512_i8_l2sq(simsimd_i8_t const* a, simsimd_i8_t const* b, simsimd_size_t d) {
    __m512i d2_i32s_vec = _mm512_setzero_si512();
    simsimd_size_t i = 0;

    do {
        __mmask32 mask = d - i >= 32 ? 0xFFFFFFFF : ((1u << (d - i)) - 1u);
        __m512i a_vec = _mm512_cvtepi8_epi16(_mm256_maskz_loadu_epi8(mask, a + i)); // Load 8-bit integers
        __m512i b_vec = _mm512_cvtepi8_epi16(_mm256_maskz_loadu_epi8(mask, b + i)); // Load 8-bit integers
        __m512i d_i16s_vec = _mm512_sub_epi16(a_vec, b_vec);
        d2_i32s_vec = _mm512_add_epi32(d2_i32s_vec, _mm512_madd_epi16(d_i16s_vec, d_i16s_vec));

        i += 32;
    } while (i < d);

    return _mm512_reduce_add_epi32(d2_i32s_vec);
}

__attribute__((target("avx512vl,avx512f"))) //
inline static simsimd_f32_t
simsimd_avx512_i8_cos(simsimd_i8_t const* a, simsimd_i8_t const* b, simsimd_size_t d) {
    __m512i ab_i32s_vec = _mm512_setzero_si512();
    __m512i a2_i32s_vec = _mm512_setzero_si512();
    __m512i b2_i32s_vec = _mm512_setzero_si512();
    simsimd_size_t i = 0;

    do {
        __mmask32 mask = d - i >= 32 ? 0xFFFFFFFF : ((1u << (d - i)) - 1u);
        __m512i a_vec = _mm512_cvtepi8_epi16(_mm256_maskz_loadu_epi8(mask, a + i)); // Load 8-bit integers
        __m512i b_vec = _mm512_cvtepi8_epi16(_mm256_maskz_loadu_epi8(mask, b + i)); // Load 8-bit integers

        ab_i32s_vec = _mm512_add_epi32(ab_i32s_vec, _mm512_madd_epi16(a_vec, b_vec));
        a2_i32s_vec = _mm512_add_epi32(a2_i32s_vec, _mm512_madd_epi16(a_vec, a_vec));
        b2_i32s_vec = _mm512_add_epi32(b2_i32s_vec, _mm512_madd_epi16(b_vec, b_vec));

        i += 32;
    } while (i < d);

    int ab = _mm512_reduce_add_epi32(ab_i32s_vec);
    int a2 = _mm512_reduce_add_epi32(a2_i32s_vec);
    int b2 = _mm512_reduce_add_epi32(b2_i32s_vec);

    __m128d a2_b2 = _mm_set_pd((double)a2, (double)b2);
    __m128d rsqrts = _mm_mask_rsqrt14_pd(_mm_setzero_pd(), 0xFF, a2_b2);
    double rsqrts_array[2];
    _mm_storeu_pd(rsqrts_array, rsqrts);
    return 1 - ab * rsqrts_array[0] * rsqrts_array[1];
}

__attribute__((target("avx512vl,avx512f"))) //
inline static simsimd_f32_t
simsimd_avx512_i8_ip(simsimd_i8_t const* a, simsimd_i8_t const* b, simsimd_size_t d) {
    return simsimd_avx512_i8_cos(a, b, d);
}

#endif // SIMSIMD_TARGET_X86_AVX512
#endif // SIMSIMD_TARGET_X86

#ifdef __cplusplus
}
#endif