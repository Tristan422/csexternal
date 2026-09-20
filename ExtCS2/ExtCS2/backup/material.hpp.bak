#pragma once
#include "config.hpp"
#include "mem.hpp"
#include <string>
#include <vector>

#define CREATE_MATERIAL_FN_PATTERN "48 89 5C 24 ?? 48 89 6C 24 ?? 56 57 41 56 48 81 EC ?? ?? ?? ?? 48 8B 05"
#define LOAD_KV3_EXPORT            "?LoadKV3@@YA_NPEAVKeyValues3@@PEAVCUtlString@@PEBDAEBUKV3ID_t@@2I@Z"

struct KV3ID_t {
    const char* szName;
    uint64_t    unk0;
    uint64_t    unk1;
};

struct CreateMaterialCtx {
    void*  kv;
    void*  loadKv3;
    void*  createMaterial;
    void*  pCreatedMaterial;
    void*  pMaterialSystem;
    const char* szMat;
    const char* szMatName;
    const char* szKvIdName;
    int    bHadErrors;
    int    bFinished;
};

inline const char* LATEX_CHAMS_KV3() {
    return R"(
{
    shader = "csgo_character.vfx"
    F_DISABLE_Z_BUFFERING = 1
    F_DISABLE_Z_PREPASS = 1
    F_DISABLE_Z_WRITE = 1
    F_BLEND_MODE = 1
    g_vColorTint = [1.0, 1.0, 1.0, 1.0]
    g_bFogEnabled = 0
    g_flMetalness = 0.000
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
    g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
    g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
})";
}

#pragma code_seg(".CreateMaterialSeg")
#pragma optimize("", off)
#pragma runtime_checks("", off)
#pragma check_stack(off)
inline __declspec(safebuffers) DWORD WINAPI CreateMaterialThread(LPVOID lpParam) {
    CreateMaterialCtx* ctx = reinterpret_cast<CreateMaterialCtx*>(lpParam);
    char* kv = (char*)ctx->kv;
    for (int i = 0; i < 0x1000; ++i) kv[i] = 0;

    KV3ID_t kvId{};
    kvId.szName = ctx->szKvIdName;
    kvId.unk0 = 0x469806E97412167CULL;
    kvId.unk1 = 0xE73790B53EE6F2AFULL;

    typedef bool(__fastcall* LoadKV3Fn)(void*, void*, const char*, void*, void*);
    LoadKV3Fn LoadKV3 = reinterpret_cast<LoadKV3Fn>(ctx->loadKv3);
    bool ok = LoadKV3(kv, nullptr, ctx->szMat, &kvId, nullptr);
    if (!ok) { ctx->bHadErrors = 1; ctx->bFinished = 1; return 0; }

    typedef void (*CreateMaterialFn)(void*, void**, const char*, void*, int, int);
    CreateMaterialFn CreateMaterial = reinterpret_cast<CreateMaterialFn>(ctx->createMaterial);
    void* mat = nullptr;
    CreateMaterial(nullptr, &mat, ctx->szMatName, kv, 0, 1);
    ctx->pCreatedMaterial = mat;
    ctx->bHadErrors = mat ? 0 : 1;
    ctx->bFinished = 1;
    return mat ? 1 : 0;
}
inline DWORD WINAPI CreateMaterialThreadEnd() { return 0; }
#pragma check_stack()
#pragma runtime_checks("", restore)
#pragma optimize("", on)
#pragma code_seg()

class MaterialSystem {
public:
    Memory* mem;
    MaterialSystem(Memory* m) : mem(m) {}

    void* createMaterial(const std::string& kvStr, const std::string& matName) {
        RemoteModule tier0 = mem->getRemoteModule("tier0.dll");
        RemoteModule matSys = mem->getRemoteModule("materialsystem2.dll");
        if (!tier0.valid() || !matSys.valid()) {
            printf("[MaterialSystem] missing tier0/materialsystem2\n");
            return nullptr;
        }

        uintptr_t loadKv3 = mem->getRemoteProcAddress("tier0.dll", LOAD_KV3_EXPORT);
        if (!loadKv3) { printf("[MaterialSystem] LoadKV3 export missing\n"); return nullptr; }

        uintptr_t createFn = mem->scanMemory(matSys.base, matSys.size, CREATE_MATERIAL_FN_PATTERN);
        if (!createFn) { printf("[MaterialSystem] CreateMaterial pattern miss\n"); return nullptr; }

        void* pKvStr  = mem->allocateAndWriteString(kvStr);
        void* pMatNm  = mem->allocateAndWriteString(matName);
        void* pKvIdNm = mem->allocateAndWriteString("genericdata_go");
        if (!pKvStr || !pMatNm || !pKvIdNm) { printf("[MaterialSystem] string alloc failed\n"); return nullptr; }

        void* pKv = mem->alloc(0x1000);
        if (!pKv) { printf("[MaterialSystem] kv alloc failed\n"); return nullptr; }

        CreateMaterialCtx ctx{};
        ctx.kv = pKv;
        ctx.loadKv3 = (void*)loadKv3;
        ctx.createMaterial = (void*)createFn;
        ctx.pCreatedMaterial = nullptr;
        ctx.pMaterialSystem = (void*)matSys.base;
        ctx.szMat = (const char*)pKvStr;
        ctx.szMatName = (const char*)pMatNm;
        ctx.szKvIdName = (const char*)pKvIdNm;
        ctx.bHadErrors = 0;
        ctx.bFinished = 0;

        void* pCtxRemote = mem->alloc(sizeof(CreateMaterialCtx));
        if (!pCtxRemote || !mem->writeBytes((uintptr_t)pCtxRemote, &ctx, sizeof(ctx))) {
            printf("[MaterialSystem] ctx write failed\n");
            return nullptr;
        }

        void* pShell = mem->allocAndWriteShellcode(
            (void*)&CreateMaterialThread,
            (void*)((char*)&CreateMaterialThread + 0x400));
        if (!pShell) { printf("[MaterialSystem] shellcode write failed\n"); return nullptr; }

        typedef HANDLE (WINAPI* CreateRemoteThread_t)(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
        static CreateRemoteThread_t pCreateRemoteThread =
            (CreateRemoteThread_t)GetProcAddress(GetModuleHandleA("kernel32.dll"), "CreateRemoteThread");
        HANDLE th = pCreateRemoteThread(mem->handle, nullptr, 0, (LPTHREAD_START_ROUTINE)pShell, pCtxRemote, 0, nullptr);
        if (!th) { printf("[MaterialSystem] CreateRemoteThread failed\n"); return nullptr; }
        WaitForSingleObject(th, 5000);
        CloseHandle(th);

        CreateMaterialCtx out{};
        mem->readBytes((uintptr_t)pCtxRemote, &out, sizeof(out));
        printf("[MaterialSystem] created material at 0x%p\n", out.pCreatedMaterial);
        return out.pCreatedMaterial;
    }
};
