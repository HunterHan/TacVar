/**
 * @file pterr.h
 * @brief: Error codes for partes.
 */

#ifndef _PTM_H
#define _PTM_H

enum pterr {
    PTERR_SUCCESS = 0,
    PTERR_TIMER_NEGATIVE = 1,
    PTERR_TIMER_OVERFLOW = 2,
    PTERR_EXIT_FLAG = 3,
    PTERR_MALLOC_FAILED = 4,
    PTERR_INVALID_ARGUMENT = 5,
    PTERR_MISSING_ARGUMENT = 6,
    PTERR_FILE_OPEN_FAILED = 7
};

const char *get_pterr_str(enum pterr err);
void pt_mpi_printf(int myrank, int nrank, const char *format, ...);

#define _ptm_return_on_error(err, fname) \
    if (err != PTERR_SUCCESS) {                             \
        printf("[ERROR] In %s, %s\n", fname, get_pterr_str(err));   \
        return err;                                         \
    }

#define _ptm_exit_on_error(err, fname) \
    if (err != PTERR_SUCCESS) {                             \
        printf("[ERROR] In %s, %s\n", fname, get_pterr_str(err));   \
        goto EXIT;                                         \
    }

#define _ptm_print_error(err, fname) \
    if (err != PTERR_SUCCESS) {                             \
        printf("[ERROR] In %s, %s\n", fname, get_pterr_str(err));   \
    }

#define _ptm_print_warning(err, fname) \
    if (err != PTERR_SUCCESS) {                             \
        printf("[WARN] In %s: %s\n", fname, get_pterr_str(err));   \
    }

#define _ptm_print_info(err, fname) \
    if (err != PTERR_SUCCESS) {                             \
        printf("[INFO] In %s: %s\n", fname, get_pterr_str(err));   \
    }

#define _ptm_print_debug(err, fname) \
    if (err != PTERR_SUCCESS) {                             \
        printf("[DEBUG] In %s: %s\n", fname, get_pterr_str(err));   \
    }

#define _ptm_return_on_error_mpi(err, fname, myrank) \
    if (err != PTERR_SUCCESS) {                             \
        printf("[ERROR][Rank %d] In %s: %s\n", myrank, fname, get_pterr_str(err));   \
        return err;                                         \
    }

#define _ptm_exit_on_error_mpi(err, fname, myrank) \
    if (err != PTERR_SUCCESS) {                             \
        printf("[ERROR][Rank %d] In %s: %s\n", myrank, fname, get_pterr_str(err));   \
        goto EXIT;                                         \
    }

#define _ptm_print_error_mpi(err, fname, myrank) \
    if (err != PTERR_SUCCESS) {                             \
        printf("[ERROR][Rank %d] In %s: %s\n", myrank, fname, get_pterr_str(err));   \
    }

#define _ptm_print_warning_mpi(err, fname, myrank) \
    if (err != PTERR_SUCCESS) {                             \
        printf("[WARN][Rank %d] In %s: %s\n", myrank, fname, get_pterr_str(err));   \
    }

#define _ptm_print_info_mpi(err, fname, myrank) \
    if (err != PTERR_SUCCESS) {                             \
        printf("[INFO][Rank %d] In %s: %s\n", myrank, fname, get_pterr_str(err));   \
    }

#define _ptm_print_debug_mpi(err, fname, myrank) \
    if (err != PTERR_SUCCESS) {                             \
        printf("[DEBUG][Rank %d] In %s: %s\n", myrank, fname, get_pterr_str(err));   \
    }

#endif
