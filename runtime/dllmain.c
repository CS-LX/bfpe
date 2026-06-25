#include <windows.h>

#include "bf_io.h"

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance;
    (void)reserved;

    switch (reason) {
    case DLL_PROCESS_ATTACH:
        bf_io_init();
        break;
    case DLL_PROCESS_DETACH:
        bf_io_shutdown();
        break;
    default:
        break;
    }

    return TRUE;
}
