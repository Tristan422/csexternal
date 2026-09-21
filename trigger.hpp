#pragma once
#include "config.hpp"
#include "entity.hpp"
#include "mem.hpp"
#include <chrono>
#include <cstdio>

inline void sendMouseDown() {
    INPUT i = {}; i.type = INPUT_MOUSE; i.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &i, sizeof(INPUT));
}
inline void sendMouseUp() {
    INPUT i = {}; i.type = INPUT_MOUSE; i.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &i, sizeof(INPUT));
}
inline int triggerVk(int idx) {
    switch (idx) {
        case 0: return VK_LMENU; case 1: return VK_RMENU;
        case 2: return VK_LSHIFT; case 3: return VK_RSHIFT;
        case 4: return VK_XBUTTON1; case 5: return VK_XBUTTON2;
        case 6: return 'C'; case 7: return 'V';
    }
    return VK_LMENU;
}

// CS2 handle resolve — chunks are 512 slots, entry stride 112
inline uintptr_t resolveHandle(Memory& mem, uintptr_t gsys, uint32_t handle) {
    if (!handle) return 0;
    int chunkIdx = (handle >> 9) & 0x7F;
    int slotIdx  = handle & 0x1FF;
    uintptr_t buckets = gsys + 16;
    uintptr_t bucket = mem.read<uintptr_t>(buckets + 8 * chunkIdx);
    if (!bucket || !EntityReader::isPtr(bucket)) return 0;
    uintptr_t entry = bucket + (uintptr_t)slotIdx * 112;
    uintptr_t pawn = mem.read<uintptr_t>(entry);
    return EntityReader::isPtr(pawn) ? pawn : 0;
}

inline void runTriggerBot(Memory& mem, EntityReader& rd, uintptr_t gsys, int localCtrlIdx) {
    static bool lmbHeld = false;
    static std::chrono::steady_clock::time_point lastFire{}, triggerStart{};
    static int diagCounter = 0;
    bool diag = (diagCounter++ % 60 == 0);

    int vk = triggerVk(gCombat.triggerKey);
    bool keyDown = (GetAsyncKeyState(vk) & 0x8000) != 0;

    if (!keyDown) {
        if (lmbHeld) { sendMouseUp(); lmbHeld = false; }
        if (diag) printf("[Trig] key not held (vk=0x%X)\n", vk);
        return;
    }
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
        if (lmbHeld) { sendMouseUp(); lmbHeld = false; }
        return;
    }

    uintptr_t localPawn = rd.localPawn();
    if (!localPawn || !EntityReader::isPtr(localPawn)) {
        if (diag) printf("[Trig] no local pawn\n"); return;
    }

    // Try m_iIDEntIndex first
    int targetHandle = mem.read<int>(localPawn + Off::m_iIDEntIndex);
    if (diag) printf("[Trig] m_iIDEntIndex=0x%X\n", targetHandle);

    uintptr_t targetPawn = 0;
    if (targetHandle > 0) {
        targetPawn = resolveHandle(mem, gsys, (uint32_t)targetHandle);
    }

    // Fallback: pick first visible enemy from spotted mask
    if (!targetPawn) {
        if (diag) printf("[Trig] fallback: scanning spotted enemies\n");
        for (int i = 1; i < 64; ++i) {
            uintptr_t e = rd.resolveSlot(gsys, i);
            if (!e || !EntityReader::isPtr(e)) continue;
            int hp = mem.read<int>(e + Off::m_iHealth);
            int tm = mem.read<int>(e + Off::m_iTeamNum);
            if (hp <= 0 || hp > 200 || tm < 1 || tm > 4) continue;
            if (gCombat.teamCheck && tm == rd.localTeam()) continue;
            if (rd.isVisibleByMask(e, localCtrlIdx)) { targetPawn = e; break; }
        }
    }

    if (!targetPawn) {
        if (lmbHeld) { sendMouseUp(); lmbHeld = false; }
        if (diag) printf("[Trig] no target\n"); return;
    }

    int hp = mem.read<int>(targetPawn + Off::m_iHealth);
    int team = mem.read<int>(targetPawn + Off::m_iTeamNum);
    if (diag) printf("[Trig] target=0x%llX hp=%d team=%d\n", (unsigned long long)targetPawn, hp, team);

    if (hp <= 0 || hp > 200) { if (lmbHeld) { sendMouseUp(); lmbHeld = false; } return; }
    if (gCombat.teamCheck && team == rd.localTeam()) { if (lmbHeld) { sendMouseUp(); lmbHeld = false; } return; }
    if (gCombat.visCheck && !rd.isVisibleByMask(targetPawn, localCtrlIdx)) {
        if (lmbHeld) { sendMouseUp(); lmbHeld = false; }
        if (diag) printf("[Trig] not visible\n"); return;
    }

    auto now = std::chrono::steady_clock::now();
    auto cd = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFire).count();

    if (!lmbHeld && cd >= gCombat.cooldownMs) {
        if (gCombat.delayMs > 0) Sleep(gCombat.delayMs);
        sendMouseDown();
        lmbHeld = true;
        triggerStart = std::chrono::steady_clock::now();
        lastFire = triggerStart;
        printf("[Trig] >>> FIRED <<<\n");
    }
    if (lmbHeld) {
        auto held = std::chrono::duration_cast<std::chrono::milliseconds>(now - triggerStart).count();
        if (held >= gCombat.holdMs) { sendMouseUp(); lmbHeld = false; }
    }
}