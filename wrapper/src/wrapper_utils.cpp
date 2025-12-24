#include "cuda_ro_internal.h"
#include <cstdio>
#include <cstdarg>

void log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stdout, "[CUDA-RO-WRAPPER] ");
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(args);
}

void log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[CUDA-RO-WRAPPER ERROR] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(args);
}
