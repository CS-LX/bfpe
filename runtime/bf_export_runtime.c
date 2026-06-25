#include "bf_export_runtime.h"

#include <string.h>

#include "bf_abi.h"

static __declspec(thread) uint8_t g_tape[BFDLL_TAPE_SIZE];

static bf_status_t bfpe_run_tape(const char* code, bf_io_mode_t mode)
{
    bf_vm_t vm = bf_vm_default(g_tape);

    memset(g_tape, 0, sizeof(g_tape));
    bf_io_bind_mode(mode);
    bf_io_reset();
    return bf_vm_run(&vm, code);
}

int bfpe_run_int_program(
    const char* code,
    bf_io_mode_t mode,
    const int* args,
    int arg_count
)
{
    bf_vm_t vm = bf_vm_default(g_tape);
    int i = 0;

    memset(g_tape, 0, sizeof(g_tape));
    for (i = 0; i < arg_count; ++i) {
        g_tape[i] = (uint8_t)args[i];
    }

    bf_io_bind_mode(mode);
    bf_io_reset();
    if (bf_vm_run(&vm, code) != BF_OK) {
        return -1;
    }

    return (int)g_tape[0];
}

const char* bfpe_run_string_program(const char* code, bf_io_mode_t mode)
{
    if (bfpe_run_tape(code, mode) != BF_OK) {
        return "";
    }
    return bf_io_output_trimmed();
}

void bfpe_run_void_program(const char* code, bf_io_mode_t mode)
{
    if (bfpe_run_tape(code, mode) != BF_OK) {
        return;
    }
    bf_io_flush();
}

const char* bfdll_run_output_program(const char* code)
{
    return bfpe_run_string_program(code, BF_IO_MODE_BUFFER);
}
