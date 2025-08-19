#include <windows.h>
#include <strsafe.h>
#include <cstdio>
#include <cstdarg>
#include <fstream>

HMODULE g_InputDLL = nullptr;
std::ofstream g_LogFile;

typedef DWORD(__stdcall* XInputGetState_t)(DWORD, void*);
typedef DWORD(__stdcall* XInputSetState_t)(DWORD, void*);
typedef DWORD(__stdcall* XInputGetCapabilities_t)(DWORD, DWORD, void*);
typedef DWORD(__stdcall* XInputEnable_t)(BOOL);
typedef DWORD(__stdcall* XInputGetBatteryInformation_t)(DWORD, BYTE, void*);
typedef DWORD(__stdcall* XInputGetKeystroke_t)(DWORD, DWORD, void*);

XInputGetState_t              g_GetState = nullptr;
XInputSetState_t              g_SetState = nullptr;
XInputGetCapabilities_t       g_GetCapabilities = nullptr;
XInputEnable_t                g_Enable = nullptr;
XInputGetBatteryInformation_t g_GetBatteryInformation = nullptr;
XInputGetKeystroke_t          g_GetKeystroke = nullptr;

void Log(const char* format, ...)
{
    if (!g_LogFile.is_open())
        g_LogFile.open("OL.log", std::ios::out | std::ios::app);

    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);

    g_LogFile << buffer;
    g_LogFile.flush();
    OutputDebugStringA(buffer);
}

bool InitProxy()
{
    char sysPath[MAX_PATH] = { 0 };
    if (GetSystemDirectoryA(sysPath, MAX_PATH) == 0)
    {
        Log("[ERROR] Failed to get system directory.\n");
        return false;
    }

    if (FAILED(StringCchCatA(sysPath, MAX_PATH, "\\xinput1_3.dll")))
    {
        Log("[ERROR] Failed to build XInput DLL path.\n");
        return false;
    }

    g_InputDLL = LoadLibraryA(sysPath);
    if (!g_InputDLL)
    {
        Log("[ERROR] Failed to load %s\n", sysPath);
        return false;
    }

    g_GetState = (XInputGetState_t)GetProcAddress(g_InputDLL, "XInputGetState");
    g_SetState = (XInputSetState_t)GetProcAddress(g_InputDLL, "XInputSetState");
    g_GetCapabilities = (XInputGetCapabilities_t)GetProcAddress(g_InputDLL, "XInputGetCapabilities");
    g_Enable = (XInputEnable_t)GetProcAddress(g_InputDLL, "XInputEnable");
    g_GetBatteryInformation = (XInputGetBatteryInformation_t)GetProcAddress(g_InputDLL, "XInputGetBatteryInformation");
    g_GetKeystroke = (XInputGetKeystroke_t)GetProcAddress(g_InputDLL, "XInputGetKeystroke");

    Log("[INFO] XInput proxy initialized successfully.\n");
    return true;
}

#define XINPUT_CALL_OR_ERROR(fn, ...) \
    do { if (!fn) return ERROR_DEVICE_NOT_CONNECTED; return fn(__VA_ARGS__); } while(0)

extern "C"
{
    __declspec(dllexport) DWORD __stdcall XInputGetState(DWORD dwUserIndex, void* pState)
    {
        XINPUT_CALL_OR_ERROR(g_GetState, dwUserIndex, pState);
    }

    __declspec(dllexport) DWORD __stdcall XInputSetState(DWORD dwUserIndex, void* pVibration)
    {
        XINPUT_CALL_OR_ERROR(g_SetState, dwUserIndex, pVibration);
    }

    __declspec(dllexport) DWORD __stdcall XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, void* pCapabilities)
    {
        XINPUT_CALL_OR_ERROR(g_GetCapabilities, dwUserIndex, dwFlags, pCapabilities);
    }

    __declspec(dllexport) DWORD __stdcall XInputEnable(BOOL enable)
    {
        XINPUT_CALL_OR_ERROR(g_Enable, enable);
    }

    __declspec(dllexport) DWORD __stdcall XInputGetBatteryInformation(DWORD dwUserIndex, BYTE devType, void* pBatteryInformation)
    {
        XINPUT_CALL_OR_ERROR(g_GetBatteryInformation, dwUserIndex, devType, pBatteryInformation);
    }

    __declspec(dllexport) DWORD __stdcall XInputGetKeystroke(DWORD dwUserIndex, DWORD dwReserved, void* pKeystroke)
    {
        XINPUT_CALL_OR_ERROR(g_GetKeystroke, dwUserIndex, dwReserved, pKeystroke);
    }
}

BOOL __stdcall DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        Log("[INFO] Attached to process.\n");

        if (!InitProxy())
        {
            Log("[ERROR] Proxy initialization failed, exiting process...\n");
            if (g_LogFile.is_open()) g_LogFile.close();
            ExitProcess(EXIT_FAILURE);
        }

        // Load program here


        if (g_LogFile.is_open()) g_LogFile.close();
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH)
    {
        if (g_InputDLL) FreeLibrary(g_InputDLL);
        if (g_LogFile.is_open()) g_LogFile.close();
    }
    return TRUE;
}
