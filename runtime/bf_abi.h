#ifndef BF_ABI_H
#define BF_ABI_H

#include "vm/bf_vm.h"

/*
 * BFC-0 (x64 Windows)
 * Calling convention: __cdecl
 * Parameter i -> tape[i] (truncated to uint8_t)
 * Return value: tape[0] after execution (zero-extended to int)
 * Termination: IP hits '\0'
 */

#endif
