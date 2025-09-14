/**
 * @file stat.h
 * @brief: Statistical calculations for partes.
 */
#include <stdint.h>
#include <stdlib.h>

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