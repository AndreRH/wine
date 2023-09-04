/*
 * Avoid issues with linking box64cpu by adding some crt functions
 *
 * Copyright 2014, 2023 André Zwing
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

extern double bsd__ieee754_pow(double, double);
extern double bsd__ieee754_sqrt(double);

#define FP_INFINITE   1
#define FP_NAN        2
#define FP_NORMAL    -1
#define FP_SUBNORMAL -2
#define FP_ZERO       0
#define isinf(x) (__builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x) == FP_INFINITE)
#define isinff(x) (__builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x) == FP_INFINITE)
#define isnan(x) (__builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x) == FP_NAN)
#define isnanf(x) (__builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x) == FP_NAN)
#define isfinite(x) (__builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x) < 0)
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define HUGE_VALF __builtin_huge_valf()

static inline double softmath_log(double x)
{
    int n, aprox = min(32, 8 / (x / 2));
    double result = 0.0;

    if (x == 0.0)
        return -HUGE_VALF;
    else if (x < 0.0001)
        aprox = 32768;

    for(n = 0; n < aprox; n++)
    {
        result += bsd__ieee754_pow((x - 1.0) / (x + 1.0), 2 * n + 1) * (1.0 / (2.0 * n + 1.0));
        if (isinf(result))
            break;
    }
    result *= 2;

    return result;
}

double exp2(double x)
{
    return bsd__ieee754_pow(2, x);
}

double log2(double x)
{
    return softmath_log(x) / 0.69314718246459960938;
}

double pow(double x, double y)
{
    return bsd__ieee754_pow(x, y);
}

double fabs(double x)
{
    return __builtin_fabs(x);
}

float sqrtf(float x)
{
    return bsd__ieee754_sqrt(x);
}

typedef struct _div_t {
    int quot;
    int rem;
} div_t;

div_t __cdecl div(int num, int denom)
{
    div_t ret;

    ret.quot = num / denom;
    ret.rem = num % denom;
    return ret;
}

typedef struct _ldiv_t {
    long quot;
    long rem;
} ldiv_t;

ldiv_t __cdecl ldiv(long num, long denom)
{
    ldiv_t ret;

    ret.quot = num / denom;
    ret.rem = num % denom;
    return ret;
}

double ldexp(double x, int exp)
{
    return x * bsd__ieee754_pow(2, exp);
}

double frexp(double x, int *exp)
{
    double a;
    int i = 0;

    if (isnan(x) || isinf(x)) return x;
    if (x == 0.0)
    {
        if (exp) *exp = 0;
        return x;
    }

    a = fabs(x);

    if (a >= 1)
        while (pow(2, i) < a) i++;
    else
        while (pow(2, i) > a) i--;

    if (a/pow(2, i) >= 1.0) i++;

    if (exp) *exp = i;
    return x/pow(2, i);
}
