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

#define MIPSDSP_LHI 0xFFFFFFFF00000000ull
#define MIPSDSP_LLO 0x00000000FFFFFFFFull
#define MIPSDSP_HI  0xFFFF0000
#define MIPSDSP_LO  0x0000FFFF
#define MIPSDSP_Q3  0xFF000000
#define MIPSDSP_Q2  0x00FF0000
#define MIPSDSP_Q1  0x0000FF00
#define MIPSDSP_Q0  0x000000FF

/** DSP Arithmetic Sub-class insns **/
uint32_t helper_addq_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    int16_t  rsh, rsl, rth, rtl, temph, templ;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    temph = mipsdsp_add_i16(env, rsh, rth);
    templ = mipsdsp_add_i16(env, rsl, rtl);
    rd = ((unsigned int)temph << 16) | ((unsigned int)templ & 0xFFFF);

    return rd;
}

uint32_t helper_addq_s_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rsl, rth, rtl, temph, templ;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    temph = mipsdsp_sat_add_i16(env, rsh, rth);
    templ = mipsdsp_sat_add_i16(env, rsl, rtl);
    rd = ((uint32_t)temph << 16) | ((uint32_t)templ & 0xFFFF);

    return rd;
}

uint32_t helper_addq_s_w(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint32_t rd;
    rd = mipsdsp_sat_add_i32(env, rs, rt);
    return rd;
}

uint32_t helper_addu_qb(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint32_t rd;
    uint8_t  rs0, rs1, rs2, rs3;
    uint8_t  rt0, rt1, rt2, rt3;
    uint8_t  temp0, temp1, temp2, temp3;

    rs0 =  rs & MIPSDSP_Q0;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs3 = (rs & MIPSDSP_Q3) >> 24;

    rt0 =  rt & MIPSDSP_Q0;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt3 = (rt & MIPSDSP_Q3) >> 24;

    temp0 = mipsdsp_add_u8(env, rs0, rt0);
    temp1 = mipsdsp_add_u8(env, rs1, rt1);
    temp2 = mipsdsp_add_u8(env, rs2, rt2);
    temp3 = mipsdsp_add_u8(env, rs3, rt3);

    rd = (((uint32_t)temp3 << 24) & MIPSDSP_Q3) | \
         (((uint32_t)temp2 << 16) & MIPSDSP_Q2) | \
         (((uint32_t)temp1 <<  8) & MIPSDSP_Q1) | \
         ((uint32_t)temp0         & MIPSDSP_Q0);

    return rd;
}

uint32_t helper_addu_s_qb(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint32_t rd;
    uint8_t rs0, rs1, rs2, rs3;
    uint8_t rt0, rt1, rt2, rt3;
    uint8_t temp0, temp1, temp2, temp3;

    rs0 =  rs & MIPSDSP_Q0;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs3 = (rs & MIPSDSP_Q3) >> 24;

    rt0 =  rt & MIPSDSP_Q0;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt3 = (rt & MIPSDSP_Q3) >> 24;

    temp0 = mipsdsp_sat_add_u8(env, rs0, rt0);
    temp1 = mipsdsp_sat_add_u8(env, rs1, rt1);
    temp2 = mipsdsp_sat_add_u8(env, rs2, rt2);
    temp3 = mipsdsp_sat_add_u8(env, rs3, rt3);

    rd = (((uint8_t)temp3 << 24) & MIPSDSP_Q3) | \
         (((uint8_t)temp2 << 16) & MIPSDSP_Q2) | \
         (((uint8_t)temp1 <<  8) & MIPSDSP_Q1) | \
         ((uint8_t)temp0         & MIPSDSP_Q0);

    return rd;
}

uint32_t helper_adduh_qb(uint32_t rs, uint32_t rt)
{
    uint32_t rd;
    uint8_t  rs0, rs1, rs2, rs3;
    uint8_t  rt0, rt1, rt2, rt3;
    uint8_t  temp0, temp1, temp2, temp3;

    rs0 =  rs & MIPSDSP_Q0;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs3 = (rs & MIPSDSP_Q3) >> 24;

    rt0 =  rt & MIPSDSP_Q0;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt3 = (rt & MIPSDSP_Q3) >> 24;

    temp0 = mipsdsp_rshift1_add_u8(rs0, rt0);
    temp1 = mipsdsp_rshift1_add_u8(rs1, rt1);
    temp2 = mipsdsp_rshift1_add_u8(rs2, rt2);
    temp3 = mipsdsp_rshift1_add_u8(rs3, rt3);

    rd = (((uint32_t)temp3 << 24) & MIPSDSP_Q3) | \
         (((uint32_t)temp2 << 16) & MIPSDSP_Q2) | \
         (((uint32_t)temp1 <<  8) & MIPSDSP_Q1) | \
         ((uint32_t)temp0         & MIPSDSP_Q0);

    return rd;
}

uint32_t helper_adduh_r_qb(uint32_t rs, uint32_t rt)
{
    uint32_t rd;
    uint8_t  rs0, rs1, rs2, rs3;
    uint8_t  rt0, rt1, rt2, rt3;
    uint8_t  temp0, temp1, temp2, temp3;

    rs0 =  rs & MIPSDSP_Q0;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs3 = (rs & MIPSDSP_Q3) >> 24;

    rt0 =  rt & MIPSDSP_Q0;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt3 = (rt & MIPSDSP_Q3) >> 24;

    temp0 = mipsdsp_rrshift1_add_u8(rs0, rt0);
    temp1 = mipsdsp_rrshift1_add_u8(rs1, rt1);
    temp2 = mipsdsp_rrshift1_add_u8(rs2, rt2);
    temp3 = mipsdsp_rrshift1_add_u8(rs3, rt3);

    rd = (((uint32_t)temp3 << 24) & MIPSDSP_Q3) | \
         (((uint32_t)temp2 << 16) & MIPSDSP_Q2) | \
         (((uint32_t)temp1 <<  8) & MIPSDSP_Q1) | \
         ((uint32_t)temp0         & MIPSDSP_Q0);

    return rd;
}

uint32_t helper_addu_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl, rth, rtl, temph, templ;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;
    temph = mipsdsp_add_u16(env, rsh, rth);
    templ = mipsdsp_add_u16(env, rsl, rtl);
    rd = ((uint32_t)temph << 16) | ((uint32_t)templ & MIPSDSP_LO);

    return rd;
}

uint32_t helper_addu_s_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl, rth, rtl, temph, templ;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;
    temph = mipsdsp_sat_add_u16(env, rsh, rth);
    templ = mipsdsp_sat_add_u16(env, rsl, rtl);
    rd = ((uint32_t)temph << 16) | ((uint32_t)templ & MIPSDSP_LO);

    return rd;
}

uint32_t helper_addqh_ph(uint32_t rs, uint32_t rt)
{
    uint32_t rd;
    int16_t rsh, rsl, rth, rtl, temph, templ;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    temph = mipsdsp_rshift1_add_q16(rsh, rth);
    templ = mipsdsp_rshift1_add_q16(rsl, rtl);
    rd = ((uint32_t)temph << 16) | ((uint32_t)templ & MIPSDSP_LO);

    return rd;
}

uint32_t helper_addqh_r_ph(uint32_t rs, uint32_t rt)
{
    uint32_t rd;
    int16_t rsh, rsl, rth, rtl, temph, templ;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    temph = mipsdsp_rrshift1_add_q16(rsh, rth);
    templ = mipsdsp_rrshift1_add_q16(rsl, rtl);
    rd = ((uint32_t)temph << 16) | ((uint32_t)templ & MIPSDSP_LO);

    return rd;
}

uint32_t helper_addqh_w(uint32_t rs, uint32_t rt)
{
    uint32_t rd;

    rd = mipsdsp_rshift1_add_q32(rs, rt);

    return rd;
}

uint32_t helper_addqh_r_w(uint32_t rs, uint32_t rt)
{
    uint32_t rd;

    rd = mipsdsp_rrshift1_add_q32(rs, rt);

    return rd;
}

uint32_t helper_subq_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl;
    uint16_t rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = mipsdsp_sub_i16(env, rsh, rth);
    tempA = mipsdsp_sub_i16(env, rsl, rtl);
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_subq_s_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl;
    uint16_t rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = mipsdsp_sat16_sub(env, rsh, rth);
    tempA = mipsdsp_sat16_sub(env, rsl, rtl);
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_subq_s_w(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint32_t rd;

    rd = mipsdsp_sat32_sub(env, rs, rt);

    return rd;
}

uint32_t helper_subu_qb(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint8_t rs3, rs2, rs1, rs0;
    uint8_t rt3, rt2, rt1, rt0;
    uint8_t tempD, tempC, tempB, tempA;
    uint32_t rd;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs0 =  rs & MIPSDSP_Q0;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    tempD = mipsdsp_sub_u8(env, rs3, rt3);
    tempC = mipsdsp_sub_u8(env, rs2, rt2);
    tempB = mipsdsp_sub_u8(env, rs1, rt1);
    tempA = mipsdsp_sub_u8(env, rs0, rt0);

    rd = ((uint32_t)tempD << 24) | ((uint32_t)tempC << 16) | \
         ((uint32_t)tempB <<  8) | (uint32_t)tempA;
    return rd;
}

uint32_t helper_subu_s_qb(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint8_t rs3, rs2, rs1, rs0;
    uint8_t rt3, rt2, rt1, rt0;
    uint8_t tempD, tempC, tempB, tempA;
    uint32_t rd;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs0 =  rs & MIPSDSP_Q0;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    tempD = mipsdsp_satu8_sub(env, rs3, rt3);
    tempC = mipsdsp_satu8_sub(env, rs2, rt2);
    tempB = mipsdsp_satu8_sub(env, rs1, rt1);
    tempA = mipsdsp_satu8_sub(env, rs0, rt0);

    rd = ((uint32_t)tempD << 24) | ((uint32_t)tempC << 16) | \
         ((uint32_t)tempB <<  8) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_subuh_qb(uint32_t rs, uint32_t rt)
{
    uint8_t rs3, rs2, rs1, rs0;
    uint8_t rt3, rt2, rt1, rt0;
    uint8_t tempD, tempC, tempB, tempA;
    uint32_t rd;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs0 =  rs & MIPSDSP_Q0;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    tempD = ((uint16_t)rs3 - (uint16_t)rt3) >> 1;
    tempC = ((uint16_t)rs2 - (uint16_t)rt2) >> 1;
    tempB = ((uint16_t)rs1 - (uint16_t)rt1) >> 1;
    tempA = ((uint16_t)rs0 - (uint16_t)rt0) >> 1;

    rd = ((uint32_t)tempD << 24) | ((uint32_t)tempC << 16) | \
         ((uint32_t)tempB <<  8) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_subuh_r_qb(uint32_t rs, uint32_t rt)
{
    uint8_t rs3, rs2, rs1, rs0;
    uint8_t rt3, rt2, rt1, rt0;
    uint8_t tempD, tempC, tempB, tempA;
    uint32_t rd;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs0 =  rs & MIPSDSP_Q0;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    tempD = ((uint16_t)rs3 - (uint16_t)rt3 + 1) >> 1;
    tempC = ((uint16_t)rs2 - (uint16_t)rt2 + 1) >> 1;
    tempB = ((uint16_t)rs1 - (uint16_t)rt1 + 1) >> 1;
    tempA = ((uint16_t)rs0 - (uint16_t)rt0 + 1) >> 1;

    rd = ((uint32_t)tempD << 24) | ((uint32_t)tempC << 16) | \
         ((uint32_t)tempB <<  8) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_subu_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl, rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = mipsdsp_sub_u16_u16(env, rth, rsh);
    tempA = mipsdsp_sub_u16_u16(env, rtl, rsl);
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;
    return rd;
}

uint32_t helper_subu_s_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl, rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = mipsdsp_satu16_sub_u16_u16(env, rth, rsh);
    tempA = mipsdsp_satu16_sub_u16_u16(env, rtl, rsl);
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_subqh_ph(uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl;
    uint16_t rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;
    tempB = mipsdsp_rshift1_sub_q16(rsh, rth);
    tempA = mipsdsp_rshift1_sub_q16(rsl, rtl);
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_subqh_r_ph(uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl;
    uint16_t rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;
    tempB = mipsdsp_rrshift1_sub_q16(rsh, rth);
    tempA = mipsdsp_rrshift1_sub_q16(rsl, rtl);
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_subqh_w(uint32_t rs, uint32_t rt)
{
    uint32_t rd;

    rd = mipsdsp_rshift1_sub_q32(rs, rt);

    return rd;
}

uint32_t helper_subqh_r_w(uint32_t rs, uint32_t rt)
{
    uint32_t rd;

    rd = mipsdsp_rrshift1_sub_q32(rs, rt);

    return rd;
}

uint32_t helper_addsc(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint32_t rd;
    uint64_t temp, tempRs, tempRt;
    int32_t flag;

    tempRs = (uint64_t)rs & MIPSDSP_LLO;
    tempRt = (uint64_t)rt & MIPSDSP_LLO;

    temp = tempRs + tempRt;
    flag = (temp & 0x0100000000ull) >> 32;
    set_DSPControl_carryflag(env, flag);
    rd = temp & MIPSDSP_LLO;

    return rd;
}

uint32_t helper_addwc(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint32_t rd;
    int32_t temp32, temp31;
    int64_t rsL, rtL, tempL;

    rsL = (int32_t)rs;
    rtL = (int32_t)rt;
    tempL = rsL + rtL + get_DSPControl_carryflag(env);
    temp31 = (tempL >> 31) & 0x01;
    temp32 = (tempL >> 32) & 0x01;

    if (temp31 != temp32) {
        set_DSPControl_overflow_flag(env, 1, 20);
    }

    rd = tempL & MIPSDSP_LLO;

    return rd;
}

uint32_t helper_modsub(uint32_t rs, uint32_t rt)
{
    int32_t decr;
    uint16_t lastindex;
    uint32_t rd;

    decr = rt & MIPSDSP_Q0;
    lastindex = (rt >> 8) & MIPSDSP_LO;

    if (rs == 0x00000000) {
        rd = (uint32_t)lastindex;
    } else {
        rd = rs - decr;
    }

    return rd;
}

uint32_t helper_raddu_w_qb(uint32_t rs)
{
    uint8_t  rs3, rs2, rs1, rs0;
    uint16_t temp;
    uint32_t rd;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs0 =  rs & MIPSDSP_Q0;

    temp = (uint16_t)rs3 + (uint16_t)rs2 + (uint16_t)rs1 + (uint16_t)rs0;
    rd = temp;

    return rd;
}

uint32_t helper_absq_s_qb(CPUMIPSState *env, uint32_t rt)
{
    uint32_t rd;
    int8_t tempD, tempC, tempB, tempA;

    tempD = (rt & MIPSDSP_Q3) >> 24;
    tempC = (rt & MIPSDSP_Q2) >> 16;
    tempB = (rt & MIPSDSP_Q1) >>  8;
    tempA =  rt & MIPSDSP_Q0;

    rd = (((uint32_t)mipsdsp_sat_abs_u8 (env, tempD) << 24) & MIPSDSP_Q3) | \
         (((uint32_t)mipsdsp_sat_abs_u8 (env, tempC) << 16) & MIPSDSP_Q2) | \
         (((uint32_t)mipsdsp_sat_abs_u8 (env, tempB) <<  8) & MIPSDSP_Q1) | \
         ((uint32_t)mipsdsp_sat_abs_u8 (env, tempA) & MIPSDSP_Q0);

    return rd;
}

uint32_t helper_absq_s_ph(CPUMIPSState *env, uint32_t rt)
{
    uint32_t rd;
    int16_t tempA, tempB;

    tempA = (rt & MIPSDSP_HI) >> 16;
    tempB =  rt & MIPSDSP_LO;

    rd = ((uint32_t)mipsdsp_sat_abs_u16 (env, tempA) << 16) | \
         ((uint32_t)(mipsdsp_sat_abs_u16 (env, tempB)) & 0xFFFF);

    return rd;
}

uint32_t helper_absq_s_w(CPUMIPSState *env, uint32_t rt)
{
    uint32_t rd;
    int32_t temp;

    temp = rt;
    rd = mipsdsp_sat_abs_u32(env, temp);

    return rd;
}

uint32_t helper_precr_qb_ph(uint32_t rs, uint32_t rt)
{
    uint8_t  rs2, rs0, rt2, rt0;
    uint32_t rd;

    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs0 =  rs & MIPSDSP_Q0;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt0 =  rt & MIPSDSP_Q0;
    rd = ((uint32_t)rs2 << 24) | ((uint32_t)rs0 << 16) | \
         ((uint32_t)rt2 <<  8) | (uint32_t)rt0;

    return rd;
}

uint32_t helper_precrq_qb_ph(uint32_t rs, uint32_t rt)
{
    uint8_t tempD, tempC, tempB, tempA;
    uint32_t rd;

    tempD = (rs & MIPSDSP_Q3) >> 24;
    tempC = (rs & MIPSDSP_Q1) >>  8;
    tempB = (rt & MIPSDSP_Q3) >> 24;
    tempA = (rt & MIPSDSP_Q1) >>  8;

    rd = ((uint32_t)tempD << 24) | ((uint32_t)tempC << 16) | \
         ((uint32_t)tempB <<  8) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_precr_sra_ph_w(int sa, uint32_t rs, uint32_t rt)
{
    uint16_t tempB, tempA;

    if (sa == 0) {
        tempB = rt & MIPSDSP_LO;
        tempA = rs & MIPSDSP_LO;
    } else {
        tempB = ((int32_t)rt >> sa) & MIPSDSP_LO;
        tempA = ((int32_t)rs >> sa) & MIPSDSP_LO;
    }
    rt = ((uint32_t)tempB << 16) | ((uint32_t)tempA & MIPSDSP_LO);

    return rt;
}

uint32_t helper_precr_sra_r_ph_w(int sa, uint32_t rs, uint32_t rt)
{
    uint64_t tempB, tempA;

    if (sa == 0) {
        tempB = (rt & MIPSDSP_LO) << 1;
        tempA = (rs & MIPSDSP_LO) << 1;
    } else {
        tempB = ((int32_t)rt >> (sa - 1)) + 1;
        tempA = ((int32_t)rs >> (sa - 1)) + 1;
    }
    rt = (((tempB >> 1) & MIPSDSP_LO) << 16) | ((tempA >> 1) & MIPSDSP_LO);

    return rt;
}

uint32_t helper_precrq_ph_w(uint32_t rs, uint32_t rt)
{
    uint16_t tempB, tempA;
    uint32_t rd;

    tempB = (rs & MIPSDSP_HI) >> 16;
    tempA = (rt & MIPSDSP_HI) >> 16;
    rd = ((uint32_t)tempB << 16) | ((uint32_t)tempA & MIPSDSP_LO);

    return rd;
}

uint32_t helper_precrq_rs_ph_w(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint16_t tempB, tempA;
    uint32_t rd;

    tempB = mipsdsp_trunc16_sat16_round(env, rs);
    tempA = mipsdsp_trunc16_sat16_round(env, rt);
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_precrqu_s_qb_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint8_t  tempD, tempC, tempB, tempA;
    uint16_t rsh, rsl, rth, rtl;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempD = mipsdsp_sat8_reduce_precision(env, rsh);
    tempC = mipsdsp_sat8_reduce_precision(env, rsl);
    tempB = mipsdsp_sat8_reduce_precision(env, rth);
    tempA = mipsdsp_sat8_reduce_precision(env, rtl);

    rd = ((uint32_t)tempD << 24) | ((uint32_t)tempC << 16) | \
         ((uint32_t)tempB <<  8) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_preceq_w_phl(uint32_t rt)
{
    uint32_t rd;

    rd = rt & MIPSDSP_HI;

    return rd;
}

uint32_t helper_preceq_w_phr(uint32_t rt)
{
    uint16_t rtl;
    uint32_t rd;

    rtl = rt & MIPSDSP_LO;
    rd  = rtl << 16;

    return rd;
}

uint32_t helper_precequ_ph_qbl(uint32_t rt)
{
    uint8_t  rt3, rt2;
    uint16_t tempB, tempA;
    uint32_t rd;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;

    tempB = (uint16_t)rt3 << 7;
    tempA = (uint16_t)rt2 << 7;
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_precequ_ph_qbr(uint32_t rt)
{
    uint8_t  rt1, rt0;
    uint16_t tempB, tempA;
    uint32_t rd;

    rt1 = (rt & MIPSDSP_Q1) >> 8;
    rt0 =  rt & MIPSDSP_Q0;
    tempB = (uint16_t)rt1 << 7;
    tempA = (uint16_t)rt0 << 7;
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_precequ_ph_qbla(uint32_t rt)
{
    uint8_t  rt3, rt1;
    uint16_t tempB, tempA;
    uint32_t rd;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt1 = (rt & MIPSDSP_Q1) >>  8;

    tempB = (uint16_t)rt3 << 7;
    tempA = (uint16_t)rt1 << 7;
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_precequ_ph_qbra(uint32_t rt)
{
    uint8_t  rt2, rt0;
    uint16_t tempB, tempA;
    uint32_t rd;

    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt0 =  rt & MIPSDSP_Q0;
    tempB = (uint16_t)rt2 << 7;
    tempA = (uint16_t)rt0 << 7;
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_preceu_ph_qbl(uint32_t rt)
{
    uint8_t  rt3, rt2;
    uint16_t tempB, tempA;
    uint32_t rd;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    tempB = (uint16_t) rt3;
    tempA = (uint16_t) rt2;
    rd = ((uint32_t)tempB << 16) | ((uint32_t)tempA & MIPSDSP_LO);

    return rd;
}

uint32_t helper_preceu_ph_qbr(uint32_t rt)
{
    uint8_t  rt1, rt0;
    uint16_t tempB, tempA;
    uint32_t rd;

    rt1 = (rt & MIPSDSP_Q1) >> 8;
    rt0 =  rt & MIPSDSP_Q0;
    tempB = (uint16_t) rt1;
    tempA = (uint16_t) rt0;
    rd = ((uint32_t)tempB << 16) | ((uint32_t)tempA & MIPSDSP_LO);
    return rd;
}

uint32_t helper_preceu_ph_qbla(uint32_t rt)
{
    uint8_t  rt3, rt1;
    uint16_t tempB, tempA;
    uint32_t rd;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    tempB = (uint16_t) rt3;
    tempA = (uint16_t) rt1;
    rd = ((uint32_t)tempB << 16) | ((uint32_t)tempA & MIPSDSP_LO);

    return rd;
}

uint32_t helper_preceu_ph_qbra(uint32_t rt)
{
    uint8_t  rt2, rt0;
    uint16_t tempB, tempA;
    uint32_t rd;

    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt0 =  rt & MIPSDSP_Q0;
    tempB = (uint16_t)rt2;
    tempA = (uint16_t)rt0;
    rd = ((uint32_t)tempB << 16) | ((uint32_t)tempA & MIPSDSP_LO);
    return rd;
}

/** DSP GPR-Based Shift Sub-class insns **/
uint32_t helper_shll_qb(CPUMIPSState *env, int sa, uint32_t rt)
{
    uint8_t  rt3, rt2, rt1, rt0;
    uint8_t  tempD, tempC, tempB, tempA;
    uint32_t rd;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    tempD = mipsdsp_lshift8(env, rt3, sa);
    tempC = mipsdsp_lshift8(env, rt2, sa);
    tempB = mipsdsp_lshift8(env, rt1, sa);
    tempA = mipsdsp_lshift8(env, rt0, sa);
    rd = ((uint32_t)tempD << 24) | ((uint32_t)tempC << 16) | \
         ((uint32_t)tempB <<  8) | ((uint32_t)tempA);

    return rd;
}

uint32_t helper_shllv_qb(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint8_t  rs2_0;
    uint8_t  rt3, rt2, rt1, rt0;
    uint8_t  tempD, tempC, tempB, tempA;
    uint32_t rd;

    rs2_0 = rs & 0x07;
    rt3   = (rt & MIPSDSP_Q3) >> 24;
    rt2   = (rt & MIPSDSP_Q2) >> 16;
    rt1   = (rt & MIPSDSP_Q1) >>  8;
    rt0   =  rt & MIPSDSP_Q0;

    tempD = mipsdsp_lshift8(env, rt3, rs2_0);
    tempC = mipsdsp_lshift8(env, rt2, rs2_0);
    tempB = mipsdsp_lshift8(env, rt1, rs2_0);
    tempA = mipsdsp_lshift8(env, rt0, rs2_0);

    rd = ((uint32_t)tempD << 24) | ((uint32_t)tempC << 16) | \
         ((uint32_t)tempB <<  8) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_shll_ph(CPUMIPSState *env, int sa, uint32_t rt)
{
    uint16_t rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;
    tempB = mipsdsp_lshift16(env, rth, sa);
    tempA = mipsdsp_lshift16(env, rtl, sa);
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_shllv_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint8_t  rs3_0;
    uint16_t rth, rtl, tempB, tempA;
    uint32_t rd;

    rth   = (rt & MIPSDSP_HI) >> 16;
    rtl   =  rt & MIPSDSP_LO;
    rs3_0 = rs & 0x0F;

    tempB = mipsdsp_lshift16(env, rth, rs3_0);
    tempA = mipsdsp_lshift16(env, rtl, rs3_0);
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_shll_s_ph(CPUMIPSState *env, int sa, uint32_t rt)
{
    uint16_t rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;
    tempB = mipsdsp_sat16_lshift(env, rth, sa);
    tempA = mipsdsp_sat16_lshift(env, rtl, sa);
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_shllv_s_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint8_t  rs3_0;
    uint16_t rth, rtl, tempB, tempA;
    uint32_t rd;

    rth   = (rt & MIPSDSP_HI) >> 16;
    rtl   =  rt & MIPSDSP_LO;
    rs3_0 = rs & 0x0F;

    tempB = mipsdsp_sat16_lshift(env, rth, rs3_0);
    tempA = mipsdsp_sat16_lshift(env, rtl, rs3_0);
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_shll_s_w(CPUMIPSState *env, int sa, uint32_t rt)
{
    uint32_t temp, rd;

    temp = mipsdsp_sat32_lshift(env, rt, sa);
    rd   = temp;

    return rd;
}

uint32_t helper_shllv_s_w(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint8_t  rs4_0;
    uint32_t rd;

    rs4_0 = rs & 0x1F;
    rd = mipsdsp_sat32_lshift(env, rt, rs4_0);

    return rd;
}

uint32_t helper_shrl_qb(int sa, uint32_t rt)
{
    uint8_t  rt3, rt2, rt1, rt0;
    uint8_t  tempD, tempC, tempB, tempA;
    uint32_t rd;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    tempD = mipsdsp_rshift8(rt3, sa);
    tempC = mipsdsp_rshift8(rt2, sa);
    tempB = mipsdsp_rshift8(rt1, sa);
    tempA = mipsdsp_rshift8(rt0, sa);

    rd = ((uint32_t)tempD << 24) | ((uint32_t)tempC << 16) | \
         ((uint32_t)tempB <<  8) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_shrlv_qb(uint32_t rs, uint32_t rt)
{
    uint8_t rs2_0;
    uint8_t rt3, rt2, rt1, rt0;
    uint8_t tempD, tempC, tempB, tempA;
    uint32_t rd;

    rs2_0 = rs & 0x07;
    rt3   = (rt & MIPSDSP_Q3) >> 24;
    rt2   = (rt & MIPSDSP_Q2) >> 16;
    rt1   = (rt & MIPSDSP_Q1) >>  8;
    rt0   =  rt & MIPSDSP_Q0;

    tempD = mipsdsp_rshift8(rt3, rs2_0);
    tempC = mipsdsp_rshift8(rt2, rs2_0);
    tempB = mipsdsp_rshift8(rt1, rs2_0);
    tempA = mipsdsp_rshift8(rt0, rs2_0);
    rd = ((uint32_t)tempD << 24) | ((uint32_t)tempC << 16) | \
         ((uint32_t)tempB <<  8) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_shrl_ph(int sa, uint32_t rt)
{
    uint16_t rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;
    tempB = rth >> sa;
    tempA = rtl >> sa;
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_shrlv_ph(uint32_t rs, uint32_t rt)
{
    uint8_t  rs3_0;
    uint16_t rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rs3_0 = rs & 0x0F;
    rth   = (rt & MIPSDSP_HI) >> 16;
    rtl   =  rt & MIPSDSP_LO;

    tempB = rth >> rs3_0;
    tempA = rtl >> rs3_0;
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_shra_qb(int sa, uint32_t rt)
{
    int8_t  rt3, rt2, rt1, rt0;
    uint8_t tempD, tempC, tempB, tempA;
    uint32_t rd;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    tempD = rt3 >> sa;
    tempC = rt2 >> sa;
    tempB = rt1 >> sa;
    tempA = rt0 >> sa;

    rd = ((uint32_t)tempD << 24) | ((uint32_t)tempC << 16) | \
          ((uint32_t)tempB << 8) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_shra_r_qb(int sa, uint32_t rt)
{
    int8_t  rt3, rt2, rt1, rt0;
    uint16_t tempD, tempC, tempB, tempA;
    uint32_t rd;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    if (sa == 0) {
        tempD = rt3 & 0x00FF;
        tempC = rt2 & 0x00FF;
        tempB = rt1 & 0x00FF;
        tempA = rt0 & 0x00FF;
    } else {
        tempD = ((int16_t)rt3 >> (sa - 1)) + 1;
        tempC = ((int16_t)rt2 >> (sa - 1)) + 1;
        tempB = ((int16_t)rt1 >> (sa - 1)) + 1;
        tempA = ((int16_t)rt0 >> (sa - 1)) + 1;
    }

    rd = ((uint32_t)((tempD >> 1) & 0x00FF) << 24) | \
         ((uint32_t)((tempC >> 1) & 0x00FF) << 16) | \
         ((uint32_t)((tempB >> 1) & 0x00FF) <<  8) | \
         (uint32_t)((tempA >>  1) & 0x00FF) ;

    return rd;
}

uint32_t helper_shrav_qb(uint32_t rs, uint32_t rt)
{
    uint8_t  rs2_0;
    int8_t   rt3, rt2, rt1, rt0;
    uint8_t  tempD, tempC, tempB, tempA;
    uint32_t rd;

    rs2_0 = rs & 0x07;
    rt3   = (rt & MIPSDSP_Q3) >> 24;
    rt2   = (rt & MIPSDSP_Q2) >> 16;
    rt1   = (rt & MIPSDSP_Q1) >>  8;
    rt0   =  rt & MIPSDSP_Q0;

    if (rs2_0 == 0) {
        tempD = rt3;
        tempC = rt2;
        tempB = rt1;
        tempA = rt0;
    } else {
        tempD = rt3 >> rs2_0;
        tempC = rt2 >> rs2_0;
        tempB = rt1 >> rs2_0;
        tempA = rt0 >> rs2_0;
    }

    rd = ((uint32_t)tempD << 24) | ((uint32_t)tempC << 16) | \
         ((uint32_t)tempB <<  8) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_shrav_r_qb(uint32_t rs, uint32_t rt)
{
    uint8_t rs2_0;
    int8_t  rt3, rt2, rt1, rt0;
    uint16_t tempD, tempC, tempB, tempA;
    uint32_t rd;

    rs2_0 = rs & 0x07;
    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    if (rs2_0 == 0) {
        tempD = (int16_t)rt3 << 1;
        tempC = (int16_t)rt2 << 1;
        tempB = (int16_t)rt1 << 1;
        tempA = (int16_t)rt0 << 1;
    } else {
        tempD = ((int16_t)rt3 >> (rs2_0 - 1)) + 1;
        tempC = ((int16_t)rt2 >> (rs2_0 - 1)) + 1;
        tempB = ((int16_t)rt1 >> (rs2_0 - 1)) + 1;
        tempA = ((int16_t)rt0 >> (rs2_0 - 1)) + 1;
    }

    rd = ((uint32_t)((tempD >> 1) & 0x00FF) << 24) | \
         ((uint32_t)((tempC >> 1) & 0x00FF) << 16) | \
         ((uint32_t)((tempB >> 1) & 0x00FF) <<  8) | \
         (uint32_t)((tempA >>  1) & 0x00FF) ;

    return rd;
}

uint32_t helper_shra_ph(int sa, uint32_t rt)
{
    uint16_t rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;
    tempB = mipsdsp_rashift16(rth, sa);
    tempA = mipsdsp_rashift16(rtl, sa);
    rd = ((uint32_t)tempB << 16) | (uint32_t) tempA;

    return rd;
}

uint32_t helper_shrav_ph(uint32_t rs, uint32_t rt)
{
    uint8_t  rs3_0;
    uint16_t rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rs3_0 = rs & 0x0F;
    rth   = (rt & MIPSDSP_HI) >> 16;
    rtl   =  rt & MIPSDSP_LO;
    tempB = mipsdsp_rashift16(rth, rs3_0);
    tempA = mipsdsp_rashift16(rtl, rs3_0);
    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_shra_r_ph(int sa, uint32_t rt)
{
    uint16_t rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;
    tempB = mipsdsp_rnd16_rashift(rth, sa);
    tempA = mipsdsp_rnd16_rashift(rtl, sa);
    rd = ((uint32_t)tempB << 16) | (uint32_t) tempA;

    return rd;
}

uint32_t helper_shrav_r_ph(uint32_t rs, uint32_t rt)
{
    uint8_t  rs3_0;
    uint16_t rth, rtl;
    uint16_t tempB, tempA;
    uint32_t rd;

    rs3_0 = rs & 0x0F;
    rth   = (rt & MIPSDSP_HI) >> 16;
    rtl   =  rt & MIPSDSP_LO;
    tempB = mipsdsp_rnd16_rashift(rth, rs3_0);
    tempA = mipsdsp_rnd16_rashift(rtl, rs3_0);

    rd = ((uint32_t)tempB << 16) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_shra_r_w(int sa, uint32_t rt)
{
    uint32_t rd;

    rd = mipsdsp_rnd32_rashift(rt, sa);

    return rd;
}

uint32_t helper_shrav_r_w(uint32_t rs, uint32_t rt)
{
    uint8_t rs4_0;
    uint32_t rd;

    rs4_0 = rs & 0x1F;
    rd = mipsdsp_rnd32_rashift(rt, rs4_0);

    return rd;
}

/** DSP Multiply Sub-class insns **/
uint32_t helper_muleu_s_ph_qbl(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint8_t rs3, rs2;
    uint16_t tempB, tempA, rth, rtl;
    uint32_t temp;
    uint32_t rd;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;
    tempB = mipsdsp_mul_u8_u16(env, rs3, rth);
    tempA = mipsdsp_mul_u8_u16(env, rs2, rtl);
    temp = ((uint32_t)tempB << 16) | ((uint32_t)tempA & MIPSDSP_LO);
    rd = temp;
    return rd;
}

uint32_t helper_muleu_s_ph_qbr(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint8_t  rs1, rs0;
    uint16_t tempB, tempA;
    uint16_t rth,   rtl;
    uint32_t temp;
    uint32_t rd;

    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs0 =  rs & MIPSDSP_Q0;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = mipsdsp_mul_u8_u16(env, rs1, rth);
    tempA = mipsdsp_mul_u8_u16(env, rs0, rtl);
    temp = ((uint32_t)tempB << 16) | ((uint32_t)tempA & MIPSDSP_LO);
    rd = temp;
    return rd;
}

uint32_t helper_mulq_rs_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    int16_t tempB, tempA, rsh, rsl, rth, rtl;
    int32_t temp;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = mipsdsp_rndq15_mul_q15_q15(env, rsh, rth);
    tempA = mipsdsp_rndq15_mul_q15_q15(env, rsl, rtl);
    temp = ((uint32_t)tempB << 16) | ((uint32_t)tempA & MIPSDSP_LO);
    rd = temp;

    return rd;
}

uint32_t helper_muleq_s_w_phl(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rth;
    int32_t temp;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rth = (rt & MIPSDSP_HI) >> 16;
    temp = mipsdsp_mul_q15_q15_overflowflag21(env, rsh, rth);
    rd = temp;

    return rd;
}

uint32_t helper_muleq_s_w_phr(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    int16_t rsl, rtl;
    int32_t temp;
    uint32_t rd;

    rsl = rs & MIPSDSP_LO;
    rtl = rt & MIPSDSP_LO;
    temp = mipsdsp_mul_q15_q15_overflowflag21(env, rsl, rtl);
    rd = temp;

    return rd;
}

void helper_dpau_h_qbl(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    uint8_t rs3, rs2;
    uint8_t rt3, rt2;
    uint16_t tempB, tempA;
    uint64_t tempC, tempBL, tempAL, dotp;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    tempB = mipsdsp_mul_u8_u8(rs3, rt3);
    tempA = mipsdsp_mul_u8_u8(rs2, rt2);
    tempBL = tempB;
    tempAL = tempA;
    dotp = tempBL + tempAL;
    tempC = (((uint64_t)env->active_tc.HI[ac] << 32) |  \
             ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO)) + dotp;

    env->active_tc.HI[ac] = (tempC & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  tempC & MIPSDSP_LLO;
}

void helper_dpau_h_qbr(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    uint8_t rs1, rs0;
    uint8_t rt1, rt0;
    uint16_t tempB, tempA;
    uint64_t tempC, tempBL, tempAL, dotp;

    rs1 = (rs & MIPSDSP_Q1) >> 8;
    rt1 = (rt & MIPSDSP_Q1) >> 8;
    rs0 = (rs & MIPSDSP_Q0);
    rt0 = (rt & MIPSDSP_Q0);
    tempB = mipsdsp_mul_u8_u8(rs1, rt1);
    tempA = mipsdsp_mul_u8_u8(rs0, rt0);
    tempBL = tempB;
    tempAL = tempA;
    dotp = tempBL + tempAL;
    tempC = (((uint64_t)env->active_tc.HI[ac] << 32) | \
             ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO)) + dotp;

    env->active_tc.HI[ac] = (tempC & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  tempC & MIPSDSP_LLO;
}

void helper_dpsu_h_qbl(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    uint8_t  rs3, rs2, rt3, rt2;
    uint16_t tempB,  tempA;
    uint64_t dotp, tempBL, tempAL, tempC;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;

    tempB = mipsdsp_mul_u8_u8(rs3, rt3);
    tempA = mipsdsp_mul_u8_u8(rs2, rt2);
    tempBL = tempB & 0xFFFF;
    tempAL = tempA & 0xFFFF;

    dotp   = tempBL + tempAL;
    tempC  = ((uint64_t)env->active_tc.HI[ac] << 32) | \
             ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    tempC -= dotp;

    env->active_tc.HI[ac] = (tempC & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  tempC & MIPSDSP_LLO;
}

void helper_dpsu_h_qbr(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    uint8_t  rs1, rs0, rt1, rt0;
    uint16_t tempB,  tempA;
    uint64_t dotp, tempBL, tempAL, tempC;

    rs1 = (rs & MIPSDSP_Q1) >> 8;
    rs0 = (rs & MIPSDSP_Q0);
    rt1 = (rt & MIPSDSP_Q1) >> 8;
    rt0 = (rt & MIPSDSP_Q0);

    tempB = mipsdsp_mul_u8_u8(rs1, rt1);
    tempA = mipsdsp_mul_u8_u8(rs0, rt0);
    tempBL = tempB & 0xFFFF;
    tempAL = tempA & 0xFFFF;

    dotp   = tempBL + tempAL;
    tempC  = ((uint64_t)env->active_tc.HI[ac] << 32) | \
             ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    tempC -= dotp;

    env->active_tc.HI[ac] = (tempC & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  tempC & MIPSDSP_LLO;
}

void helper_dpa_w_ph(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl, rth, rtl;
    int32_t  tempA, tempB;
    int64_t  acc, tempAL, tempBL;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = (int32_t)rsh * (int32_t)rth;
    tempA = (int32_t)rsl * (int32_t)rtl;
    tempBL = tempB;
    tempAL = tempA;

    acc = ((uint64_t)env->active_tc.HI[ac] << 32) | \
          ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    acc += tempBL + tempAL;

    env->active_tc.HI[ac] = (acc & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  acc & MIPSDSP_LLO;
}

void helper_dpax_w_ph(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl, rth, rtl;
    int32_t tempB, tempA;
    int64_t acc, dotp, tempBL, tempAL;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB  = (uint32_t)rsh * (uint32_t)rth;
    tempA  = (uint32_t)rsl * (uint32_t)rtl;
    tempBL = tempB;
    tempAL = tempA;
    dotp =  tempBL + tempAL;
    acc  =  ((uint64_t)env->active_tc.HI[ac] << 32) | \
            ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    acc  += dotp;

    env->active_tc.HI[ac] = (acc & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  acc & MIPSDSP_LLO;
}

void helper_dpaq_s_w_ph(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rsl, rth, rtl;
    int32_t tempB, tempA;
    int64_t acc, dotp, tempBL, tempAL;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = mipsdsp_mul_q15_q15(env, ac, rsh, rth);
    tempA = mipsdsp_mul_q15_q15(env, ac, rsl, rtl);
    tempBL = tempB;
    tempAL = tempA;
    dotp = tempBL + tempAL;
    acc = ((uint64_t)env->active_tc.HI[ac] << 32) | \
          ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    acc += dotp;

    env->active_tc.HI[ac] = (acc & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  acc & MIPSDSP_LLO;
}

void helper_dpaqx_s_w_ph(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl, rth, rtl;
    int32_t tempB, tempA;
    int64_t acc, dotp, tempBL, tempAL;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = mipsdsp_mul_q15_q15(env, ac, rsh, rtl);
    tempA = mipsdsp_mul_q15_q15(env, ac, rsl, rth);
    tempBL = tempB;
    tempAL = tempA;
    dotp = tempBL + tempAL;
    acc = ((uint64_t)env->active_tc.HI[ac] << 32) | \
          ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    acc += dotp;

    env->active_tc.HI[ac] = (acc & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  acc & MIPSDSP_LLO;
}

void helper_dpaqx_sa_w_ph(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rsl, rth, rtl;
    int32_t tempB, tempA, tempC62_31, tempC63;
    int64_t acc, dotp, tempBL, tempAL, tempC;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = mipsdsp_mul_q15_q15(env, ac, rsh, rtl);
    tempA = mipsdsp_mul_q15_q15(env, ac, rsl, rth);
    tempBL = tempB;
    tempAL = tempA;
    dotp = tempBL + tempAL;
    acc = ((uint64_t)env->active_tc.HI[ac] << 32) | \
          ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    tempC = acc + dotp;
    tempC63 = (tempC >> 63) & 0x01;
    tempC62_31 = (tempC >> 31) & 0xFFFFFFFF;

    if ((tempC63 == 0) && (tempC62_31 == 0xFFFFFFFF)) {
        tempC = 0x80000000;
        set_DSPControl_overflow_flag(env, 1, 16 + ac);
    }

    env->active_tc.HI[ac] = (tempC & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  tempC & MIPSDSP_LLO;
}

void helper_dps_w_ph(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl, rth, rtl;
    int32_t tempB, tempA;
    int64_t acc, dotp, tempBL, tempAL;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB  = (int32_t)rsh * (int32_t)rth;
    tempA  = (int32_t)rsl * (int32_t)rtl;
    tempBL = tempB;
    tempAL = tempA;
    dotp =  tempBL + tempAL;
    acc  =  ((uint64_t)env->active_tc.HI[ac] << 32) | \
            ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    acc  -= dotp;

    env->active_tc.HI[ac] = (acc & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  acc & MIPSDSP_LLO;
}

void helper_dpsx_w_ph(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl, rth, rtl;
    int32_t  tempB,  tempA;
    int64_t acc, dotp, tempBL, tempAL;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = (int32_t)rsh * (int32_t)rtl;
    tempA = (int32_t)rsl * (int32_t)rth;
    tempBL = tempB;
    tempAL = tempA;
    dotp = tempBL + tempAL;

    acc  = ((uint64_t)env->active_tc.HI[ac] << 32) | \
           ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    acc -= dotp;
    env->active_tc.HI[ac] = (acc & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  acc & MIPSDSP_LLO;
}

void helper_dpsq_s_w_ph(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rsl, rth, rtl;
    int32_t tempB, tempA;
    int64_t acc, dotp, tempBL, tempAL;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = mipsdsp_mul_q15_q15(env, ac, rsh, rth);
    tempA = mipsdsp_mul_q15_q15(env, ac, rsl, rtl);
    tempBL = tempB;
    tempAL = tempA;
    dotp = tempBL + tempAL;
    acc = ((uint64_t)env->active_tc.HI[ac] << 32) | \
           ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    acc -= dotp;

    env->active_tc.HI[ac] = (acc & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  acc & MIPSDSP_LLO;
}

void helper_dpsqx_s_w_ph(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rsl, rth, rtl;
    int32_t tempB, tempA;
    int64_t dotp, tempC, tempBL, tempAL;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = mipsdsp_mul_q15_q15(env, ac, rsh, rtl);
    tempA = mipsdsp_mul_q15_q15(env, ac, rsl, rth);
    tempBL = tempB;
    tempAL = tempA;
    dotp = tempBL + tempAL;
    tempC = (((uint64_t)env->active_tc.HI[ac] << 32) | \
            ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO)) - dotp;

    env->active_tc.HI[ac] = (tempC & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  tempC & MIPSDSP_LLO;
}

void helper_dpsqx_sa_w_ph(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rsl, rth, rtl;
    int32_t tempB, tempA, tempC63, tempC62_31;
    int64_t dotp, tempBL, tempAL, tempC;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;
    tempB = mipsdsp_mul_q15_q15(env, ac, rsh, rtl);
    tempA = mipsdsp_mul_q15_q15(env, ac, rsl, rth);

    tempBL = tempB;
    tempAL = tempA;
    dotp   = tempBL + tempAL;
    tempC  = ((uint64_t)env->active_tc.HI[ac] << 32) | \
             ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    tempC -= dotp;

    tempC63 = (tempC >> 63) & 0x01;
    tempC62_31 = (tempC >> 31) & 0xFFFFFFFF;

    if ((tempC63 == 0) && (tempC62_31 != 0)) {
        tempC = 0x7FFFFFFF;
        set_DSPControl_overflow_flag(env, 1, 16 + ac);
    }

    if ((tempC63 == 1) && (tempC62_31 != 0xFFFFFFFF)) {
        tempC = 0xFFFFFFFF80000000ull;
        set_DSPControl_overflow_flag(env, 1, 16 + ac);
    }

    env->active_tc.HI[ac] = (tempC & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  tempC & MIPSDSP_LLO;
}

void helper_mulsaq_s_w_ph(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rsl, rth, rtl;
    int32_t tempB, tempA;
    int64_t tempBL, tempAL, acc, dotp;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = mipsdsp_mul_q15_q15(env, ac, rsh, rth);
    tempA = mipsdsp_mul_q15_q15(env, ac, rsl, rtl);
    tempBL = tempB;
    tempAL = tempA;
    dotp = tempBL - tempAL;
    acc = ((uint64_t)env->active_tc.HI[ac] << 32) | \
          ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    dotp = dotp + acc;
    env->active_tc.HI[ac] = (dotp & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  dotp & MIPSDSP_LLO;
}

void helper_dpaq_sa_l_w(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    int32_t temp64, temp63, tempacc63, tempdotp63, tempDL63;
    int64_t dotp, acc;
    int64_t tempDL[2];
    uint64_t temp;

    dotp = mipsdsp_mul_q31_q31(env, ac, rs, rt);
    acc = ((uint64_t)env->active_tc.HI[ac] << 32) | \
          ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    tempDL[0] = acc + dotp;

    tempacc63  = (acc >> 63) & 0x01;
    tempdotp63 = (dotp >> 63) & 0x01;
    tempDL63   = (tempDL[0] >> 63) & 0x01;

    if (((tempacc63 == 1) && (tempdotp63 == 1)) | \
        (((tempacc63 == 1) || (tempdotp63 == 1)) && tempDL63 == 0)) {
        tempDL[1] = 1;
    } else {
        tempDL[1] = 0;
    }

    temp = tempDL[0];
    temp64 = tempDL[1] & 0x01;
    temp63 = (tempDL[0] >> 63) & 0x01;

    if (temp64 != temp63) {
        if (temp64 == 1) {
            temp = 0x8000000000000000ull;
        } else {
            temp = 0x7FFFFFFFFFFFFFFFull;
        }

        set_DSPControl_overflow_flag(env, 1, 16 + ac);
    }

    env->active_tc.HI[ac] = (temp & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  temp & MIPSDSP_LLO;
}

void helper_dpsq_sa_l_w(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    int32_t temp64, temp63, tempacc63, tempdotp63, tempDL63;
    int64_t dotp, acc;
    int64_t tempDL[2];
    uint64_t temp;

    dotp = mipsdsp_mul_q31_q31(env, ac, rs, rt);
    acc = ((uint64_t)env->active_tc.HI[ac] << 32) | \
          ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    tempDL[0] = acc - dotp;

    tempacc63  = (acc >> 63) & 0x01;
    tempdotp63 = (dotp >> 63) & 0x01;
    tempDL63   = (tempDL[0] >> 63) & 0x01;

    if (((tempacc63 == 1) && (tempdotp63 == 0)) | \
        (((tempacc63 == 1) || (tempdotp63 == 0)) && tempDL63 == 0)) {
        tempDL[1] = 1;
    } else {
        tempDL[1] = 0;
    }

    temp = tempDL[0];
    temp64 = tempDL[1] & 0x01;
    temp63 = (tempDL[0] >> 63) & 0x01;
    if (temp64 != temp63) {
        if (temp64 == 1) {
            temp = 0x8000000000000000ull;
        } else {
            temp = 0x7FFFFFFFFFFFFFFFull;
        }
        set_DSPControl_overflow_flag(env, 1, ac + 16);
    }

    env->active_tc.HI[ac] = (temp & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  temp & MIPSDSP_LLO;
}

void helper_maq_s_w_phl(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rth;
    int32_t  tempA;
    int64_t tempL, tempAL, acc;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rth = (rt & MIPSDSP_HI) >> 16;
    tempA  = mipsdsp_mul_q15_q15(env, ac, rsh, rth);
    tempAL = tempA;
    acc = ((uint64_t)env->active_tc.HI[ac] << 32) | \
          ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    tempL  = tempAL + acc;
    env->active_tc.HI[ac] = (tempL & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  tempL & MIPSDSP_LLO;
}

void helper_maq_s_w_phr(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    int16_t rsl, rtl;
    int32_t tempA;
    int64_t tempL, tempAL, acc;

    rsl = rs & MIPSDSP_LO;
    rtl = rt & MIPSDSP_LO;
    tempA  = mipsdsp_mul_q15_q15(env, ac, rsl, rtl);
    tempAL = tempA;
    acc = ((uint64_t)env->active_tc.HI[ac] << 32) | \
          ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    tempL = tempAL + acc;

    env->active_tc.HI[ac] = (tempL & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  tempL & MIPSDSP_LLO;
}

void helper_maq_sa_w_phl(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rth;
    int32_t tempA;
    int64_t tempAL;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rth = (rt & MIPSDSP_HI) >> 16;
    tempA = mipsdsp_mul_q15_q15(env, ac, rsh, rth);
    tempA = mipsdsp_sat32_acc_q31(env, ac, tempA);
    tempAL = tempA;

    env->active_tc.HI[ac] = (tempAL & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  tempAL & MIPSDSP_LLO;
}

void helper_maq_sa_w_phr(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    int16_t rsl, rtl;
    int32_t tempA;
    int64_t tempAL;

    rsl = rs & MIPSDSP_LO;
    rtl = rs & MIPSDSP_LO;

    tempA = mipsdsp_mul_q15_q15(env, ac, rsl, rtl);
    tempA = mipsdsp_sat32_acc_q31(env, ac, tempA);
    tempAL = tempA;

    env->active_tc.HI[ac] = (tempAL & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  tempAL & MIPSDSP_LLO;
}

uint32_t helper_mul_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rsl, rth, rtl;
    int32_t tempB, tempA;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;
    tempB = mipsdsp_mul_i16_i16(env, rsh, rth);
    tempA = mipsdsp_mul_i16_i16(env, rsl, rtl);

    rd = ((tempB & MIPSDSP_LO) << 16) | (tempA & MIPSDSP_LO);

    return rd;
}

uint32_t helper_mul_s_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    int16_t  rsh, rsl, rth, rtl;
    int32_t  tempB, tempA;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;
    tempB = mipsdsp_sat16_mul_i16_i16(env, rsh, rth);
    tempA = mipsdsp_sat16_mul_i16_i16(env, rsl, rtl);

    rd = ((tempB & MIPSDSP_LO) << 16) | (tempA & MIPSDSP_LO);

    return rd;
}

uint32_t helper_mulq_s_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rsl, rth, rtl;
    int32_t temp, tempB, tempA;
    uint32_t rd;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = mipsdsp_sat16_mul_q15_q15(env, rsh, rth);
    tempA = mipsdsp_sat16_mul_q15_q15(env, rsl, rtl);
    temp = ((tempB & MIPSDSP_LO) << 16) | (tempA & MIPSDSP_LO);
    rd = temp;

    return rd;
}

uint32_t helper_mulq_s_w(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint32_t rd;
    int32_t tempI;
    int64_t tempL;

    if ((rs == 0x80000000) && (rt == 0x80000000)) {
        tempL = 0x7FFFFFFF00000000ull;
        set_DSPControl_overflow_flag(env, 1, 21);
    } else {
        tempL  = ((int64_t)rs * (int64_t)rt) << 1;
    }
    tempI = (tempL & MIPSDSP_LHI) >> 32;
    rd = tempI;

    return rd;
}

uint32_t helper_mulq_rs_w(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint32_t rd;
    int32_t tempI;
    int64_t tempL;

    if ((rs == 0x80000000) && (rt == 0x80000000)) {
        tempL = 0x7FFFFFFF00000000ull;
        set_DSPControl_overflow_flag(env, 1, 21);
    } else {
        tempL  = ((int64_t)rs * (int64_t)rt) << 1;
        tempL += 0x80000000;
    }
    tempI = (tempL & MIPSDSP_LHI) >> 32;
    rd = tempI;

    return rd;
}

void helper_mulsa_w_ph(CPUMIPSState *env, int ac, uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl, rth, rtl;
    int32_t tempB, tempA;
    int64_t dotp, acc, tempBL, tempAL;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    tempB = (int32_t)rsh * (int32_t)rth;
    tempA = (int32_t)rsl * (int32_t)rtl;
    tempBL = tempB;
    tempAL = tempA;

    dotp = tempBL - tempAL;
    acc  = ((int64_t)env->active_tc.HI[ac] << 32) | \
           ((int64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    acc = acc + dotp;

    env->active_tc.HI[ac] = (acc & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  acc & MIPSDSP_LLO;
}

/** DSP Bit/Manipulation Sub-class insns **/
uint32_t helper_bitrev(uint32_t rt)
{
    int32_t temp;
    uint32_t rd;
    int i, last;

    temp = rt & MIPSDSP_LO;
    rd = 0;
    for (i = 0; i < 16; i++) {
        last = temp % 2;
        temp = temp >> 1;
        rd = rd | (last << (15 - i));
    }

    return rd;
}

uint32_t helper_insv(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint32_t pos, size, msb, lsb, rs_f, rt_f;
    uint32_t temp, temprs, temprt;
    target_ulong dspc;

    dspc = env->active_tc.DSPControl;
    pos  = dspc & 0x1F;
    size = (dspc >> 7) & 0x1F;
    msb  = pos + size - 1;
    lsb  = pos;

    if (lsb > msb) {
        return rt;
    }

    rs_f = (((int32_t)0x01 << (msb - lsb + 1 + 1)) - 1) << lsb;
    rt_f = rs_f ^ 0xFFFFFFFF;
    temprs = rs & rs_f;
    temprt = rt & rt_f;
    temp = temprs | temprt;
    return temp;
}

/** DSP Compare-Pick Sub-class insns **/
void helper_cmpu_eq_qb(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint8_t rs3, rs2, rs1, rs0;
    uint8_t rt3, rt2, rt1, rt0;
    uint32_t cc3 = 0, cc2 = 0, cc1 = 0, cc0 = 0;
    uint32_t flag;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs0 =  rs & MIPSDSP_Q0;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    cc3 = (rs3 == rt3);
    cc2 = (rs2 == rt2);
    cc1 = (rs1 == rt1);
    cc0 = (rs0 == rt0);

    flag = (cc3 << 3) | (cc2 << 2) | (cc1 << 1) | cc0;
    set_DSPControl_24(env, flag, 4);
}

void helper_cmpu_lt_qb(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint8_t rs3, rs2, rs1, rs0;
    uint8_t rt3, rt2, rt1, rt0;
    uint32_t cc3 = 0, cc2 = 0, cc1 = 0, cc0 = 0;
    uint32_t flag;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs0 =  rs & MIPSDSP_Q0;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    cc3 = (rs3 < rt3);
    cc2 = (rs2 < rt2);
    cc1 = (rs1 < rt1);
    cc0 = (rs0 < rt0);

    flag = (cc3 << 3) | (cc2 << 2) | (cc1 << 1) | cc0;
    set_DSPControl_24(env, flag, 4);
}

void helper_cmpu_le_qb(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint8_t rs3, rs2, rs1, rs0;
    uint8_t rt3, rt2, rt1, rt0;
    uint32_t cc3 = 0, cc2 = 0, cc1 = 0, cc0 = 0;
    uint32_t flag;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs0 =  rs & MIPSDSP_Q0;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    cc3 = (rs3 <= rt3);
    cc2 = (rs2 <= rt2);
    cc1 = (rs1 <= rt1);
    cc0 = (rs0 <= rt0);

    flag = (cc3 << 3) | (cc2 << 2) | (cc1 << 1) | cc0;
    set_DSPControl_24(env, flag, 4);
}


uint32_t helper_cmpgu_eq_qb(uint32_t rs, uint32_t rt)
{
    uint8_t rs3, rs2, rs1, rs0;
    uint8_t rt3, rt2, rt1, rt0;
    uint8_t cc3 = 0, cc2 = 0, cc1 = 0, cc0 = 0;
    uint32_t temp;
    uint32_t rd;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs0 =  rs & MIPSDSP_Q0;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    cc3 = (rs3 == rt3);
    cc2 = (rs2 == rt2);
    cc1 = (rs1 == rt1);
    cc0 = (rs0 == rt0);

    temp = (cc3 << 3) | (cc2 << 2) | (cc1 << 1) | cc0;
    rd = temp;

    return rd;
}

uint32_t helper_cmpgu_lt_qb(uint32_t rs, uint32_t rt)
{
    uint8_t rs3, rs2, rs1, rs0;
    uint8_t rt3, rt2, rt1, rt0;
    uint32_t cc3 = 0, cc2 = 0, cc1 = 0, cc0 = 0;
    uint32_t temp;
    uint32_t rd;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs0 =  rs & MIPSDSP_Q0;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    cc3 = (rs3 < rt3);
    cc2 = (rs2 < rt2);
    cc1 = (rs1 < rt1);
    cc0 = (rs0 < rt0);

    temp = (cc3 << 3) | (cc2 << 2) | (cc1 << 1) | cc0;
    rd = temp;

    return rd;
}

uint32_t helper_cmpgu_le_qb(uint32_t rs, uint32_t rt)
{
    uint8_t rs3, rs2, rs1, rs0;
    uint8_t rt3, rt2, rt1, rt0;
    uint32_t cc3 = 0, cc2 = 0, cc1 = 0, cc0 = 0;
    uint32_t temp;
    uint32_t rd;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs0 =  rs & MIPSDSP_Q0;

    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    cc3 = (rs3 <= rt3);
    cc2 = (rs2 <= rt2);
    cc1 = (rs1 <= rt1);
    cc0 = (rs0 <= rt0);

    temp = (cc3 << 3) | (cc2 << 2) | (cc1 << 1) | cc0;
    rd = temp;

    return rd;
}

void helper_cmp_eq_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rsl, rth, rtl;
    int32_t flag;
    int32_t ccA = 0, ccB = 0;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    ccB = (rsh == rth);
    ccA = (rsl == rtl);

    flag = (ccB << 1) | ccA;
    set_DSPControl_24(env, flag, 2);
}

void helper_cmp_lt_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rsl, rth, rtl;
    int32_t flag;
    int32_t ccA = 0, ccB = 0;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    ccB = (rsh < rth);
    ccA = (rsl < rtl);

    flag = (ccB << 1) | ccA;
    set_DSPControl_24(env, flag, 2);
}

void helper_cmp_le_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    int16_t rsh, rsl, rth, rtl;
    int32_t flag;
    int32_t ccA = 0, ccB = 0;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    ccB = (rsh <= rth);
    ccA = (rsl <= rtl);

    flag = (ccB << 1) | ccA;
    set_DSPControl_24(env, flag, 2);
}

uint32_t helper_pick_qb(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint8_t rs3, rs2, rs1, rs0;
    uint8_t rt3, rt2, rt1, rt0;
    uint8_t tp3, tp2, tp1, tp0;

    uint32_t dsp27, dsp26, dsp25, dsp24, rd;
    target_ulong dsp;

    rs3 = (rs & MIPSDSP_Q3) >> 24;
    rs2 = (rs & MIPSDSP_Q2) >> 16;
    rs1 = (rs & MIPSDSP_Q1) >>  8;
    rs0 =  rs & MIPSDSP_Q0;
    rt3 = (rt & MIPSDSP_Q3) >> 24;
    rt2 = (rt & MIPSDSP_Q2) >> 16;
    rt1 = (rt & MIPSDSP_Q1) >>  8;
    rt0 =  rt & MIPSDSP_Q0;

    dsp = env->active_tc.DSPControl;
    dsp27 = (dsp >> 27) & 0x01;
    dsp26 = (dsp >> 26) & 0x01;
    dsp25 = (dsp >> 25) & 0x01;
    dsp24 = (dsp >> 24) & 0x01;

    tp3 = dsp27 == 1 ? rs3 : rt3;
    tp2 = dsp26 == 1 ? rs2 : rt2;
    tp1 = dsp25 == 1 ? rs1 : rt1;
    tp0 = dsp24 == 1 ? rs0 : rt0;

    rd = ((uint32_t)tp3 << 24) | \
         ((uint32_t)tp2 << 16) | \
         ((uint32_t)tp1 <<  8) | \
         (uint32_t)tp0;

    return rd;
}

uint32_t helper_pick_ph(CPUMIPSState *env, uint32_t rs, uint32_t rt)
{
    uint16_t rsh, rsl, rth, rtl;
    uint16_t tempB, tempA;
    uint32_t dsp25, dsp24;
    uint32_t rd;
    target_ulong dsp;

    rsh = (rs & MIPSDSP_HI) >> 16;
    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rtl =  rt & MIPSDSP_LO;

    dsp = env->active_tc.DSPControl;
    dsp25 = (dsp >> 25) & 0x01;
    dsp24 = (dsp >> 24) & 0x01;

    tempB = (dsp25 == 1) ? rsh : rth;
    tempA = (dsp24 == 1) ? rsl : rtl;
    rd = (((uint32_t)tempB << 16) & MIPSDSP_HI) | (uint32_t)tempA;

    return rd;
}

uint32_t helper_append(uint32_t rt, uint32_t rs, int sa)
{
    int len;
    uint32_t temp;

    len = sa & 0x1F;

    if (len == 0) {
        temp = rt;
    } else {
        temp = (rt << len) | (rs & (((uint32_t)0x01 << len) - 1));
    }
    rt = temp;

    return temp;
}

uint32_t helper_prepend(int sa, uint32_t rs, uint32_t rt)
{
    uint32_t temp;

    if (sa == 0) {
        temp = rt;
    } else {
        temp = (rs << (32 - sa)) | rt >> sa;
    }

    rt = temp;

    return rt;
}

uint32_t helper_balign(uint32_t rt, uint32_t rs, uint32_t bp)
{
    uint32_t temp;
    bp = bp & 0x03;

    if (bp == 0 || bp == 2) {
        return rt;
    } else {
        temp = (rt << (8 * bp)) | (rs >> (8 * (4 - bp)));
    }
    rt = temp;

    return rt;
}

uint32_t helper_packrl_ph(uint32_t rs, uint32_t rt)
{
    uint16_t rsl, rth;
    uint32_t rd;

    rsl =  rs & MIPSDSP_LO;
    rth = (rt & MIPSDSP_HI) >> 16;
    rd = (rsl << 16) | rth;

    return rd;
}

/** DSP Accumulator and DSPControl Access Sub-class insns **/
uint32_t helper_extr_w(CPUMIPSState *env, int ac, int shift)
{
    int32_t tempI;
    int64_t tempDL[2];

    mipsdsp__rashift_short_acc(env, tempDL, ac, shift);
    if ((tempDL[1] != 0 || (tempDL[0] & MIPSDSP_LHI) != 0) && \
        (tempDL[1] != 1 || (tempDL[0] & MIPSDSP_LHI) != MIPSDSP_LHI)) {
        set_DSPControl_overflow_flag(env, 1, 23);
    }

    tempI = (tempDL[0] >> 1) & MIPSDSP_LLO;

    tempDL[0] += 1;
    if (tempDL[0] == 0) {
        tempDL[1] += 1;
    }

    if ((!(tempDL[1] == 0 && (tempDL[0] & MIPSDSP_LHI) == 0x00)) && \
        (!(tempDL[1] == 1 && (tempDL[0] & MIPSDSP_LHI) == MIPSDSP_LHI))) {
        set_DSPControl_overflow_flag(env, 1, 23);
    }

    return tempI;
}

uint32_t helper_extr_r_w(CPUMIPSState *env, int ac, int shift)
{
    int32_t tempI;
    int64_t tempDL[2];

    mipsdsp__rashift_short_acc(env, tempDL, ac, shift);
    if ((tempDL[1] != 0 || (tempDL[0] & MIPSDSP_LHI) != 0) && \
        (tempDL[1] != 1 || (tempDL[0] & MIPSDSP_LHI) != MIPSDSP_LHI)) {
        set_DSPControl_overflow_flag(env, 1, 23);
    }

    tempDL[0] += 1;
    if (tempDL[0] == 0) {
        tempDL[1] += 1;
    }

    if ((tempDL[1] != 0 || (tempDL[0] & MIPSDSP_LHI) != 0) && \
        (tempDL[1] != 1 && (tempDL[0] & MIPSDSP_LHI) != MIPSDSP_LHI)) {
        set_DSPControl_overflow_flag(env, 1, 23);
    }
    tempI = tempDL[0] >> 1;

    return tempI;
}

uint32_t helper_extr_rs_w(CPUMIPSState *env, int ac, int shift)
{
    int32_t tempI, temp64;
    int64_t tempDL[2];

    mipsdsp__rashift_short_acc(env, tempDL, ac, shift);
    if ((tempDL[1] != 0 || (tempDL[0] & MIPSDSP_LHI) != 0) && \
        (tempDL[1] != 1 || (tempDL[0] & MIPSDSP_LHI) != MIPSDSP_LHI)) {
        set_DSPControl_overflow_flag(env, 1, 23);
    }
    tempDL[0] += 1;
    if (tempDL[0] == 0) {
        tempDL[1] += 1;
    }
    tempI = tempDL[0] >> 1;

    if ((tempDL[1] != 0 || (tempDL[0] & MIPSDSP_LHI) != 0) && \
        (tempDL[1] != 1 || (tempDL[0] & MIPSDSP_LHI) != MIPSDSP_LHI)) {
        temp64 = tempDL[1];
        if (temp64 == 0) {
            tempI = 0x7FFFFFFF;
        } else {
            tempI = 0x80000000;
        }
        set_DSPControl_overflow_flag(env, 1, 23);
    }

    return tempI;
}

uint32_t helper_extr_s_h(CPUMIPSState *env, int ac, int shift)
{
    int64_t temp;
    uint32_t tempI;

    temp = mipsdsp_rashift_short_acc(env, ac, shift);
    if (temp > 0x0000000000007FFFull) {
        temp &= MIPSDSP_LHI;
        temp |= 0x00007FFF;
        set_DSPControl_overflow_flag(env, 1, 23);
    } else if (temp < 0xFFFFFFFFFFFF8000ull) {
        temp &= MIPSDSP_LHI;
        temp |= 0xFFFF8000;
        set_DSPControl_overflow_flag(env, 1, 23);
    }

    tempI = temp & 0xFFFFFFFF;
    return tempI;
}

uint32_t helper_extrv_s_h(CPUMIPSState *env, int ac, uint32_t rs)
{
    uint32_t rd;
    int32_t shift, tempI;
    int64_t tempL;

    shift = rs & 0x0F;
    tempL = mipsdsp_rashift_short_acc(env, ac, shift);
    if (tempL > 0x000000000007FFFull) {
        tempI = 0x00007FFF;
        set_DSPControl_overflow_flag(env, 1, 23);
    } else if (tempL < 0xFFFFFFFFFFF8000ull) {
        tempI = 0xFFFF8000;
        set_DSPControl_overflow_flag(env, 1, 23);
    }
    rd = tempI;

    return rd;
}

uint32_t helper_extrv_w(CPUMIPSState *env, int ac, uint32_t rs)
{
    int32_t shift, tempI;
    int64_t tempDL[2];

    shift = rs & 0x0F;
    mipsdsp__rashift_short_acc(env, tempDL, ac, shift);
    if ((tempDL[1] != 0 || (tempDL[0] & MIPSDSP_LHI) != 0) && \
        (tempDL[1] != 1 || (tempDL[0] & MIPSDSP_LHI) != MIPSDSP_LHI)) {
        set_DSPControl_overflow_flag(env, 1, 23);
    }

    tempI = tempDL[0] >> 1;

    tempDL[0] += 1;
    if (tempDL[0] == 0) {
        tempDL[1] += 1;
    }
    if ((tempDL[1] != 0 || (tempDL[0] & MIPSDSP_LHI) != 0) && \
        (tempDL[1] != 1 || (tempDL[0] & MIPSDSP_LHI) != MIPSDSP_LHI)) {
        set_DSPControl_overflow_flag(env, 1, 23);
    }

    return tempI;
}

uint32_t helper_extrv_r_w(CPUMIPSState *env, int ac, uint32_t rs)
{
    int32_t shift, tempI;
    int64_t tempDL[2];

    shift = rs & 0x0F;
    mipsdsp__rashift_short_acc(env, tempDL, ac, shift);
    if ((tempDL[1] != 0 || (tempDL[0] & MIPSDSP_LHI) != 0) && \
        (tempDL[1] != 1 || (tempDL[0] & MIPSDSP_LHI) != MIPSDSP_LHI)) {
        set_DSPControl_overflow_flag(env, 1, 23);
    }

    tempDL[0] += 1;
    if (tempDL[0] == 0) {
        tempDL[1] += 1;
    }

    if ((tempDL[1] != 0 || (tempDL[0] & MIPSDSP_LHI) != 0) && \
        (tempDL[1] != 1 || (tempDL[0] & MIPSDSP_LHI) != MIPSDSP_LHI)) {
        set_DSPControl_overflow_flag(env, 1, 23);
    }
    tempI = tempDL[0] >> 1;

    return tempI;
}

uint32_t helper_extrv_rs_w(CPUMIPSState *env, int ac, uint32_t rs)
{
    int32_t shift, tempI;
    int64_t tempDL[2];

    shift = rs & 0x0F;
    mipsdsp__rashift_short_acc(env, tempDL, ac, shift);
    if ((tempDL[1] != 0 || (tempDL[0] & MIPSDSP_LHI) != 0) && \
        (tempDL[1] != 1 || (tempDL[0] & MIPSDSP_LHI) != MIPSDSP_LHI)) {
        set_DSPControl_overflow_flag(env, 1, 23);
    }

    tempDL[0] += 1;
    if (tempDL[0] == 0) {
        tempDL[1] += 1;
    }
    tempI = tempDL[0] >> 1;

    if ((tempDL[1] != 0 || (tempDL[0] & MIPSDSP_LHI) != 0) && \
        (tempDL[1] != 1 || (tempDL[0] & MIPSDSP_LHI) != MIPSDSP_LHI)) {
        if (tempDL[1] == 0) {
            tempI = 0x7FFFFFFF;
        } else {
            tempI = 0x80000000;
        }
        set_DSPControl_overflow_flag(env, 1, 23);
    }

    return tempI;
}

uint32_t helper_extp(CPUMIPSState *env, int ac, int size)
{
    int32_t start_pos;
    uint32_t temp;
    uint64_t acc;

    temp = 0;
    start_pos = get_DSPControl_pos(env);
    if (start_pos - (size + 1) >= -1) {
        acc = ((uint64_t)env->active_tc.HI[ac] << 32) | \
              ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
        temp = (acc >> (start_pos - size)) & \
               (((uint32_t)0x01 << (size + 1)) - 1);
        set_DSPControl_efi(env, 0);
    } else {
        set_DSPControl_efi(env, 1);
    }

    return temp;
}

uint32_t helper_extpv(CPUMIPSState *env, int ac, uint32_t rs)
{
    int32_t start_pos, size;
    uint32_t temp;
    uint64_t acc;

    temp = 0;
    start_pos = get_DSPControl_pos(env);
    size = rs & 0x1F;

    if (start_pos - (size + 1) >= -1) {
        acc = ((uint64_t)env->active_tc.HI[ac] << 32) | \
              ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
        temp = (acc >> (start_pos - size)) & \
               (((uint32_t)0x01 << (size + 1)) - 1);
        set_DSPControl_efi(env, 0);
    } else {
        set_DSPControl_efi(env, 1);
    }

    return temp;
}

uint32_t helper_extpdp(CPUMIPSState *env, int ac, int size)
{
    int32_t start_pos;
    uint32_t temp;
    uint64_t acc;

    temp = 0;
    start_pos = get_DSPControl_pos(env);
    if (start_pos - (size + 1) >= -1) {
        acc  = ((uint64_t)env->active_tc.HI[ac] << 32) | \
               ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
        temp = (acc >> (start_pos - size)) & \
               (((uint32_t)0x01 << (size + 1)) - 1);

        set_DSPControl_pos(env, start_pos - (size + 1));
        set_DSPControl_efi(env, 0);
    } else {
        set_DSPControl_efi(env, 1);
    }

    return temp;
}

uint32_t helper_extpdpv(CPUMIPSState *env, int ac, uint32_t rs)
{
    int32_t start_pos, size;
    uint32_t temp;
    uint64_t acc;

    temp = 0;
    start_pos = get_DSPControl_pos(env);
    size = rs & 0x1F;

    if (start_pos - (size + 1) >= -1) {
        acc  = ((uint64_t)env->active_tc.HI[ac] << 32) | \
               ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
        temp = (acc >> (start_pos - size)) & (((int)0x01 << (size + 1)) - 1);
        set_DSPControl_pos(env, start_pos - (size + 1));
        set_DSPControl_efi(env, 0);
    } else {
        set_DSPControl_efi(env, 1);
    }

    return temp;
}

void helper_shilo(CPUMIPSState *env, int ac, int shift)
{
    uint8_t  sign;
    uint64_t temp, acc;

    shift = (shift << 26) >> 26;
    sign  = (shift >>  5) & 0x01;
    shift = (sign == 0) ? shift : -shift;
    acc = (((uint64_t)env->active_tc.HI[ac] << 32) & MIPSDSP_LHI) | \
          ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);

    if (shift == 0) {
        temp = acc;
    } else {
        if (sign == 0) {
            temp = acc >> shift;
        } else {
            temp = acc << shift;
        }
    }

    env->active_tc.HI[ac] = (temp & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  temp & MIPSDSP_LLO;
}

void helper_shilov(CPUMIPSState *env, int ac, uint32_t rs)
{
    uint8_t sign;
    int8_t  rs5_0;
    uint64_t temp, acc;

    rs5_0 = rs & 0x3F;
    rs    = (rs5_0 << 2) >> 2;
    sign  = (rs5_0 >> 5) & 0x01;
    rs5_0 = (sign == 0) ? rs : -rs;
    acc   = (((uint64_t)env->active_tc.HI[ac] << 32) & MIPSDSP_LHI) | \
            ((uint64_t)env->active_tc.LO[ac] & MIPSDSP_LLO);
    if (rs5_0 == 0) {
        temp = acc;
    } else {
        if (sign == 0) {
            temp = acc >> rs5_0;
        } else {
            temp = acc << rs5_0;
        }
    }

    env->active_tc.HI[ac] = (temp & MIPSDSP_LHI) >> 32;
    env->active_tc.LO[ac] =  temp & MIPSDSP_LLO;
}

void helper_mthlip(CPUMIPSState *env, int ac, uint32_t rs)
{
    int32_t tempA, tempB, pos;

    tempA = rs;
    tempB = env->active_tc.LO[ac];
    env->active_tc.HI[ac] = tempB;
    env->active_tc.LO[ac] = tempA;
    pos = get_DSPControl_pos(env);

    if (pos > 32) {
        return;
    } else {
        set_DSPControl_pos(env, pos + 32);
    }
}

void helper_wrdsp(CPUMIPSState *env, uint32_t rs, int mask_num)
{
    uint8_t  mask[6];
    uint8_t  i;
    uint32_t newbits, overwrite;
    target_ulong dsp;

    newbits   = 0x00;
    overwrite = 0xFFFFFFFF;
    dsp = env->active_tc.DSPControl;

    for (i = 0; i < 6; i++) {
        mask[i] = (mask_num >> i) & 0x01;
    }

    if (mask[0] == 1) {
        overwrite &= 0xFFFFFFC0;
        newbits   &= 0xFFFFFFC0;
        newbits   |= 0x0000003F & rs;
    }

    if (mask[1] == 1) {
        overwrite &= 0xFFFFE07F;
        newbits   &= 0xFFFFE07F;
        newbits   |= 0x00001F80 & rs;
    }

    if (mask[2] == 1) {
        overwrite &= 0xFFFFDFFF;
        newbits   &= 0xFFFFDFFF;
        newbits   |= 0x00002000 & rs;
    }

    if (mask[3] == 1) {
        overwrite &= 0xFF00FFFF;
        newbits   &= 0xFF00FFFF;
        newbits   |= 0x00FF0000 & rs;
    }

    if (mask[4] == 1) {
        overwrite &= 0x00FFFFFF;
        newbits   &= 0x00FFFFFF;
        newbits   |= 0xFF000000 & rs;
    }

    if (mask[5] == 1) {
        overwrite &= 0xFFFFBFFF;
        newbits   &= 0xFFFFBFFF;
        newbits   |= 0x00004000 & rs;
    }

    dsp = dsp & overwrite;
    dsp = dsp | newbits;
    env->active_tc.DSPControl = dsp;
}

uint32_t helper_rddsp(CPUMIPSState *env, uint32_t masknum)
{
    uint8_t  mask[6];
    uint32_t ruler, i;
    uint32_t temp;
    uint32_t rd;
    target_ulong dsp;

    ruler = 0x01;
    for (i = 0; i < 6; i++) {
        mask[i] = (masknum & ruler) >> i ;
        ruler = ruler << 1;
    }

    temp  = 0x00;
    dsp = env->active_tc.DSPControl;

    if (mask[0] == 1) {
        temp |= dsp & 0x3F;
    }

    if (mask[1] == 1) {
        temp |= dsp & 0x1F80;
    }

    if (mask[2] == 1) {
        temp |= dsp & 0x2000;
    }

    if (mask[3] == 1) {
        temp |= dsp & 0x00FF0000;
    }

    if (mask[4] == 1) {
        temp |= dsp & 0xFF000000;
    }

    if (mask[5] == 1) {
        temp |= dsp & 0x4000;
    }

    rd = temp;

    return rd;
}

#undef MIPSDSP_LHI
#undef MIPSDSP_LLO
#undef MIPSDSP_HI
#undef MIPSDSP_LO
#undef MIPSDSP_Q0
#undef MIPSDSP_Q1
#undef MIPSDSP_Q2
#undef MIPSDSP_Q3
