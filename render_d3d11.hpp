#pragma once
#include "config.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <windows.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace Render {
    inline ID3D11Device*           g_device    = nullptr;
    inline ID3D11DeviceContext*    g_context   = nullptr;
    inline IDXGISwapChain*         g_swap      = nullptr;
    inline ID3D11RenderTargetView* g_rtv       = nullptr;
    inline HWND                    g_hwnd      = nullptr;
    inline bool                    g_init      = false;

    inline LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return true;
        if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
        if (m == WM_ERASEBKGND) return 1;
        if (m == WM_MOUSEMOVE)  { gInput.mouse.x = GET_X_LPARAM(l); gInput.mouse.y = GET_Y_LPARAM(l); return 0; }
        if (m == WM_LBUTTONDOWN){ gInput.lmbDown = true; gInput.lmbJustPressed = true; SetCapture(h); return 0; }
        if (m == WM_LBUTTONUP)  { gInput.lmbDown = false; ReleaseCapture(); return 0; }
        return DefWindowProc(h, m, w, l);
    }

    inline bool CreateDevice(HWND hWnd) {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount        = 2;
        sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 0;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow       = hWnd;
        sd.SampleDesc.Count   = 1;
        sd.Windowed           = TRUE;
        sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;
        D3D_FEATURE_LEVEL fl;
        const D3D_FEATURE_LEVEL fls[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        HRESULT r = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            fls, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_device, &fl, &g_context);
        if (r == DXGI_ERROR_UNSUPPORTED)
            r = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                fls, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_device, &fl, &g_context);
        if (FAILED(r)) return false;

        ID3D11Texture2D* bb = nullptr;
        g_swap->GetBuffer(0, IID_PPV_ARGS(&bb));
        if (bb) { g_device->CreateRenderTargetView(bb, nullptr, &g_rtv); bb->Release(); }
        return true;
    }

    inline bool Init(HWND hwnd) {
        g_hwnd = hwnd;
        if (!CreateDevice(hwnd)) return false;
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        ImGui::StyleColorsDark();
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 8.0f; s.FrameRounding = 4.0f; s.GrabRounding = 4.0f;
        s.WindowPadding = ImVec2(12,12); s.FramePadding = ImVec2(8,6); s.ItemSpacing = ImVec2(8,8);
        ImVec4* c = s.Colors;
        c[ImGuiCol_WindowBg]       = ImVec4(0.05f,0.05f,0.07f,0.98f);
        c[ImGuiCol_Border]         = ImVec4(0.13f,0.11f,0.17f,1.00f);
        c[ImGuiCol_FrameBg]        = ImVec4(0.08f,0.08f,0.11f,1.00f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.11f,0.11f,0.15f,1.00f);
        c[ImGuiCol_CheckMark]      = ImVec4(0.66f,0.33f,0.97f,1.00f);
        c[ImGuiCol_SliderGrab]     = ImVec4(0.66f,0.33f,0.97f,1.00f);
        c[ImGuiCol_SliderGrabActive]=ImVec4(0.85f,0.27f,0.94f,1.00f);
        c[ImGuiCol_Button]         = ImVec4(0.11f,0.09f,0.15f,1.00f);
        c[ImGuiCol_ButtonHovered]  = ImVec4(0.66f,0.33f,0.97f,0.60f);
        c[ImGuiCol_Header]         = ImVec4(0.13f,0.11f,0.17f,1.00f);
        c[ImGuiCol_HeaderHovered]  = ImVec4(0.20f,0.16f,0.28f,1.00f);
        c[ImGuiCol_HeaderActive]   = ImVec4(0.66f,0.33f,0.97f,0.60f);
        c[ImGuiCol_Tab]            = ImVec4(0.08f,0.08f,0.11f,1.00f);
        c[ImGuiCol_TabActive]      = ImVec4(0.66f,0.33f,0.97f,0.80f);
        c[ImGuiCol_TabHovered]     = ImVec4(0.66f,0.33f,0.97f,0.60f);

        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(g_device, g_context);
        g_init = true;
        return true;
    }

    inline void BeginFrame() {
        if (!g_init) return;
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    inline void EndFrame() {
        if (!g_init) return;
        ImGui::Render();
        const float clear[4] = { 0, 0, 0, 0 };
        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_context->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        // VSync Present — caps at monitor refresh, zero CPU spin
        g_swap->Present(1, 0);
    }

    inline void Shutdown() {
        if (!g_init) return;
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        if (g_rtv)    g_rtv->Release();
        if (g_swap)   g_swap->Release();
        if (g_context)g_context->Release();
        if (g_device) g_device->Release();
        g_init = false;
    }

    inline bool CreateOverlayWindow() {
        WNDCLASSEXA wc{};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_CLASSDC;
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = GetModuleHandle(nullptr);
        wc.lpszClassName = "NullWareD3D11";
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassExA(&wc);

        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);

        // NO WS_EX_LAYERED — DWM handles transparency via margins
        g_hwnd = CreateWindowExA(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
            "NullWareD3D11", "NullWare",
            WS_POPUP, 0, 0, sw, sh,
            nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        if (!g_hwnd) return false;

        MARGINS m = { -1 };
        DwmExtendFrameIntoClientArea(g_hwnd, &m);

        gOverlay = g_hwnd;
        gW = sw; gH = sh;
        return Init(g_hwnd);
    }

    inline void SetClickThrough(bool ct) {
        if (!g_hwnd) return;
        LONG_PTR ex = GetWindowLongPtr(g_hwnd, GWL_EXSTYLE);
        if (ct) ex |= WS_EX_TRANSPARENT;
        else    ex &= ~WS_EX_TRANSPARENT;
        SetWindowLongPtr(g_hwnd, GWL_EXSTYLE, ex);
    }
}