#undef UNICODE
#undef _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <thread>
#include <chrono>
#include <cstdio>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "config.hpp"
#include "mem.hpp"
#include "math.hpp"
#include "entity.hpp"
#include "material.hpp"
#include "chams.hpp"
#include "overlay.hpp"
#include "menu.hpp"
#include "esp.hpp"
#include "trigger.hpp"

// ============================================================
// GLOBAL DEFINITIONS
// ============================================================
MenuState   gMenu;
CombatState gCombat;

HWND gOverlay = nullptr;
HWND gCs2Hwnd = nullptr;
HDC  gHdc = nullptr, gMemDC = nullptr;
HBITMAP gMemBmp = nullptr, gOldBmp = nullptr;
int  gW = 0, gH = 0;
bool gRunning = true, gMenuOpen = false;
bool gAttachedToCs2 = false;

InputState gInput;

int gMenuX = 180, gMenuY = 80;
bool gMenuDragging = false;
POINT gMenuDragOffset = { 0, 0 };

int gTopTab = 1, gSubTab = 0;

bool  gPickerOpen = false;
POINT gPickerPos = { 0, 0 };
CustomColor* gPickerRef = nullptr;
const char* gPickerTitle = "Color";
bool  gSliderDrag = false;
int   gSliderActive = -1;

const char* gTriggerKeyNames[] = {
    "Left Alt", "Right Alt", "Left Shift", "Right Shift",
    "Mouse 4", "Mouse 5", "C (Crouch)", "V (Walk)"
};

CAnimatableSceneObjectDescRenderHookData* g_pHookData = nullptr;
void* g_pOriginalRenderObjects = nullptr;

// ============================================================
// CLICK-THROUGH (UI thread only)
// ============================================================
static void setClickThrough(bool clickThrough) {
    if (!gOverlay) return;
    LONG_PTR ex = GetWindowLongPtr(gOverlay, GWL_EXSTYLE);
    if (clickThrough) ex |= WS_EX_TRANSPARENT;
    else              ex &= ~WS_EX_TRANSPARENT;
    SetWindowLongPtr(gOverlay, GWL_EXSTYLE, ex);
    SetWindowPos(gOverlay, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
}

// ============================================================
// HOTKEY THREAD
// ============================================================
static volatile bool gToggleRequested = false;

static void hotkeyThread() {
    bool prev = false;
    while (gRunning) {
        bool now = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;
        if (now && !prev) gToggleRequested = true;
        prev = now;
        Sleep(25);
    }
}

// ============================================================
// MAIN
// ============================================================
int main() {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);

    // ============================================================
    // HIGH PRIORITY — set before anything else
    // ============================================================
    if (SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
        printf("[NullWare] process priority: HIGH\n");
    else
        printf("[NullWare] SetPriorityClass failed (err %lu)\n", GetLastError());

    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL))
        printf("[NullWare] thread priority: ABOVE_NORMAL\n");

    printf("[NullWare] v14.2 starting...\n");

    timeBeginPeriod(1);

    if (!OverlayWin::create()) {
        printf("[NullWare] overlay failed\n");
        return 1;
    }
    setClickThrough(true);

    // ============================================================
    // Bump overlay window to topmost + no-activate priority
    // ============================================================
    SetWindowPos(gOverlay, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    Memory mem;
    while (gRunning && !mem.attach("cs2.exe")) {
        gAttachedToCs2 = false;
        Sleep(500);
    }
    gAttachedToCs2 = true;
    printf("[NullWare] attached. client.dll = 0x%llX\n",
           (unsigned long long)mem.clientBase);

    gCs2Hwnd = OverlayWin::findCs2Window(mem.pid);

    EntityReader reader(&mem, mem.clientBase);

    MaterialSystem matSys(&mem);
    void* hChamsMaterial = nullptr;
    ChamsHook chams(&mem);
    bool chamsInstalled = false;
    bool chamsTried = false;

    std::thread hk(hotkeyThread);

    MSG msg;
    while (gRunning) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { gRunning = false; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!gRunning) break;

        if (gToggleRequested) {
            gToggleRequested = false;
            gMenuOpen = !gMenuOpen;
            setClickThrough(!gMenuOpen);
            if (!gMenuOpen) {
                gPickerOpen = false;
                gMenuDragging = false;
                gSliderDrag = false;
            }
        }

        OverlayWin::clearBuffer();

        if (mem.pid && (!gCs2Hwnd || !IsWindow(gCs2Hwnd))) {
            gCs2Hwnd = OverlayWin::findCs2Window(mem.pid);
        }

        bool focused = (gCs2Hwnd && OverlayWin::isCs2Foreground(gCs2Hwnd)) || gMenuOpen;

        if (focused) {
            float matrix[16]{};
            if (mem.readBytes(mem.clientBase + Off::dwViewMatrix,
                              matrix, sizeof(matrix))) {
                uintptr_t gsys = reader.gameEntitySystem();
                int localCtrlIdx = (gsys && EntityReader::isPtr(gsys))
                    ? reader.localControllerIndex(gsys) : -1;

                if (gCombat.triggerBot && !gMenuOpen && gsys &&
                        EntityReader::isPtr(gsys)) {
                    runTriggerBot(mem, reader, gsys, localCtrlIdx);
                }

                // ---- Chams ----
                if (gMenu.chamsStyle != CHAMS_OFF) {
                    if (!chamsInstalled && !chamsTried) {
                        if (!hChamsMaterial) {
                            printf("[NullWare] creating chams material...\n");
                            hChamsMaterial = matSys.createMaterial(
                                LATEX_CHAMS_KV3(), "NullWareLatexChams");
                            printf("[NullWare] material = 0x%p\n", hChamsMaterial);
                        }
                        if (hChamsMaterial) {
                            if (chams.install(hChamsMaterial)) {
                                chamsInstalled = true;
                            }
                            chamsTried = true;
                        } else {
                            chamsTried = true;
                        }
                    }
                    if (chamsInstalled) {
                        chams.setEnabled(true);
                        uint8_t r = (uint8_t)(gMenu.chamsColorR * 255);
                        uint8_t g = (uint8_t)(gMenu.chamsColorG * 255);
                        uint8_t b = (uint8_t)(gMenu.chamsColorB * 255);
                        uint8_t a = (uint8_t)(gMenu.chamsColorA * 255);
                        chams.setColor(r, g, b, a);
                    }
                } else if (chamsInstalled) {
                    chams.setEnabled(false);
                }

                drawESP(reader, mem, matrix);
            }
        }

        if (gMenuOpen) {
            drawMenu();
            updateMenuInput();
        }

        BitBlt(gHdc, 0, 0, gW, gH, gMemDC, 0, 0, SRCCOPY);
        gInput.lmbJustPressed = false;
        Sleep(4);
    }

    gRunning = false;
    hk.join();
    timeEndPeriod(1);
    return 0;
}
