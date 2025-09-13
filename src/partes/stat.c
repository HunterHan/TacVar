/**
 * @file stat.c
 * @brief: Statistical calculations for partes.
 */
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "pterr.h"

void calc_cdf_i64(int64_t *raw, size_t len, int64_t *cdf, uint64_t ntiles);
void calc_cdf_u64(uint64_t *raw, size_t len, uint64_t *cdf, uint64_t ntiles);
int calc_w(int64_t *cdf_a, int64_t *cdf_b, int ntiles, double p_zcut, double *w);

// Variance calculation functions - all return double variance
int calc_sample_var_1d_fp64(double *arr, size_t n, double *var);
int calc_sample_var_1d_i64(int64_t *arr, size_t n, double *var);
int calc_sample_var_1d_u64(uint64_t *arr, size_t n, double *var);
int calc_sample_var_2d_fp64(double **arr, size_t n1d, size_t n2d, int direction, double **var);
int calc_sample_var_2d_i64(int64_t **arr, size_t n1d, size_t n2d, int direction, double **var);
int calc_sample_var_2d_u64(uint64_t **arr, size_t n1d, size_t n2d, int direction, double **var);

double stat_linreg_slope_u64(const uint64_t *x, const uint64_t *y, int n);
double stat_relative_diff(double a, double b);
static int _comp_u64(const void *a, const void *b);
static int _comp_i64(const void *a, const void *b);

static int
_comp_u64(const void *a, const void *b)
{
    return (*(const uint64_t *)a > *(const uint64_t *)b) - (*(const uint64_t *)a < *(const uint64_t *)b);
}

static int
_comp_i64(const void *a, const void *b)
{
    return (*(const int64_t *)a > *(const int64_t *)b) - (*(const int64_t *)a < *(const int64_t *)b);
}

void
calc_cdf_i64(int64_t *raw, size_t len, int64_t *cdf, uint64_t ntiles)
{
    qsort(raw, len, sizeof(int64_t), _comp_i64);
    for (uint64_t i = 0; i < ntiles; i++) {
        uint64_t idx = (uint64_t)((double)i / (double)(ntiles - 1) * (double)(len - 1));
        if (idx >= len) idx = len - 1;
        cdf[i] = raw[idx];
    }
}

void
calc_cdf_u64(uint64_t *raw, size_t len, uint64_t *cdf, uint64_t ntiles)
{
    qsort(raw, len, sizeof(uint64_t), _comp_u64);
    for (uint64_t i = 0; i < ntiles; i++) {
        uint64_t idx = (uint64_t)((double)i / (double)(ntiles - 1) * (double)(len - 1));
        if (idx >= len) idx = len - 1;
        cdf[i] = raw[idx];
    }
}



/**
 * @brief Calculate Wasserstein distance between measured and theoretical timing distributions
 */
int 
calc_w(int64_t *cdf_a, int64_t *cdf_b, int ntiles, double p_zcut, double *w) 
{
    int err = PTERR_SUCCESS;
    int64_t *w_arr = (int64_t *)malloc(ntiles * sizeof(int64_t));
    if (w_arr == NULL) {
        err = PTERR_MALLOC_FAILED;
        _ptm_exit_on_error(err, "calc_w");
    }
    *w = 0;
    int tile_max = (int)((p_zcut) * (double)ntiles);
    
    for (int i = 0; i < tile_max; i++) {
        w_arr[i] = llabs(cdf_a[i] - cdf_b[i]);
    }

    for (int i = tile_max; i < ntiles; i++) {
        w_arr[i] = 0;
    }

    for (int i = 0; i < ntiles; i++) {
        *w += w_arr[i];
    }
    *w /= (double)ntiles;

EXIT:
    if (w_arr) {
        free(w_arr);
        w_arr = NULL;
    }
    return err;
}

int
calc_sample_var_1d_fp64(double *arr, size_t n, double *var)
{
    if (n <= 1) return PTERR_INVALID_ARGUMENT;
    double mean = 0.0;
    *var = 0.0;
    for (size_t i = 0; i < n; i++) {
        mean += arr[i];
    }
    mean /= (double)n;
    for (size_t i = 0; i < n; i++) {
        *var += (arr[i] - mean) * (arr[i] - mean);
    }
    *var = *var / (double)(n - 1);
    return 0;
}

int
calc_sample_var_1d_i64(int64_t *arr, size_t n, double *var)
{
    if (n <= 1) return PTERR_INVALID_ARGUMENT;
    double mean = 0.0;
    *var = 0.0;
    for (size_t i = 0; i < n; i++) {
        mean += (double)arr[i];
    }
    mean /= (double)n;
    for (size_t i = 0; i < n; i++) {
        double diff = (double)arr[i] - mean;
        *var += diff * diff;
    }
    *var = *var / (double)(n - 1);
    return 0;
}

int
calc_sample_var_1d_u64(uint64_t *arr, size_t n, double *var)
{
    if (n <= 1) return PTERR_INVALID_ARGUMENT;
    double mean = 0.0;
    *var = 0.0;
    for (size_t i = 0; i < n; i++) {
        mean += (double)arr[i];
    }
    mean /= (double)n;
    for (size_t i = 0; i < n; i++) {
        double diff = (double)arr[i] - mean;
        *var += diff * diff;
    }
    *var = *var / (double)(n - 1);
    return 0;
}

int
calc_sample_var_2d_fp64(double **arr, size_t n1d, size_t n2d, int direction, double **var)
{
    if (direction == 0) {
        // Calculate variance across the whole 2D array
        // Use the existing 1D function by treating the 2D array as flattened
        return calc_sample_var_1d_fp64(arr[0], n1d * n2d, *var);
    } else if (direction == 1) {
        // Calculate variance along first dimension (across columns, var of each row)
        for (size_t i = 0; i < n2d; i++) {
            int ret = calc_sample_var_1d_fp64(arr[i], n1d, &(*var)[i]);
            if (ret != 0) return ret;
        }
    } else if (direction == 2) {
        // Calculate variance along second dimension (across rows, var of each column)
        for (size_t j = 0; j < n1d; j++) {
            double *col = (double *)malloc(n2d * sizeof(double));
            if (col == NULL) return PTERR_MALLOC_FAILED;
            for (size_t i = 0; i < n2d; i++) {
                col[i] = arr[i][j];
            }
            int ret = calc_sample_var_1d_fp64(col, n2d, &(*var)[j]);
            free(col);
            if (ret != 0) return ret;
        }
    } else {
        return PTERR_INVALID_ARGUMENT;
    }
    return 0;
}

int
calc_sample_var_2d_i64(int64_t **arr, size_t n1d, size_t n2d, int direction, double **var)
{
    if (direction == 0) {
        // Calculate variance across the whole 2D array
        // Use the existing 1D function by treating the 2D array as flattened
        return calc_sample_var_1d_i64(arr[0], n1d * n2d, *var);
    } else if (direction == 1) {
        // Calculate variance along first dimension (across columns, var of each row)
        for (size_t i = 0; i < n2d; i++) {
            int ret = calc_sample_var_1d_i64(arr[i], n1d, &(*var)[i]);
            if (ret != 0) return ret;
        }
    } else if (direction == 2) {
        // Calculate variance along second dimension (across rows, var of each column)
        for (size_t j = 0; j < n1d; j++) {
            int64_t *col = (int64_t *)malloc(n2d * sizeof(int64_t));
            if (col == NULL) return PTERR_MALLOC_FAILED;
            for (size_t i = 0; i < n2d; i++) {
                col[i] = arr[i][j];
            }
            int ret = calc_sample_var_1d_i64(col, n2d, &(*var)[j]);
            free(col);
            if (ret != 0) return ret;
        }
    } else {
        return PTERR_INVALID_ARGUMENT;
    }
    return 0;
}

int
calc_sample_var_2d_u64(uint64_t **arr, size_t n1d, size_t n2d, int direction, double **var)
{
    if (direction == 0) {
        // Calculate variance across the whole 2D array
        // Use the existing 1D function by treating the 2D array as flattened
        return calc_sample_var_1d_u64(arr[0], n1d * n2d, *var);
    } else if (direction == 1) {
        // Calculate variance along first dimension (across columns, var of each row)
        for (size_t i = 0; i < n2d; i++) {
            int ret = calc_sample_var_1d_u64(arr[i], n1d, &(*var)[i]);
            if (ret != 0) return ret;
        }
    } else if (direction == 2) {
        // Calculate variance along second dimension (across rows, var of each column)
        for (size_t j = 0; j < n1d; j++) {
            uint64_t *col = (uint64_t *)malloc(n2d * sizeof(uint64_t));
            if (col == NULL) return PTERR_MALLOC_FAILED;
            for (size_t i = 0; i < n2d; i++) {
                col[i] = arr[i][j];
            }
            int ret = calc_sample_var_1d_u64(col, n2d, &(*var)[j]);
            free(col);
            if (ret != 0) return ret;
        }
    } else {
        return PTERR_INVALID_ARGUMENT;
    }
    return 0;
}

/* ===== Linear regression helpers for detection ===== */
double
stat_linreg_slope_u64(const uint64_t *x, const uint64_t *y, int n)
{
    if (n <= 1) return 0.0;
    long double sx = 0.0L, sy = 0.0L, sxx = 0.0L, sxy = 0.0L;
    for (int i = 0; i < n; i++) {
        long double xd = (long double)x[i];
        long double yd = (long double)y[i];
        sx += xd; sy += yd; sxx += xd * xd; sxy += xd * yd;
    }
    long double denom = (long double)n * sxx - sx * sx;
    if (fabsl(denom) < 1e-12L) return 0.0;
    long double slope = ((long double)n * sxy - sx * sy) / denom;
    return (double)slope;
}

double 
stat_relative_diff(double a, double b)
{
    double denom = (fabs(a) + fabs(b)) * 0.5;
    if (denom < 1e-30) return 0.0;
    return fabs(a - b) / denom;
}
