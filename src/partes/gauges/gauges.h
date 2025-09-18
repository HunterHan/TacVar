/**
 * @file gauges.h
 * @brief: Header file for all gauge functions (external interface)
 */
#ifndef GAUGES_H
#define GAUGES_H

#include <stdint.h>

enum gauge_name {
    GAUGE_SUB_INTRINSIC = 0,
    GAUGE_SUB_SCALAR
};

// SUB_SCALAR gauge
int init_gauge_sub_scalar(void);
void run_gauge_sub_scalar(int64_t n);
void cleanup_gauge_sub_scalar(void);

#endif
