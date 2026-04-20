/**
 * @file papi_timer.c
 * @brief PAPI timer: PAPI_get_real_nsec() (ns).
 *
 * Enabled only when built with -DUSE_PAPI.
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "timers.h"
#include "../pterr.h"

#ifdef USE_PAPI

#include <papi.h>

#ifdef PTOPT_USE_MPI
#include <mpi.h>
static int _pt_rank(void) {
    int r = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &r);
    return r;
}
#else
static int _pt_rank(void) { return 0; }
#endif

static int g_inited = 0;

static int _papi_fail(const char *where, int ret) {
    fprintf(stderr, "[ERROR][Rank %d] PAPI timer init failed at %s: ret=%d (%s)\n",
            _pt_rank(), where, ret, PAPI_strerror(ret));
    return PTERR_TIMER_INIT_FAILED;
}

int init_timer_papi(void) {
    if (g_inited) return PTERR_SUCCESS;

    int ret = PAPI_library_init(PAPI_VER_CURRENT);
    if (ret != PAPI_VER_CURRENT) return _papi_fail("PAPI_library_init", ret);

    // We only use PAPI's wall clock in ns (PAPI_get_real_nsec()).
    // Do not create/start an EventSet here: starting an empty EventSet can fail
    // (e.g. "Component Index isn't set") depending on the PAPI component setup.

    g_inited = 1;
    return PTERR_SUCCESS;
}

int64_t tick_papi(void) { return (int64_t)PAPI_get_real_nsec(); }
int64_t tock_papi(void) { return (int64_t)PAPI_get_real_nsec(); }
int64_t get_stamp_papi(void) { return (int64_t)PAPI_get_real_nsec(); }

#endif  // USE_PAPI

