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
 * @param tm_arr Array of measured timing values
 * @param tm_len Length of the timing array
 * @param sim_cdf Array of simulated CDF values
 * @param w_arr Array to store Wasserstein differences
 * @param wp_arr Array to store normalized Wasserstein differences
 * @param p_zcut Cutoff percentile for calculation
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
        w += abs(w_arr[i]);
        wm += tm_arr[itm];
    }
    
    w = w / (double)wtile;
    wm = wm / (double)wtile;
    double er = w / wm;

    printf(" W-Distance=%f  ", w);
    FILE *fp = fopen("wd.out", "w");
    if (fp) {
        fprintf(fp, "%f", w);
        fclose(fp);
    }
}
