#ifndef BF_EXPORT_RUNTIME_H
#define BF_EXPORT_RUNTIME_H

#include "bf_io.h"

int bfpe_run_int_program(
    const char* code,
    bf_io_mode_t mode,
    const int* args,
    int arg_count
);
const char* bfpe_run_string_program(const char* code, bf_io_mode_t mode);
void bfpe_run_void_program(const char* code, bf_io_mode_t mode);
const char* bfdll_run_output_program(const char* code);

#endif
