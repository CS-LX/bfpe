#pragma code_seg(".text$a")

#include "bf_io.h"

__declspec(dllexport) const char* __cdecl BF_GetLastOutput(void)
{
    return bf_io_output_buffer();
}
