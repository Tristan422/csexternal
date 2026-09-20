#pragma once
#include "config.hpp"
#include "mem.hpp"
#include <string>
#include <cstddef>
#include <cstdint>

#define CANIMATABLE_RENDER_FN_PATTERN "48 8B C4 53 57 41 54 48 81 EC ?? ?? ?? ?? 49 63 F9 49"

struct CAnimatableSceneObjectDescRenderHookData {
    uint64_t originalFunc;
    void*    hMaterialToUse;      // CStrongHandle value (pBinding)
    int      bChamsEnabled;
    int      pad0;
    uint8_t  r, g, b, a;
    uint8_t  pad1[4];
};

extern CAnimatableSceneObjectDescRenderHookData* g_pHookData;
extern void* g_pOriginalRenderObjects;

#pragma code_seg(".ChamsHookSeg")
#pragma optimize("", off)
#pragma runtime_checks("", off)
#pragma check_stack(off)

// Hook entry — runs on the game's render thread.
// Writes material handle + color into every CBaseSceneData in the batch.
inline void* __fastcall ChamsRenderShellcode(uint64_t a1, uint64_t a2, void* a3, int32_t a4,
                                             uint64_t a5, uint64_t a6, uint64_t a7) {
    CAnimatableSceneObjectDescRenderHookData* data = g_pHookData;
    typedef void* (__fastcall* RenderFn)(uint64_t, uint64_t, void*, int32_t, uint64_t, uint64_t, uint64_t);
    RenderFn original = (RenderFn)g_pOriginalRenderObjects;

    if (!data || !data->bChamsEnabled) {
        return original(a1, a2, a3, a4, a5, a6, a7);
    }

    uint8_t* scenes = (uint8_t*)a3;
    if (scenes) {
        uint64_t handleVal = (uint64_t)(uintptr_t)data->hMaterialToUse;
        for (int i = 0; i < a4; ++i) {
            uint8_t* scene = scenes + (size_t)i * 0x20;
            if (!scene) continue;

            if (handleVal) {
                // CBaseSceneData.pMaterial / pMaterial2 are CStrongHandle (8 bytes each)
                // Copy our CStrongHandle value directly into both slots.
                *(uint64_t*)(scene + 0x00) = handleVal;
                *(uint64_t*)(scene + 0x08) = handleVal;
            }
            // RGBA tint at +0x10
            scene[0x10] = data->r;
            scene[0x11] = data->g;
            scene[0x12] = data->b;
            scene[0x13] = data->a;
        }
    }

    return original(a1, a2, a3, a4, a5, a6, a7);
}

inline void ChamsRenderShellcode_End() {}
#pragma check_stack()
#pragma runtime_checks("", restore)
#pragma optimize("", on)
#pragma code_seg()

class ChamsHook {
public:
    Memory*    mem;
    uintptr_t  vtableTargetAddr = 0;
    void*      pDataRemote = nullptr;
    void*      pShellcodeRemote = nullptr;
    void*      pOrigPtrStorage = nullptr;
    void*      pDataPtrStorage = nullptr;
    bool       installed = false;

    ChamsHook(Memory* m) : mem(m) {}

    bool install(void* hMaterialToUse) {
        if (installed) return true;

        RemoteModule sceneSys = mem->getRemoteModule("scenesystem.dll");
        if (!sceneSys.valid()) { printf("[Chams] scenesystem.dll missing\n"); return false; }
        printf("[Chams] scenesystem.dll @ 0x%llX size 0x%zX\n",
               (unsigned long long)sceneSys.base, sceneSys.size);

        uintptr_t renderFn = mem->scanMemory(sceneSys.base, sceneSys.size,
                                             CANIMATABLE_RENDER_FN_PATTERN);
        if (!renderFn) { printf("[Chams] render fn pattern MISS\n"); return false; }
        printf("[Chams] render fn @ 0x%llX\n", (unsigned long long)renderFn);

        VTableFunctionInfo vt = mem->findVTableContainingFunction(renderFn, "scenesystem.dll");
        if (!vt.vTableAddr || vt.index < 0) {
            printf("[Chams] vtable lookup FAILED\n");
            return false;
        }
        printf("[Chams] vtable @ 0x%llX idx %d\n",
               (unsigned long long)vt.vTableAddr, vt.index);

        vtableTargetAddr = vt.vTableAddr + (uintptr_t)vt.index * 8;
        uint64_t original = mem->read<uint64_t>(vtableTargetAddr);
        g_pOriginalRenderObjects = (void*)original;
        printf("[Chams] original fn ptr = 0x%llX\n", (unsigned long long)original);

        // Allocate remote hook data
        pDataRemote = mem->alloc(sizeof(CAnimatableSceneObjectDescRenderHookData));
        if (!pDataRemote) { printf("[Chams] hook data alloc FAILED\n"); return false; }

        CAnimatableSceneObjectDescRenderHookData d{};
        d.originalFunc = original;
        d.hMaterialToUse = hMaterialToUse;
        d.bChamsEnabled = 0;
        d.r = 255; d.g = 255; d.b = 255; d.a = 255;
        if (!mem->writeBytes((uintptr_t)pDataRemote, &d, sizeof(d))) {
            printf("[Chams] hook data write FAILED\n");
            return false;
        }

        // Allocate + write shellcode blob (function bytes)
        pShellcodeRemote = mem->allocAndWriteShellcode(
            (void*)&ChamsRenderShellcode,
            (void*)&ChamsRenderShellcode_End);
        if (!pShellcodeRemote) { printf("[Chams] shellcode write FAILED\n"); return false; }

        uint8_t* localCode = (uint8_t*)&ChamsRenderShellcode;
        size_t   codeSize  = (uintptr_t)&ChamsRenderShellcode_End - (uintptr_t)&ChamsRenderShellcode;
        printf("[Chams] shellcode size = %zu bytes\n", codeSize);

        // Storage slots in remote memory for the two globals
        pOrigPtrStorage = mem->alloc(8);
        pDataPtrStorage = mem->alloc(8);
        if (!pOrigPtrStorage || !pDataPtrStorage) {
            printf("[Chams] ptr storage alloc FAILED\n");
            return false;
        }
        mem->writeBytes((uintptr_t)pOrigPtrStorage, &original, 8);
        mem->writeBytes((uintptr_t)pDataPtrStorage, &pDataRemote, 8);

        // Patch EVERY RIP-relative load in the shellcode that references our globals.
        // Covers: 48/4C 8B <modrm> disp32   (mov r64, [rip+disp])
        //   48 8B 05 = mov rax, [rip+d]
        //   48 8B 0D = mov rcx, [rip+d]
        //   48 8B 15 = mov rdx, [rip+d]
        //   48 8B 1D = mov rbx, [rip+d]
        //   48 8B 35 = mov rsi, [rip+d]
        //   48 8B 3D = mov rdi, [rip+d]
        //   4C 8B 05 = mov r8,  [rip+d]
        //   4C 8B 0D = mov r9,  [rip+d]
        //   4C 8B 15 = mov r10, [rip+d]
        //   4C 8B 1D = mov r11, [rip+d]
        //   4C 8B 25 = mov r12, [rip+d]
        //   4C 8B 2D = mov r13, [rip+d]
        //   4C 8B 35 = mov r14, [rip+d]
        //   4C 8B 3D = mov r15, [rip+d]
        static const uint8_t modrmList[] = {
            0x05, 0x0D, 0x15, 0x1D, 0x35, 0x3D,        // r64 : rax..rdi
            0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D // r8..r15
        };

        int patchedData = 0, patchedOrig = 0;
        for (size_t i = 0; i + 7 <= codeSize; ++i) {
            bool isLoad = false;
            if (localCode[i] == 0x48) isLoad = true;
            else if (localCode[i] == 0x4C) isLoad = true;
            if (!isLoad) continue;

            uint8_t modrm = localCode[i + 1];
            bool validModrm = false;
            for (uint8_t m : modrmList) {
                if (modrm == m) { validModrm = true; break; }
            }
            if (!validModrm) continue;

            int32_t rel = *(int32_t*)(localCode + i + 3);
            uintptr_t targetLocal = (uintptr_t)&localCode[i] + 7 + rel;

            uintptr_t targetRemote = 0;
            if (targetLocal == (uintptr_t)&g_pHookData) {
                targetRemote = (uintptr_t)pDataPtrStorage;
                patchedData++;
            } else if (targetLocal == (uintptr_t)&g_pOriginalRenderObjects) {
                targetRemote = (uintptr_t)pOrigPtrStorage;
                patchedOrig++;
            } else {
                continue;
            }

            uintptr_t instrRemote = (uintptr_t)pShellcodeRemote + i;
            int32_t newRel = (int32_t)((intptr_t)targetRemote - (intptr_t)(instrRemote + 7));
            mem->writeBytes(instrRemote + 3, &newRel, 4);
        }
        printf("[Chams] patched data=%d orig=%d\n", patchedData, patchedOrig);

        if (patchedData == 0 || patchedOrig == 0) {
            printf("[Chams] FAILED — no globals found in shellcode. Hook aborted.\n");
            return false;
        }

        // Redirect vtable slot -> shellcode
        uint64_t shellAddr = (uint64_t)pShellcodeRemote;
        if (!mem->writeBytes(vtableTargetAddr, &shellAddr, 8)) {
            printf("[Chams] vtable write FAILED\n");
            return false;
        }

        // Verify vtable now points at shellcode
        uint64_t verify = mem->read<uint64_t>(vtableTargetAddr);
        if (verify != shellAddr) {
            printf("[Chams] vtable verification FAILED (0x%llX != 0x%llX)\n",
                   (unsigned long long)verify, (unsigned long long)shellAddr);
            return false;
        }

        installed = true;
        printf("[Chams] hook INSTALLED\n");
        return true;
    }

    void setEnabled(bool en) {
        if (!installed || !pDataRemote) return;
        int val = en ? 1 : 0;
        mem->write<int>((uintptr_t)pDataRemote +
                        offsetof(CAnimatableSceneObjectDescRenderHookData, bChamsEnabled),
                        val);
    }

    void setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (!installed || !pDataRemote) return;
        mem->write<uint8_t>((uintptr_t)pDataRemote +
            offsetof(CAnimatableSceneObjectDescRenderHookData, r), r);
        mem->write<uint8_t>((uintptr_t)pDataRemote +
            offsetof(CAnimatableSceneObjectDescRenderHookData, g), g);
        mem->write<uint8_t>((uintptr_t)pDataRemote +
            offsetof(CAnimatableSceneObjectDescRenderHookData, b), b);
        mem->write<uint8_t>((uintptr_t)pDataRemote +
            offsetof(CAnimatableSceneObjectDescRenderHookData, a), a);
    }

    void setMaterial(void* hMaterial) {
        if (!installed || !pDataRemote) return;
        mem->write<void*>((uintptr_t)pDataRemote +
            offsetof(CAnimatableSceneObjectDescRenderHookData, hMaterialToUse),
            hMaterial);
    }
};
