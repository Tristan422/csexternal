#pragma once
#include "config.hpp"
#include "mem.hpp"
#include "math.hpp"

class EntityReader {
public:
    Memory*   mem;
    uintptr_t client;
    EntityReader(Memory* m, uintptr_t c) : mem(m), client(c) {}

    uintptr_t gameEntitySystem() { return mem->read<uintptr_t>(client + Off::dwGameEntitySystem); }
    uintptr_t localController()  { return mem->read<uintptr_t>(client + Off::dwLocalPlayerController); }
    uintptr_t localPawn()        { return mem->read<uintptr_t>(client + Off::dwLocalPlayerPawn); }

    int localTeam() {
        auto ctrl = localController();
        if (!ctrl || !isPtr(ctrl)) return -1;
        int t = mem->read<int>(ctrl + Off::m_iTeamNum);
        return (t>=1&&t<=4) ? t : -1;
    }

    Vec3 localOrigin() {
        auto pawn = localPawn();
        if (!pawn || !isPtr(pawn)) return {0,0,0};
        return mem->read<Vec3>(pawn + Off::m_vOldOrigin);
    }

    uintptr_t resolveSlot(uintptr_t gsys, int index) {
        if (index<0||index>0x7FFE) return 0;
        uintptr_t buckets = gsys + 16;
        int bi = index>>9;
        if (bi>0x3F) return 0;
        uintptr_t bucket = mem->read<uintptr_t>(buckets + 8*bi);
        if (!bucket) return 0;
        uintptr_t entry = bucket + 112ull*(index & 0x1FF);
        int stored = mem->read<int>(entry + 16);
        if ((stored & 0x7FFF) != index) return 0;
        return mem->read<uintptr_t>(entry);
    }

    int localControllerIndex(uintptr_t gsys) {
        uintptr_t ctrl = localController();
        if (!ctrl || !isPtr(ctrl)) return -1;
        for (int i = 0; i < 64; ++i) {
            if (resolveSlot(gsys, i) == ctrl) return i;
        }
        return -1;
    }

    uintptr_t pawnFromController(uintptr_t gsys, uintptr_t ctrl) {
        int h = mem->read<int>(ctrl + Off::m_hPlayerPawn);
        if (!h) return 0;
        return resolveSlot(gsys, h & 0x7FFF);
    }

    bool readBone(uintptr_t pawn, int idx, Vec3& out) {
        uintptr_t scene = mem->read<uintptr_t>(pawn + Off::m_pGameSceneNode);
        if (!isPtr(scene)) return false;
        uintptr_t ba = mem->read<uintptr_t>(scene + Off::m_modelState + Off::m_boneArray);
        if (!isPtr(ba)) return false;
        out = mem->read<Vec3>(ba + Off::boneStride*idx);
        if (out.x != out.x) return false;
        if (fabsf(out.x)>20000||fabsf(out.y)>20000||fabsf(out.z)>20000) return false;
        return true;
    }

    bool isVisibleByMask(uintptr_t pawn, int localCtrlIdx) {
        if (localCtrlIdx < 0 || localCtrlIdx > 63) {
            return mem->read<bool>(pawn + Off::m_entitySpottedState + Off::spotted_bSpotted);
        }
        uint32_t m0 = mem->read<uint32_t>(pawn + Off::m_entitySpottedState + Off::spotted_bSpottedByMask0);
        uint32_t m1 = mem->read<uint32_t>(pawn + Off::m_entitySpottedState + Off::spotted_bSpottedByMask1);
        if (localCtrlIdx < 32) return (m0 & (1u << localCtrlIdx)) != 0;
        return (m1 & (1u << (localCtrlIdx - 32))) != 0;
    }

    static bool isPtr(uintptr_t p) { return p>0x10000 && p<0x7FFFFFFFFFFF; }
};
