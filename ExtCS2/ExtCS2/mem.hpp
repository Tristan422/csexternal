#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#pragma comment(lib, "psapi.lib")

struct RemoteModule {
    uintptr_t base = 0;
    size_t    size = 0;
    bool valid() const { return base != 0; }
};

struct VTableFunctionInfo {
    uintptr_t vTableAddr = 0;
    int       index      = -1;
};

class Memory {
public:
    HANDLE    handle = nullptr;
    DWORD     pid    = 0;
    uintptr_t clientBase = 0;

    bool attach(const char* procName) {
        pid = getPid(procName);
        if (!pid) return false;
        handle = OpenProcess(
            PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_VM_OPERATION|PROCESS_QUERY_INFORMATION,
            FALSE, pid);
        if (!handle) return false;
        clientBase = getModuleBase(pid, "client.dll");
        return clientBase != 0;
    }

    template <typename T> T read(uintptr_t addr) {
        T out{};
        ReadProcessMemory(handle,(LPCVOID)addr,&out,sizeof(T),nullptr);
        return out;
    }
    bool readBytes(uintptr_t addr, void* buf, size_t size) {
        return ReadProcessMemory(handle,(LPCVOID)addr,buf,size,nullptr);
    }
    template <typename T> bool write(uintptr_t addr, T val) {
        return WriteProcessMemory(handle,(LPVOID)addr,&val,sizeof(T),nullptr) != 0;
    }
    bool writeBytes(uintptr_t addr, const void* data, size_t size) {
        return WriteProcessMemory(handle,(LPVOID)addr,(LPVOID)data,size,nullptr) != 0;
    }

    void* alloc(size_t size) {
        return VirtualAllocEx(handle, nullptr, size, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
    void* allocateAndWriteString(const std::string& str) {
        void* p = alloc(str.size() + 1);
        if (!p) return nullptr;
        if (!writeBytes((uintptr_t)p, str.c_str(), str.size() + 1)) {
            VirtualFreeEx(handle, p, 0, MEM_RELEASE);
            return nullptr;
        }
        return p;
    }
    void* allocAndWriteShellcode(void* localStart, void* localEnd) {
        size_t size = (uintptr_t)localEnd - (uintptr_t)localStart;
        void* p = alloc(size);
        if (!p) return nullptr;
        if (!writeBytes((uintptr_t)p, localStart, size)) {
            VirtualFreeEx(handle, p, 0, MEM_RELEASE);
            return nullptr;
        }
        return p;
    }

    RemoteModule getRemoteModule(const char* name) {
        RemoteModule rm{};
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) return rm;
        MODULEENTRY32 me{}; me.dwSize = sizeof(me);
        if (Module32First(snap, &me)) {
            do {
                if (_stricmp(me.szModule, name) == 0) {
                    rm.base = (uintptr_t)me.modBaseAddr;
                    rm.size = me.modBaseSize;
                    break;
                }
            } while (Module32Next(snap, &me));
        }
        CloseHandle(snap);
        return rm;
    }

    uintptr_t getRemoteProcAddress(const char* module, const char* proc) {
        HMODULE h = GetModuleHandleA(module);
        if (!h) return 0;
        FARPROC local = GetProcAddress(h, proc);
        if (!local) return 0;
        RemoteModule rm = getRemoteModule(module);
        if (!rm.valid()) return 0;
        return rm.base + ((uintptr_t)local - (uintptr_t)h);
    }

    uintptr_t scanMemory(uintptr_t base, size_t size, const char* pattern) {
        std::vector<uint8_t> buf(size);
        if (!readBytes(base, buf.data(), size)) return 0;
        auto toks = parsePattern(pattern);
        size_t plen = toks.size();
        if (plen == 0 || plen > size) return 0;
        for (size_t i = 0; i + plen <= size; ++i) {
            bool ok = true;
            for (size_t j = 0; j < plen; ++j) {
                int b = toks[j];
                if (b >= 0 && buf[i+j] != (uint8_t)b) { ok = false; break; }
            }
            if (ok) return base + i;
        }
        return 0;
    }

    VTableFunctionInfo findVTableContainingFunction(uintptr_t fnAddr, const char* moduleName) {
        VTableFunctionInfo out{};
        RemoteModule rm = getRemoteModule(moduleName);
        if (!rm.valid()) return out;
        std::vector<uint8_t> buf(rm.size);
        if (!readBytes(rm.base, buf.data(), rm.size)) return out;
        for (size_t i = 0; i + 8 <= rm.size; i += 8) {
            uint64_t qw = 0;
            memcpy(&qw, buf.data() + i, 8);
            if ((uintptr_t)qw != fnAddr) continue;
            size_t start = i;
            while (start >= 8) {
                uint64_t prev = 0;
                memcpy(&prev, buf.data() + start - 8, 8);
                if (prev < rm.base || prev >= rm.base + rm.size) break;
                start -= 8;
            }
            if ((i - start) % 8 != 0) continue;
            out.vTableAddr = rm.base + start;
            out.index = (int)((i - start) / 8);
            return out;
        }
        return out;
    }

private:
    static std::vector<int> parsePattern(const char* pat) {
        std::vector<int> out;
        while (*pat) {
            while (*pat == ' ') ++pat;
            if (!*pat) break;
            if (*pat == '?') { out.push_back(-1); ++pat; if (*pat=='?') ++pat; }
            else { char* end; long v = strtol(pat, &end, 16); out.push_back((int)v); pat = end; }
        }
        return out;
    }
    static DWORD getPid(const char* name) {
        HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (s == INVALID_HANDLE_VALUE) return 0;
        PROCESSENTRY32 p{}; p.dwSize = sizeof(p);
        DWORD found = 0;
        if (Process32First(s, &p)) {
            do { if (_stricmp(p.szExeFile, name) == 0) { found = p.th32ProcessID; break; } }
            while (Process32Next(s, &p));
        }
        CloseHandle(s);
        return found;
    }
    static uintptr_t getModuleBase(DWORD pid, const char* mod) {
        HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32, pid);
        if (s == INVALID_HANDLE_VALUE) return 0;
        MODULEENTRY32 m{}; m.dwSize = sizeof(m);
        uintptr_t base = 0;
        if (Module32First(s, &m)) {
            do { if (_stricmp(m.szModule, mod) == 0) { base = (uintptr_t)m.modBaseAddr; break; } }
            while (Module32Next(s, &m));
        }
        CloseHandle(s);
        return base;
    }
};
