/**
 * @file parse_args.c
 * @brief: Parse the arguments for partes.
 */
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "partes_types.h"
#include "./kernels/kernels.h"
#include "pterr.h"

/**
 * --fkern: Front flush kernel name. 
 *          Current supported: none, triad, scale, copy, add, pow, dgemm, mpi_bcast.
 * --fsize: Front kernel memory footprint size before timing area.
 * --rkern: Rear flush kernel name.
 *          Current supported: none, triad, scale, copy, add, pow, dgemm, mpi_bcast.
 * --rsize: Rear kernel memory footprint size after timing area.
 * --timer: Timer to measure. Current supported: clock_gettime.
 * --ntests: Number of gauge measurements.
 * --nsub: Number of subtractions per gauge measurement.
 */

// Function to set kernel function pointers based on kernel types
void 
set_kernel_functions(pt_test_options_t *ptopts, pt_kern_func_t *ptfuncs) 
{
    // Initialize all function pointers to NULL
    ptfuncs->init_fkern = NULL;
    ptfuncs->run_fkern = NULL;
    ptfuncs->cleanup_fkern = NULL;
    ptfuncs->init_rkern = NULL;
    ptfuncs->run_rkern = NULL;
    ptfuncs->cleanup_rkern = NULL;

    // Set front kernel functions
    switch (ptopts->fkern) {
        case KERN_NONE:
            ptfuncs->init_fkern = init_kern_none;
            ptfuncs->run_fkern = run_kern_none;
            ptfuncs->cleanup_fkern = cleanup_kern_none;
            break;
        case KERN_TRIAD:
            ptfuncs->init_fkern = init_kern_triad;
            ptfuncs->run_fkern = run_kern_triad;
            ptfuncs->cleanup_fkern = cleanup_kern_triad;
            break;
        case KERN_SCALE:
            ptfuncs->init_fkern = init_kern_scale;
            ptfuncs->run_fkern = run_kern_scale;
            ptfuncs->cleanup_fkern = cleanup_kern_scale;
            break;
        case KERN_COPY:
            ptfuncs->init_fkern = init_kern_copy;
            ptfuncs->run_fkern = run_kern_copy;
            ptfuncs->cleanup_fkern = cleanup_kern_copy;
            break;
        case KERN_ADD:
            ptfuncs->init_fkern = init_kern_add;
            ptfuncs->run_fkern = run_kern_add;
            ptfuncs->cleanup_fkern = cleanup_kern_add;
            break;
        case KERN_POW:
            ptfuncs->init_fkern = init_kern_pow;
            ptfuncs->run_fkern = run_kern_pow;
            ptfuncs->cleanup_fkern = cleanup_kern_pow;
            break;
        case KERN_DGEMM:
            ptfuncs->init_fkern = init_kern_dgemm;
            ptfuncs->run_fkern = run_kern_dgemm;
            ptfuncs->cleanup_fkern = cleanup_kern_dgemm;
            break;
        case KERN_MPI_BCAST:
            ptfuncs->init_fkern = init_kern_bcast;
            ptfuncs->run_fkern = run_kern_bcast;
            ptfuncs->cleanup_fkern = cleanup_kern_bcast;
            break;
    }

    // Set rear kernel functions
    switch (ptopts->rkern) {
        case KERN_NONE:
            ptfuncs->init_rkern = init_kern_none;
            ptfuncs->run_rkern = run_kern_none;
            ptfuncs->cleanup_rkern = cleanup_kern_none;
            break;
        case KERN_TRIAD:
            ptfuncs->init_rkern = init_kern_triad;
            ptfuncs->run_rkern = run_kern_triad;
            ptfuncs->cleanup_rkern = cleanup_kern_triad;
            break;
        case KERN_SCALE:
            ptfuncs->init_rkern = init_kern_scale;
            ptfuncs->run_rkern = run_kern_scale;
            ptfuncs->cleanup_rkern = cleanup_kern_scale;
            break;
        case KERN_COPY:
            ptfuncs->init_rkern = init_kern_copy;
            ptfuncs->run_rkern = run_kern_copy;
            ptfuncs->cleanup_rkern = cleanup_kern_copy;
            break;
        case KERN_ADD:
            ptfuncs->init_rkern = init_kern_add;
            ptfuncs->run_rkern = run_kern_add;
            ptfuncs->cleanup_rkern = cleanup_kern_add;
            break;
        case KERN_POW:
            ptfuncs->init_rkern = init_kern_pow;
            ptfuncs->run_rkern = run_kern_pow;
            ptfuncs->cleanup_rkern = cleanup_kern_pow;
            break;
        case KERN_DGEMM:
            ptfuncs->init_rkern = init_kern_dgemm;
            ptfuncs->run_rkern = run_kern_dgemm;
            ptfuncs->cleanup_rkern = cleanup_kern_dgemm;
            break;
        case KERN_MPI_BCAST:
            ptfuncs->init_rkern = init_kern_bcast;
            ptfuncs->run_rkern = run_kern_bcast;
            ptfuncs->cleanup_rkern = cleanup_kern_bcast;
            break;
    }
}

int
parse_ptargs(int argc, char *argv[], pt_test_options_t *ptopts, pt_kern_func_t *ptfuncs)
{
    // Initialize with defaults
    ptopts->fsize = 0;
    ptopts->rsize = 0;
    ptopts->fkern = KERN_NONE;
    ptopts->rkern = KERN_NONE;
    ptopts->timer = TIMER_CLOCK_GETTIME;
    ptopts->ntests = 100;
    ptopts->nsub = 1000;

    // Initialize kernel functions to NULL
    ptfuncs->init_fkern = NULL;
    ptfuncs->run_fkern = NULL;
    ptfuncs->cleanup_fkern = NULL;
    ptfuncs->init_rkern = NULL;
    ptfuncs->run_rkern = NULL;
    ptfuncs->cleanup_rkern = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fkern") == 0) {
            if (i + 1 < argc) {
                if (strcmp(argv[i + 1], "none") == 0) {
                    ptopts->fkern = KERN_NONE;
                    ptfuncs->init_fkern = init_kern_none;
                    ptfuncs->run_fkern = run_kern_none;
                    ptfuncs->cleanup_fkern = cleanup_kern_none;
                } else if (strcmp(argv[i + 1], "triad") == 0) {
                    ptopts->fkern = KERN_TRIAD;
                    ptfuncs->init_fkern = init_kern_triad;
                    ptfuncs->run_fkern = run_kern_triad;
                    ptfuncs->cleanup_fkern = cleanup_kern_triad;
                } else if (strcmp(argv[i + 1], "scale") == 0) {
                    ptopts->fkern = KERN_SCALE;
                    ptfuncs->init_fkern = init_kern_scale;
                    ptfuncs->run_fkern = run_kern_scale;
                    ptfuncs->cleanup_fkern = cleanup_kern_scale;
                } else if (strcmp(argv[i + 1], "copy") == 0) {
                    ptopts->fkern = KERN_COPY;
                    ptfuncs->init_fkern = init_kern_copy;
                    ptfuncs->run_fkern = run_kern_copy;
                    ptfuncs->cleanup_fkern = cleanup_kern_copy;
                } else if (strcmp(argv[i + 1], "add") == 0) {
                    ptopts->fkern = KERN_ADD;
                    ptfuncs->init_fkern = init_kern_add;
                    ptfuncs->run_fkern = run_kern_add;
                    ptfuncs->cleanup_fkern = cleanup_kern_add;
                } else if (strcmp(argv[i + 1], "pow") == 0) {
                    ptopts->fkern = KERN_POW;
                    ptfuncs->init_fkern = init_kern_pow;
                    ptfuncs->run_fkern = run_kern_pow;
                    ptfuncs->cleanup_fkern = cleanup_kern_pow;
                } else if (strcmp(argv[i + 1], "dgemm") == 0) {
                    ptopts->fkern = KERN_DGEMM;
                    ptfuncs->init_fkern = init_kern_dgemm;
                    ptfuncs->run_fkern = run_kern_dgemm;
                    ptfuncs->cleanup_fkern = cleanup_kern_dgemm;
                } else if (strcmp(argv[i + 1], "mpi_bcast") == 0) {
                    ptopts->fkern = KERN_MPI_BCAST;
                    ptfuncs->init_fkern = init_kern_bcast;
                    ptfuncs->run_fkern = run_kern_bcast;
                    ptfuncs->cleanup_fkern = cleanup_kern_bcast;
                } else {
                    fprintf(stderr, "Unknown front kernel: %s\n", argv[i + 1]);
                    return 1;
                }
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--fsize") == 0) {
            if (i + 1 < argc) {
                ptopts->fsize = atol(argv[i + 1]);
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--rkern") == 0) {
            if (i + 1 < argc) {
                if (strcmp(argv[i + 1], "none") == 0) {
                    ptopts->rkern = KERN_NONE;
                    ptfuncs->init_rkern = init_kern_none;
                    ptfuncs->run_rkern = run_kern_none;
                    ptfuncs->cleanup_rkern = cleanup_kern_none;
                } else if (strcmp(argv[i + 1], "triad") == 0) {
                    ptopts->rkern = KERN_TRIAD;
                    ptfuncs->init_rkern = init_kern_triad;
                    ptfuncs->run_rkern = run_kern_triad;
                    ptfuncs->cleanup_rkern = cleanup_kern_triad;
                } else if (strcmp(argv[i + 1], "scale") == 0) {
                    ptopts->rkern = KERN_SCALE;
                    ptfuncs->init_rkern = init_kern_scale;
                    ptfuncs->run_rkern = run_kern_scale;
                    ptfuncs->cleanup_rkern = cleanup_kern_scale;
                } else if (strcmp(argv[i + 1], "copy") == 0) {
                    ptopts->rkern = KERN_COPY;
                    ptfuncs->init_rkern = init_kern_copy;
                    ptfuncs->run_rkern = run_kern_copy;
                    ptfuncs->cleanup_rkern = cleanup_kern_copy;
                } else if (strcmp(argv[i + 1], "add") == 0) {
                    ptopts->rkern = KERN_ADD;
                    ptfuncs->init_rkern = init_kern_add;
                    ptfuncs->run_rkern = run_kern_add;
                    ptfuncs->cleanup_rkern = cleanup_kern_add;
                } else if (strcmp(argv[i + 1], "pow") == 0) {
                    ptopts->rkern = KERN_POW;
                    ptfuncs->init_rkern = init_kern_pow;
                    ptfuncs->run_rkern = run_kern_pow;
                    ptfuncs->cleanup_rkern = cleanup_kern_pow;
                } else if (strcmp(argv[i + 1], "dgemm") == 0) {
                    ptopts->rkern = KERN_DGEMM;
                    ptfuncs->init_rkern = init_kern_dgemm;
                    ptfuncs->run_rkern = run_kern_dgemm;
                    ptfuncs->cleanup_rkern = cleanup_kern_dgemm;
                } else if (strcmp(argv[i + 1], "mpi_bcast") == 0) {
                    ptopts->rkern = KERN_MPI_BCAST;
                    ptfuncs->init_rkern = init_kern_bcast;
                    ptfuncs->run_rkern = run_kern_bcast;
                    ptfuncs->cleanup_rkern = cleanup_kern_bcast;
                } else {
                    fprintf(stderr, "Unknown rear kernel: %s\n", argv[i + 1]);
                    return 1;
                }
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--rsize") == 0) {
            if (i + 1 < argc) {
                ptopts->rsize = atol(argv[i + 1]);
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--timer") == 0) {
            if (i + 1 < argc) {
                if (strcmp(argv[i + 1], "clock_gettime") == 0) {
                    ptopts->timer = TIMER_CLOCK_GETTIME;
                } else if (strcmp(argv[i + 1], "mpi_wtime") == 0) {
                    ptopts->timer = TIMER_MPI_WTIME;
                } else {
                    fprintf(stderr, "Unknown timer: %s\n", argv[i + 1]);
                    return 1;
                }
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--ntests") == 0) {
            if (i + 1 < argc) {
                ptopts->ntests = atoi(argv[i + 1]);
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--nsub") == 0) {
            if (i + 1 < argc) {
                ptopts->nsub = atoi(argv[i + 1]);
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --fkern <kernel>    Front kernel (none, triad, scale, copy, add, pow, dgemm, mpi_bcast)\n");
            printf("  --fsize <size>      Front kernel memory size in KiB\n");
            printf("  --rkern <kernel>    Rear kernel (none, triad, scale, copy, add, pow, dgemm, mpi_bcast)\n");
            printf("  --rsize <size>      Rear kernel memory size in KiB\n");
            printf("  --timer <timer>     Timer method (clock_gettime, mpi_wtime)\n");
            printf("  --ntests <num>      Number of gauge measurements (default: 100)\n");
            printf("  --nsub <num>        Number of subtractions per gauge (default: 1000)\n");
            printf("  --help, -h          Show this help message\n");
            return PTERR_EXIT_FLAG;
        }
    }

    return PTERR_SUCCESS;
}