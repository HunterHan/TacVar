/**
 * @file papix6_timer.c
 * @brief PAPIx6 timer: ns timestamp + 6 event reads (simulated heavier timer).
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
static int g_eventset = PAPI_NULL;
static long long g_ev_vals[6];
static int g_started = 0;

static int _papi_fail(const char *where, int ret) {
    fprintf(stderr, "[ERROR][Rank %d] PAPIx6 timer init failed at %s: ret=%d (%s)\n",
            _pt_rank(), where, ret, PAPI_strerror(ret));
    return PTERR_TIMER_INIT_FAILED;
}

static void _papi_warn(const char *where, int ret) {
    fprintf(stderr, "[WARN][Rank %d] PAPIx6 timer: %s: ret=%d (%s) — disabling event reads\n",
            _pt_rank(), where, ret, PAPI_strerror(ret));
}

static int _try_add_event_candidates(int eventset, const char *label, const int *codes, int ncode,
                                     const char *const *names, int nname) {
    int ret;
    for (int i = 0; i < ncode; i++) {
        if (codes[i] == 0) continue;
        ret = PAPI_add_event(eventset, codes[i]);
        if (ret == PAPI_OK) return PAPI_OK;
    }
    for (int i = 0; i < nname; i++) {
        if (!names[i] || !names[i][0]) continue;
        ret = PAPI_add_named_event(eventset, (char *)names[i]);
        if (ret == PAPI_OK) return PAPI_OK;
    }
    // Return the last failure code if any candidate was attempted.
    // If nothing was attempted (shouldn't happen), treat as not supported.
    (void)label;
    return ret;
}

int init_timer_papix6(void) {
    if (g_inited) return PTERR_SUCCESS;

    int ret = PAPI_library_init(PAPI_VER_CURRENT);
    if (ret != PAPI_VER_CURRENT) return _papi_fail("PAPI_library_init", ret);

    g_eventset = PAPI_NULL;
    ret = PAPI_create_eventset(&g_eventset);
    if (ret != PAPI_OK) return _papi_fail("PAPI_create_eventset", ret);

    // Add up to 6 "typical" events, but be portable:
    // - On some platforms (e.g. ARM/Kunpeng), Linux perf named aliases like "cpu-cycles"
    //   may not be supported by the active PAPI component.
    // - Prefer PAPI preset events when possible; otherwise try common named aliases.
    //
    // If we fail to add/start any events, we still keep the timer usable (ns only),
    // and simply disable the extra PAPI_read overhead to avoid init-time MPI_Abort.
    int added = 0;

    {
        const int codes[] = {PAPI_TOT_CYC};
        const char *const names[] = {"PAPI_TOT_CYC", "cpu-cycles"};
        ret = _try_add_event_candidates(g_eventset, "cycles", codes, 1, names, 2);
        if (ret == PAPI_OK) added++;
    }
    {
        const int codes[] = {PAPI_TOT_INS};
        const char *const names[] = {"PAPI_TOT_INS", "instructions"};
        ret = _try_add_event_candidates(g_eventset, "instructions", codes, 1, names, 2);
        if (ret == PAPI_OK) added++;
    }
    {
        const int codes[] = {PAPI_L1_DCM};
        const char *const names[] = {"PAPI_L1_DCM", "cache-misses"};
        ret = _try_add_event_candidates(g_eventset, "l1-dcache-misses", codes, 1, names, 2);
        if (ret == PAPI_OK) added++;
    }
    {
        const int codes[] = {PAPI_L1_DCA};
        const char *const names[] = {"PAPI_L1_DCA", "cache-references"};
        ret = _try_add_event_candidates(g_eventset, "l1-dcache-accesses", codes, 1, names, 2);
        if (ret == PAPI_OK) added++;
    }
    {
        const int codes[] = {PAPI_BR_INS};
        const char *const names[] = {"PAPI_BR_INS", "branches"};
        ret = _try_add_event_candidates(g_eventset, "branches", codes, 1, names, 2);
        if (ret == PAPI_OK) added++;
    }
    {
        const int codes[] = {PAPI_BR_MSP};
        const char *const names[] = {"PAPI_BR_MSP", "branch-misses"};
        ret = _try_add_event_candidates(g_eventset, "branch-misses", codes, 1, names, 2);
        if (ret == PAPI_OK) added++;
    }

    if (added > 0) {
        ret = PAPI_start(g_eventset);
        if (ret != PAPI_OK) {
            _papi_warn("PAPI_start", ret);
            g_started = 0;
        } else {
            // Prime first read (helps avoid first-use overhead in the measurement loop).
            ret = PAPI_read(g_eventset, g_ev_vals);
            if (ret != PAPI_OK) {
                _papi_warn("PAPI_read(prime)", ret);
                g_started = 0;
            } else {
                g_started = 1;
            }
        }
    } else {
        _papi_warn("no supported events added", PAPI_ENOEVNT);
        g_started = 0;
    }

    g_inited = 1;
    return PTERR_SUCCESS;
}

int64_t tick_papix6(void) {
    // Timestamp + event read to model overhead/variability.
    if (g_started) (void)PAPI_read(g_eventset, g_ev_vals);
    return (int64_t)PAPI_get_real_nsec();
}

int64_t tock_papix6(void) {
    if (g_started) (void)PAPI_read(g_eventset, g_ev_vals);
    return (int64_t)PAPI_get_real_nsec();
}

int64_t get_stamp_papix6(void) {
    if (g_started) (void)PAPI_read(g_eventset, g_ev_vals);
    return (int64_t)PAPI_get_real_nsec();
}

#endif  // USE_PAPI

