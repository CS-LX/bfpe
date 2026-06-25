#include "bf_io.h"

#include <windows.h>

#include <stdint.h>
#include <string.h>

#define BF_IO_BUFFER_SIZE 4096

static __declspec(thread) char g_output_buffer[BF_IO_BUFFER_SIZE];
static __declspec(thread) size_t g_output_length;
static __declspec(thread) bf_output_callback_t g_output_callback;
static __declspec(thread) void* g_output_callback_user;

void bf_io_init(void)
{
    g_output_length = 0;
    g_output_buffer[0] = '\0';
}

void bf_io_shutdown(void)
{
    g_output_length = 0;
    g_output_buffer[0] = '\0';
    g_output_callback = NULL;
    g_output_callback_user = NULL;
}

void bf_io_reset(void)
{
    g_output_length = 0;
    g_output_buffer[0] = '\0';
}

void bf_io_set_output_callback(bf_output_callback_t callback, void* user)
{
    g_output_callback = callback;
    g_output_callback_user = user;
}

__declspec(dllexport) void __cdecl BF_SetOutputCallback(bf_output_callback_t callback, void* user)
{
    bf_io_set_output_callback(callback, user);
}

void bf_io_write(bf_vm_t* vm, uint8_t byte)
{
    (void)vm;
    if (g_output_length + 1 < BF_IO_BUFFER_SIZE) {
        g_output_buffer[g_output_length++] = (char)byte;
        g_output_buffer[g_output_length] = '\0';
    }
    if (g_output_callback) {
        g_output_callback(byte, g_output_callback_user);
    }
}

uint8_t bf_io_read(bf_vm_t* vm)
{
    (void)vm;
    return 0;
}

void bf_io_flush(void)
{
    if (g_output_length > 0) {
        OutputDebugStringA(g_output_buffer);
    }
}

const char* bf_io_output_buffer(void)
{
    return g_output_buffer;
}

const char* bf_io_output_trimmed(void)
{
    size_t len = g_output_length;
    while (len > 0 && (g_output_buffer[len - 1] == '\n' || g_output_buffer[len - 1] == '\r')) {
        --len;
    }
    g_output_buffer[len] = '\0';
    return g_output_buffer;
}
