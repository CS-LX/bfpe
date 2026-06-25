#pragma code_seg(".text$a")

#include <string.h>

#include "bf_abi.h"
#include "bf_export_runtime.h"
#include "bf_io.h"

static __declspec(thread) uint8_t g_tape[BFDLL_TAPE_SIZE];

static bf_status_t bf_run_program(const char* code)
{
    bf_vm_t vm = bf_vm_default(g_tape);
    memset(g_tape, 0, sizeof(g_tape));
    bf_io_reset();
    return bf_vm_run(&vm, code);
}

const char* bfdll_run_output_program(const char* code)
{
    if (bf_run_program(code) != BF_OK) {
        return "";
    }
    return bf_io_output_trimmed();
}

__declspec(dllexport) const char* __cdecl BF_GetLastOutput(void)
{
    return bf_io_output_buffer();
}
