/**
 * @file stat.c
 * @brief: Statistical calculations for partes.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "pterr.h"

void calc_w(int64_t *tm_arr, uint64_t tm_len, int64_t *sim_cdf, int64_t *w_arr, double *wp_arr, double p_zcut);

// Variance calculation functions - all return double variance
int calc_sample_var_1d_fp64(double *arr, size_t n, double *var);
int calc_sample_var_1d_i64(int64_t *arr, size_t n, double *var);
int calc_sample_var_1d_u64(uint64_t *arr, size_t n, double *var);
int calc_sample_var_2d_fp64(double **arr, size_t n1d, size_t n2d, int direction, double **var);
int calc_sample_var_2d_i64(int64_t **arr, size_t n1d, size_t n2d, int direction, double **var);
int calc_sample_var_2d_u64(uint64_t **arr, size_t n1d, size_t n2d, int direction, double **var);

double stat_linreg_slope_u64(const uint64_t *x, const uint64_t *y, int n);
double stat_relative_diff(double a, double b);

/**
 * @brief Calculate Wasserstein distance between measured and theoretical timing distributions
 */
void 
calc_w(int64_t *tm_arr, uint64_t tm_len, int64_t *sim_cdf, int64_t *w_arr, double *wp_arr, double p_zcut) 
{
    double w = 0, wm = 0;
    int wtile = (int)((1 - p_zcut) * 100.0); // Using 100 as NTILE
    
    for (size_t i = 0; i < 100; i++) {
        size_t itm = (size_t)((double)i / 100.0 * tm_len);
        w_arr[i] = sim_cdf[i] - tm_arr[itm];
        wp_arr[i] = (double)w_arr[i] / tm_arr[itm];
    }
    
    for (size_t i = 0; i < wtile; i++) {
        size_t itm = (size_t)((double)i / 100.0 * tm_len);
        w += llabs(w_arr[i]);
        wm += tm_arr[itm];
    }
    
    w = w / (double)wtile;
    wm = wm / (double)wtile;
    double er = (wm != 0.0) ? (w / wm) : 0.0;

    printf(" W-Distance=%f  ", w);
    FILE *fp = fopen("wd.out", "w");
    if (fp) {
        fprintf(fp, "%f", w);
        fclose(fp);
    }
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
