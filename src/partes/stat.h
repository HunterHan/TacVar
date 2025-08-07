/**
 * @file stat.h
 * @brief: Statistical function declarations for partes.
 */
#ifndef STAT_H
#define STAT_H

#include <stdint.h>

/**
 * @brief Calculate Wasserstein distance between measured and theoretical timing distributions
 * @param tm_arr Array of measured timing values
 * @param tm_len Length of the timing array
 * @param sim_cdf Array of simulated CDF values
 * @param w_arr Array to store Wasserstein differences
 * @param wp_arr Array to store normalized Wasserstein differences
 * @param p_zcut Cutoff percentile for calculation
 */
void calc_w(int64_t *tm_arr, uint64_t tm_len, int64_t *sim_cdf, int64_t *w_arr, double *wp_arr, double p_zcut);

#endif 