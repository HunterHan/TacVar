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
#include "kernels/kernels.h"
#include "timers/timers.h"
#include "gauges/gauges.h"
#include "pterr.h"

#ifdef PTOPT_USE_MPI

#include <mpi.h>

#endif

void
print_usage(char *argv[])
{
    int myrank = 0;
#ifdef PTOPT_USE_MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
#endif
    if (myrank == 0) {
        printf("Usage: %s [options]\n", argv[0]);
        printf("Mandatory options:\n");
        printf("  --ta <ns>           Target gauge time ta in nanoseconds\n");
        printf("  --tb <ns>           Target gauge time tb in nanoseconds\n");
        printf("Options:\n");
        printf("  --ntiles <num>      Number of tiles (default: 100)\n");
        printf("  --cut-p <p>         p in (0.0, 1.0), cut deviation after p for W calculation (default: 1.0)\n");
        printf("  --fkern <kernel>    Front kernel (none, triad, scale, copy, add, pow, dgemm, mpi_bcast)\n");
        printf("  --fsize-a <size>    The memory size of ta's fkern in KiB\n");
        printf("  --fsize-b <size>    The memory size of tb's fkern in KiB\n");
        printf("  --rkern <kernel>    Rear kernel (none, triad, scale, copy, add, pow, dgemm, mpi_bcast)\n");
        printf("  --rsize-a <size>    The memory size of ta's rkern in KiB\n");
        printf("  --rsize-b <size>    The memory size of tb's rkern in KiB\n");
        printf("  --timer <timer>     Timer method (clock_gettime, mpi_wtime, tsc_asym)\n");
        printf("  --gauge <gauge>     Gauge method (sub_scalar, fma_scalar, fma_avx2, fma_avx512)\n");
        printf("  --ntests <num>      Number of gauge measurements (default: 1000)\n");
        printf("  --help, -h          Show this help message\n");
    }
}

int
parse_ptargs(int argc, char *argv[], pt_opts_t *ptopts, pt_kern_func_t *ptfuncs, pt_timer_func_t *pttimers, pt_gauge_func_t *ptgauges)
{
    int myrank = 0;
#ifdef PTOPT_USE_MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
#endif
    // Initialize with defaults
    ptopts->fsize_a = 0;
    ptopts->rsize_a = 0;
    ptopts->fsize_b = 0;
    ptopts->rsize_b = 0;
    ptopts->fsize_real_a = 0;
    ptopts->rsize_real_a = 0;
    ptopts->fsize_real_b = 0;
    ptopts->rsize_real_b = 0;
    ptopts->fkern = KERN_NONE;
    ptopts->rkern = KERN_NONE;
    ptopts->timer = TIMER_CLOCK_GETTIME;
    ptopts->gauge = GAUGE_SUB_SCALAR;
    ptopts->ntests = 1000;
    ptopts->ntiles = 100;
    ptopts->cut_p = 1.0;
    ptopts->ta = INT64_MIN;
    ptopts->tb = INT64_MIN;

    // Initialize kernel functions to NULL
    strcpy(ptopts->fkern_name, "NONE");
    strcpy(ptopts->rkern_name, "NONE");
    ptfuncs->init_fkern = init_kern_none;
    ptfuncs->run_fkern = run_kern_none;
    ptfuncs->cleanup_fkern = cleanup_kern_none;
    ptfuncs->init_rkern = init_kern_none;
    ptfuncs->run_rkern = run_kern_none;
    ptfuncs->cleanup_rkern = cleanup_kern_none;
    ptfuncs->update_fkern_key = update_key_none;
    ptfuncs->update_rkern_key = update_key_none;
    ptfuncs->check_fkern_key = check_key_none;
    ptfuncs->check_rkern_key = check_key_none;
    
    // Initialize timer functions to clock_gettime
    strcpy(ptopts->timer_name, "clock_gettime");
    pttimers->init_timer = init_timer_clock_gettime;
    pttimers->tick = tick_clock_gettime;
    pttimers->tock = tock_clock_gettime;
    pttimers->get_stamp = get_stamp_clock_gettime;
    
    // Initialize gauge functions to sub_intrinsic (default macro-based)
    strcpy(ptopts->gauge_name, "sub_scalar");
    ptgauges->init_gauge = init_gauge_sub_scalar;
    ptgauges->run_gauge = run_gauge_sub_scalar;
    ptgauges->cleanup_gauge = cleanup_gauge_sub_scalar;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ta") == 0) {
            if (i + 1 < argc) {
                ptopts->ta = atol(argv[i + 1]);
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--tb") == 0) {
            if (i + 1 < argc) {
                ptopts->tb = atol(argv[i + 1]);
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--fkern") == 0) {
            if (i + 1 < argc) {
                if (strcmp(argv[i + 1], "none") == 0) {
                    ptopts->fkern = KERN_NONE;
                    ptfuncs->init_fkern = init_kern_none;
                    ptfuncs->run_fkern = run_kern_none;
                    ptfuncs->update_fkern_key = update_key_none;
                    ptfuncs->check_fkern_key = check_key_none;
                    ptfuncs->cleanup_fkern = cleanup_kern_none;
                    strcpy(ptopts->fkern_name, "none");
                } else if (strcmp(argv[i + 1], "triad") == 0) {
                    ptopts->fkern = KERN_TRIAD;
                    ptfuncs->init_fkern = init_kern_triad;
                    ptfuncs->run_fkern = run_kern_triad;
                    ptfuncs->update_fkern_key = update_key_triad;
                    ptfuncs->check_fkern_key = check_key_triad;
                    ptfuncs->cleanup_fkern = cleanup_kern_triad;
                    strcpy(ptopts->fkern_name, "triad");
                } else if (strcmp(argv[i + 1], "scale") == 0) {
                    ptopts->fkern = KERN_SCALE;
                    ptfuncs->init_fkern = init_kern_scale;
                    ptfuncs->run_fkern = run_kern_scale;
                    ptfuncs->update_fkern_key = update_key_scale;
                    ptfuncs->check_fkern_key = check_key_scale;
                    ptfuncs->cleanup_fkern = cleanup_kern_scale;
                    strcpy(ptopts->fkern_name, "scale");
                } else if (strcmp(argv[i + 1], "copy") == 0) {
                    ptopts->fkern = KERN_COPY;
                    ptfuncs->init_fkern = init_kern_copy;
                    ptfuncs->run_fkern = run_kern_copy;
                    ptfuncs->update_fkern_key = update_key_copy;
                    ptfuncs->check_fkern_key = check_key_copy;
                    ptfuncs->cleanup_fkern = cleanup_kern_copy;
                    strcpy(ptopts->fkern_name, "copy");
                } else if (strcmp(argv[i + 1], "add") == 0) {
                    ptopts->fkern = KERN_ADD;
                    ptfuncs->init_fkern = init_kern_add;
                    ptfuncs->run_fkern = run_kern_add;
                    ptfuncs->update_fkern_key = update_key_add;
                    ptfuncs->check_fkern_key = check_key_add;
                    ptfuncs->cleanup_fkern = cleanup_kern_add;
                    strcpy(ptopts->fkern_name, "add");
                } else if (strcmp(argv[i + 1], "pow") == 0) {
                    ptopts->fkern = KERN_POW;
                    ptfuncs->init_fkern = init_kern_pow;
                    ptfuncs->run_fkern = run_kern_pow;
                    ptfuncs->update_fkern_key = update_key_pow;
                    ptfuncs->check_fkern_key = check_key_pow;
                    ptfuncs->cleanup_fkern = cleanup_kern_pow;
                    strcpy(ptopts->fkern_name, "pow");
                } else if (strcmp(argv[i + 1], "dgemm") == 0) {
                    ptopts->fkern = KERN_DGEMM;
                    ptfuncs->init_fkern = init_kern_dgemm;
                    ptfuncs->run_fkern = run_kern_dgemm;
                    ptfuncs->update_fkern_key = update_key_dgemm;
                    ptfuncs->check_fkern_key = check_key_dgemm;
                    ptfuncs->cleanup_fkern = cleanup_kern_dgemm;
                    strcpy(ptopts->fkern_name, "dgemm");
                } else if (strcmp(argv[i + 1], "mpi_bcast") == 0) {
                    ptopts->fkern = KERN_MPI_BCAST;
                    ptfuncs->init_fkern = init_kern_mpi_bcast;
                    ptfuncs->run_fkern = run_kern_mpi_bcast;
                    ptfuncs->update_fkern_key = update_key_mpi_bcast;
                    ptfuncs->check_fkern_key = check_key_mpi_bcast;
                    ptfuncs->cleanup_fkern = cleanup_kern_mpi_bcast;
                    strcpy(ptopts->fkern_name, "mpi_bcast");
                } else {
                    fprintf(stderr, "Unknown front kernel: %s\n", argv[i + 1]);
                    return PTERR_INVALID_ARGUMENT;
                }
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--rkern") == 0) {
            if (i + 1 < argc) {
                if (strcmp(argv[i + 1], "none") == 0) {
                    ptopts->rkern = KERN_NONE;
                    ptfuncs->init_rkern = init_kern_none;
                    ptfuncs->run_rkern = run_kern_none;
                    ptfuncs->update_rkern_key = update_key_none;
                    ptfuncs->check_rkern_key = check_key_none;
                    ptfuncs->cleanup_rkern = cleanup_kern_none;
                    strcpy(ptopts->rkern_name, "none");
                } else if (strcmp(argv[i + 1], "triad") == 0) {
                    ptopts->rkern = KERN_TRIAD;
                    ptfuncs->init_rkern = init_kern_triad;
                    ptfuncs->run_rkern = run_kern_triad;
                    ptfuncs->update_rkern_key = update_key_triad;
                    ptfuncs->check_rkern_key = check_key_triad;
                    ptfuncs->cleanup_rkern = cleanup_kern_triad;
                    strcpy(ptopts->rkern_name, "triad");
                } else if (strcmp(argv[i + 1], "scale") == 0) {
                    ptopts->rkern = KERN_SCALE;
                    ptfuncs->init_rkern = init_kern_scale;
                    ptfuncs->run_rkern = run_kern_scale;
                    ptfuncs->update_rkern_key = update_key_scale;
                    ptfuncs->check_rkern_key = check_key_scale;
                    ptfuncs->cleanup_rkern = cleanup_kern_scale;
                    strcpy(ptopts->rkern_name, "scale");
                } else if (strcmp(argv[i + 1], "copy") == 0) {
                    ptopts->rkern = KERN_COPY;
                    ptfuncs->init_rkern = init_kern_copy;
                    ptfuncs->run_rkern = run_kern_copy;
                    ptfuncs->update_rkern_key = update_key_copy;
                    ptfuncs->check_rkern_key = check_key_copy;
                    ptfuncs->cleanup_rkern = cleanup_kern_copy;
                    strcpy(ptopts->rkern_name, "copy");
                } else if (strcmp(argv[i + 1], "add") == 0) {
                    ptopts->rkern = KERN_ADD;
                    ptfuncs->init_rkern = init_kern_add;
                    ptfuncs->run_rkern = run_kern_add;
                    ptfuncs->update_rkern_key = update_key_add;
                    ptfuncs->check_rkern_key = check_key_add;
                    ptfuncs->cleanup_rkern = cleanup_kern_add;
                    strcpy(ptopts->rkern_name, "add");
                } else if (strcmp(argv[i + 1], "pow") == 0) {
                    ptopts->rkern = KERN_POW;
                    ptfuncs->init_rkern = init_kern_pow;
                    ptfuncs->run_rkern = run_kern_pow;
                    ptfuncs->update_rkern_key = update_key_pow;
                    ptfuncs->check_rkern_key = check_key_pow;
                    ptfuncs->cleanup_rkern = cleanup_kern_pow;
                    strcpy(ptopts->rkern_name, "pow");
                } else if (strcmp(argv[i + 1], "dgemm") == 0) {
                    ptopts->rkern = KERN_DGEMM;
                    ptfuncs->init_rkern = init_kern_dgemm;
                    ptfuncs->run_rkern = run_kern_dgemm;
                    ptfuncs->update_rkern_key = update_key_dgemm;
                    ptfuncs->check_rkern_key = check_key_dgemm;
                    ptfuncs->cleanup_rkern = cleanup_kern_dgemm;
                    strcpy(ptopts->rkern_name, "dgemm");
                } else if (strcmp(argv[i + 1], "mpi_bcast") == 0) {
                    ptopts->rkern = KERN_MPI_BCAST;
                    ptfuncs->init_rkern = init_kern_mpi_bcast;
                    ptfuncs->run_rkern = run_kern_mpi_bcast;
                    ptfuncs->update_rkern_key = update_key_mpi_bcast;
                    ptfuncs->check_rkern_key = check_key_mpi_bcast;
                    ptfuncs->cleanup_rkern = cleanup_kern_mpi_bcast;
                    strcpy(ptopts->rkern_name, "mpi_bcast");
                } else {
                    fprintf(stderr, "Unknown rear kernel: %s\n", argv[i + 1]);
                    return PTERR_INVALID_ARGUMENT;
                }
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--fsize-a") == 0) {
            if (i + 1 < argc) {
                ptopts->fsize_a = atol(argv[i + 1]);
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--rsize-a") == 0) {
            if (i + 1 < argc) {
                ptopts->rsize_a = atol(argv[i + 1]);
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--fsize-b") == 0) {
            if (i + 1 < argc) {
                ptopts->fsize_b = atol(argv[i + 1]);
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--rsize-b") == 0) {
            if (i + 1 < argc) {
                ptopts->rsize_b = atol(argv[i + 1]);
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--timer") == 0) {
            if (i + 1 < argc) {
                if (strcmp(argv[i + 1], "clock_gettime") == 0) {
                    ptopts->timer = TIMER_CLOCK_GETTIME;
                    pttimers->init_timer = init_timer_clock_gettime;
                    pttimers->tick = tick_clock_gettime;
                    pttimers->tock = tock_clock_gettime;
                    pttimers->get_stamp = get_stamp_clock_gettime;
                    strcpy(ptopts->timer_name, "clock_gettime");
                } else if (strcmp(argv[i + 1], "mpi_wtime") == 0) {
                    ptopts->timer = TIMER_MPI_WTIME;
                    pttimers->init_timer = init_timer_mpi_wtime;
                    pttimers->tick = tick_mpi_wtime;
                    pttimers->tock = tock_mpi_wtime;
                    pttimers->get_stamp = get_stamp_mpi_wtime;
                    strcpy(ptopts->timer_name, "mpi_wtime");
                }  else if (strcmp(argv[i + 1], "tsc_asym") == 0) {
                    ptopts->timer = TIMER_TSC_ASYM;
                    pttimers->init_timer = init_timer_tsc_asym;
                    pttimers->tick = tick_tsc_asym;
                    pttimers->tock = tock_tsc_asym;
                    pttimers->get_stamp = get_stamp_tsc_asym;
                    strcpy(ptopts->timer_name, "tsc_asym");
                }   else {
                    fprintf(stderr, "Unknown timer: %s\n", argv[i + 1]);
                    return PTERR_INVALID_ARGUMENT;
                }
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--gauge") == 0) {
            if (i + 1 < argc) {
                if (strcmp(argv[i + 1], "sub_scalar") == 0) {
                    ptopts->gauge = GAUGE_SUB_SCALAR;
                    ptgauges->init_gauge = init_gauge_sub_scalar;
                    ptgauges->run_gauge = run_gauge_sub_scalar;
                    ptgauges->cleanup_gauge = cleanup_gauge_sub_scalar;
                    strcpy(ptopts->gauge_name, "sub_scalar");
                } else if (strcmp(argv[i + 1], "fma_scalar") == 0) {
#if defined(__x86_64__)
                    ptopts->gauge = GAUGE_FMA_SCALAR;
                    ptgauges->init_gauge = init_gauge_fma_scalar;
                    ptgauges->run_gauge = run_gauge_fma_scalar;
                    ptgauges->cleanup_gauge = cleanup_gauge_fma_scalar;
                    strcpy(ptopts->gauge_name, "fma_scalar");
#else
                    fprintf(stderr, "Unknown gauge: %s\n", argv[i + 1]);
                    return PTERR_INVALID_ARGUMENT;
#endif
                } else if (strcmp(argv[i + 1], "fma_avx2") == 0) {
#if defined(__x86_64__)
                    ptopts->gauge = GAUGE_FMA_AVX2;
                    ptgauges->init_gauge = init_gauge_fma_avx2;
                    ptgauges->run_gauge = run_gauge_fma_avx2;
                    ptgauges->cleanup_gauge = cleanup_gauge_fma_avx2;
                    strcpy(ptopts->gauge_name, "fma_avx2");
#else
                    fprintf(stderr, "Unknown gauge: %s\n", argv[i + 1]);
                    return PTERR_INVALID_ARGUMENT;
#endif
                } else if (strcmp(argv[i + 1], "fma_avx512") == 0) {
#if defined(__x86_64__)
                    ptopts->gauge = GAUGE_FMA_AVX512;
                    ptgauges->init_gauge = init_gauge_fma_avx512;
                    ptgauges->run_gauge = run_gauge_fma_avx512;
                    ptgauges->cleanup_gauge = cleanup_gauge_fma_avx512;
                    strcpy(ptopts->gauge_name, "fma_avx512");
#else
                    fprintf(stderr, "Unknown gauge: %s\n", argv[i + 1]);
                    return PTERR_INVALID_ARGUMENT;
#endif
                } else {
                    fprintf(stderr, "Unknown gauge: %s\n", argv[i + 1]);
                    return PTERR_INVALID_ARGUMENT;
                }
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--ntests") == 0) {
            if (i + 1 < argc) {
                ptopts->ntests = atoi(argv[i + 1]);
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--ntiles") == 0) {
            if (i + 1 < argc) {
                ptopts->ntiles = atoi(argv[i + 1]);
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--cut-p") == 0) {
            if (i + 1 < argc) {
                ptopts->cut_p = atof(argv[i + 1]);
                i++; // Skip the next argument
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv);
            return PTERR_EXIT_FLAG;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return PTERR_INVALID_ARGUMENT;
        }
    }

    // Check ta, tb
    if (ptopts->ta == INT64_MIN || ptopts->tb == INT64_MIN) {
        print_usage(argv);
        return PTERR_MISSING_ARGUMENT;
    } else if (ptopts->ta <= 0 || ptopts->tb <= 0) {
        print_usage(argv);
        return PTERR_INVALID_ARGUMENT;
    } else if (ptopts->ta > ptopts->tb) {
        print_usage(argv);
        return PTERR_INVALID_ARGUMENT;
    }

    if (ptopts->cut_p < 0.0 || ptopts->cut_p > 1.0) {
        if (myrank == 0) {
            fprintf(stderr, "Error: cut_p must be (0.0, 1.0]\n");
        }
        return PTERR_INVALID_ARGUMENT;
    }

    return PTERR_SUCCESS;
}