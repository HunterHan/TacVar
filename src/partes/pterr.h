/**
 * @file pterr.h
 * @brief: Error codes for partes.
 */

enum pterr {
    PTERR_SUCCESS = 0,
    PTERR_TIMER_NEGATIVE = 1,
    PTERR_TIMER_OVERFLOW = 2,
    PTERR_EXIT_FLAG = 3,
    PTERR_MALLOC_FAILED = 4,
    PTERR_INVALID_ARGUMENT = 5
};

const char *get_pterr_str(enum pterr err);