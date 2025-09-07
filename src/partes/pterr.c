/**
 * @file pterr.c
 * @brief: Error codes for partes.
 */
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "pterr.h"
#include <stdarg.h>
#include <stdio.h>
#include <mpi.h>

/**
 * @brief Get string representation of error code
 * @param err The error code
 * @return String description of the error
 */
const char *get_pterr_str(enum pterr err) {
    switch (err) {
        case PTERR_SUCCESS:
            return "Success";
        case PTERR_TIMER_NEGATIVE:
            return "Timer returned negative value";
        case PTERR_TIMER_OVERFLOW:
            return "Timer overflow detected";
        case PTERR_EXIT_FLAG:
            return "Exit flag detected";
        case PTERR_MALLOC_FAILED:
            return "Memory allocation failed";
        case PTERR_INVALID_ARGUMENT:
            return "Invalid argument";
        default:
            return "Unknown error";
    }
}

/**
 * @brief Ordered MPI printf
 * @param myrank: The rank of the current process
 * @param nrank: The total number of ranks
 * @param format: The format string
 * @param ...: The arguments to the format string
 */
void 
pt_mpi_printf(int myrank, int nrank, const char *format, ...) {
    va_list args;
    int robin_flag = 0;
    
    if (myrank == 0) {
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        fflush(stdout);
        
        if (nrank > 1) {
            MPI_Send(&robin_flag, 1, MPI_INT, (myrank + 1) % nrank, 0, MPI_COMM_WORLD);
            MPI_Recv(&robin_flag, 1, MPI_INT, (myrank - 1 + nrank) % nrank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    } else {
        MPI_Recv(&robin_flag, 1, MPI_INT, (myrank - 1 + nrank) % nrank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        fflush(stdout);
        
        MPI_Send(&robin_flag, 1, MPI_INT, (myrank + 1) % nrank, 0, MPI_COMM_WORLD);
    }

    return;
}