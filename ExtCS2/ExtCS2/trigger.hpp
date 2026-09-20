#pragma once
#include "config.hpp"
#include "entity.hpp"
#include "mem.hpp"
#include <chrono>

inline void sendMouseDown() {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &input, sizeof(INPUT));
}
inline void sendMouseUp() {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
}

inline int triggerVk(int idx) {
    switch (idx) {
        case 0: return VK_LMENU;
        case 1: return VK_RMENU;
        case 2: return VK_LSHIFT;
        case 3: return VK_RSHIFT;
        case 4: return VK_XBUTTON1;
        case 5: return VK_XBUTTON2;
        case 6: return 'C';
        case 7: return 'V';
    }
    return VK_LMENU;
}

inline void runTriggerBot(Memory& mem, EntityReader& rd, uintptr_t gsys, int localCtrlIdx) {
    static bool lmbHeld = false;
    static std::chrono::steady_clock::time_point lastFire{}, triggerStart{};

    int vk = triggerVk(gCombat.triggerKey);
    if (!(GetAsyncKeyState(vk) & 0x8000)) {
        if (lmbHeld) { sendMouseUp(); lmbHeld = false; }
        return;
    }

    uintptr_t localPawn = rd.localPawn();
    if (!localPawn || !EntityReader::isPtr(localPawn)) return;

    int targetIdx = mem.read<int>(localPawn + Off::m_iIDEntIndex);
    if (targetIdx <= 0) { if (lmbHeld) { sendMouseUp(); lmbHeld = false; } return; }

    uintptr_t targetEnt = rd.resolveSlot(gsys, targetIdx & 0x7FFF);
    if (!targetEnt || !EntityReader::isPtr(targetEnt)) { if (lmbHeld) { sendMouseUp(); lmbHeld = false; } return; }

    int hp = mem.read<int>(targetEnt + Off::m_iHealth);
    int team = mem.read<int>(targetEnt + Off::m_iTeamNum);
    if (hp <= 0 || hp > 200) { if (lmbHeld) { sendMouseUp(); lmbHeld = false; } return; }
    if (gCombat.teamCheck && team == rd.localTeam()) { if (lmbHeld) { sendMouseUp(); lmbHeld = false; } return; }
    if (gCombat.visCheck && !rd.isVisibleByMask(targetEnt, localCtrlIdx)) { if (lmbHeld) { sendMouseUp(); lmbHeld = false; } return; }

    if (gCombat.useSeedTrigger) {
        uintptr_t ws = mem.read<uintptr_t>(localPawn + Off::m_pWeaponServices);
        if (ws && EntityReader::isPtr(ws)) {
            uint32_t h = mem.read<uint32_t>(ws + Off::m_hActiveWeapon);
            uintptr_t weapon = rd.resolveSlot(gsys, h & 0x7FFF);
            if (weapon && EntityReader::isPtr(weapon)) {
                int seed = mem.read<int>(weapon + Off::m_nLastShotSeed);
                if (seed != 0) {
                    uint32_t x = (uint32_t)seed;
                    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
                    float chance = (float)(x % 1000) / 1000.0f;
                    if (chance < gCombat.hitChance) { if (lmbHeld) { sendMouseUp(); lmbHeld = false; } return; }
                }
            }
        }
    }

    auto now = std::chrono::steady_clock::now();
    auto cooldown = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFire).count();

    if (!lmbHeld && cooldown >= gCombat.cooldownMs) {
        if (gCombat.delayMs > 0) Sleep(gCombat.delayMs);
        sendMouseDown(); lmbHeld = true;
        triggerStart = std::chrono::steady_clock::now(); lastFire = triggerStart;
    }
    if (lmbHeld) {
        auto held = std::chrono::duration_cast<std::chrono::milliseconds>(now - triggerStart).count();
        if (held >= gCombat.holdMs) { sendMouseUp(); lmbHeld = false; }
    }
}
