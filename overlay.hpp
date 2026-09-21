#pragma once
#include "config.hpp"
#include "render_d3d11.hpp"
#include <windows.h>
#include <windowsx.h>

namespace OverlayWin {
    inline bool create() { return Render::CreateOverlayWindow(); }
    inline void clearBuffer() {}
    struct FindData { DWORD pid; HWND hwnd; };
    inline BOOL CALLBACK EnumProc(HWND h, LPARAM lp) {
        FindData* d = (FindData*)lp;
        DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
        if (pid == d->pid && IsWindowVisible(h) && GetWindow(h, GW_OWNER) == nullptr) { d->hwnd = h; return FALSE; }
        return TRUE;
    }
    inline HWND findCs2Window(DWORD pid) { FindData d{pid, nullptr}; EnumWindows(EnumProc, (LPARAM)&d); return d.hwnd; }
    inline bool isCs2Foreground(HWND cs2) {
        if (!cs2) return false;
        HWND fg = GetForegroundWindow(); if (!fg) return false;
        if (fg == cs2) return true;
        if (GetAncestor(fg, GA_ROOTOWNER) == cs2) return true;
        if (GetAncestor(fg, GA_ROOT) == cs2) return true;
        return false;
    }
}