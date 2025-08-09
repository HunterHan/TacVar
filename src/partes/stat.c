/**
 * @file stat.c
 * @brief: Statistical calculations for partes.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

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
