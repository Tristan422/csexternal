#pragma once
#include <windows.h>
#include <cstdint>

// Session-local name — works across processes in the same user session,
// regardless of elevation level. No privilege required.
#define SKIN_SHM_NAME   L"NullWare_SkinShared_v2"
#define SKIN_SHM_SIZE   4096

struct SkinSharedData {
    volatile uint32_t  magic;
    volatile int       enabled;
    volatile int       paintKit;
    volatile int       seed;
    volatile float     wear;
    volatile int       statTrak;
    volatile int       statTrakKills;
    volatile int       forceUpdate;
    volatile uint32_t  frame;
    volatile uint32_t  dllHeartbeat;
    volatile int       dllAttached;
    volatile int       lastAppliedKit;
    volatile int       lastDefIdx;
    uint8_t            pad[4024];
};

// Create a SECURITY_DESCRIPTOR granting Everyone full access.
// Prevents the "elevated creator blocks lower-integrity opener" problem.
inline SECURITY_ATTRIBUTES* SkinShm_MakeSA() {
    static SECURITY_ATTRIBUTES sa{};
    static SECURITY_DESCRIPTOR sd{};
    // NULL DACL = everyone has full access
    if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) return nullptr;
    if (!SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE)) return nullptr;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;
    return &sa;
}

inline HANDLE SkinShm_Create(SkinSharedData** out) {
    SECURITY_ATTRIBUTES* sa = SkinShm_MakeSA();
    HANDLE h = CreateFileMappingW(INVALID_HANDLE_VALUE, sa, PAGE_READWRITE,
                                  0, SKIN_SHM_SIZE, SKIN_SHM_NAME);
    if (!h) return nullptr;
    void* p = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, SKIN_SHM_SIZE);
    if (!p) { CloseHandle(h); return nullptr; }
    *out = (SkinSharedData*)p;
    // Zero out on first creation
    ZeroMemory(p, SKIN_SHM_SIZE);
    return h;
}

inline HANDLE SkinShm_Open(SkinSharedData** out) {
    HANDLE h = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, SKIN_SHM_NAME);
    if (!h) {
        // Fallback: some systems need read/write only
        h = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, SKIN_SHM_NAME);
    }
    if (!h) return nullptr;
    void* p = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, SKIN_SHM_SIZE);
    if (!p) {
        p = MapViewOfFile(h, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, SKIN_SHM_SIZE);
    }
    if (!p) { CloseHandle(h); return nullptr; }
    *out = (SkinSharedData*)p;
    return h;
}