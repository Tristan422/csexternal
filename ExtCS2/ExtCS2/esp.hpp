#pragma once
#include "config.hpp"
#include "entity.hpp"
#include "math.hpp"
#include "menu.hpp"
#include <float.h>
#include <unordered_map>

struct BoxBounds { float minX, minY, maxX, maxY; bool valid; };

// ---- smoothing state per pawn ----
struct SmoothBox { float minX=0, minY=0, maxX=0, maxY=0; bool init=false; };
static std::unordered_map<uintptr_t, SmoothBox> g_smoothBoxes;

inline float lerpF(float a, float b, float t) { return a + (b - a) * t; }

// ---- bone-based box: project ALL bones, min/max, pad, require N valid ----
inline BoxBounds computeBoneBox(EntityReader& rd, uintptr_t pawn, float matrix[16]) {
    BoxBounds bb = { FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX, false };
    int valid = 0;
    for (int i = 0; i < gBoxBoneCount; ++i) {
        Vec3 w;
        if (!rd.readBone(pawn, gBoxBones[i], w)) continue;
        ScreenPoint sp = worldToScreen(w, matrix, gW, gH);
        if (!sp.ok) continue;
        if (sp.x < bb.minX) bb.minX = sp.x;
        if (sp.y < bb.minY) bb.minY = sp.y;
        if (sp.x > bb.maxX) bb.maxX = sp.x;
        if (sp.y > bb.maxY) bb.maxY = sp.y;
        valid++;
    }
    // require at least 8 of 18 bones for a stable box
    if (valid < 8) return bb;
    bb.valid = true;
    float w = bb.maxX - bb.minX;
    float h = bb.maxY - bb.minY;
    bb.minX -= w * gMenu.boxBonePaddingX; bb.maxX += w * gMenu.boxBonePaddingX;
    bb.minY -= h * gMenu.boxBonePaddingY; bb.maxY += h * gMenu.boxBonePaddingY;
    return bb;
}

inline void drawESP(EntityReader& rd, Memory& mem, float matrix[16]) {
    int localTeam = rd.localTeam();
    Vec3 localOrigin = rd.localOrigin();
    uintptr_t localPawn = rd.localPawn();

    uintptr_t gsys = rd.gameEntitySystem();
    if (!gsys || !EntityReader::isPtr(gsys)) return;

    int localCtrlIdx = rd.localControllerIndex(gsys);

    for (int i = 1; i < 64; ++i) {
        uintptr_t ent = rd.resolveSlot(gsys, i);
        if (!ent || !EntityReader::isPtr(ent)) continue;

        int hp   = mem.read<int>(ent + Off::m_iHealth);
        int team = mem.read<int>(ent + Off::m_iTeamNum);
        uintptr_t pawn = ent;
        if (hp <= 0 || hp > 200 || team < 1 || team > 4) {
            uintptr_t p = rd.pawnFromController(gsys, ent);
            if (!p || !EntityReader::isPtr(p)) continue;
            pawn = p;
            hp   = mem.read<int>(pawn + Off::m_iHealth);
            team = mem.read<int>(pawn + Off::m_iTeamNum);
            if (hp <= 0 || hp > 200 || team < 1 || team > 4) continue;
        }

        // ---- SELF FILTER ----
        if (pawn == localPawn) continue;

        // ---- team filter ----
        if (gMenu.teamCheck && team == localTeam) continue;

        bool visible = rd.isVisibleByMask(pawn, localCtrlIdx);

        // ---- LOCKED BONE BOX ----
        BoxBounds bb = computeBoneBox(rd, pawn, matrix);
        if (!bb.valid) continue;

        float x1 = bb.minX, y1 = bb.minY, x2 = bb.maxX, y2 = bb.maxY;

        if (x2 - x1 < 3 || y2 - y1 < 3) continue;
        if (x1 < -gW || x2 > gW * 2 || y1 < -gH || y2 > gH * 2) continue;

        // ---- SMOOTHING (per pawn) ----
        auto& s = g_smoothBoxes[pawn];
        if (!s.init) {
            s.minX = x1; s.minY = y1; s.maxX = x2; s.maxY = y2;
            s.init = true;
        } else {
            const float t = 0.5f;
            s.minX = lerpF(s.minX, x1, t);
            s.minY = lerpF(s.minY, y1, t);
            s.maxX = lerpF(s.maxX, x2, t);
            s.maxY = lerpF(s.maxY, y2, t);
        }
        x1 = s.minX; y1 = s.minY; x2 = s.maxX; y2 = s.maxY;

        float cx = (x1 + x2) * 0.5f;

        // ---- SNAPLINE ----
        if (gMenu.snapline) {
            COLORREF sc = visible ? gMenu.snaplineColor.toCOLORREF()
                                  : gMenu.snaplineColorI.toCOLORREF();
            line((float)(gW / 2), (float)gH, cx, y2, sc, 1);
        }

        // ---- BOX ----
        if (gMenu.box) {
            if (gMenu.boxFilled) {
                COLORREF fc = visible ? gMenu.filledColor.toCOLORREF()
                                      : gMenu.filledColorI.toCOLORREF();
                fillRect(x1, y1, x2, y2, fc);
            }
            COLORREF c = visible ? gMenu.boxColor.toCOLORREF()
                                 : gMenu.boxColorI.toCOLORREF();
            int t = (int)gMenu.boxThickness; if (t < 1) t = 1;
            if (gMenu.fullBox) {
                strokeRect(x1, y1, x2, y2, c, t);
            } else {
                float len = min((x2 - x1) * 0.25f, (y2 - y1) * 0.25f);
                line(x1, y1, x1 + len, y1, c, t);
                line(x1, y1, x1, y1 + len, c, t);
                line(x2, y1, x2 - len, y1, c, t);
                line(x2, y1, x2, y1 + len, c, t);
                line(x1, y2, x1 + len, y2, c, t);
                line(x1, y2, x1, y2 - len, c, t);
                line(x2, y2, x2 - len, y2, c, t);
                line(x2, y2, x2, y2 - len, c, t);
            }
        }

        // ---- HEALTH BAR ----
        if (gMenu.health) {
            COLORREF hc = visible ? gMenu.healthColor.toCOLORREF()
                                  : gMenu.healthColorI.toCOLORREF();
            float frac = hp / 100.0f;
            float hy = y2 - (y2 - y1) * frac;
            fillRect(x1 - 6, y1, x1 - 3, y2, RGB(20, 20, 24));
            fillRect(x1 - 6, hy, x1 - 3, y2, hc);
        }

        // ---- ARMOR BAR ----
        if (gMenu.armor) {
            int armor = mem.read<int>(pawn + Off::m_ArmorValue);
            if (armor > 0) {
                COLORREF ac = visible ? gMenu.armorColor.toCOLORREF()
                                      : gMenu.armorColorI.toCOLORREF();
                float frac = armor / 100.0f; if (frac > 1.0f) frac = 1.0f;
                float ay = y2 - (y2 - y1) * frac;
                fillRect(x2 + 3, y1, x2 + 6, y2, RGB(20, 20, 24));
                fillRect(x2 + 3, ay, x2 + 6, y2, ac);
            }
        }

        // ---- NAME ----
        if (gMenu.name) {
            COLORREF nc = visible ? gMenu.nameColor.toCOLORREF()
                                  : gMenu.nameColorI.toCOLORREF();
            char buf[64]; sprintf_s(buf, "%d hp", hp);
            int tw = textWidth(buf, 13, FW_SEMIBOLD);
            text(cx - tw * 0.5f, y1 - 20, buf, nc, 13, FW_SEMIBOLD);
        }

        // ---- DISTANCE ----
        if (gMenu.distance) {
            COLORREF dc = visible ? gMenu.distanceColor.toCOLORREF()
                                  : gMenu.distanceColorI.toCOLORREF();
            Vec3 w = mem.read<Vec3>(pawn + Off::m_vOldOrigin);
            if (w.x == w.x) {
                float dx = w.x - localOrigin.x;
                float dy = w.y - localOrigin.y;
                float dz = w.z - localOrigin.z;
                float meters = sqrtf(dx * dx + dy * dy + dz * dz) * 0.01905f;
                char buf[32]; sprintf_s(buf, "%.0fm", meters);
                int dw = textWidth(buf, 11, FW_NORMAL);
                float ny = gMenu.name ? y1 - 34 : y1 - 20;
                text(cx - dw * 0.5f, ny, buf, dc, 11, FW_NORMAL);
            }
        }

        // ---- SKELETON ----
        if (gMenu.skeleton) {
            COLORREF sc = visible ? gMenu.skeletonColor.toCOLORREF()
                                  : gMenu.skeletonColorI.toCOLORREF();
            int thick = (int)gMenu.skeletonThick; if (thick < 1) thick = 1;
            for (int ci = 0; ci < gSkeletonConnCount; ++ci) {
                Vec3 a, b;
                if (!rd.readBone(pawn, gSkeletonConnections[ci].from, a)) continue;
                if (!rd.readBone(pawn, gSkeletonConnections[ci].to, b)) continue;
                ScreenPoint sa = worldToScreen(a, matrix, gW, gH);
                ScreenPoint sb = worldToScreen(b, matrix, gW, gH);
                if (!sa.ok || !sb.ok) continue;
                line(sa.x, sa.y, sb.x, sb.y, sc, thick);
            }
            if (gMenu.headDot) {
                Vec3 bh;
                if (rd.readBone(pawn, Bone::HEAD, bh)) {
                    ScreenPoint hs = worldToScreen(bh, matrix, gW, gH);
                    if (hs.ok) circle(hs.x, hs.y, 3.0f, sc);
                }
            }
        }
    }

    // ---- GC: remove smoothing entries for pawns no longer seen ----
    if (g_smoothBoxes.size() > 128) g_smoothBoxes.clear();
}
