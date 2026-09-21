#pragma once
#include "config.hpp"
#include "render_d3d11.hpp"
#include "SkinChanger.h"
#include "imgui/imgui.h"
#include <cstring>
#include <string>
#include <algorithm>

inline void drawMenu() {
    if (!gMenuOpen) return;

    ImGui::SetNextWindowSize(ImVec2(760, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2((float)gMenuX, (float)gMenuY), ImGuiCond_FirstUseEver);
    ImGui::Begin("NullWare v17.0", &gMenuOpen,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    // Sync drag position back to globals so hotkey-toggle keeps position
    ImVec2 wp = ImGui::GetWindowPos();
    gMenuX = (int)wp.x; gMenuY = (int)wp.y;

    if (ImGui::BeginTabBar("MainTabs")) {

        // ── VISUALS ──
        if (ImGui::BeginTabItem("Visuals")) {
            ImGui::BeginChild("vL", ImVec2(360, 0));
            ImGui::TextColored(ImVec4(0.66f,0.33f,0.97f,1), "ESP FEATURES");
            ImGui::Separator();
            ImGui::Checkbox("Box ESP",       &gMenu.box);
            ImGui::Checkbox("Box Filled",    &gMenu.boxFilled);
            ImGui::Checkbox("Health Bar",    &gMenu.health);
            ImGui::Checkbox("Armor Bar",     &gMenu.armor);
            ImGui::Checkbox("Name",          &gMenu.name);
            ImGui::Checkbox("Distance",      &gMenu.distance);
            ImGui::Checkbox("Skeleton",      &gMenu.skeleton);
            ImGui::Checkbox("Snapline",      &gMenu.snapline);
            ImGui::Checkbox("Head Dot",      &gMenu.headDot);
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("vR", ImVec2(0, 0));
            ImGui::TextColored(ImVec4(0.66f,0.33f,0.97f,1), "SETTINGS");
            ImGui::Separator();
            ImGui::Checkbox("Team Check", &gMenu.teamCheck);
            ImGui::Checkbox("Full Box",   &gMenu.fullBox);
            ImGui::SliderFloat("Thickness",  &gMenu.boxThickness,    1.0f, 6.0f, "%.0f");
            ImGui::SliderFloat("Padding X",  &gMenu.boxBonePaddingX, 0.0f, 0.5f, "%.2f");
            ImGui::SliderFloat("Padding Y",  &gMenu.boxBonePaddingY, 0.0f, 0.3f, "%.2f");
            ImGui::SliderFloat("Skel Width", &gMenu.skeletonThick,   1.0f, 4.0f, "%.1f");
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // ── COMBAT ──
        if (ImGui::BeginTabItem("Combat")) {
            ImGui::TextColored(ImVec4(0.66f,0.33f,0.97f,1), "TRIGGER BOT");
            ImGui::Separator();
            ImGui::Checkbox("Enable",        &gCombat.triggerBot);
            ImGui::Checkbox("Team Check",    &gCombat.teamCheck);
            ImGui::Checkbox("Visible Only",  &gCombat.visCheck);
            ImGui::Checkbox("Seed Trigger",  &gCombat.useSeedTrigger);

            ImGui::Separator();
            ImGui::Text("Trigger Key (hold to fire):");
            ImGui::SetNextItemWidth(260.0f);
            ImGui::Combo("##trigkey", &gCombat.triggerKey, gTriggerKeyNames, gTriggerKeyCount);
            ImGui::TextColored(ImVec4(0.5f,0.7f,1.0f,1),
                "Active: %s", gTriggerKeyNames[gCombat.triggerKey]);
            ImGui::SliderFloat("Hit Chance", &gCombat.hitChance, 0.5f, 1.0f, "%.2f");
            ImGui::SliderInt("Delay",        &gCombat.delayMs,    0, 300);
            ImGui::SliderInt("Hold",         &gCombat.holdMs,     5, 200);
            ImGui::SliderInt("Cooldown",     &gCombat.cooldownMs, 10, 1000);
            ImGui::EndTabItem();
        }

        // ── SKINS ──
        if (ImGui::BeginTabItem("Skins")) {
            if (!gSkinChanger) {
                ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "SkinChanger not initialized!");
            } else {
                ImGui::Checkbox("Enable Skinchanger", &gSkinChanger->enabled);
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.66f,0.33f,0.97f,1), "(client-side visual only)");
                ImGui::Separator();

                ImGui::BeginChild("skinL", ImVec2(360, 0));

                ImGui::Text("Wear (0.0 = FN, 1.0 = BS)");
                ImGui::SliderFloat("##wear", &gSkinChanger->wear, 0.0f, 1.0f, "%.3f");
                ImGui::Text("Pattern Seed");
                ImGui::SliderInt("##seed", &gSkinChanger->seed, 0, 1000);
                ImGui::Checkbox("StatTrak", &gSkinChanger->statTrak);
                if (gSkinChanger->statTrak) {
                    ImGui::Text("Kill Count");
                    ImGui::SliderInt("##kills", &gSkinChanger->statTrakKills, 0, 9999);
                }

                ImGui::Separator();
                ImGui::Text("Search:");
                static char searchBuf[64] = "";
                ImGui::InputText("##search", searchBuf, sizeof(searchBuf));
                ImGui::EndChild();

                ImGui::SameLine();
                ImGui::BeginChild("skinR", ImVec2(0, 0));

                ImGui::TextColored(ImVec4(0.66f,0.33f,0.97f,1), "SELECT SKIN");
                ImGui::Separator();

                // Lower-case query for filtering
                std::string query = searchBuf;
                std::transform(query.begin(), query.end(), query.begin(), ::tolower);

                ImGui::BeginChild("SkinList", ImVec2(0, 0), true);
                for (int i = 0; i < g_skinCount; ++i) {
                    std::string skinName = g_skinList[i].name;
                    std::string weaponName = g_skinList[i].weapon;
                    std::string skinLower = skinName;
                    std::string weaponLower = weaponName;
                    std::transform(skinLower.begin(), skinLower.end(), skinLower.begin(), ::tolower);
                    std::transform(weaponLower.begin(), weaponLower.end(), weaponLower.begin(), ::tolower);

                    if (!query.empty()) {
                        if (skinLower.find(query) == std::string::npos &&
                            weaponLower.find(query) == std::string::npos) continue;
                    }

                    char label[160];
                    sprintf_s(label, "%-10s | %s", g_skinList[i].weapon, g_skinList[i].name);
                    bool selected = (gSkinChanger->selectedSkinIdx == i);
                    if (ImGui::Selectable(label, selected)) {
                        gSkinChanger->selectedSkinIdx = i;
                        if (g_skinShm) {
                            g_skinShm->paintKit = g_skinList[i].paintKit;
                            g_skinShm->forceUpdate = 1;
                        }
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndChild();
                ImGui::EndChild();

                ImGui::Separator();
                if (g_skinShm) {
                    ImGui::TextColored(ImVec4(0.3f,1.0f,0.5f,1),
                        "Current: %s | %s  (paintkit %d)",
                        gSkinChanger->currentWeaponName(),
                        gSkinChanger->currentSkinName(),
                        g_skinList[gSkinChanger->selectedSkinIdx].paintKit);
                    ImGui::TextColored(ImVec4(0.5f,0.7f,1.0f,1),
                        "DLL: %s | Heartbeat: %u | LastKit: %d | LastDefIdx: %d",
                        g_skinShm->dllAttached ? "ATTACHED" : "NOT ATTACHED",
                        g_skinShm->dllHeartbeat,
                        g_skinShm->lastAppliedKit,
                        g_skinShm->lastDefIdx);
                }
            }
            ImGui::EndTabItem();
        }

        // ── STATUS ──
        if (ImGui::BeginTabItem("Status")) {
            ImGui::TextColored(ImVec4(0.66f,0.33f,0.97f,1), "SYSTEM");
            ImGui::Separator();
            ImGui::Text("Attached:     %s", gAttachedToCs2 ? "YES" : "NO");
            ImGui::Text("CS2 Focus:    %s", (gCs2Hwnd && IsWindow(gCs2Hwnd)) ? "YES" : "NO");
            ImGui::Text("Renderer:     D3D11 + ImGui + VSync");
            ImGui::Text("Priority:     REALTIME");
            ImGui::Text("FPS:          %.1f", ImGui::GetIO().Framerate);
            if (g_skinShm) {
                ImGui::Separator();
                ImGui::Text("DLL Attached: %s", g_skinShm->dllAttached ? "YES" : "NO");
                ImGui::Text("DLL Heartbeat: %u", g_skinShm->dllHeartbeat);
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

inline void updateMenuInput() {}