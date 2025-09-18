/**
 * @file timer_model_fit.c
 * @brief: 
 */

#include <stdint.h>
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../gauges/sub.h"
#include "../partes_types.h"
#include "../pterr.h"
#include "../timers/timers.h"

#ifdef PTOPT_USE_MPI
#include <mpi.h>
#endif

int myrank, nrank;

typedef struct {
    int64_t tmax;
    char timer_name[256];
} argopts_t;


static inline int64_t
_run_nsub(int nsub, pt_timer_func_t *pttimers)
{
    int64_t t0 = pttimers->tick();
    __gauge_sub_intrinsic(nsub);
    int64_t t1 = pttimers->tock();
    return t1 - t0;
}

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
        printf("  --timer <timer_name>  Timer to test.\n");
        printf("  --tmax <ns>           Largest timing interval.\n");
        printf("Optional:\n");
        printf("  --help, -h            Show this help message\n");
    }
}

int
parse_args(int argc, char *argv[], argopts_t *opts, pt_timer_func_t *pttimers)
{
    int err = PTERR_SUCCESS;
    opts->tmax = -1;
    strcpy(opts->timer_name, "\0");

    for (int i = 1; i < argc; i ++) {
        if (strcmp(argv[i], "--tmax") == 0 && i+1 < argc) {
            opts->tmax = atoll(argv[i+1]);
            i ++;
        } else if (strcmp(argv[i], "--timer") == 0 && i+1 < argc) {
            strcpy(opts->timer_name, argv[i+1]);
            if (strcmp(opts->timer_name, "clock_gettime") == 0) {
                pttimers->init_timer = init_timer_clock_gettime;
                pttimers->tick = tick_clock_gettime;
                pttimers->tock = tock_clock_gettime;
            } else if (strcmp(opts->timer_name, "mpi_wtime") == 0) {
                pttimers->init_timer = init_timer_mpi_wtime;
                pttimers->tick = tick_mpi_wtime;
                pttimers->tock = tock_mpi_wtime;
            } else {
                printf("[Error] Unknown timer: %s\n", opts->timer_name);
                return PTERR_EXIT_FLAG;
            }
            i ++;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv);
            return PTERR_EXIT_FLAG;
        }
    }

    if (opts->tmax == -1) {
        printf("[Error] --tmax is required\n");
        print_usage(argv);
        return PTERR_MISSING_ARGUMENT;
    }

    if (strcmp(opts->timer_name, "\0") == 0) {
        printf("[Error] --timer is required\n");
        print_usage(argv);
        return PTERR_MISSING_ARGUMENT;
    }

    return err;
}


int 
timer_model_fit(int ntest, int64_t tmax, pt_timer_func_t *pttimers) {
    int err = PTERR_SUCCESS;
    int64_t ng, n, tpre;
    int64_t p_tm_min[64], p_tm_raw[64][ntest];
    int64_t tmin;

    ng = 1;
    n = 0;
    tpre = 0;
    while (tpre <= tmax && n < 64) {
        for (int i = 0; i < ntest; i++) {
            p_tm_raw[n][i] = _run_nsub(ng, pttimers);
        }
        tmin = p_tm_raw[n][0];
        for (int i = 0; i < ntest; i++) {
            tmin = p_tm_raw[n][i] < tmin ? p_tm_raw[n][i] : tmin;
        }
        p_tm_min[n] = tmin;
        ng *= 2;
        tpre = tmin * 2;
        n++;
        printf("n=%" PRIi64 ", tpre=%" PRIi64 ", tmin=%" PRIi64 "\n", n, tpre, tmin);
    }

    /* From nmax, nmax-1 to 0, calculate R square to model t[i] = 2t[i-1] */
    printf("ng\t\tR^2\t\tGP/ns\n");
    int istep = 1;
    int ist = n - istep - 1;
    while (ist >= 0) {
        double mean_t = 0, sum_res = 0, sum_tot = 0, t_model = p_tm_min[ist];
        double rsquare = 0;
        for (int i = ist; i < n; i++) {
            mean_t += p_tm_min[i];
        }
        mean_t /= (istep + 1);
        for (int i = ist; i < n; i++) {
            sum_res += (p_tm_min[i] - t_model) * (p_tm_min[i] - t_model);
            sum_tot += (p_tm_min[i] - mean_t) * (p_tm_min[i] - mean_t);
            t_model = t_model * 2;
        }
        rsquare = 1 - sum_res / sum_tot;
        double gpns = (pow(2, ist+1) - pow(2, ist)) / (p_tm_min[ist+1] - p_tm_min[ist]);
        printf("2^%d:%" PRIi64 "\t\t%f\t\t%f\n", ist, p_tm_min[ist], rsquare, gpns);
        istep += 1;
        ist = n - istep - 1;
    }

    /* Write raw results to file */
    char filename[256];
    snprintf(filename, sizeof(filename), "timer_model_fit_r%d.csv", myrank);
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Rank %d: failed to open %s for writing\n", myrank, filename);
        err = PTERR_FILE_OPEN_FAILED;
        goto EXIT;
    }
    fprintf(fp, "iloop,");
    ng = 1;
    for (int i = 0; i < n; i++) {
        fprintf(fp, "%" PRIi64 ",", ng);
        ng *= 2;
    }
    fprintf(fp, "\n");
    for (int i = 0; i < ntest; i++) {
        fprintf(fp, "%d,", i);
        for (int j = 0; j < n; j++) {
            fprintf(fp, "%" PRIi64 ",", p_tm_raw[j][i]);
        }
        fprintf(fp, "\n");
    }
        fclose(fp);

EXIT:
    return err;
}

int
main(int argc, char *argv[])
{
    int err = PTERR_SUCCESS;
    argopts_t opts;
    pt_timer_func_t pttimers;

    myrank = 0;
    nrank = 1;
#ifdef PTOPT_USE_MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
#endif

    err = parse_args(argc, argv, &opts, &pttimers);
    if (err == PTERR_EXIT_FLAG) {
        print_usage(argv);
        goto EXIT;
    }
    if (err != PTERR_SUCCESS) {
        printf("[Error] Rank %d: parse_args failed: %d\n", myrank, err);
        goto EXIT;
    }
    if (myrank == 0) {
        printf("Timer: %s, tmax=%" PRIi64 "ns\n", opts.timer_name, opts.tmax);
    }
    err = timer_model_fit(100, opts.tmax, &pttimers);
EXIT:
#ifdef PTOPT_USE_MPI
    MPI_Finalize();
#endif
    return err;
}