// internal_main.cpp — NullWare internal skinchanger (SHM-fixed)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cstdint>
#include <cstdio>
#include "../SkinShared.h"

namespace CS2 {
    constexpr uintptr_t dwLocalPlayerPawn    = 0x23CCC08;
    constexpr uintptr_t dwGameEntitySystem   = 0x2577BE0;
    constexpr uintptr_t m_pWeaponServices    = 0x1208;
    constexpr uintptr_t m_hActiveWeapon      = 0x58;
    constexpr uintptr_t m_AttributeManager   = 0x11A8;
    constexpr uintptr_t m_Item               = 0x50;
    constexpr uintptr_t m_iItemIDHigh        = 0x1D0;
    constexpr uintptr_t m_iItemDefinitionIndex = 0x1BA;
    constexpr uintptr_t m_nFallbackPaintKit  = 0x1680;
    constexpr uintptr_t m_flFallbackWear     = 0x1688;
    constexpr uintptr_t m_nFallbackSeed      = 0x1684;
    constexpr uintptr_t m_nFallbackStatTrak  = 0x168C;
    constexpr uintptr_t dwNetworkGameClient  = 0x90D490;
    constexpr uintptr_t m_nDeltaTick         = 0x24C;
}

static SkinSharedData* g_shm = nullptr;
static HANDLE g_shmHandle = nullptr;
static uintptr_t g_clientBase = 0;
static uintptr_t g_engineBase = 0;
static FILE* g_logFile = nullptr;

static void DllLog(const char* fmt, ...) {
    if (!g_logFile) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(g_logFile, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list args; va_start(args, fmt);
    vfprintf(g_logFile, fmt, args);
    va_end(args);
    fprintf(g_logFile, "\n");
    fflush(g_logFile);
}

static uintptr_t GetModuleBase(const char* name) {
    uintptr_t base = 0;
    wchar_t wname[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, MAX_PATH);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W me{}; me.dwSize = sizeof(me);
    if (Module32FirstW(snap, &me)) {
        do { if (_wcsicmp(me.szModule, wname) == 0) { base = (uintptr_t)me.modBaseAddr; break; } }
        while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return base;
}

static uintptr_t ResolveHandle(uintptr_t gsys, uint32_t handle) {
    if (!handle || handle == 0xFFFFFFFF) return 0;
    int chunk = (handle >> 9) & 0x7F;
    int slot  = handle & 0x1FF;
    uintptr_t bucket = *(uintptr_t*)(gsys + 16 + 8 * chunk);
    if (!bucket) return 0;
    return *(uintptr_t*)(bucket + (uintptr_t)slot * 112);
}

static void ApplySkin() {
    if (!g_shm) return;
    g_shm->dllHeartbeat++;
    if (g_shm->magic != 0x4E57534B) return;
    if (!g_clientBase) g_clientBase = GetModuleBase("client.dll");
    if (!g_engineBase) g_engineBase = GetModuleBase("engine2.dll");
    if (!g_clientBase) return;

    uintptr_t localPawn = *(uintptr_t*)(g_clientBase + CS2::dwLocalPlayerPawn);
    if (!localPawn) return;
    uintptr_t ws = *(uintptr_t*)(localPawn + CS2::m_pWeaponServices);
    if (!ws) return;
    uint32_t hWeapon = *(uint32_t*)(ws + CS2::m_hActiveWeapon);
    if (!hWeapon) return;
    uintptr_t gsys = *(uintptr_t*)(g_clientBase + CS2::dwGameEntitySystem);
    if (!gsys) return;
    uintptr_t weapon = ResolveHandle(gsys, hWeapon);
    if (!weapon) return;

    uintptr_t itemView = weapon + CS2::m_AttributeManager + CS2::m_Item;
    int defIdx = *(int*)(itemView + CS2::m_iItemDefinitionIndex);
    g_shm->lastDefIdx = defIdx;

    static uint32_t lastLog = 0;
    if (g_shm->dllHeartbeat - lastLog > 300) {
        DllLog("[DLL] localPawn=0x%llX ws=0x%llX hWep=0x%X weapon=0x%llX defIdx=%d enabled=%d targetPaint=%d",
            (unsigned long long)localPawn, (unsigned long long)ws, hWeapon,
            (unsigned long long)weapon, defIdx, g_shm->enabled, g_shm->paintKit);
        lastLog = g_shm->dllHeartbeat;
    }

    if (!g_shm->enabled) return;
    if (defIdx <= 0 || defIdx > 100) return;

    *(int*)(itemView + CS2::m_iItemIDHigh) = -1;
    *(int*)(weapon + CS2::m_nFallbackPaintKit) = g_shm->paintKit;
    *(float*)(weapon + CS2::m_flFallbackWear)  = g_shm->wear;
    *(int*)(weapon + CS2::m_nFallbackSeed)     = g_shm->seed;
    if (g_shm->statTrak) *(int*)(weapon + CS2::m_nFallbackStatTrak) = g_shm->statTrakKills;
    g_shm->lastAppliedKit = g_shm->paintKit;

    if (g_shm->forceUpdate) {
        if (g_engineBase) {
            uintptr_t ngc = *(uintptr_t*)(g_engineBase + CS2::dwNetworkGameClient);
            if (ngc) *(int*)(ngc + CS2::m_nDeltaTick) = -1;
        }
        g_shm->forceUpdate = 0;
    }
}

static DWORD WINAPI SkinThread(LPVOID) {
    char logPath[MAX_PATH];
    GetTempPathA(MAX_PATH, logPath);
    strcat_s(logPath, "NullWare_DLL.log");
    fopen_s(&g_logFile, logPath, "w");
    DllLog("[DLL] thread started. clientBase=0x%llX engineBase=0x%llX",
        (unsigned long long)g_clientBase, (unsigned long long)g_engineBase);

    while (true) {
        __try { ApplySkin(); } __except(EXCEPTION_EXECUTE_HANDLER) {}
        Sleep(6);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(GetModuleHandle(nullptr));

        // Attach shm FIRST, quickly
        for (int i = 0; i < 40 && !g_shm; ++i) {
            g_shmHandle = SkinShm_Open(&g_shm);
            if (!g_shm) Sleep(25);
        }
        if (!g_shmHandle) return FALSE;

        g_shm->dllAttached = 1;

        // Module bases can be resolved in the thread (don't block DllMain)
        HANDLE th = CreateThread(nullptr, 0, SkinThread, nullptr, 0, nullptr);
        if (th) CloseHandle(th);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_shm) g_shm->dllAttached = 0;
        if (g_shmHandle) CloseHandle(g_shmHandle);
        if (g_logFile) fclose(g_logFile);
    }
    return TRUE;
}