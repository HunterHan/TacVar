/**
 * @file mpi_wtime.c
 * @brief: Implementation of MPI_Wtime timer.
 */

#include <mpi.h>
#include <stdint.h>
#include "timers.h"
#include "../pterr.h"

int init_timer_mpi_wtime(void) {
    // No initialization needed for MPI_Wtime
    return PTERR_SUCCESS;
}

int64_t tick_mpi_wtime(void) {
    double time = MPI_Wtime();
    return (int64_t)(time * 1000000000.0);
}

int64_t tock_mpi_wtime(void) {
    double time = MPI_Wtime();
    return (int64_t)(time * 1000000000.0);
}

int64_t get_stamp_mpi_wtime(void) {
    double time = MPI_Wtime();
    return (int64_t)(time * 1000000000.0);
}
