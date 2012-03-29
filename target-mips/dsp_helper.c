/*
 *  MIPS ASE DSP Instruction emulation helpers for QEMU.
 *
 *  Copyright (c) 2012  Jia Liu <proljc@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "cpu.h"
#include "helper.h"

/*** MIPS DSP internal functions begin ***/
static inline void set_DSPControl_overflow_flag(CPUMIPSState *env,
                                                uint32_t flag, int position)
{
    env->active_tc.DSPControl |= (target_ulong)flag << position;
}

static inline void set_DSPControl_carryflag(CPUMIPSState *env, uint32_t flag)
{
    env->active_tc.DSPControl |= (target_ulong)flag << 13;
}

static inline uint32_t get_DSPControl_carryflag(CPUMIPSState *env)
{
    uint32_t flag;

    flag = (env->active_tc.DSPControl >> 13) & 0x01;

    return flag;
}

static inline void set_DSPControl_24(CPUMIPSState *env, uint32_t flag, int len)
{
    if (len == 2) {
        env->active_tc.DSPControl &= 0xFCFFFFFF;
    } else if (len == 4) {
        env->active_tc.DSPControl &= 0xF0FFFFFF;
    }

    env->active_tc.DSPControl |= (target_ulong)flag << 24;
}

static inline void set_DSPControl_pos(CPUMIPSState *env, uint32_t pos)
{
    target_ulong dspc;

    dspc = env->active_tc.DSPControl;
    dspc = dspc & 0xFFFFFFC0;
    dspc |= pos;
    env->active_tc.DSPControl = dspc;
}

static inline uint32_t get_DSPControl_pos(CPUMIPSState *env)
{
    target_ulong dspc;
    uint32_t pos;

    dspc = env->active_tc.DSPControl;
    pos = dspc & 0x3F;

    return pos;
}

static inline void set_DSPControl_efi(CPUMIPSState *env, uint32_t flag)
{
    env->active_tc.DSPControl &= 0xFFFFBFFF;
    env->active_tc.DSPControl |= (target_ulong)flag << 14;
}

/* get abs value */
static inline int8_t mipsdsp_sat_abs_u8(CPUMIPSState *env, uint8_t a)
{
    int8_t temp;
    temp = a;

    if (a == 0x80) {
        set_DSPControl_overflow_flag(env, 1, 20);
        temp = 0x7f;
    } else {
        if ((a & 0x80) == 0x80) {
            temp = -temp;
        }
    }

    return temp;
}

static inline int16_t mipsdsp_sat_abs_u16(CPUMIPSState *env, uint16_t a)
{
    int16_t temp;
    temp = a;

    if (a == 0x8000) {
        set_DSPControl_overflow_flag(env, 1, 20);
        temp = 0x7fff;
    } else {
        if ((a & 0x8000) == 0x8000) {
            temp = -temp;
        }
    }

    return temp;
}

static inline int32_t mipsdsp_sat_abs_u32(CPUMIPSState *env, uint32_t a)
{
    int32_t temp;
    temp = a;

    if (a == 0x80000000) {
        set_DSPControl_overflow_flag(env, 1, 20);
        temp = 0x7FFFFFFF;
    } else {
        if ((a & 0x80000000) == 0x80000000) {
            temp = -temp;
        }
    }

    return temp;
}

/* get sum value */
static inline int16_t mipsdsp_add_i16(CPUMIPSState *env, int16_t a, int16_t b)
{
    int16_t tempS;
    int32_t tempI, temp15, temp16;

    tempS = a + b;
    tempI = a + b;
    temp15 = (tempI & 0x8000) >> 15;
    temp16 = (tempI & 0x10000) >> 16;

    if (temp15 != temp16) {
        set_DSPControl_overflow_flag(env, 1, 20);
    }

    return tempS;
}

static inline int16_t mipsdsp_sat_add_i16(CPUMIPSState *env,
                                          int16_t a, int16_t b)
{
    int16_t tempS;
    int32_t tempI, temp15, temp16;

    tempS = a + b;
    tempI = (int32_t)a + (int32_t)b;
    temp15 = (tempI & 0x8000) >> 15;
    temp16 = (tempI & 0x10000) >> 16;

    if (temp15 != temp16) {
        if (temp16 == 0) {
            tempS = 0x7FFF;
        } else {
            tempS = 0x8000;
        }
        set_DSPControl_overflow_flag(env, 1, 20);
    }

    return tempS;
}

static inline int32_t mipsdsp_sat_add_i32(CPUMIPSState *env,
                                          int32_t a, int32_t b)
{
    int32_t tempI;
    int64_t tempL, temp31, temp32;

    tempI = a + b;
    tempL = (int64_t)a + (int64_t)b;
    temp31 = (tempL & 0x80000000) >> 31;
    temp32 = (tempL & 0x100000000ull) >> 32;

    if (temp31 != temp32) {
        if (temp32 == 0) {
            tempI = 0x7FFFFFFF;
        } else {
            tempI = 0x80000000;
        }
        set_DSPControl_overflow_flag(env, 1, 20);
    }

    return tempI;
}

static inline uint8_t mipsdsp_add_u8(CPUMIPSState *env, uint8_t a, uint8_t b)
{
    uint8_t  result;
    uint16_t tempA, tempB, temp;

    tempA = a;
    tempB = b;
    temp = tempA + tempB;
    result = temp & 0xFF;

    if ((temp & 0x0100) == 0x0100) {
        set_DSPControl_overflow_flag(env, 1, 20);
    }

    return result;
}

static inline uint16_t mipsdsp_add_u16(CPUMIPSState *env,
                                       uint16_t a, uint16_t b)
{
    uint16_t result;
    uint32_t tempA, tempB, temp;

    tempA = a;
    tempB = b;
    temp = tempA + tempB;
    result = temp & 0xFFFF;

    if ((temp & 0x00010000) == 0x00010000) {
        set_DSPControl_overflow_flag(env, 1, 20);
    }

    return result;
}

static inline uint8_t mipsdsp_sat_add_u8(CPUMIPSState *env,
                                         uint8_t a, uint8_t b)
{
    uint8_t  result;
    uint16_t tempA, tempB, temp;

    tempA = a;
    tempB = b;
    temp = tempA + tempB;
    result = temp & 0xFF;

    if ((0x0100 & temp) == 0x0100) {
        result = 0xFF;
        set_DSPControl_overflow_flag(env, 1, 20);
    }

    return result;
}

static inline uint16_t mipsdsp_sat_add_u16(CPUMIPSState *env,
                                           uint16_t a, uint16_t b)
{
    uint16_t result;
    uint32_t tempA, tempB, temp;

    tempA = a;
    tempB = b;
    temp = tempA + tempB;
    result = temp & 0xFFFF;

    if ((0x00010000 & temp) == 0x00010000) {
        result = 0xFFFF;
        set_DSPControl_overflow_flag(env, 1, 20);
    }

    return result;
}

static inline int32_t mipsdsp_sat32_acc_q31(CPUMIPSState *env,
                                            int32_t acc, int32_t a)
{
    int64_t tempA, temp;
    int32_t temp32, temp31, result;

    temp = ((int64_t)env->active_tc.HI[acc] << 32) | \
           ((int64_t)env->active_tc.LO[acc] & 0x00000000FFFFFFFFull);
    tempA = a;
    temp += tempA;
    temp32 = (temp >> 32) & 0x01;
    temp31 = (temp >> 31) & 0x01;
    result = temp & 0xFFFFFFFF;

    if (temp32 != temp31) {
        if (temp32 == 0) {
            result = 0x80000000;
        } else {
            result = 0x7FFFFFFF;
        }
        set_DSPControl_overflow_flag(env, 1, 16 + acc);
    }

    return result;
}

static inline int32_t mipsdsp_mul_i16_i16(CPUMIPSState *env,
                                          int16_t a, int16_t b)
{
    int32_t temp, tempA, tempB;

    tempA = a;
    tempB = b;
    temp = tempA * tempB;

    if ((temp > 0x7FFF) || (temp < 0xFFFF8000)) {
        set_DSPControl_overflow_flag(env, 1, 21);
    }
    temp &= 0x0000FFFF;

    return temp;
}

static inline int32_t mipsdsp_sat16_mul_i16_i16(CPUMIPSState *env,
                                                int16_t a, int16_t b)
{
    int32_t temp, tempA, tempB;

    tempA = a;
    tempB = b;
    temp = tempA * tempB;

    if (temp > 0x7FFF) {
        temp = 0x00007FFF;
        set_DSPControl_overflow_flag(env, 1, 21);
    } else if (temp < 0x00007FFF) {
        temp = 0xFFFF8000;
        set_DSPControl_overflow_flag(env, 1, 21);
    }
    temp &= 0x0000FFFF;

    return temp;
}

static inline int32_t mipsdsp_mul_q15_q15_overflowflag21(CPUMIPSState *env,
                                                         uint16_t a, uint16_t b)
{
    int16_t tempA, tempB;
    int32_t temp;

    tempA = a;
    tempB = b;

    if ((a == 0x8000) && (b == 0x8000)) {
        temp = 0x7FFFFFFF;
        set_DSPControl_overflow_flag(env, 1, 21);
    } else {
        temp = ((int32_t)tempA * (int32_t)tempB) << 1;
    }

    return temp;
}

/* right shift */
static inline int16_t mipsdsp_rshift1_add_q16(int16_t a, int16_t b)
{
    int32_t temp, tempA, tempB;
    int16_t result;

    tempA = a;
    tempB = b;
    temp = tempA + tempB;
    result = (temp >> 1) & 0xFFFF;

    return result;
}

/* round right shift */
static inline int16_t mipsdsp_rrshift1_add_q16(int16_t a, int16_t b)
{
    int32_t temp, tempA, tempB;
    int16_t result;

    tempA = a;
    tempB = b;
    temp = tempA + tempB;
    temp += 1;
    result = (temp >> 1) & 0xFFFF;

    return result;
}

static inline int32_t mipsdsp_rshift1_add_q32(int32_t a, int32_t b)
{
    int64_t temp, tempA, tempB;
    int32_t result;

    tempA = a;
    tempB = b;
    temp = tempA + tempB;
    result = (temp >> 1) & 0xFFFFFFFF;

    return result;
}

static inline int32_t mipsdsp_rrshift1_add_q32(int32_t a, int32_t b)
{
    int64_t temp, tempA, tempB;
    int32_t result;
    tempA = a;
    tempB = b;

    temp = tempA + tempB;
    temp += 1;
    result = (temp >> 1) & 0xFFFFFFFF;

    return result;
}

static inline uint8_t mipsdsp_rshift1_add_u8(uint8_t a, uint8_t b)
{
    uint16_t tempA, tempB, temp;
    uint8_t  result;

    tempA = a;
    tempB = b;
    temp = tempA + tempB;
    result = (temp >> 1) & 0x00FF;

    return result;
}

static inline uint8_t mipsdsp_rrshift1_add_u8(uint8_t a, uint8_t b)
{
    uint16_t tempA, tempB, temp;
    uint8_t  result;

    tempA = a;
    tempB = b;
    temp = tempA + tempB + 1;
    result = (temp >> 1) & 0x00FF;

    return result;
}

static inline int64_t mipsdsp_rashift_short_acc(CPUMIPSState *env,
                                                int32_t ac,
                                                int32_t shift)
{
    int32_t sign, temp31;
    int64_t temp, acc;

    sign = (env->active_tc.HI[ac] >> 31) & 0x01;
    acc = ((int64_t)env->active_tc.HI[ac] << 32) | \
          ((int64_t)env->active_tc.LO[ac] & 0xFFFFFFFF);
    if (shift == 0) {
        temp = acc;
    } else {
        if (sign == 0) {
            temp = (((int64_t)0x01 << (32 - shift + 1)) - 1) & (acc >> shift);
        } else {
            temp = ((((int64_t)0x01 << (shift + 1)) - 1) << (32 - shift)) | \
                   (acc >> shift);
        }
    }

    temp31 = (temp >> 31) & 0x01;
    if (sign != temp31) {
        set_DSPControl_overflow_flag(env, 1, 23);
    }

    return temp;
}

/*  128 bits long. p[0] is LO, p[1] is HI. */
static inline void mipsdsp__rashift_short_acc(CPUMIPSState *env,
                                              int64_t *p,
                                              int32_t ac,
                                              int32_t shift)
{
    int64_t acc;

    acc = ((int64_t)env->active_tc.HI[ac] << 32) | \
          ((int64_t)env->active_tc.LO[ac] & 0xFFFFFFFF);
    if (shift == 0) {
        p[0] = acc << 1;
        p[1] = (acc >> 63) & 0x01;
    } else {
        p[0] = acc >> (shift - 1);
        p[1] = 0;
    }
}

static inline int32_t mipsdsp_mul_q15_q15(CPUMIPSState *env,
                                          int32_t ac, uint16_t a, uint16_t b)
{
    uint16_t tempA, tempB;
    int32_t temp;

    tempA = a;
    tempB = b;

    if ((a == 0x8000) && (b == 0x8000)) {
        temp = 0x7FFFFFFF;
        set_DSPControl_overflow_flag(env, 1, 16 + ac);
    } else {
        temp = ((uint32_t)tempA * (uint32_t)tempB) << 1;
    }

    return temp;
}

static inline int64_t mipsdsp_mul_q31_q31(CPUMIPSState *env,
                                          int32_t ac, uint32_t a, uint32_t b)
{
    uint32_t tempA, tempB;
    uint64_t temp;

    tempA = a;
    tempB = b;

    if ((a == 0x80000000) && (b == 0x80000000)) {
        temp = 0x7FFFFFFFFFFFFFFFull;
        set_DSPControl_overflow_flag(env, 1, 16 + ac);
    } else {
        temp = ((uint64_t)tempA * (uint64_t)tempB) << 1;
    }

    return temp;
}

static inline uint16_t mipsdsp_mul_u8_u8(uint8_t a, uint8_t b)
{
    uint16_t temp;
    temp = (uint16_t)a * (uint16_t)b;

    return temp;
}

static inline uint16_t mipsdsp_mul_u8_u16(CPUMIPSState *env,
                                          uint8_t a, uint16_t b)
{
    uint16_t tempS;
    uint32_t tempI, tempA, tempB;

    tempA = a;
    tempB = b;
    tempI = tempA * tempB;
    if (tempI > 0x00) {
        tempI = 0x0000FFFF;
        set_DSPControl_overflow_flag(env, 1, 21);
    }
    tempS = tempI & 0x0000FFFF;
    return tempS;
}

static inline int16_t mipsdsp_rndq15_mul_q15_q15(CPUMIPSState *env,
                                                 uint16_t a, uint16_t b)
{
    int16_t result, tempA, tempB;
    int32_t temp;

    tempA = a;
    tempB = b;

    if ((a == 0x8000) && (b == 0x8000)) {
        temp = 0x7FFF0000;
        set_DSPControl_overflow_flag(env, 1, 21);
    } else {
        temp = ((int32_t)tempA * (int32_t)tempB) << 1;
        temp = temp + 0x00008000;
    }
    result = (temp & 0xFFFF0000) >> 16;

    return result;
}

static inline int32_t mipsdsp_sat16_mul_q15_q15(CPUMIPSState *env,
                                                uint16_t a, uint16_t b)
{
    int16_t tempA, tempB;
    int32_t temp;

    tempA = a;
    tempB = b;

    if ((a == 0x8000) && (b == 0x8000)) {
        temp = 0x7FFF0000;
        set_DSPControl_overflow_flag(env, 1, 21);
    } else {
        temp = ((uint32_t)tempA * (uint32_t)tempB);
        temp = temp << 1;
    }
    temp = (temp >> 16) & 0x0000FFFF;

    return temp;
}

static inline uint16_t mipsdsp_trunc16_sat16_round(CPUMIPSState *env,
                                                   uint32_t a)
{
    uint16_t result;
    uint32_t temp32, temp31;
    int64_t temp;

    temp = (int32_t)a + 0x00008000;
    temp32 = (temp >> 32) & 0x01;
    temp31 = (temp >> 31) & 0x01;

    if (temp32 != temp31) {
        temp = 0x7FFFFFFF;
        set_DSPControl_overflow_flag(env, 1, 22);
    }
    result = (temp >> 16) & 0xFFFF;

    return result;
}

static inline uint8_t mipsdsp_sat8_reduce_precision(CPUMIPSState *env,
                                                    uint16_t a)
{
    uint8_t  result;
    uint16_t mag;
    uint32_t sign;

    sign = (a >> 15) & 0x01;
    mag = a & 0x7FFF;

    if (sign == 0) {
        if (mag > 0x7F80) {
            result = 0xFF;
            set_DSPControl_overflow_flag(env, 1, 22);
        } else {
            result = (mag >> 7) & 0xFFFF;
        }
    } else {
        result = 0x00;
        set_DSPControl_overflow_flag(env, 1, 22);
    }

    return result;
}

static inline uint8_t mipsdsp_lshift8(CPUMIPSState *env, uint8_t a, uint8_t s)
{
    uint8_t sign;
    uint8_t temp, discard;

    if (s == 0) {
        temp = a;
    } else {
        sign = (a >> 7) & 0x01;
        temp = a << s;
        if (sign != 0) {
            discard = (((0x01 << (8 - s)) - 1) << s) | \
                      ((a >> (6 - (s - 1))) & ((0x01 << s) - 1));
        } else {
            discard = a >> (6 - (s - 1));
        }

        if (discard != 0x00) {
            set_DSPControl_overflow_flag(env, 1, 22);
        }
    }

    return temp;
}

static inline uint8_t mipsdsp_rshift8(uint8_t a, uint8_t s)
{
    uint8_t temp;
    if (s == 0) {
        temp = a;
    } else {
        temp = a >> s;
    }
    return temp;
}

static inline uint16_t mipsdsp_lshift16(CPUMIPSState *env,
                                        uint16_t a, uint8_t s)
{
    uint8_t  sign;
    uint16_t temp, discard;

    if (s == 0) {
        temp = a;
    } else {
        sign = (a >> 15) & 0x01;
        temp = a << s;
        if (sign != 0) {
            discard = (((0x01 << (16 - s)) - 1) << s) | \
                      ((a >> (14 - (s - 1))) & ((0x01 << s) - 1));
        } else {
            discard = a >> (14 - (s - 1));
        }

        if ((discard != 0x0000) && (discard != 0xFFFF)) {
            set_DSPControl_overflow_flag(env, 1, 22);
        }
    }

    return temp;
}

static inline uint16_t mipsdsp_sat16_lshift(CPUMIPSState *env,
                                            uint16_t a, uint8_t s)
{
    uint8_t  sign;
    uint16_t temp, discard;

    if (s == 0) {
        temp = a;
    } else {
        sign = (a >> 15) & 0x01;
        temp = a << s;
        if (sign != 0) {
            discard = (((0x01 << (16 - s)) - 1) << s) | \
                      ((a >> (14 - (s - 1))) & ((0x01 << s) - 1));
        } else {
            discard = a >> (14 - (s - 1));
        }

        if ((discard != 0x0000) && (discard != 0xFFFF)) {
            temp = (sign == 0) ? 0x7FFF : 0x8000;
            set_DSPControl_overflow_flag(env, 1, 22);
        }
    }

    return temp;
}

static inline uint32_t mipsdsp_sat32_lshift(CPUMIPSState *env,
                                            uint32_t a, uint8_t s)
{
    uint8_t  sign;
    uint32_t temp, discard;

    if (s == 0) {
        temp = a;
    } else {
        sign = (a >> 31) & 0x01;
        temp = a << s;
        if (sign != 0) {
            discard = (((0x01 << (32 - s)) - 1) << s) | \
                      ((a >> (30 - (s - 1))) & ((0x01 << s) - 1));
        } else {
            discard = a >> (30 - (s - 1));
        }

        if ((discard != 0x00000000) && (discard != 0xFFFFFFFF)) {
            temp = (sign == 0) ? 0x7FFFFFFF : 0x80000000;
            set_DSPControl_overflow_flag(env, 1, 22);
        }
    }

    return temp;
}

static inline uint16_t mipsdsp_rashift16(uint16_t a, uint8_t s)
{
    int16_t i, temp;

    i = a;
    if (s == 0) {
        temp = a;
    } else {
        temp = i >> s;
    }

    return temp;
}

static inline uint16_t mipsdsp_rnd16_rashift(uint16_t a, uint8_t s)
{
    int16_t  i, result;
    uint32_t temp;

    i = a;
    if (s == 0) {
        temp = (uint32_t)a << 1;
    } else {
        temp = (int32_t)i >> (s - 1);
    }
    temp   = temp + 1;
    result = temp >> 1;

    return result;
}

static inline uint32_t mipsdsp_rnd32_rashift(uint32_t a, uint8_t s)
{
    int32_t i;
    int64_t temp;
    uint32_t result;

    i = a;
    if (s == 0) {
        temp = a << 1;
    } else {
        temp = (int64_t)i >> (s - 1);
    }
    temp += 1;
    result = (temp >> 1) & 0x00000000FFFFFFFFull;

    return result;
}

static inline uint16_t mipsdsp_sub_i16(CPUMIPSState *env, int16_t a, int16_t b)
{
    uint8_t  temp16, temp15;
    uint16_t result;
    int32_t  temp;

    temp = (int32_t)a - (int32_t)b;
    temp16 = (temp >> 16) & 0x01;
    temp15 = (temp >> 15) & 0x01;
    if (temp16 != temp15) {
        set_DSPControl_overflow_flag(env, 1, 20);
    }
    result = temp & 0x0000FFFF;

    return result;
}

static inline uint16_t mipsdsp_sat16_sub(CPUMIPSState *env,
                                         int16_t a, int16_t b)
{
    uint8_t  temp16, temp15;
    uint16_t result;
    int32_t  temp;

    temp = (int32_t)a - (int32_t)b;
    temp16 = (temp >> 16) & 0x01;
    temp15 = (temp >> 15) & 0x01;
    if (temp16 != temp15) {
        if (temp16 == 0) {
            temp = 0x7FFF;
        } else {
            temp = 0x8000;
        }
        set_DSPControl_overflow_flag(env, 1, 20);
    }
    result = temp & 0x0000FFFF;

    return result;
}

static inline uint32_t mipsdsp_sat32_sub(CPUMIPSState *env,
                                         int32_t a, int32_t b)
{
    uint8_t  temp32, temp31;
    uint32_t result;
    int64_t  temp;

    temp = (int64_t)a - (int64_t)b;
    temp32 = (temp >> 32) & 0x01;
    temp31 = (temp >> 31) & 0x01;
    if (temp32 != temp31) {
        if (temp32 == 0) {
            temp = 0x7FFFFFFF;
        } else {
            temp = 0x80000000;
        }
        set_DSPControl_overflow_flag(env, 1, 20);
    }
    result = temp & 0x00000000FFFFFFFFull;

    return result;
}

static inline uint16_t mipsdsp_rshift1_sub_q16(int16_t a, int16_t b)
{
    int32_t  temp;
    uint16_t result;

    temp = (int32_t)a - (int32_t)b;
    result = (temp >> 1) & 0x0000FFFF;

    return result;
}

static inline uint16_t mipsdsp_rrshift1_sub_q16(int16_t a, int16_t b)
{
    int32_t  temp;
    uint16_t result;

    temp = (int32_t)a - (int32_t)b;
    temp += 1;
    result = (temp >> 1) & 0x0000FFFF;

    return result;
}

static inline uint32_t mipsdsp_rshift1_sub_q32(int32_t a, int32_t b)
{
    int64_t  temp;
    uint32_t result;

    temp   = (int64_t)a - (int64_t)b;
    result = (temp >> 1) & 0x00000000FFFFFFFFull;

    return result;
}

static inline uint32_t mipsdsp_rrshift1_sub_q32(int32_t a, int32_t b)
{
    int64_t  temp;
    uint32_t result;

    temp = (int64_t)a - (int64_t)b;
    temp += 1;
    result = (temp >> 1) & 0x00000000FFFFFFFFull;

    return result;
}

static inline uint16_t mipsdsp_sub_u16_u16(CPUMIPSState *env,
                                           uint16_t a, uint16_t b)
{
    uint8_t  temp16;
    uint16_t result;
    uint32_t temp;

    temp = (uint32_t)a - (uint32_t)b;
    temp16 = (temp >> 16) & 0x01;
    if (temp16 == 1) {
        set_DSPControl_overflow_flag(env, 1, 20);
    }
    result = temp & 0x0000FFFF;
    return result;
}

static inline uint16_t mipsdsp_satu16_sub_u16_u16(CPUMIPSState *env,
                                                  uint16_t a, uint16_t b)
{
    uint8_t  temp16;
    uint16_t result;
    uint32_t temp;

    temp   = (uint32_t)a - (uint32_t)b;
    temp16 = (temp >> 16) & 0x01;

    if (temp16 == 1) {
        temp = 0x0000;
        set_DSPControl_overflow_flag(env, 1, 20);
    }
    result = temp & 0x0000FFFF;

    return result;
}

static inline uint8_t mipsdsp_sub_u8(CPUMIPSState *env, uint8_t a, uint8_t b)
{
    uint8_t  result, temp8;
    uint16_t temp;

    temp = (uint16_t)a - (uint16_t)b;
    temp8 = (temp >> 8) & 0x01;
    if (temp8 == 0) {
        set_DSPControl_overflow_flag(env, 1, 20);
    }
    result = temp & 0x00FF;

    return result;
}

static inline uint8_t mipsdsp_satu8_sub(CPUMIPSState *env, uint8_t a, uint8_t b)
{
    uint8_t  result, temp8;
    uint16_t temp;

    temp = (uint16_t)a - (uint16_t)b;
    temp8 = (temp >> 8) & 0x01;
    if (temp8 == 1) {
        temp = 0x00;
        set_DSPControl_overflow_flag(env, 1, 20);
    }
    result = temp & 0x00FF;

    return result;
}
/*** MIPS DSP internal functions end ***/
