#ifndef BF_VM_H
#define BF_VM_H

#include <stddef.h>
#include <stdint.h>

#define BFDLL_TAPE_SIZE 65536
#define BFDLL_STEP_LIMIT 10000000ULL

typedef struct {
    uint8_t* tape;
    size_t   tape_size;
    size_t   tape_ptr;
    uint64_t step_limit;
    void*    io_user;
} bf_vm_t;

typedef enum {
    BF_OK = 0,
    BF_ERR_UNMATCHED_BRACKET,
    BF_ERR_STEP_LIMIT,
    BF_ERR_IO,
} bf_status_t;

bf_vm_t bf_vm_default(uint8_t* tape);
bf_status_t bf_vm_run(bf_vm_t* vm, const char* code);

#endif
