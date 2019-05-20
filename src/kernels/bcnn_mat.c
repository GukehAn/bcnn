/*
 * Copyright (c) 2016-present Jean-Noel Braun.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "bcnn_mat.h"

#include <math.h>

#include <bh/bh_log.h>
#include <bh/bh_macros.h>
#include <bh/bh_mem.h>

#include "bcnn/bcnn.h"

#include <bh/bh_timer.h>

#if (defined(__aarch64__))
#include "openblas/openblas_sgemm.h"
#endif

int bcnn_fill_f32(int n, float a, float *x) {
    int i;
    for (i = 0; i < n; ++i) {
        x[i] = a;
    }
    return 0;
}

int bcnn_copy_f32(int n, float *x, float *y) {
    memcpy(y, x, n * sizeof(float));
    return 0;
}

int bcnn_axpy(int n, float a, float *x, float *y) {
#ifndef BCNN_USE_AVX
    int i;
    for (i = 0; i < n; ++i) y[i] += a * x[i];
#else
    int i, nd, nm;
    __m256 sum0;
    __m256 sum1;
    __m256 reg0, reg1, reg2, reg3;
    __m256 areg = _mm256_set1_ps(a);
    __m256 prod;
    int data_is_aligned = bh_is_aligned32(x) & bh_is_aligned32(y);

    nd = n / 16 * 16;
    nm = n % 16;
    if (data_is_aligned) {
        for (i = 0; i < nd; i += 16) {
            reg0 = _mm256_load_ps(x + 0);
            reg1 = _mm256_load_ps(x + 8);
            reg2 = _mm256_load_ps(y + 0);
            reg3 = _mm256_load_ps(y + 8);
            prod = _mm256_mul_ps(reg0, areg);
            sum0 = _mm256_add_ps(prod, reg2);
            prod = _mm256_mul_ps(reg1, areg);
            sum1 = _mm256_add_ps(prod, reg3);
            _mm256_store_ps(y + 0, sum0);
            _mm256_store_ps(y + 8, sum1);
            x += 16;
            y += 16;
        }
    } else {
        for (i = 0; i < nd; i += 16) {
            reg0 = _mm256_loadu_ps(x + 0);
            reg1 = _mm256_loadu_ps(x + 8);
            reg2 = _mm256_loadu_ps(y + 0);
            reg3 = _mm256_loadu_ps(y + 8);
            prod = _mm256_mul_ps(reg0, areg);
            sum0 = _mm256_add_ps(prod, reg2);
            prod = _mm256_mul_ps(reg1, areg);
            sum1 = _mm256_add_ps(prod, reg3);
            _mm256_storeu_ps(y + 0, sum0);
            _mm256_storeu_ps(y + 8, sum1);
            x += 16;
            y += 16;
        }
    }
    for (i = 0; i < nm; ++i) y[i] += a * x[i];
#endif
    return 0;
}

int bcnn_axpby(int n, float a, float *x, float b, float *y) {
#ifndef BCNN_USE_AVX
    int i;
    for (i = 0; i < n; ++i) y[i] = a * x[i] + b * y[i];
#else
    int i, nd, nm;
    __m256 sum0;
    __m256 sum1;
    __m256 reg0, reg1, reg2, reg3;
    __m256 areg = _mm256_set1_ps(a);
    __m256 breg = _mm256_set1_ps(b);
    __m256 prod0, prod1;
    int data_is_aligned = bh_is_aligned32(x) & bh_is_aligned32(y);

    nd = n / 16 * 16;
    nm = n % 16;
    if (data_is_aligned) {
        for (i = 0; i < nd; i += 16) {
            reg0 = _mm256_load_ps(x + 0);
            reg1 = _mm256_load_ps(x + 8);
            reg2 = _mm256_load_ps(y + 0);
            reg3 = _mm256_load_ps(y + 8);
            prod0 = _mm256_mul_ps(reg0, areg);
            prod1 = _mm256_mul_ps(reg2, breg);
            sum0 = _mm256_add_ps(prod0, prod1);
            prod0 = _mm256_mul_ps(reg1, areg);
            prod1 = _mm256_mul_ps(reg3, breg);
            sum1 = _mm256_add_ps(prod0, prod1);
            _mm256_store_ps(y + 0, sum0);
            _mm256_store_ps(y + 8, sum1);
            x += 16;
            y += 16;
        }
    } else {
        for (i = 0; i < nd; i += 16) {
            reg0 = _mm256_loadu_ps(x + 0);
            reg1 = _mm256_loadu_ps(x + 8);
            reg2 = _mm256_loadu_ps(y + 0);
            reg3 = _mm256_loadu_ps(y + 8);
            prod0 = _mm256_mul_ps(reg0, areg);
            prod1 = _mm256_mul_ps(reg2, breg);
            sum0 = _mm256_add_ps(prod0, prod1);
            prod0 = _mm256_mul_ps(reg1, areg);
            prod1 = _mm256_mul_ps(reg3, breg);
            sum1 = _mm256_add_ps(prod0, prod1);
            _mm256_storeu_ps(y + 0, sum0);
            _mm256_storeu_ps(y + 8, sum1);
            x += 16;
            y += 16;
        }
    }
    for (i = 0; i < nm; ++i) y[i] = a * x[i] + b * y[i];
#endif
    return 0;
}

void bcnn_axpy_strided(int num_batches, float a, float *x, float *y,
                       int stride[2], int x_dim[3], int y_dim[3],
                       int min_dim[3]) {
    for (int n = 0; n < num_batches; ++n) {
        for (int k = 0; k < min_dim[0]; ++k) {
            for (int j = 0; j < min_dim[1]; ++j) {
                for (int i = 0; i < min_dim[2]; ++i) {
                    int dst_ind = i * stride[0] +
                                  y_dim[2] * (j * stride[0] +
                                              y_dim[1] * (y_dim[0] * n + k));
                    int src1_ind = i * stride[1] +
                                   x_dim[2] * (j * stride[1] +
                                               x_dim[1] * (x_dim[0] * n + k));
                    y[dst_ind] += a * x[src1_ind];
                }
            }
        }
    }
}

int bcnn_pow(int n, float *x, float a, float *y) {
    int i;
    for (i = 0; i < n; ++i) {
        y[i] = powf(x[i], a);
    }
    return 0;
}

int bcnn_vadd(int n, float *a, float *b, float *y) {
#ifndef BCNN_USE_AVX
    int i;
    for (i = 0; i < n; ++i) {
        y[i] = a[i] + b[i];
    }
#else
    int i, nd, nm;
    __m128 r0, r1, r2, r3;

    nd = n / 8 * 8;
    nm = n % 8;
    for (i = 0; i < nd; i += 8) {
        r0 = _mm_loadu_ps(a);
        r1 = _mm_loadu_ps(a + 4);
        r2 = _mm_loadu_ps(b);
        r3 = _mm_loadu_ps(b + 4);
        r0 = _mm_add_ps(r0, r2);
        r1 = _mm_add_ps(r1, r3);
        _mm_storeu_ps(y, r0);
        _mm_storeu_ps(y + 4, r1);
        a += 8;
        b += 8;
        y += 8;
    }
    for (i = 0; i < nm; ++i) {
        y[i] = a[i] + b[i];
    }
#endif
    return 0;
}

int bcnn_vsub(int n, float *a, float *b, float *y) {
#ifndef BCNN_USE_AVX
    int i;
    for (i = 0; i < n; ++i) {
        y[i] = a[i] - b[i];
    }
#else
    int i, nd, nm;
    __m128 r0, r1, r2, r3;

    nd = n / 8 * 8;
    nm = n % 8;
    for (i = 0; i < nd; i += 8) {
        r0 = _mm_loadu_ps(a);
        r1 = _mm_loadu_ps(a + 4);
        r2 = _mm_loadu_ps(b);
        r3 = _mm_loadu_ps(b + 4);
        r0 = _mm_sub_ps(r0, r2);
        r1 = _mm_sub_ps(r1, r3);
        _mm_storeu_ps(y, r0);
        _mm_storeu_ps(y + 4, r1);
        a += 8;
        b += 8;
        y += 8;
    }
    for (i = 0; i < nm; ++i) {
        y[i] = a[i] - b[i];
    }
#endif
    return 0;
}

int bcnn_vmul(int n, float *a, float *b, float *y) {
#ifndef BCNN_USE_AVX
    int i;
    for (i = 0; i < n; ++i) {
        y[i] = a[i] * b[i];
    }
#else
    int i, nd, nm;
    __m128 r0, r1, r2, r3;

    nd = n / 8 * 8;
    nm = n % 8;
    for (i = 0; i < nd; i += 8) {
        r0 = _mm_loadu_ps(a);
        r1 = _mm_loadu_ps(a + 4);
        r2 = _mm_loadu_ps(b);
        r3 = _mm_loadu_ps(b + 4);
        r0 = _mm_mul_ps(r0, r2);
        r1 = _mm_mul_ps(r1, r3);
        _mm_storeu_ps(y, r0);
        _mm_storeu_ps(y + 4, r1);
        a += 8;
        b += 8;
        y += 8;
    }
    for (i = 0; i < nm; ++i) {
        y[i] = a[i] * b[i];
    }
#endif
    return 0;
}

int bcnn_vdiv(int n, float *a, float *b, float *y) {
#ifndef BCNN_USE_AVX
    int i;
    for (i = 0; i < n; ++i) {
        if (bh_abs(b[i]) > 0.00001f)
            y[i] = a[i] / b[i];
        else
            y[i] = 0.0f;
    }
#else
    int i, nd, nm;
    __m128 r0, r1, r2, r3;

    nd = n / 8 * 8;
    nm = n % 8;
    for (i = 0; i < nd; i += 8) {
        r0 = _mm_loadu_ps(a);
        r1 = _mm_loadu_ps(a + 4);
        r2 = _mm_loadu_ps(b);
        r3 = _mm_loadu_ps(b + 4);
        r0 = _mm_div_ps(r0, r2);
        r1 = _mm_div_ps(r1, r3);
        _mm_storeu_ps(y, r0);
        _mm_storeu_ps(y + 4, r1);
        a += 8;
        b += 8;
        y += 8;
    }
    for (i = 0; i < nm; ++i) {
        if (bh_abs(b[i]) > 0.00001f)
            y[i] = a[i] / b[i];
        else
            y[i] = 0.0f;
    }
#endif
    return 0;
}

int bcnn_scal(int n, float a, float *x) {
#ifndef BCNN_USE_AVX
    int i;
    if (a == 0.0f) {
        memset(x, 0, n * sizeof(float));
    } else if (a != 1.0f) {
        for (i = 0; i < n; ++i) x[i] *= a;
    }
#else
    int i, nd, nm;
    __m128 reg0, reg1;
    __m128 areg = _mm_set1_ps(a);
    __m128 prod;
    int data_is_aligned = bh_is_aligned32(x);

    if (a == 0.0f) {
        memset(x, 0, n * sizeof(float));
    } else if (a != 1.0f) {
        nd = n / 8 * 8;
        nm = n % 8;
        if (data_is_aligned) {
            for (i = 0; i < nd; i += 8) {
                reg0 = _mm_load_ps(x + 0);
                reg1 = _mm_load_ps(x + 4);
                prod = _mm_mul_ps(reg0, areg);
                _mm_store_ps(x + 0, prod);
                prod = _mm_mul_ps(reg1, areg);
                _mm_store_ps(x + 4, prod);
                x += 8;
            }
        } else {
            for (i = 0; i < nd; i += 8) {
                reg0 = _mm_loadu_ps(x + 0);
                reg1 = _mm_loadu_ps(x + 4);
                prod = _mm_mul_ps(reg0, areg);
                _mm_storeu_ps(x + 0, prod);
                prod = _mm_mul_ps(reg1, areg);
                _mm_storeu_ps(x + 4, prod);
                x += 8;
            }
        }
        for (i = 0; i < nm; ++i) x[i] *= a;
    }
#endif
    return 0;
}

int bcnn_add_scalar(int n, float a, float *x) {
#ifndef BCNN_USE_AVX
    int i;
    for (i = 0; i < n; ++i) {
        x[i] += a;
    }
#else
    int i, nd, nm;
    __m128 reg0, reg1;
    __m128 areg = _mm_set1_ps(a);
    __m128 prod;
    int data_is_aligned = bh_is_aligned32(x);

    if (a == 0.0f) {
        return 0;
    } else if (a != 1.0f) {
        nd = n / 8 * 8;
        nm = n % 8;
        if (data_is_aligned) {
            for (i = 0; i < nd; i += 8) {
                reg0 = _mm_load_ps(x + 0);
                reg1 = _mm_load_ps(x + 4);
                prod = _mm_add_ps(reg0, areg);
                _mm_store_ps(x + 0, prod);
                prod = _mm_add_ps(reg1, areg);
                _mm_store_ps(x + 4, prod);
                x += 8;
            }
        } else {
            for (i = 0; i < nd; i += 8) {
                reg0 = _mm_loadu_ps(x + 0);
                reg1 = _mm_loadu_ps(x + 4);
                prod = _mm_add_ps(reg0, areg);
                _mm_storeu_ps(x + 0, prod);
                prod = _mm_add_ps(reg1, areg);
                _mm_storeu_ps(x + 4, prod);
                x += 8;
            }
        }
        for (i = 0; i < nm; ++i) x[i] += a;
    }
#endif
    return 0;
}

float bcnn_dot(int n, float *x, float *y) {
#ifndef BCNN_USE_AVX
    int i;
    float dot = 0;
    for (i = 0; i < n; ++i) dot += x[i] * y[i];
    return dot;
#else
    int i, nd, nm;
    float sum = 0;
    float sum_res[4];
    __m128 sum_r = _mm_setzero_ps();
    __m128 r0, r1, r2, r3;

    nd = n / 8 * 8;
    nm = n % 8;
    for (i = 0; i < nd; i += 8) {
        r0 = _mm_loadu_ps(x);
        r1 = _mm_loadu_ps(y);
        r2 = _mm_loadu_ps(x + 4);
        r3 = _mm_loadu_ps(y + 4);
        r0 = _mm_mul_ps(r0, r1);
        r2 = _mm_mul_ps(r2, r3);
        sum_r = _mm_add_ps(sum_r, r0);
        sum_r = _mm_add_ps(sum_r, r2);
        x += 8;
        y += 8;
    }
    _mm_storeu_ps(sum_res, sum_r);
    sum += sum_res[0] + sum_res[1] + sum_res[2] + sum_res[3];
    for (i = 0; i < nm; ++i) sum += x[i] * y[i];
    return sum;
#endif
}

int bcnn_vsum(int n, float *x, float *sum) {
#ifndef BCNN_USE_AVX
    int i;
    float s = 0.0f;
    for (i = 0; i < n; ++i) s += x[i];
    *(sum) = s;
#else
    int i, nd, nm;
    float s = 0.0f;
    float sum_res[4];
    __m128 sum_r = _mm_setzero_ps();
    __m128 r0, r1;

    nd = n / 8 * 8;
    nm = n % 8;
    for (i = 0; i < nd; i += 8) {
        r0 = _mm_loadu_ps(x);
        r1 = _mm_loadu_ps(x + 4);
        sum_r = _mm_add_ps(sum_r, r0);
        sum_r = _mm_add_ps(sum_r, r1);
        x += 8;
    }
    _mm_storeu_ps(sum_res, sum_r);
    s += sum_res[0] + sum_res[1] + sum_res[2] + sum_res[3];
    for (i = 0; i < nm; ++i) s += x[i];
    *(sum) = s;
#endif
    return 0;
}

int bcnn_gemv(int trans_a, int m, int n, float alpha, float *a, float *x,
              float beta, float *y) {
    int i, j;
#ifdef BCNN_USE_AVX
    int nd, md;
    __m128 apart, mula, mul0, areg, xreg, yreg;
    float sum[4] = {0};
#endif

    if (!trans_a) {
        if (beta != 1.0f) {
            for (i = 0; i < m; ++i) {
                y[i] *= beta;
            }
        }
#ifndef BCNN_USE_AVX
        for (i = 0; i < m; ++i) {
            for (j = 0; j < n; ++j) {
                y[i] += alpha * a[i * n + j] * x[j];
            }
        }
#else
        nd = n / 4 * 4;
        apart = _mm_set1_ps(alpha);
        for (i = 0; i < m; ++i) {
            memset(sum, 0, 4 * sizeof(float));
            yreg = _mm_setzero_ps();
            for (j = 0; j < nd; j += 4) {
                areg = _mm_loadu_ps(&a[i * n + j]);
                xreg = _mm_loadu_ps(&x[j]);
                mula = _mm_mul_ps(apart, areg);
                mul0 = _mm_mul_ps(xreg, mula);
                yreg = _mm_add_ps(yreg, mul0);
            }
            _mm_storeu_ps(sum, yreg);
            y[i] += sum[0] + sum[1] + sum[2] + sum[3];
            for (; j < n; ++j) y[i] += alpha * a[i * n + j] * x[j];
        }
#endif
    } else {
        if (beta != 1.0f) {
            for (i = 0; i < n; ++i) {
                y[i] *= beta;
            }
        }
#ifndef BCNN_USE_AVX
        for (i = 0; i < n; ++i) {
            for (j = 0; j < m; ++j) {
                y[i] += alpha * a[i * m + j] * x[j];
            }
        }
#else
        md = m / 4 * 4;
        apart = _mm_set1_ps(alpha);
        for (i = 0; i < n; ++i) {
            memset(sum, 0, 4 * sizeof(float));
            yreg = _mm_setzero_ps();
            for (j = 0; j < md; j += 4) {
                areg = _mm_loadu_ps(&a[i * m + j]);
                xreg = _mm_loadu_ps(&x[j]);
                mula = _mm_mul_ps(apart, areg);
                mul0 = _mm_mul_ps(xreg, mula);
                yreg = _mm_add_ps(yreg, mul0);
            }
            _mm_storeu_ps(sum, yreg);
            y[i] += sum[0] + sum[1] + sum[2] + sum[3];
            for (; j < m; ++j) y[i] += alpha * a[i * m + j] * x[j];
        }
#endif
    }
    return 0;
}

float bcnn_l2_distance(float *x, float *y, int n) {
    float dist = 0.0f;
    int i;
#ifdef BCNN_USE_AVX
    int data_is_aligned = bh_is_aligned32(x) & bh_is_aligned32(y);
    __m128 vx0, vy0, vx1, vy1, vdiff0, vdiff1;
    __m128 vdist = _mm_set1_ps(0.0f);
    float dist4f[4] = {0.0f};
    int nd, nm;

    nd = n / 8 * 8;
    nm = n % 8;
    if (data_is_aligned) {
        for (i = 0; i < nd; i += 8) {
            vx0 = _mm_load_ps(x);
            vy0 = _mm_load_ps(y);
            vx1 = _mm_load_ps(x + 4);
            vy1 = _mm_load_ps(y + 4);
            vdiff0 = _mm_sub_ps(vx0, vy0);
            vdiff0 = _mm_mul_ps(vdiff0, vdiff0);
            vdiff1 = _mm_sub_ps(vx1, vy1);
            vdiff1 = _mm_mul_ps(vdiff1, vdiff1);
            vdist = _mm_add_ps(vdist, vdiff0);
            vdist = _mm_add_ps(vdist, vdiff1);
            x += 8;
            y += 8;
        }
        _mm_store_ps(dist4f, vdist);
    } else {
        for (i = 0; i < nd; i += 8) {
            vx0 = _mm_loadu_ps(x);
            vy0 = _mm_loadu_ps(y);
            vx1 = _mm_loadu_ps(x + 4);
            vy1 = _mm_loadu_ps(y + 4);
            vdiff0 = _mm_sub_ps(vx0, vy0);
            vdiff0 = _mm_mul_ps(vdiff0, vdiff0);
            vdiff1 = _mm_sub_ps(vx1, vy1);
            vdiff1 = _mm_mul_ps(vdiff1, vdiff1);
            vdist = _mm_add_ps(vdist, vdiff0);
            vdist = _mm_add_ps(vdist, vdiff1);
            x += 8;
            y += 8;
        }
        _mm_storeu_ps(dist4f, vdist);
    }
    dist += dist4f[0] + dist4f[1] + dist4f[2] + dist4f[3];
    for (i = 0; i < nm; ++i) dist += (x[i] - y[i]) * (x[i] - y[i]);
#else
    for (i = 0; i < n; ++i) dist += (x[i] - y[i]) * (x[i] - y[i]);
#endif
    return dist;
}

float bcnn_sqrdiff_vs(float *x, float a, int n) {
    float dist = 0.0f;
    int i;
#ifndef BCNN_USE_AVX
    for (i = 0; i < n; ++i) dist += (x[i] - a) * (x[i] - a);
#else
    int data_is_aligned = bh_is_aligned32(x);
    __m128 vx0, vx1, vdiff0, vdiff1;
    __m128 vdist = _mm_set1_ps(0.0f);
    __m128 areg = _mm_set1_ps(a);
    float dist4f[4] = {0.0f};
    int nd, nm;

    nd = n / 8 * 8;
    nm = n % 8;
    if (data_is_aligned) {
        for (i = 0; i < nd; i += 8) {
            vx0 = _mm_load_ps(x);
            vx1 = _mm_load_ps(x + 4);
            vdiff0 = _mm_sub_ps(vx0, areg);
            vdiff0 = _mm_mul_ps(vdiff0, vdiff0);
            vdiff1 = _mm_sub_ps(vx1, areg);
            vdiff1 = _mm_mul_ps(vdiff1, vdiff1);
            vdist = _mm_add_ps(vdist, vdiff0);
            vdist = _mm_add_ps(vdist, vdiff1);
            x += 8;
        }
        _mm_store_ps(dist4f, vdist);
    } else {
        for (i = 0; i < nd; i += 8) {
            vx0 = _mm_loadu_ps(x);
            vx1 = _mm_loadu_ps(x + 4);
            vdiff0 = _mm_sub_ps(vx0, areg);
            vdiff0 = _mm_mul_ps(vdiff0, vdiff0);
            vdiff1 = _mm_sub_ps(vx1, areg);
            vdiff1 = _mm_mul_ps(vdiff1, vdiff1);
            vdist = _mm_add_ps(vdist, vdiff0);
            vdist = _mm_add_ps(vdist, vdiff1);
            x += 8;
        }
        _mm_storeu_ps(dist4f, vdist);
    }
    dist += dist4f[0] + dist4f[1] + dist4f[2] + dist4f[3];
    for (i = 0; i < nm; ++i) dist += (x[i] - a) * (x[i] - a);
#endif

    return dist;
}

float bcnn_shiftdot(int n, float *x, float a, float *y, float b) {
#ifndef BCNN_USE_AVX
    int i;
    float dot = 0;
    for (i = 0; i < n; ++i) dot += (x[i] - a) * (y[i] - b);
    return dot;
#else
    int i, nd, nm;
    float sum = 0;
    float sum_res[4];
    __m128 sum_r = _mm_setzero_ps();
    __m128 r0, r1, r2, r3;
    __m128 areg = _mm_set1_ps(a);
    __m128 breg = _mm_set1_ps(b);

    nd = n / 8 * 8;
    nm = n % 8;
    for (i = 0; i < nd; i += 8) {
        r0 = _mm_loadu_ps(x);
        r1 = _mm_loadu_ps(y);
        r2 = _mm_loadu_ps(x + 4);
        r3 = _mm_loadu_ps(y + 4);
        r0 = _mm_sub_ps(r0, areg);
        r1 = _mm_sub_ps(r1, breg);
        r2 = _mm_sub_ps(r2, areg);
        r3 = _mm_sub_ps(r3, breg);
        r0 = _mm_mul_ps(r0, r1);
        r2 = _mm_mul_ps(r2, r3);
        sum_r = _mm_add_ps(sum_r, r0);
        sum_r = _mm_add_ps(sum_r, r2);
        x += 8;
        y += 8;
    }
    _mm_storeu_ps(sum_res, sum_r);
    sum += sum_res[0] + sum_res[1] + sum_res[2] + sum_res[3];
    for (i = 0; i < nm; ++i) sum += (x[i] - a) * (y[i] - b);
    return sum;
#endif
}

int bcnn_varnorm(int n, float *a, float c, float *y) {
#ifndef BCNN_USE_AVX
    int i;
    for (i = 0; i < n; ++i) {
        y[i] *= c / (a[i] * sqrtf(a[i]) + 0.00001f);
    }
#else
    int i, nd, nm;
    __m128 r0, r1, reg0, reg1;
    __m128 creg = _mm_set1_ps(c);
    __m128 epsreg = _mm_set1_ps(0.00001f);

    nd = n / 8 * 8;
    nm = n % 8;
    for (i = 0; i < nd; i += 8) {
        reg0 = _mm_loadu_ps(y);
        reg1 = _mm_loadu_ps(y + 4);
        r0 = _mm_loadu_ps(a);
        r1 = _mm_loadu_ps(a + 4);
        r0 = _mm_mul_ps(
            reg0, _mm_div_ps(creg, _mm_add_ps(_mm_mul_ps(r0, _mm_sqrt_ps(r0)),
                                              epsreg)));
        r1 = _mm_mul_ps(
            reg1, _mm_div_ps(creg, _mm_add_ps(_mm_mul_ps(r1, _mm_sqrt_ps(r1)),
                                              epsreg)));
        _mm_storeu_ps(y, r0);
        _mm_storeu_ps(y + 4, r1);
        a += 8;
        y += 8;
    }
    for (i = 0; i < nm; ++i) {
        y[i] *= c / (a[i] * sqrtf(a[i]) + 0.00001f);
    }
#endif
    return 0;
}

int bcnn_varmean(int n, float *m, float a, float *var) {
#ifndef BCNN_USE_AVX
    int i;
    for (i = 0; i < n; ++i) {
        var[i] = var[i] * a - m[i] * m[i];
    }
#else
    int i, nd, nm;
    __m128 r0, r1, reg0, reg1;
    __m128 areg = _mm_set1_ps(a);

    nd = n / 8 * 8;
    nm = n % 8;
    for (i = 0; i < nd; i += 8) {
        reg0 = _mm_loadu_ps(var);
        reg1 = _mm_loadu_ps(var + 4);
        r0 = _mm_loadu_ps(m);
        r1 = _mm_loadu_ps(m + 4);
        r0 = _mm_sub_ps(_mm_mul_ps(reg0, areg), _mm_mul_ps(r0, r0));
        r1 = _mm_sub_ps(_mm_mul_ps(reg1, areg), _mm_mul_ps(r1, r1));
        _mm_storeu_ps(var, r0);
        _mm_storeu_ps(var + 4, r1);
        m += 8;
        var += 8;
    }
    for (i = 0; i < nm; ++i) {
        var[i] = var[i] * a - m[i] * m[i];
    }
#endif
    return 0;
}

void bcnn_add_bias(float *output, float *bias, int batch_size, int num_channels,
                   int spatial_size) {
    int i, j, b;
    for (b = 0; b < batch_size; ++b) {
#pragma omp parallel for
        for (i = 0; i < num_channels; ++i) {
            bcnn_add_scalar(spatial_size, bias[i], output + i * spatial_size);
        }
        output += num_channels * spatial_size;
    }
}

void bcnn_scales(float *output, float *scales, int batch_size, int num_channels,
                 int spatial_size) {
    int i, j, b;
    for (b = 0; b < batch_size; ++b) {
#pragma omp parallel for
        for (i = 0; i < num_channels; ++i) {
            bcnn_scal(spatial_size, scales[i], output + i * spatial_size);
        }
        output += num_channels * spatial_size;
    }
}

void bcnn_grad_scales(float *x_norm, float *delta, int batch, int n, int size,
                      float *scale_updates) {
    int i, b, f;
    for (f = 0; f < n; ++f) {
        float sum = 0;
        for (b = 0; b < batch; ++b) {
            for (i = 0; i < size; ++i) {
                int index = i + size * (f + n * b);
                sum += delta[index] * x_norm[index];
            }
        }
        scale_updates[f] += sum;
    }
}

void bcnn_grad_bias(float *grad_bias, float *grad_data, int batch_size,
                    int num_channels, int spatial_size) {
    int i, j, b;
    float *p = NULL;

    for (b = 0; b < batch_size; ++b) {
        for (i = 0; i < num_channels; ++i) {
            p = grad_data + spatial_size * (i + b * num_channels);
            for (j = 0; j < spatial_size; ++j) {
                grad_bias[i] += p[j];
            }
        }
    }
}

static inline int is_a_positive_and_inferior_to_b(int a, int b) {
    return (unsigned int)a < (unsigned int)b;
}

void bcnn_im2col(const float *data_im, const int channels, const int height,
                 const int width, const int kernel_size, const int pad,
                 const int stride, float *data_col) {
    int channel, kernel_row, kernel_col, output_rows, output_cols, input_col,
        input_row, output_col;
    const int output_h = (height + 2 * pad - kernel_size) / stride + 1;
    const int output_w = (width + 2 * pad - kernel_size) / stride + 1;
    const int channel_size = height * width;

    for (channel = channels; channel--; data_im += channel_size) {
        for (kernel_row = 0; kernel_row < kernel_size; kernel_row++) {
            for (kernel_col = 0; kernel_col < kernel_size; kernel_col++) {
                input_row = -pad + kernel_row;
                for (output_rows = output_h; output_rows; output_rows--) {
                    if (!is_a_positive_and_inferior_to_b(input_row, height)) {
                        for (output_cols = output_w; output_cols;
                             output_cols--) {
                            *(data_col++) = 0;
                        }
                    } else {
                        input_col = -pad + kernel_col;
                        for (output_col = output_w; output_col; output_col--) {
                            if (is_a_positive_and_inferior_to_b(input_col,
                                                                width)) {
                                *(data_col++) =
                                    data_im[input_row * width + input_col];
                            } else {
                                *(data_col++) = 0;
                            }
                            input_col += stride;
                        }
                    }
                    input_row += stride;
                }
            }
        }
    }
}

static void bcnn_im2col_mt_st1(const float *data_im, const int channels,
                               const int height, const int width,
                               const int kernel_size, const int pad,
                               float *data_col) {
    int height_col = (height + 2 * pad - kernel_size) + 1;
    int width_col = (width + 2 * pad - kernel_size) + 1;
    int channels_col = channels * kernel_size * kernel_size;

#pragma omp parallel for schedule(runtime)
    for (int c = 0; c < channels_col; ++c) {
        int w_offset = c % kernel_size;
        int h_offset = (c / kernel_size) % kernel_size;
        int c_im = c / kernel_size / kernel_size;

        const int hc0 = h_offset - pad;
        const int wc0 = w_offset - pad;
        int wleft = bh_max(0, pad - w_offset);
        int wmid = bh_min(width_col, width + pad - w_offset) - wleft;
        int wright = bh_max(0, width_col - (width + pad - w_offset));
        for (int h = 0; h < pad - h_offset; ++h) {
            const int row_offset = (c * height_col + h) * width_col;
            memset(data_col + row_offset, 0, width_col * sizeof(float));
        }
        for (int h = bh_max(0, pad - h_offset);
             h < bh_min(height_col, height + pad - h_offset); ++h) {
            int h_pad = h + hc0;
            const int row_offset = (c * height_col + h) * width_col;
            const int srow_offset = (c_im * height + h_pad) * width;
            memset(data_col + row_offset, 0, wleft * sizeof(float));
            memcpy(data_col + row_offset + wleft,
                   data_im + srow_offset + wleft + wc0, wmid * sizeof(float));
            memset(data_col + row_offset + wleft + wmid, 0,
                   wright * sizeof(float));
        }
        for (int h = height + pad - h_offset; h < height_col; ++h) {
            const int row_offset = (c * height_col + h) * width_col;
            memset(data_col + row_offset, 0, width_col * sizeof(float));
        }
    }
}

void bcnn_im2col_mt(const float *data_im, const int channels, const int height,
                    const int width, const int kernel_size, const int pad,
                    const int stride, float *data_col) {
    int height_col = (height + 2 * pad - kernel_size) / stride + 1;
    int width_col = (width + 2 * pad - kernel_size) / stride + 1;
    int channels_col = channels * kernel_size * kernel_size;

    if (stride == 1) {
        bcnn_im2col_mt_st1(data_im, channels, height, width, kernel_size, pad,
                           data_col);
    } else {
#pragma omp parallel for schedule(runtime)
        for (int c = 0; c < channels_col; ++c) {
            int w_offset = c % kernel_size;
            int h_offset = (c / kernel_size) % kernel_size;
            int c_im = c / kernel_size / kernel_size;

            const int hc0 = h_offset - pad;
            const int wc0 = w_offset - pad;
            for (int h = 0; h < height_col; ++h) {
                int h_pad = h * stride + hc0;

                const int row_offset = (c * height_col + h) * width_col;
                const int srow_offset = (c_im * height + h_pad) * width;
                for (int w = 0; w < width_col; ++w) {
                    int w_pad = w * stride + wc0;
                    if ((((unsigned)h_pad) < ((unsigned)height)) &&
                        (((unsigned)w_pad) < ((unsigned)width)))
                        data_col[row_offset + w] = data_im[srow_offset + w_pad];
                    else {
                        data_col[row_offset + w] = 0.;
                    }
                }
            }
        }
    }
}

void bcnn_col2im(const float *data_col, const int channels, const int height,
                 const int width, const int kernel, const int pad,
                 const int stride, float *data_im) {
    int channel, kernel_row, kernel_col, output_rows, input_col, input_row,
        output_col;
    const int output_h = (height + 2 * pad - kernel) / stride + 1;
    const int output_w = (width + 2 * pad - kernel) / stride + 1;
    const int channel_size = height * width;

    bcnn_fill_f32(height * width * channels, 0.0f, data_im);

    for (channel = channels; channel--; data_im += channel_size) {
        for (kernel_row = 0; kernel_row < kernel; kernel_row++) {
            for (kernel_col = 0; kernel_col < kernel; kernel_col++) {
                input_row = -pad + kernel_row;
                for (output_rows = output_h; output_rows; output_rows--) {
                    if (!is_a_positive_and_inferior_to_b(input_row, height)) {
                        data_col += output_w;
                    } else {
                        input_col = -pad + kernel_col;
                        for (output_col = output_w; output_col; output_col--) {
                            if (is_a_positive_and_inferior_to_b(input_col,
                                                                width)) {
                                data_im[input_row * width + input_col] +=
                                    *data_col;
                            }
                            data_col++;
                            input_col += stride;
                        }
                    }
                    input_row += stride;
                }
            }
        }
    }
}

void bcnn_add_bias_with_relu(float *dst, const float *bias, size_t planeNumber,
                             size_t biasNumber) {
#if defined(BCNN_USE_AVX)
    __m128 mv = _mm_set1_ps(0.0f);
    for (int z = 0; z < biasNumber; ++z) {
        __m128 biasV = _mm_load_ps(bias + 4 * z);
        float *dst_z = dst + planeNumber * 4 * z;
        for (int p = 0; p < planeNumber; ++p) {
            __m128 dstV = _mm_add_ps(_mm_load_ps(dst_z + 4 * p), biasV);
            dstV = _mm_max_ps(dstV, mv);
            _mm_store_ps(dst_z + 4 * p, dstV);
        }
    }
#elif defined(BCNN_USE_NEON)
// TODO
#else
    for (int z = 0; z < biasNumber; ++z) {
        float *dstZ = dst + planeNumber * 4 * z;
        const float *biasZ = bias + 4 * z;
        for (int p = 0; p < planeNumber; ++p) {
            float *dstX = dstZ + 4 * p;
            for (int i = 0; i < 4; ++i) {
                dstX[i] += biasZ[i];
                if (dstX[i] < 0) {
                    dstX[i] = 0;
                }
            }
        }
    }
#endif
}

void bcnn_scale_and_add_bias(float *dst, const float *src, const float *bias,
                             const float *alpha, size_t planeNumber,
                             size_t biasNumber) {
    for (int z = 0; z < biasNumber; ++z) {
        float *dstZ = dst + planeNumber * 4 * z;
        const float *srcZ = src + planeNumber * 4 * z;
        const float *biasZ = bias + 4 * z;
        const float *alphaZ = alpha + 4 * z;
        for (int p = 0; p < planeNumber; ++p) {
            float *dstX = dstZ + 4 * p;
            const float *srcX = srcZ + 4 * p;
            for (int i = 0; i < 4; ++i) {
                dstX[i] = srcX[i] * alphaZ[i] + biasZ[i];
            }
        }
    }
}

void bcnn_scale_and_add_bias_with_lrelu(float *dst, const float *src,
                                        const float *bias, const float *alpha,
                                        size_t planeNumber, size_t biasNumber) {
    for (int z = 0; z < biasNumber; ++z) {
        float *dstZ = dst + planeNumber * 4 * z;
        const float *srcZ = src + planeNumber * 4 * z;
        const float *biasZ = bias + 4 * z;
        const float *alphaZ = alpha + 4 * z;
        for (int p = 0; p < planeNumber; ++p) {
            float *dstX = dstZ + 4 * p;
            const float *srcX = srcZ + 4 * p;
            for (int i = 0; i < 4; ++i) {
                dstX[i] = srcX[i] * alphaZ[i] + biasZ[i];
                dstX[i] = (dstX[i] > 0 ? dstX[i] : 0.1f * dstX[i]);
            }
        }
    }
}

void bcnn_nchw_to_nc4hw4(float *dst, const float *src, size_t area,
                         size_t depth) {
    int z, x;
    int cur = 0;
    memset(dst, 0, area * bh_div_up(depth, 4) * 4 * sizeof(float));
    for (z = 0; z < depth; ++z) {
        int plane = z / 4;
        float *dstPlane = plane * area * 4 + dst;
        int offset = z % 4;
        for (x = 0; x < area; ++x) {
            dstPlane[4 * x + offset] = src[cur++];
        }
    }
}

void bcnn_nc4hw4_to_nchw(float *dst, const float *src, size_t area,
                         size_t depth) {
    int x;
    int z;
    int cur = 0;
    for (z = 0; z < depth; ++z) {
        int plane = z / 4;
        const float *srcPlane = plane * area * 4 + src;
        int offset = z % 4;
        for (x = 0; x < area; ++x) {
            dst[cur++] = srcPlane[4 * x + offset];
        }
    }
}

void bcnn_conv3x3_convert_src(const float *src, float *dst, size_t step) {
    float *_x = (float *)src;
    bv_float4 m00;
    bv_float4 m01;
    bv_float4 m02;
    bv_float4 m03;
    bv_float4 m10;
    bv_float4 m11;
    bv_float4 m12;
    bv_float4 m13;
    bv_float4 m20;
    bv_float4 m21;
    bv_float4 m22;
    bv_float4 m23;
    bv_float4 m30;
    bv_float4 m31;
    bv_float4 m32;
    bv_float4 m33;
    float *_y = dst;
    m00 = bv_float4_sub(bv_float4_load(_x + 4 * 0), bv_float4_load(_x + 4 * 8));
    m01 = bv_float4_sub(bv_float4_load(_x + 4 * 1), bv_float4_load(_x + 4 * 9));
    m02 =
        bv_float4_sub(bv_float4_load(_x + 4 * 2), bv_float4_load(_x + 4 * 10));
    m03 =
        bv_float4_sub(bv_float4_load(_x + 4 * 3), bv_float4_load(_x + 4 * 11));
    m10 = bv_float4_add(bv_float4_load(_x + 4 * 4), bv_float4_load(_x + 4 * 8));
    m11 = bv_float4_add(bv_float4_load(_x + 4 * 5), bv_float4_load(_x + 4 * 9));
    m12 =
        bv_float4_add(bv_float4_load(_x + 4 * 6), bv_float4_load(_x + 4 * 10));
    m13 =
        bv_float4_add(bv_float4_load(_x + 4 * 7), bv_float4_load(_x + 4 * 11));
    m20 = bv_float4_sub(bv_float4_load(_x + 4 * 8), bv_float4_load(_x + 4 * 4));
    m21 = bv_float4_sub(bv_float4_load(_x + 4 * 9), bv_float4_load(_x + 4 * 5));
    m22 =
        bv_float4_sub(bv_float4_load(_x + 4 * 10), bv_float4_load(_x + 4 * 6));
    m23 =
        bv_float4_sub(bv_float4_load(_x + 4 * 11), bv_float4_load(_x + 4 * 7));
    m30 =
        bv_float4_sub(bv_float4_load(_x + 4 * 12), bv_float4_load(_x + 4 * 4));
    m31 =
        bv_float4_sub(bv_float4_load(_x + 4 * 13), bv_float4_load(_x + 4 * 5));
    m32 =
        bv_float4_sub(bv_float4_load(_x + 4 * 14), bv_float4_load(_x + 4 * 6));
    m33 =
        bv_float4_sub(bv_float4_load(_x + 4 * 15), bv_float4_load(_x + 4 * 7));

    bv_float4_store(bv_float4_sub(m00, m02), _y + step * 0);
    bv_float4_store(bv_float4_add(m01, m02), _y + step * 1);
    bv_float4_store(bv_float4_sub(m02, m01), _y + step * 2);
    bv_float4_store(bv_float4_sub(m03, m01), _y + step * 3);
    bv_float4_store(bv_float4_sub(m10, m12), _y + step * 4);
    bv_float4_store(bv_float4_add(m11, m12), _y + step * 5);
    bv_float4_store(bv_float4_sub(m12, m11), _y + step * 6);
    bv_float4_store(bv_float4_sub(m13, m11), _y + step * 7);
    bv_float4_store(bv_float4_sub(m20, m22), _y + step * 8);
    bv_float4_store(bv_float4_add(m21, m22), _y + step * 9);
    bv_float4_store(bv_float4_sub(m22, m21), _y + step * 10);
    bv_float4_store(bv_float4_sub(m23, m21), _y + step * 11);
    bv_float4_store(bv_float4_sub(m30, m32), _y + step * 12);
    bv_float4_store(bv_float4_add(m31, m32), _y + step * 13);
    bv_float4_store(bv_float4_sub(m32, m31), _y + step * 14);
    bv_float4_store(bv_float4_sub(m33, m31), _y + step * 15);
}

void bcnn_conv3x3_convert_dst(const float *srcZ, float *dstBlock, size_t step) {
    float *yy = dstBlock;
    bv_float4 m00;
    bv_float4 m01;
    bv_float4 m02;
    bv_float4 m03;
    bv_float4 m10;
    bv_float4 m11;
    bv_float4 m12;
    bv_float4 m13;
    float *x = (float *)srcZ;
    m00 = bv_float4_add(bv_float4_add(bv_float4_load(x + step * 0),
                                      bv_float4_load(x + step * 4)),
                        bv_float4_load(x + step * 8));
    m01 = bv_float4_add(bv_float4_add(bv_float4_load(x + step * 1),
                                      bv_float4_load(x + step * 5)),
                        bv_float4_load(x + step * 9));
    m02 = bv_float4_add(bv_float4_add(bv_float4_load(x + step * 2),
                                      bv_float4_load(x + step * 6)),
                        bv_float4_load(x + step * 10));
    m03 = bv_float4_add(bv_float4_add(bv_float4_load(x + step * 3),
                                      bv_float4_load(x + step * 7)),
                        bv_float4_load(x + step * 11));
    m10 = bv_float4_add(bv_float4_sub(bv_float4_load(x + step * 4),
                                      bv_float4_load(x + step * 8)),
                        bv_float4_load(x + step * 12));
    m11 = bv_float4_add(bv_float4_sub(bv_float4_load(x + step * 5),
                                      bv_float4_load(x + step * 9)),
                        bv_float4_load(x + step * 13));
    m12 = bv_float4_add(bv_float4_sub(bv_float4_load(x + step * 6),
                                      bv_float4_load(x + step * 10)),
                        bv_float4_load(x + step * 14));
    m13 = bv_float4_add(bv_float4_sub(bv_float4_load(x + step * 7),
                                      bv_float4_load(x + step * 11)),
                        bv_float4_load(x + step * 15));
    bv_float4_store(bv_float4_add(bv_float4_add(m00, m01), m02), yy + 4 * 0);
    bv_float4_store(bv_float4_add(bv_float4_sub(m01, m02), m03), yy + 4 * 1);
    bv_float4_store(bv_float4_add(bv_float4_add(m10, m11), m12), yy + 4 * 2);
    bv_float4_store(bv_float4_add(bv_float4_sub(m11, m12), m13), yy + 4 * 3);
}

void bcnn_conv3x3_convert_weights(const float *src_weights, float *dst_weights,
                                  int src_channels, int dst_channels) {
    float weight[CONV3x3_BLOCK_UNIT * CONV3x3_BLOCK_UNIT];
    int srcDepthD4 = bh_div_up(src_channels, 4);
    int dstDepthD4 = bh_div_up(dst_channels, 4);

    for (int dz = 0; dz < dst_channels; ++dz) {
        int dz_4 = dz / CONV3x3_BLOCK_UNIT;
        int mx = dz % CONV3x3_BLOCK_UNIT;
        float *dst_dz = dst_weights + dz_4 * srcDepthD4 * 16;
        for (int sz = 0; sz < src_channels; ++sz) {
            int sz_4 = sz / CONV3x3_BLOCK_UNIT;
            int my = sz % CONV3x3_BLOCK_UNIT;
            float *dst_sz =
                dst_dz + sz_4 * CONV3x3_BLOCK_UNIT * CONV3x3_BLOCK_UNIT;
            float *src = (float *)src_weights + 9 * (sz + dz * src_channels);
            float *dst = weight;
            float *k = (float *)src;
            float m00;
            float m01;
            float m02;
            float m10;
            float m11;
            float m12;
            float m20;
            float m21;
            float m22;
            float m30;
            float m31;
            float m32;
            m00 = k[0];
            m01 = k[1];
            m02 = k[2];
            m10 = 0.500000 * k[0] + 0.500000 * k[3] + 0.500000 * k[6];
            m11 = 0.500000 * k[1] + 0.500000 * k[4] + 0.500000 * k[7];
            m12 = 0.500000 * k[2] + 0.500000 * k[5] + 0.500000 * k[8];
            m20 = 0.500000 * k[0] + -0.500000 * k[3] + 0.500000 * k[6];
            m21 = 0.500000 * k[1] + -0.500000 * k[4] + 0.500000 * k[7];
            m22 = 0.500000 * k[2] + -0.500000 * k[5] + 0.500000 * k[8];
            m30 = 0 + k[6];
            m31 = 0 + k[7];
            m32 = 0 + k[8];

            k = dst;
            k[0] = m00;
            k[1] = 0.500000 * m00 + 0.500000 * m01 + 0.500000 * m02;
            k[2] = 0.500000 * m00 + -0.500000 * m01 + 0.500000 * m02;
            k[3] = 0 + m02;
            k[4] = m10;
            k[5] = 0.500000 * m10 + 0.500000 * m11 + 0.500000 * m12;
            k[6] = 0.500000 * m10 + -0.500000 * m11 + 0.500000 * m12;
            k[7] = 0 + m12;
            k[8] = m20;
            k[9] = 0.500000 * m20 + 0.500000 * m21 + 0.500000 * m22;
            k[10] = 0.500000 * m20 + -0.500000 * m21 + 0.500000 * m22;
            k[11] = 0 + m22;
            k[12] = m30;
            k[13] = 0.500000 * m30 + 0.500000 * m31 + 0.500000 * m32;
            k[14] = 0.500000 * m30 + -0.500000 * m31 + 0.500000 * m32;
            k[15] = 0 + m32;

            for (int ki = 0; ki < CONV3x3_BLOCK_UNIT * CONV3x3_BLOCK_UNIT;
                 ++ki) {
                float *dst_i = dst_sz + ki * srcDepthD4 * dstDepthD4 * 16;
                dst_i[4 * my + mx] = weight[ki];
            }
        }
    }
}

static void bcnn_gemm_kernel4x4(float *dst, const float *src,
                                const float *weight, size_t src_depth_quad,
                                size_t dst_step, size_t dst_depth_quad,
                                size_t width, size_t weight_depth_offset) {
#if defined(BCNN_USE_AVX)
    int src_depth_step = 4 * width;
    int wC4 = width / 4;
    int w4End = wC4 * 4;
    for (int dz = 0; dz < dst_depth_quad; ++dz) {
        float *dst_z = dst + dz * dst_step;
        float *weight_dz =
            weight + dz * (src_depth_quad * 16 + weight_depth_offset);

        for (int dx = 0; dx < wC4; ++dx) {
            float *dst_x = dst_z + dx * 4 * 4;
            __m128 dst0 = _mm_set1_ps(0.0f);
            __m128 dst1 = _mm_set1_ps(0.0f);
            __m128 dst2 = _mm_set1_ps(0.0f);
            __m128 dst3 = _mm_set1_ps(0.0f);
            const float *src_dx = src + 4 * dx * 4;
            for (int sz = 0; sz < src_depth_quad; ++sz) {
                const float *src_z = src_dx + sz * src_depth_step;
                const float *weight_z = weight_dz + sz * 16;
                __m128 w0 = _mm_loadu_ps(weight_z + 4 * 0);
                __m128 w1 = _mm_loadu_ps(weight_z + 4 * 1);
                __m128 w2 = _mm_loadu_ps(weight_z + 4 * 2);
                __m128 w3 = _mm_loadu_ps(weight_z + 4 * 3);
#define COMPUTE(v)                                     \
    {                                                  \
        __m128 srcValue = _mm_loadu_ps(src_z + 4 * v); \
        __m128 s0 = _mm_set1_ps(srcValue[0]);          \
        __m128 s1 = _mm_set1_ps(srcValue[1]);          \
        __m128 s2 = _mm_set1_ps(srcValue[2]);          \
        __m128 s3 = _mm_set1_ps(srcValue[3]);          \
        __m128 sw0 = _mm_mul_ps(s0, w0);               \
        __m128 sw1 = _mm_mul_ps(s1, w1);               \
        __m128 sw2 = _mm_mul_ps(s2, w2);               \
        __m128 sw3 = _mm_mul_ps(s3, w3);               \
        dst##v = _mm_add_ps(dst##v, sw0);              \
        dst##v = _mm_add_ps(dst##v, sw1);              \
        dst##v = _mm_add_ps(dst##v, sw2);              \
        dst##v = _mm_add_ps(dst##v, sw3);              \
    }

                COMPUTE(0);
                COMPUTE(1);
                COMPUTE(2);
                COMPUTE(3);
            }

            _mm_store_ps(dst_x + 4 * 0, dst0);
            _mm_store_ps(dst_x + 4 * 1, dst1);
            _mm_store_ps(dst_x + 4 * 2, dst2);
            _mm_store_ps(dst_x + 4 * 3, dst3);
        }

        for (int dx = w4End; dx < width; ++dx) {
            float *dst_x = dst_z + dx * 4;
            __m128 dstValue = _mm_set1_ps(0.0f);

            const float *src_dx = src + 4 * dx;
            for (int sz = 0; sz < src_depth_quad; ++sz) {
                const float *src_z = src_dx + sz * src_depth_step;
                const float *weight_z = weight_dz + sz * 16;
                __m128 w0 = _mm_loadu_ps(weight_z + 4 * 0);
                __m128 w1 = _mm_loadu_ps(weight_z + 4 * 1);
                __m128 w2 = _mm_loadu_ps(weight_z + 4 * 2);
                __m128 w3 = _mm_loadu_ps(weight_z + 4 * 3);

                __m128 srcValue = _mm_loadu_ps(src_z);
                __m128 s0 = _mm_set1_ps(srcValue[0]);
                __m128 s1 = _mm_set1_ps(srcValue[1]);
                __m128 s2 = _mm_set1_ps(srcValue[2]);
                __m128 s3 = _mm_set1_ps(srcValue[3]);

                __m128 sw0 = _mm_mul_ps(s0, w0);
                __m128 sw1 = _mm_mul_ps(s1, w1);
                __m128 sw2 = _mm_mul_ps(s2, w2);
                __m128 sw3 = _mm_mul_ps(s3, w3);
                dstValue = _mm_add_ps(dstValue, sw0);
                dstValue = _mm_add_ps(dstValue, sw1);
                dstValue = _mm_add_ps(dstValue, sw2);
                dstValue = _mm_add_ps(dstValue, sw3);
            }
            _mm_store_ps(dst_x, dstValue);
        }
    }
#elif defined(BCNN_USE_NEON)
// TODO
#else
    int dx, sz, fx, fy, dz;
    size_t src_depth_step = 4 * width;
    for (dz = 0; dz < dst_depth_quad; ++dz) {
        float *dst_z = dst + dz * dst_step;
        float *weight_dz =
            (float *)weight + dz * (src_depth_quad * 16 + weight_depth_offset);
        for (dx = 0; dx < width; ++dx) {
            float *dst_x = dst_z + dx * 4;
            dst_x[0] = 0.0f;
            dst_x[1] = 0.0f;
            dst_x[2] = 0.0f;
            dst_x[3] = 0.0f;
            const float *src_dx = src + 4 * dx;
            for (sz = 0; sz < src_depth_quad; ++sz) {
                const float *src_z = src_dx + sz * src_depth_step;
                const float *weight_z = weight_dz + sz * 16;
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        dst_x[j] += src_z[i] * weight_z[4 * i + j];
                    }
                }
            }
        }
    }
#endif
}

static void bcnn_gemm_kernel4x4_tiled(float *dstOrigin, const float *src,
                                      const float *weight,
                                      size_t src_depth_quad, size_t dst_step,
                                      size_t dst_depth_quad,
                                      size_t weight_depth_offset) {
    bcnn_gemm_kernel4x4(dstOrigin, src, weight, src_depth_quad, dst_step,
                        dst_depth_quad, CONV_TILED, weight_depth_offset);
}

void bcnn_conv3x3s1_kernel(float *src, int src_w, int src_h, int src_c,
                           float *dst, int dst_w, int dst_h, int dst_c,
                           int batch_size, int pad, float *weights,
                           float *scales, float *biases, float *workspace,
                           int workspace_sz, int num_threads) {
    int ic_4 = bh_div_up(src_c, 4);
    int dc_4 = bh_div_up(dst_c, 4);
    int wUnit = bh_div_up(dst_w, 2);
    int hUnit = bh_div_up(dst_h, 2);
    // fprintf(stderr, "num_threads %d\n", num_threads);
    int workspace_thread_stride = workspace_sz / num_threads;

    // auto postFunction = mPostFunction;
    // print("dst_w=%d, dst_h=%d\n", dst_w, dst_h);
    /*fprintf(stderr, "%d %d %d %d %d %d %d %d %d %d\n", dst_w, dst_h, src_w,
            src_h, ic_4, dc_4, pad, pad, wUnit, hUnit);*/

    for (int batchIndex = 0; batchIndex < batch_size; ++batchIndex) {
        float *srcOrigin = src + src_w * src_h * ic_4 * 4 * batchIndex;
        float *dstOrigin = dst + dst_w * dst_h * dc_4 * 4 * batchIndex;
        int totalCount = hUnit * wUnit;

        int tileCount = bh_div_up(totalCount, CONV_TILED);
        num_threads = bh_min(num_threads, tileCount);

        float *weight = weights;
        float *bias = biases;

#pragma omp parallel for
        for (int tId = 0; tId < num_threads; tId++) {
            // outsideFunction((int)tId);
            float *_srcOrigin = workspace + tId * workspace_thread_stride;
            /*fprintf(stderr, "num_threads %d stride %d\n", num_threads,
                    workspace_thread_stride);*/
            for (int tIndex = (int)tId; tIndex < tileCount;
                 tIndex += num_threads) {
                int xIndex = (int)tIndex * CONV_TILED;
                int xReamin = totalCount - xIndex;
                int xC = xReamin > CONV_TILED ? CONV_TILED : xReamin;
                float *dstBlock =
                    _srcOrigin + xC * CONV3x3_SRC_BLOCK * (ic_4 + dc_4);
                float *_dstOrigin = _srcOrigin + xC * CONV3x3_SRC_BLOCK * ic_4;

                // sourceTransformLambda(xIndex, xC, _srcOrigin, dstBlock);
                // Source Transform
                // auto sourceTransformLambda = [&](int xIndex, int xC,
                // float
                // *_srcOrigin,
                //                         float *dstBlock)
                // bh_timer t = {0};
                // bh_timer_start(&t);
                for (int xi = 0; xi < xC; ++xi) {
                    int index = xIndex + xi;
                    float *dstUnit = _srcOrigin + 4 * xi;

                    int wIndex = index % wUnit;
                    int hIndex = index / wUnit;

                    int srcX = wIndex * 2 - pad;
                    int srcY = hIndex * 2 - pad;
                    int sy = bh_max(0, srcY) - srcY;
                    int ey = bh_min(srcY + 4, src_h) - srcY;
                    int sx = bh_max(0, srcX) - srcX;
                    int ex = bh_min(srcX + 4, src_w) - srcX;

                    float *srcStart = srcOrigin + (srcX + srcY * src_w) * 4;

                    for (int z = 0; z < ic_4; ++z) {
                        memset(dstBlock, 0, CONV3x3_SRC_BLOCK * sizeof(float));

                        float *_dstStart = dstUnit + z * 4 * xC;

                        float *src_z = srcStart + z * 4 * src_w * src_h;
                        if (ex > sx) {
                            // Extract One Block
                            for (int yy = sy; yy < ey; ++yy) {
                                float *dst_yy = dstBlock + yy * 16;
                                float *src_yy = src_z + 4 * src_w * yy;
                                memcpy(dst_yy + 4 * sx, src_yy + sx * 4,
                                       4 * (ex - sx) * sizeof(float));
                            }
                        }
                        // Transform
                        bcnn_conv3x3_convert_src(dstBlock, _dstStart,
                                                 4 * xC * ic_4);
                    }
                }
                // bh_timer_stop(&t);
                // fprintf(stderr, "conv3x3 src %f\n", bh_timer_get_msec(&t));

                /*fprintf(stderr, "%d %d %d %d %f %f\n",
                        ((((xIndex + xC) % wUnit) * 2 - pad) +
                         (((xIndex + xC) / wUnit) * 2 - pad) * src_w) *
                                4 +
                            (ic_4 - 1) * 4 * src_w * src_h,
                        xC * CONV3x3_SRC_BLOCK * (ic_4),
                        xC * CONV3x3_SRC_BLOCK * (ic_4 + dc_4), tIndex,
                        _srcOrigin[0], _srcOrigin[5]);*/
                // fprintf(stderr, "w %d %f %f\n", tIndex, weight[0],
                // weight[5]);

                // gemmFunctionLambda(xC, _srcOrigin, _dstOrigin);
                // gemmFunctionLambda = [&](int xC, const float *_srcOrigin,
                //                     float *_dstOrigin)
                if (0) {
                    // bh_timer_start(&t);
                    if (xC == CONV_TILED) {
                        for (int i = 0;
                             i < CONV3x3_BLOCK_UNIT * CONV3x3_BLOCK_UNIT; ++i) {
                            bcnn_gemm_kernel4x4_tiled(
                                _dstOrigin + i * dc_4 * 4 * xC,
                                _srcOrigin + i * ic_4 * 4 * xC,
                                weight + i * 16 * ic_4 * dc_4, ic_4, xC * 4,
                                dc_4, 0);
                        }
                    } else {
                        for (int i = 0;
                             i < CONV3x3_BLOCK_UNIT * CONV3x3_BLOCK_UNIT; ++i) {
                            bcnn_gemm_kernel4x4(
                                _dstOrigin + (i * dc_4) * xC * 4,
                                _srcOrigin + i * ic_4 * 4 * xC,
                                weight + (i * dc_4) * ic_4 * 16, ic_4, xC * 4,
                                dc_4, xC, 0);
                        }
                    }
                } else {
                    // bh_timer_start(&t);
                    // Multi
                    if (xC == CONV_TILED) {
#pragma omp parallel for
                        for (int tiid = 0; tiid < num_threads; tiid++) {
                            for (int i = (int)tiid;
                                 i < CONV3x3_BLOCK_UNIT * CONV3x3_BLOCK_UNIT;
                                 i += num_threads) {
                                bcnn_gemm_kernel4x4_tiled(
                                    _dstOrigin + i * dc_4 * 4 * xC,
                                    _srcOrigin + i * ic_4 * 4 * xC,
                                    weight + i * 16 * ic_4 * dc_4, ic_4, xC * 4,
                                    dc_4, 0);
                            }
                        }

                    } else {
#pragma omp parallel for
                        for (int tiid = 0; tiid < num_threads; tiid++) {
                            for (int i = (int)tiid;
                                 i < CONV3x3_BLOCK_UNIT * CONV3x3_BLOCK_UNIT;
                                 i += num_threads) {
                                bcnn_gemm_kernel4x4(
                                    _dstOrigin + (i * dc_4) * xC * 4,
                                    _srcOrigin + i * ic_4 * 4 * xC,
                                    weight + (i * dc_4) * ic_4 * 16, ic_4,
                                    xC * 4, dc_4, xC, 0);
                            }
                        }
                    }
                }
                // bh_timer_stop(&t);
                // fprintf(stderr, "conv3x3 gemm %f\n", bh_timer_get_msec(&t));

                /*fprintf(stderr, "%d %f %f\n", tIndex, _dstOrigin[0],
                        _dstOrigin[5]);*/

                // bh_timer_start(&t);
                // Dest Transform
                for (int xi = 0; xi < xC; ++xi) {
                    int index = xIndex + xi;
                    float *srcUnit = _dstOrigin + 4 * xi;

                    int wIndex = index % wUnit;
                    int hIndex = index / wUnit;
                    int dstX = wIndex * 2;
                    int dstY = hIndex * 2;

                    float *dstStart = dstOrigin + 4 * (dstX + dstY * dst_w);

                    for (int z = 0; z < dc_4; ++z) {
                        float *srcZ = srcUnit + z * xC * 4;
                        float *dstZ = dstStart + z * dst_w * dst_h * 4;
                        bcnn_conv3x3_convert_dst(srcZ, dstBlock, dc_4 * 4 * xC);
                        // bias addition and relu
                        float *bias_z = bias + 4 * z;
                        float *scales_z = scales + 4 * z;
                        // bcnn_add_bias_with_relu(dstBlock, bias_z, 4, 1);
                        /*bcnn_scale_and_add_bias(dstBlock, dstBlock, bias_z,
                                                scales_z, 4, 1);*/
                        bcnn_scale_and_add_bias_with_lrelu(
                            dstBlock, dstBlock, bias_z, scales_z, 4, 1);
                        bv_float4_store(bv_float4_load(dstBlock), dstZ);
                        if (wIndex * 2 + 1 < dst_w) {
                            bv_float4_store(bv_float4_load(dstBlock + 4),
                                            dstZ + 4);
                        }
                        if (hIndex * 2 + 1 < dst_h) {
                            bv_float4_store(bv_float4_load(dstBlock + 8),
                                            dstZ + dst_w * 4);
                            if (wIndex * 2 + 1 < dst_w) {
                                bv_float4_store(bv_float4_load(dstBlock + 12),
                                                dstZ + dst_w * 4 + 4);
                            }
                        }
                    }
                }
                // bh_timer_stop(&t);
                // fprintf(stderr, "conv3x3 dst %f\n", bh_timer_get_msec(&t));
            }
        }
    }

    return;
}

// General Matrix-Matrix multiplication
//             ldb n
//          _________
//          |       |
//          |   B   | k
//          |       |
//  ________|______ |
//  |       |       |
// m|       |       | m
//  |   A   |   C   |
//  |_______|_______|
//  lda k     ldc n
//

// This implementation follows the Blis micro-kernel algorithm
// Reference: BLIS: A Framework for Rapidly Instantiating BLAS Functionality
static int equal(float a, float b) {
    const float EPSILON = 1e-5;
    if (fabsf(a - b) < EPSILON) {
        return 1;
    }
    return 0;
}

static void sgemm_nn_pack_MRxk8(int k, const float *A, int inc_row_A,
                                int inc_col_A, float *buffer, int mr) {
    int j, a2 = inc_row_A, a3 = 2 * inc_row_A, a4 = 3 * inc_row_A;
    int a5 = 4 * inc_row_A;
    int a6 = 5 * inc_row_A;
    int a7 = 6 * inc_row_A;
    int a8 = 7 * inc_row_A;
    for (j = 0; j < k; ++j) {
        buffer[0] = A[0];
        buffer[1] = A[a2];
        buffer[2] = A[a3];
        buffer[3] = A[a4];
        buffer[4] = A[a5];
        buffer[5] = A[a6];
        buffer[6] = A[a7];
        buffer[7] = A[a8];
        A += 1;
        buffer += mr;
    }
}

static void sgemm_nn_pack_MRxk4(int k, const float *A, int inc_row_A,
                                int inc_col_A, float *buffer, int mr) {
    int j, a2 = inc_row_A, a3 = 2 * inc_row_A, a4 = 3 * inc_row_A;
    for (j = 0; j < k; ++j) {
        buffer[0] = A[0];
        buffer[1] = A[a2];
        buffer[2] = A[a3];
        buffer[3] = A[a4];
        A += 1;
        buffer += mr;
    }
}

static void sgemm_nn_pack_A(int mc, int kc, const float *A, int inc_row_A,
                            int inc_col_A, float *buffer, int mr) {
    int mp = mc / mr;
    int _mr = mc % mr;
    int tmp1 = kc * mr;
    int tmp2 = mr * inc_row_A;
#pragma omp parallel for
    for (int i = 0; i < mp; ++i) {
#ifdef BCNN_USE_NEON
#if (defined(__aarch64__))
        sgemm_nn_pack_MRxk8(kc, A + tmp2 * i, inc_row_A, inc_col_A,
                            buffer + tmp1 * i, mr);
#else
        sgemm_nn_pack_MRxk4(kc, A + tmp2 * i, inc_row_A, inc_col_A,
                            buffer + tmp1 * i, mr);
#endif  // __aarch64__
#else
        sgemm_nn_pack_MRxk8(kc, A + tmp2 * i, inc_row_A, inc_col_A,
                            buffer + tmp1 * i, mr);
#endif
    }
    if (_mr > 0) {
        for (int j = 0; j < kc; ++j) {
            for (int i = 0; i < _mr; ++i) {
                buffer[i] = A[i * inc_row_A];
            }
            for (int i = _mr; i < mr; ++i) {
                buffer[i] = 0.0;
            }
            A += 1;
            buffer += mr;
        }
    }
}

static void sgemm_pack_A(int mc, int kc, const float *A, int inc_row_A,
                         int inc_col_A, float *p, int mr) {
    int j, l, i0, i, nu;
    int mp = (mc + mr - 1) / mr;

    for (j = 0; j < kc; ++j) {
        for (l = 0; l < mp; ++l) {
            for (i0 = 0; i0 < mr; ++i0) {
                i = l * mr + i0;
                nu = l * mr * kc + j * mr + i0;
                p[nu] = (i < mc) ? A[i * inc_row_A + j * inc_col_A] : 0;
            }
        }
    }
}

static void sgemm_pack_B(int kc, int nc, const float *B, int inc_row_B,
                         int inc_col_B, float *p, int nr) {
    int i, l, j0;
    const int np = (nc + nr - 1) / nr;

    for (l = 0; l < np; ++l) {
        for (i = 0; i < kc; ++i) {
            for (j0 = 0; j0 < nr; ++j0) {
                int j = l * nr + j0;
                int nu = l * nr * kc + i * nr + j0;
                p[nu] = (j < nc) ? B[i * inc_row_B + j * inc_col_B] : 0;
            }
        }
    }
}

static void sgemm_nn_pack_kxNR(int k, const float *B, int inc_row_B,
                               int inc_col_B, float *buffer, int nr) {
    int i, j;
    for (i = 0; i < k; ++i) {
        for (j = 0; j < nr; ++j) {
            buffer[j] = B[j];
        }
        B += inc_row_B;
        buffer += nr;
    }
}

static void sgemm_nn_pack_B(int kc, int nc, const float *B, int inc_row_B,
                            int inc_col_B, float *buffer, int nr) {
    int np = nc / nr;
    int _nr = nc % nr;
    int tmp1 = kc * nr;
#pragma omp parallel for
    for (int j = 0; j < np; ++j) {
        sgemm_nn_pack_kxNR(kc, B + nr * j, inc_row_B, inc_col_B,
                           buffer + tmp1 * j, nr);
    }
    if (_nr > 0) {
        for (int i = 0; i < kc; ++i) {
            for (int j = 0; j < _nr; ++j) {
                buffer[j] = B[j];
            }
            for (int j = _nr; j < nr; ++j) {
                buffer[j] = 0.0;
            }
            buffer += nr;
            B += inc_row_B;
        }
    }
}

static void sgemm_ukernel(int kc, float alpha, const float *A, const float *B,
                          float beta, float *C, int inc_row_C, int inc_col_C,
                          int mr, int nr, float *AB0) {
    float AB[MR * NR];
#if (defined(BCNN_USE_AVX))
    __m256 abv0 = _mm256_setzero_ps();
    __m256 abv1 = _mm256_setzero_ps();
    __m256 abv2 = _mm256_setzero_ps();
    __m256 abv3 = _mm256_setzero_ps();

    __m256 abv4 = _mm256_setzero_ps();
    __m256 abv5 = _mm256_setzero_ps();
    __m256 abv6 = _mm256_setzero_ps();
    __m256 abv7 = _mm256_setzero_ps();

    __m256 av;

    for (int l = 0; l < kc; ++l) {
        av = _mm256_load_ps(A);
        abv0 = _mm256_add_ps(abv0, _mm256_mul_ps(_mm256_broadcast_ss(B), av));
        abv1 =
            _mm256_add_ps(abv1, _mm256_mul_ps(_mm256_broadcast_ss(B + 1), av));
        abv2 =
            _mm256_add_ps(abv2, _mm256_mul_ps(_mm256_broadcast_ss(B + 2), av));
        abv3 =
            _mm256_add_ps(abv3, _mm256_mul_ps(_mm256_broadcast_ss(B + 3), av));
        abv4 =
            _mm256_add_ps(abv4, _mm256_mul_ps(_mm256_broadcast_ss(B + 4), av));
        abv5 =
            _mm256_add_ps(abv5, _mm256_mul_ps(_mm256_broadcast_ss(B + 5), av));
        abv6 =
            _mm256_add_ps(abv6, _mm256_mul_ps(_mm256_broadcast_ss(B + 6), av));
        abv7 =
            _mm256_add_ps(abv7, _mm256_mul_ps(_mm256_broadcast_ss(B + 7), av));

        A += mr;
        B += nr;
    }
    _mm256_store_ps(AB + 0, abv0);
    _mm256_store_ps(AB + 8, abv1);
    _mm256_store_ps(AB + 16, abv2);
    _mm256_store_ps(AB + 24, abv3);
    _mm256_store_ps(AB + 32, abv4);
    _mm256_store_ps(AB + 40, abv5);
    _mm256_store_ps(AB + 48, abv6);
    _mm256_store_ps(AB + 56, abv7);
#elif (defined(BCNN_USE_NEON))
#if (defined(__aarch64__))
    float32x4_t av0, av1, bv0, bv1;
    float32x4_t abv0, abv1, abv2, abv3, abv4, abv5, abv6, abv7, abv8, abv9,
        abv10, abv11, abv12, abv13, abv14, abv15;
    abv0 = vdupq_n_f32(0.0f);
    abv1 = vdupq_n_f32(0.0f);
    abv2 = vdupq_n_f32(0.0f);
    abv3 = vdupq_n_f32(0.0f);
    abv4 = vdupq_n_f32(0.0f);
    abv5 = vdupq_n_f32(0.0f);
    abv6 = vdupq_n_f32(0.0f);
    abv7 = vdupq_n_f32(0.0f);
    abv8 = vdupq_n_f32(0.0f);
    abv9 = vdupq_n_f32(0.0f);
    abv10 = vdupq_n_f32(0.0f);
    abv11 = vdupq_n_f32(0.0f);
    abv12 = vdupq_n_f32(0.0f);
    abv13 = vdupq_n_f32(0.0f);
    abv14 = vdupq_n_f32(0.0f);
    abv15 = vdupq_n_f32(0.0f);
    for (int p = 0; p < kc; ++p) {
        av0 = vld1q_f32(A);
        av1 = vld1q_f32(A + 4);
        bv0 = vld1q_f32(B);
        bv1 = vld1q_f32(B + 4);
        abv0 = vfmaq_laneq_f32(abv0, av0, bv0, 0);
        abv1 = vfmaq_laneq_f32(abv1, av1, bv0, 0);
        abv2 = vfmaq_laneq_f32(abv2, av0, bv0, 1);
        abv3 = vfmaq_laneq_f32(abv3, av1, bv0, 1);
        abv4 = vfmaq_laneq_f32(abv4, av0, bv0, 2);
        abv5 = vfmaq_laneq_f32(abv5, av1, bv0, 2);
        abv6 = vfmaq_laneq_f32(abv6, av0, bv0, 3);
        abv7 = vfmaq_laneq_f32(abv7, av1, bv0, 3);
        abv8 = vfmaq_laneq_f32(abv8, av0, bv1, 0);
        abv9 = vfmaq_laneq_f32(abv9, av1, bv1, 0);
        abv10 = vfmaq_laneq_f32(abv10, av0, bv1, 1);
        abv11 = vfmaq_laneq_f32(abv11, av1, bv1, 1);
        abv12 = vfmaq_laneq_f32(abv12, av0, bv1, 2);
        abv13 = vfmaq_laneq_f32(abv13, av1, bv1, 2);
        abv14 = vfmaq_laneq_f32(abv14, av0, bv1, 3);
        abv15 = vfmaq_laneq_f32(abv15, av1, bv1, 3);
        B += nr;
        A += mr;
    }
    vst1q_f32(AB, abv0);
    vst1q_f32(AB + 4, abv1);
    vst1q_f32(AB + 8, abv2);
    vst1q_f32(AB + 12, abv3);
    vst1q_f32(AB + 16, abv4);
    vst1q_f32(AB + 20, abv5);
    vst1q_f32(AB + 24, abv6);
    vst1q_f32(AB + 28, abv7);
    vst1q_f32(AB + 32, abv8);
    vst1q_f32(AB + 36, abv9);
    vst1q_f32(AB + 40, abv10);
    vst1q_f32(AB + 44, abv11);
    vst1q_f32(AB + 48, abv12);
    vst1q_f32(AB + 52, abv13);
    vst1q_f32(AB + 56, abv14);
    vst1q_f32(AB + 60, abv15);
#else
    float32x4_t abv0 = vdupq_n_f32(0.0f);
    float32x4_t abv1 = vdupq_n_f32(0.0f);
    float32x4_t abv2 = vdupq_n_f32(0.0f);
    float32x4_t abv3 = vdupq_n_f32(0.0f);
    float32x4_t av;
    float32x4_t bv;
    float32x2_t bv01;
    float32x2_t bv23;
    for (int p = 0; p < kc; ++p) {
        av = vld1q_f32(A);
        bv = vld1q_f32(B);
        bv01 = vget_low_f32(bv);
        abv0 = vmlaq_lane_f32(abv0, av, bv01, 0);
        abv1 = vmlaq_lane_f32(abv1, av, bv01, 1);
        bv23 = vget_high_f32(bv);
        abv2 = vmlaq_lane_f32(abv2, av, bv23, 0);
        abv3 = vmlaq_lane_f32(abv3, av, bv23, 1);
        A += nr;
        B += nr;
    }
    vst1q_f32(AB + 0, abv0);
    vst1q_f32(AB + 4, abv1);
    vst1q_f32(AB + 8, abv2);
    vst1q_f32(AB + 12, abv3);
#endif  // __aarch64__
#else
    for (int i = 0; i < nr * nr; ++i) {
        AB[i] = 0.0f;
    }
    for (int l = 0; l < kc; ++l) {
        for (int j = 0; j < nr; ++j) {
            for (int i = 0; i < mr; ++i) {
                AB[i + j * mr] += A[i] * B[j];
            }
        }
        A += mr;
        B += nr;
    }
#endif
    if (equal(beta, 0.0)) {
        for (int j = 0; j < nr; ++j) {
            for (int i = 0; i < mr; ++i) {
                C[i * inc_row_C + j * inc_col_C] = 0.0;
            }
        }
    } else if (!equal(beta, 1.0)) {
        for (int j = 0; j < nr; ++j) {
            for (int i = 0; i < mr; ++i) {
                C[i * inc_row_C + j * inc_col_C] *= beta;
            }
        }
    }
    if (!equal(alpha, 1.0)) {
        for (int j = 0; j < nr; ++j) {
            for (int i = 0; i < mr; ++i) {
                C[i * inc_row_C + j * inc_col_C] += alpha * AB[i + j * mr];
            }
        }
    } else {
        for (int j = 0; j < nr; ++j) {
            for (int i = 0; i < mr; ++i) {
                C[i * inc_row_C + j * inc_col_C] += AB[i + j * mr];
            }
        }
    }
}

static void sgemm_axpy(int m, int n, float alpha, const float *X, int incRowX,
                       int incColX, float *Y, int incRowY, int incColY) {
    int i, j;
    if (!equal(alpha, 1.0)) {
        for (j = 0; j < n; ++j) {
            for (i = 0; i < m; ++i) {
                Y[i * incRowY + j] += alpha * X[i + j * incColX];
            }
        }
    } else {
        for (j = 0; j < n; ++j) {
            for (i = 0; i < m; ++i) {
                Y[i * incRowY + j] += X[i + j * incColX];
            }
        }
    }
}

static void sgemm_scal(int m, int n, float alpha, float *X, int incRowX,
                       int incColX) {
    int i, j;
    if (!equal(alpha, 0.0)) {
        for (i = 0; i < m; ++i) {
            for (j = 0; j < n; ++j) {
                X[i * incRowX + j] *= alpha;
            }
        }
    } else {
        for (i = 0; i < m; ++i) {
            for (j = 0; j < n; ++j) {
                X[i * incRowX + j] = 0.0;
            }
        }
    }
}

static void sgemm_mkernel(int mc, int nc, int kc, float alpha, float beta,
                          float *C, int inc_row_C, int inc_col_C,
                          float *buffer_A, float *buffer_B, float *buffer_AB,
                          float *buffer_C, int mr, int nr) {
    int mp = (mc + mr - 1) / mr;
    int np = (nc + nr - 1) / nr;

    int _mr = mc % mr;
    int _nr = nc % nr;
#pragma omp parallel for
    for (int j = 0; j < np; ++j) {
        int nrj = (j != np - 1 || _nr == 0) ? nr : _nr;
#pragma omp parallel for
        for (int i = 0; i < mp; ++i) {
            int mri = (i != mp - 1 || _mr == 0) ? mr : _mr;
            if (mri == mr && nrj == nr) {
                sgemm_ukernel(kc, alpha, &buffer_A[i * kc * mr],
                              &buffer_B[j * kc * nr], beta,
                              &C[i * mr * inc_row_C + j * nr], inc_row_C,
                              inc_col_C, mr, nr, buffer_AB);
            } else {
                float buf_c[MR * NR];
                sgemm_ukernel(kc, alpha, &buffer_A[i * kc * mr],
                              &buffer_B[j * kc * nr], 0.0, buf_c, 1, mr, mr, nr,
                              buffer_AB);
                sgemm_scal(mri, nrj, beta, &C[i * mr * inc_row_C + j * nr],
                           inc_row_C, inc_col_C);
                sgemm_axpy(mri, nrj, 1.0, buf_c, 1, mr,
                           &C[i * mr * inc_row_C + j * nr], inc_row_C,
                           inc_col_C);
            }
        }
    }
}

static void sgemm_nn(bcnn_gemm_context *ctx, int m, int n, int k, float alpha,
                     const float *A, int inc_row_A, int inc_col_A,
                     const float *B, int inc_row_B, int inc_col_B, float beta,
                     float *C, int inc_row_C, int inc_col_C) {
    int mb = (m + MC - 1) / MC;
    int nb = (n + NC - 1) / NC;
    int kb = (k + KC - 1) / KC;
    int _mc = m % MC;
    int _nc = n % NC;
    int _kc = k % KC;

    if (equal(alpha, 0.0) || k == 0) {
        sgemm_scal(m, n, beta, C, inc_row_C, inc_col_C);
        return;
    }

    for (int j = 0; j < nb; ++j) {
        int nc = (j != nb - 1 || _nc == 0) ? NC : _nc;
        for (int l = 0; l < kb; ++l) {
            int kc = (l != kb - 1 || _kc == 0) ? KC : _kc;
            float _beta = (l == 0) ? beta : 1.0f;
            sgemm_nn_pack_B(kc, nc, &B[l * KC * inc_row_B + j * NC], inc_row_B,
                            inc_col_B, ctx->buffer_b, NR);
            for (int i = 0; i < mb; ++i) {
                int mc = (i != mb - 1 || _mc == 0) ? MC : _mc;
                sgemm_nn_pack_A(mc, kc, &A[i * MC * inc_row_A + l * KC],
                                inc_row_A, inc_col_A, ctx->buffer_a, MR);
                sgemm_mkernel(mc, nc, kc, alpha, _beta,
                              &C[i * MC * inc_row_C + j * NC], inc_row_C,
                              inc_col_C, ctx->buffer_a, ctx->buffer_b,
                              ctx->buffer_ab, ctx->buffer_c, MR, NR);
            }
        }
    }
}

static void sgemm(bcnn_gemm_context *ctx, int m, int n, int k, float alpha,
                  const float *A, int inc_row_A, int inc_col_A, const float *B,
                  int inc_row_B, int inc_col_B, float beta, float *C,
                  int inc_row_C, int inc_col_C) {
    int mb = (m + MC - 1) / MC;
    int nb = (n + NC - 1) / NC;
    int kb = (k + KC - 1) / KC;
    int _mc = m % MC;
    int _nc = n % NC;
    int _kc = k % KC;

    if (equal(alpha, 0.0) || k == 0) {
        sgemm_scal(m, n, beta, C, inc_row_C, inc_col_C);
        return;
    }

    for (int j = 0; j < nb; ++j) {
        int nc = (j != nb - 1 || _nc == 0) ? NC : _nc;
        for (int l = 0; l < kb; ++l) {
            int kc = (l != kb - 1 || _kc == 0) ? KC : _kc;
            float _beta = (l == 0) ? beta : 1.0f;

            sgemm_pack_B(kc, nc, &B[l * KC * inc_row_B + j * NC], inc_row_B,
                         inc_col_B, ctx->buffer_b, NR);
            for (int i = 0; i < mb; ++i) {
                int mc = (i != mb - 1 || _mc == 0) ? MC : _mc;
                sgemm_pack_A(mc, kc, &A[i * MC * inc_row_A + l * KC], inc_row_A,
                             inc_col_A, ctx->buffer_a, MR);
                sgemm_mkernel(mc, nc, kc, alpha, _beta,
                              &C[i * MC * inc_row_C + j * NC], inc_row_C,
                              inc_col_C, ctx->buffer_a, ctx->buffer_b,
                              ctx->buffer_ab, ctx->buffer_c, MR, NR);
            }
        }
    }
}

int bcnn_gemm(bcnn_gemm_context *ctx, int trans_a, int trans_b, int m, int n,
              int k, float alpha, float *A, int lda, float *B, int ldb,
              float beta, float *C, int ldc) {
#if (defined(__aarch64__))
    // Switch A and B as OpenBlas is column major
    openblas_sgemm(ctx, trans_b, trans_a, n, m, k, alpha, B, ldb, A, lda, beta,
                   C, ldc);
#else
    int inc_row_A = (!trans_a) ? lda : 1;
    int inc_col_A = (!trans_a) ? 1 : lda;

    int inc_row_B = (!trans_b) ? ldb : 1;
    int inc_col_B = (!trans_b) ? 1 : ldb;

    if (!trans_a && !trans_b) {
        sgemm_nn(ctx, m, n, k, alpha, A, inc_row_A, inc_col_A, B, inc_row_B,
                 inc_col_B, beta, C, ldc, 1);
    } else {
        sgemm(ctx, m, n, k, alpha, A, inc_row_A, inc_col_A, B, inc_row_B,
              inc_col_B, beta, C, ldc, 1);
    }
#endif
    return 0;
}
