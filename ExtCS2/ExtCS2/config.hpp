#pragma once
#include <windows.h>
#include <cstdint>

namespace Off {
    constexpr uintptr_t dwGameEntitySystem      = 0x2577BE0;
    constexpr uintptr_t dwLocalPlayerController = 0x23A78D0;
    constexpr uintptr_t dwLocalPlayerPawn       = 0x23CCC08;
    constexpr uintptr_t dwViewMatrix            = 0x23D21F0;

    constexpr uintptr_t m_iHealth               = 0x34C;
    constexpr uintptr_t m_iTeamNum              = 0x3E7;
    constexpr uintptr_t m_vOldOrigin            = 0x13B8;
    constexpr uintptr_t m_pGameSceneNode        = 0x330;
    constexpr uintptr_t m_modelState            = 0x140;
    constexpr uintptr_t m_boneArray             = 0x80;
    constexpr uintptr_t m_hPlayerPawn           = 0x914;
    constexpr uintptr_t m_ArmorValue            = 0x1CA4;
    constexpr uintptr_t m_entitySpottedState    = 0x1C60;
    constexpr uintptr_t m_pWeaponServices       = 0x1208;
    constexpr uintptr_t m_hActiveWeapon         = 0x58;
    constexpr uintptr_t m_iIDEntIndex           = 0x342C;
    constexpr uintptr_t m_nLastShotSeed         = 0x16D8;

    constexpr uintptr_t spotted_bSpotted        = 0x08;
    constexpr uintptr_t spotted_bSpottedByMask0 = 0x0C;
    constexpr uintptr_t spotted_bSpottedByMask1 = 0x10;

    constexpr uintptr_t boneStride = 32;
}

namespace Bone {
    constexpr int HEAD=7, NECK=6, SPINE1=3, SPINE2=4, PELVIS=1, CHEST=23;
    constexpr int LSHOULDER=9, LELBOW=10, LHAND=11;
    constexpr int RSHOULDER=13, RELBOW=14, RHAND=15;
    constexpr int LHIP=17, LKNEE=18, LFOOT=19;
    constexpr int RHIP=20, RKNEE=21, RFOOT=22;
}

struct BoneConn { int from, to; };
static constexpr BoneConn gSkeletonConnections[] = {
    {Bone::HEAD, Bone::NECK}, {Bone::NECK, Bone::SPINE1},
    {Bone::SPINE1, Bone::SPINE2}, {Bone::SPINE2, Bone::PELVIS},
    {Bone::NECK, Bone::LSHOULDER}, {Bone::LSHOULDER, Bone::LELBOW},
    {Bone::LELBOW, Bone::LHAND}, {Bone::NECK, Bone::RSHOULDER},
    {Bone::RSHOULDER, Bone::RELBOW}, {Bone::RELBOW, Bone::RHAND},
    {Bone::PELVIS, Bone::LHIP}, {Bone::LHIP, Bone::LKNEE},
    {Bone::LKNEE, Bone::LFOOT}, {Bone::PELVIS, Bone::RHIP},
    {Bone::RHIP, Bone::RKNEE}, {Bone::RKNEE, Bone::RFOOT},
};
constexpr int gSkeletonConnCount = sizeof(gSkeletonConnections)/sizeof(gSkeletonConnections[0]);

static constexpr int gBoxBones[] = {
    Bone::HEAD, Bone::NECK, Bone::SPINE1, Bone::SPINE2,
    Bone::PELVIS, Bone::CHEST,
    Bone::LSHOULDER, Bone::LELBOW, Bone::LHAND,
    Bone::RSHOULDER, Bone::RELBOW, Bone::RHAND,
    Bone::LHIP, Bone::LKNEE, Bone::LFOOT,
    Bone::RHIP, Bone::RKNEE, Bone::RFOOT,
};
constexpr int gBoxBoneCount = sizeof(gBoxBones)/sizeof(gBoxBones[0]);

namespace T {
    const COLORREF bg          = RGB(8, 8, 12);
    const COLORREF panel       = RGB(14, 14, 20);
    const COLORREF panelAlt    = RGB(20, 20, 28);
    const COLORREF panelHover  = RGB(28, 28, 38);
    const COLORREF contentBg   = RGB(11, 11, 16);
    const COLORREF topBar      = RGB(16, 16, 22);
    const COLORREF sidebar     = RGB(10, 10, 15);
    const COLORREF border      = RGB(34, 28, 44);
    const COLORREF borderLight = RGB(52, 42, 68);
    const COLORREF divider     = RGB(28, 22, 38);
    const COLORREF accent      = RGB(168, 85, 247);
    const COLORREF accentBright= RGB(217, 70, 239);
    const COLORREF text        = RGB(230, 224, 240);
    const COLORREF textMuted   = RGB(140, 128, 158);
    const COLORREF textDim     = RGB(90, 82, 108);
    const COLORREF track       = RGB(30, 26, 40);
    const COLORREF danger      = RGB(210, 65, 95);
    const COLORREF dangerHover = RGB(235, 85, 115);
    const COLORREF success     = RGB(100, 210, 140);
    const COLORREF warning     = RGB(230, 180, 80);
}

struct CustomColor {
    float r=1.0f,g=1.0f,b=1.0f;
    COLORREF toCOLORREF() const {
        return RGB((BYTE)(r*255),(BYTE)(g*255),(BYTE)(b*255));
    }
};

enum ChamsStyle {
    CHAMS_OFF = 0,
    CHAMS_LATEX = 1,
    CHAMS_FLAT = 2,
    CHAMS_GLASS = 3,
    CHAMS_GLOW = 4,
};

struct MenuState {
    bool box=false, boxFilled=false, health=false, name=false;
    bool skeleton=false, teamCheck=false, useBone=false;
    bool snapline=false, distance=false, armor=false, headDot=false;
    bool fullBox=false, boxBoneBased=true;

    float boxThickness=2.0f;
    float boxWidthRatio=0.208f;
    float headClearance=7.0f;
    float headOffset=72.0f;
    float skeletonThick=1.5f;
    float boxBonePaddingX=0.15f;
    float boxBonePaddingY=0.05f;

    CustomColor boxColor      = {0.00f, 0.88f, 0.64f};
    CustomColor boxColorI     = {0.85f, 0.65f, 0.15f};
    CustomColor healthColor   = {1.00f, 0.22f, 0.35f};
    CustomColor healthColorI  = {1.00f, 0.55f, 0.10f};
    CustomColor armorColor    = {0.30f, 0.60f, 1.00f};
    CustomColor armorColorI   = {0.55f, 0.80f, 1.00f};
    CustomColor nameColor     = {0.94f, 0.94f, 0.98f};
    CustomColor nameColorI    = {0.80f, 0.60f, 0.30f};
    CustomColor filledColor   = {0.06f, 0.02f, 0.09f};
    CustomColor filledColorI  = {0.10f, 0.05f, 0.12f};
    CustomColor snaplineColor = {0.78f, 0.36f, 0.78f};
    CustomColor snaplineColorI= {0.95f, 0.55f, 0.85f};
    CustomColor distanceColor = {0.80f, 0.80f, 0.86f};
    CustomColor distanceColorI= {0.70f, 0.55f, 0.40f};
    CustomColor skeletonColor = {0.25f, 0.65f, 1.00f};
    CustomColor skeletonColorI= {0.80f, 0.50f, 1.00f};

    int   chamsStyle = 0;
    bool  chamsTeamCheck = true;
    float chamsColorR = 0.66f, chamsColorG = 0.33f, chamsColorB = 0.97f, chamsColorA = 1.0f;
};
extern MenuState gMenu;

struct CombatState {
    bool  triggerBot = false;
    int   triggerKey = 0;
    bool  teamCheck = true;
    bool  useSeedTrigger = false;
    float hitChance = 0.75f;
    int   delayMs = 20;
    int   holdMs = 30;
    int   cooldownMs = 150;
    bool  visCheck = true;
};
extern CombatState gCombat;

extern const char* gTriggerKeyNames[];
constexpr int gTriggerKeyCount = 8;

extern HWND gOverlay;
extern HWND gCs2Hwnd;
extern HDC  gHdc;
extern HDC  gMemDC;
extern HBITMAP gMemBmp;
extern HBITMAP gOldBmp;
extern int  gW;
extern int  gH;
extern bool gRunning;
extern bool gMenuOpen;
extern bool gAttachedToCs2;

struct InputState {
    POINT mouse={0,0};
    bool  lmbDown=false, lmbJustPressed=false;
};
extern InputState gInput;

extern int gMenuX, gMenuY;
extern bool gMenuDragging;
extern POINT gMenuDragOffset;

constexpr int MENU_W = 780;
constexpr int MENU_H = 680;
constexpr int TOPBAR_H = 44;
constexpr int BOTTOM_H = 32;
constexpr int SIDEBAR_W = 155;
extern int gTopTab;
extern int gSubTab;

extern bool gPickerOpen;
extern POINT gPickerPos;
extern CustomColor* gPickerRef;
extern const char* gPickerTitle;
extern bool gSliderDrag;
extern int gSliderActive;

void fillRect(float x1,float y1,float x2,float y2,COLORREF c);
void strokeRect(float x1,float y1,float x2,float y2,COLORREF c,int t=1);
void line(float x1,float y1,float x2,float y2,COLORREF c,int t=1);
void text(float x,float y,const char* s,COLORREF c,int sz=13,int weight=FW_NORMAL);
int  textWidth(const char* s,int sz=13,int weight=FW_NORMAL);
bool over(int mx,int my,int x,int y,int w,int h);
void circle(float cx,float cy,float r,COLORREF c);
void ellipseFilled(float cx,float cy,float rx,float ry,COLORREF c);
