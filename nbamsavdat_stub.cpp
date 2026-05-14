#include <windows.h>

// Stub nbamsavdat.dll - all functions return 0 (success)

extern "C" {

__declspec(dllexport) int nbamSavdatDelete(void* a, void* b, void* c, void* d) { return 0; }
__declspec(dllexport) int nbamSavdatExit(void* a) { return 0; }
__declspec(dllexport) int nbamSavdatGetAPIVersion() { return 0x105; }
__declspec(dllexport) int nbamSavdatGetCtrlSize() { return 0xE0; }
__declspec(dllexport) int nbamSavdatGetLibVersion() { return 0x105; }
__declspec(dllexport) int nbamSavdatInit(void* a) { return 0; }
__declspec(dllexport) void* nbamSavdatNew(void* ctrl, void* a, void* b, void* c, void* d) {
    if (ctrl) memset(ctrl, 0, 0xE0);
    return ctrl;
}
__declspec(dllexport) void* nbamSavdatNewA(void* ctrl, void* a, void* b, void* c, void* d) {
    if (ctrl) memset(ctrl, 0, 0xE0);
    return ctrl;
}
__declspec(dllexport) void* nbamSavdatNewW(void* ctrl, void* a, void* b, void* c, void* d) {
    if (ctrl) memset(ctrl, 0, 0xE0);
    return ctrl;
}
__declspec(dllexport) int nbamSavdatRead(void* a, void* b, void* c, void* d) { return 0; }
__declspec(dllexport) void nbamSavdatSetLogFunc(void* a) { }
__declspec(dllexport) void nbamSavdatSetLogLevel(int level) { }
__declspec(dllexport) int nbamSavdatUnlink(void* a, void* b) { return 0; }
__declspec(dllexport) int nbamSavdatWrite(void* a, void* b, void* c, void* d) { return 0; }

}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    return TRUE;
}
