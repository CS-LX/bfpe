#ifndef BF_IO_H
#define BF_IO_H

#include "vm/bf_vm.h"

typedef void (__cdecl *bf_output_callback_t)(uint8_t byte, void* user);

void bf_io_init(void);
void bf_io_shutdown(void);
void bf_io_reset(void);
void bf_io_set_output_callback(bf_output_callback_t callback, void* user);
void bf_io_write(bf_vm_t* vm, uint8_t byte);
uint8_t bf_io_read(bf_vm_t* vm);
void bf_io_flush(void);
const char* bf_io_output_buffer(void);
const char* bf_io_output_trimmed(void);

#endif
