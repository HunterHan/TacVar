/**
 * @file pterr.c
 * @brief: Error codes for partes.
 */
#include "pterr.h"

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



