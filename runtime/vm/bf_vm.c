#include "bf_vm.h"

#include "bf_io.h"

#include <stdlib.h>
#include <string.h>

bf_vm_t bf_vm_default(uint8_t* tape)
{
    bf_vm_t vm = {0};
    vm.tape = tape;
    vm.tape_size = BFDLL_TAPE_SIZE;
    vm.tape_ptr = 0;
    vm.step_limit = BFDLL_STEP_LIMIT;
    vm.io_user = NULL;
    return vm;
}

static int is_bf_char(char c)
{
    switch (c) {
    case '+':
    case '-':
    case '>':
    case '<':
    case '.':
    case ',':
    case '[':
    case ']':
        return 1;
    default:
        return 0;
    }
}

static bf_status_t build_jump_table(const char* code, size_t** out_pairs, size_t* out_count)
{
    size_t capacity = 16;
    size_t count = 0;
    size_t* pairs = (size_t*)malloc(capacity * 2 * sizeof(size_t));
    size_t* stack = (size_t*)malloc(capacity * sizeof(size_t));
    size_t stack_top = 0;
    size_t ip = 0;

    if (!pairs || !stack) {
        free(pairs);
        free(stack);
        return BF_ERR_IO;
    }

    for (ip = 0; code[ip] != '\0'; ++ip) {
        if (code[ip] != '[' && code[ip] != ']') {
            continue;
        }

        if (code[ip] == '[') {
            if (stack_top >= capacity) {
                size_t new_cap = capacity * 2;
                size_t* new_stack = (size_t*)realloc(stack, new_cap * sizeof(size_t));
                if (!new_stack) {
                    free(pairs);
                    free(stack);
                    return BF_ERR_IO;
                }
                stack = new_stack;
                capacity = new_cap;
            }
            stack[stack_top++] = ip;
            continue;
        }

        if (stack_top == 0) {
            free(pairs);
            free(stack);
            return BF_ERR_UNMATCHED_BRACKET;
        }

        size_t open = stack[--stack_top];
        if (count >= capacity) {
            size_t new_cap = capacity * 2;
            size_t* new_pairs = (size_t*)realloc(pairs, new_cap * 2 * sizeof(size_t));
            if (!new_pairs) {
                free(pairs);
                free(stack);
                return BF_ERR_IO;
            }
            pairs = new_pairs;
            capacity = new_cap;
        }

        pairs[count * 2] = open;
        pairs[count * 2 + 1] = ip;
        ++count;
    }

    free(stack);

    if (stack_top != 0) {
        free(pairs);
        return BF_ERR_UNMATCHED_BRACKET;
    }

    *out_pairs = pairs;
    *out_count = count;
    return BF_OK;
}

static size_t find_jump_target(size_t ip, size_t* pairs, size_t pair_count)
{
    size_t i = 0;
    for (i = 0; i < pair_count; ++i) {
        if (pairs[i * 2] == ip) {
            return pairs[i * 2 + 1];
        }
        if (pairs[i * 2 + 1] == ip) {
            return pairs[i * 2];
        }
    }
    return ip;
}

bf_status_t bf_vm_run(bf_vm_t* vm, const char* code)
{
    size_t ip = 0;
    uint64_t steps = 0;
    size_t* jump_pairs = NULL;
    size_t jump_count = 0;
    bf_status_t status = BF_OK;

    if (!vm || !code) {
        return BF_ERR_IO;
    }

    status = build_jump_table(code, &jump_pairs, &jump_count);
    if (status != BF_OK) {
        return status;
    }

    while (code[ip] != '\0') {
        char op = code[ip];

        if (!is_bf_char(op)) {
            ++ip;
            continue;
        }

        switch (op) {
        case '+':
            ++vm->tape[vm->tape_ptr];
            ++ip;
            break;
        case '-':
            --vm->tape[vm->tape_ptr];
            ++ip;
            break;
        case '>':
            vm->tape_ptr = (vm->tape_ptr + 1) % vm->tape_size;
            ++ip;
            break;
        case '<':
            vm->tape_ptr = (vm->tape_ptr + vm->tape_size - 1) % vm->tape_size;
            ++ip;
            break;
        case '.':
            bf_io_write(vm, vm->tape[vm->tape_ptr]);
            ++ip;
            break;
        case ',':
            vm->tape[vm->tape_ptr] = bf_io_read(vm);
            ++ip;
            break;
        case '[':
            if (vm->tape[vm->tape_ptr] == 0) {
                ip = find_jump_target(ip, jump_pairs, jump_count);
            }
            ++ip;
            break;
        case ']':
            if (vm->tape[vm->tape_ptr] != 0) {
                ip = find_jump_target(ip, jump_pairs, jump_count);
            }
            ++ip;
            break;
        default:
            ++ip;
            break;
        }

        if (++steps > vm->step_limit) {
            status = BF_ERR_STEP_LIMIT;
            goto cleanup;
        }
    }

cleanup:
    free(jump_pairs);
    return status;
}
