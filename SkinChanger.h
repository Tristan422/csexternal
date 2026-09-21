#pragma once
#include "config.hpp"
#include <cstdint>

struct SkinEntry { int paintKit; const char* name; const char* weapon; };

inline const SkinEntry g_skinList[] = {
    { 180,  "Fire Serpent",    "AK-47"  }, { 302,  "Vulcan",          "AK-47"  },
    { 639,  "Bloodsport",      "AK-47"  }, { 675,  "Neon Rider",      "AK-47"  },
    { 801,  "Asiimov",         "AK-47"  }, { 309,  "Howl",            "M4A4"   },
    { 695,  "Dragon King",     "M4A4"   }, { 400,  "Asiimov",         "M4A4"   },
    { 497,  "Hyper Beast",     "M4A1-S" }, { 600,  "Golden Coil",     "M4A1-S" },
    { 984,  "Printstream",     "M4A1-S" }, { 344,  "Dragon Lore",     "AWP"    },
    { 279,  "Asiimov",         "AWP"    }, { 803,  "Gungnir",         "AWP"    },
    { 899,  "Wildfire",        "AWP"    }, { 37,   "Blaze",           "Deagle" },
    { 527,  "Kumicho Dragon",  "Deagle" }, { 711,  "Code Red",        "Deagle" },
    { 504,  "Kill Confirmed",  "USP-S"  }, { 653,  "Neo-Noir",        "USP-S"  },
    { 705,  "Cortex",          "USP-S"  }, { 38,   "Fade",            "Glock"  },
    { 586,  "Water Elemental", "Glock"  }, { 568,  "Doppler",         "Knife"  },
    { 569,  "Doppler Phase 2", "Knife"  }, { 415,  "Case Hardened",   "Knife"  },
    { 414,  "Marble Fade",     "Knife"  },
};
inline constexpr int g_skinCount = sizeof(g_skinList) / sizeof(g_skinList[0]);

class SkinChanger {
public:
    bool  enabled = false;
    int   selectedSkinIdx = 0;
    float wear = 0.01f;
    int   seed = 420;
    bool  statTrak = false;
    int   statTrakKills = 1337;

    SkinChanger() = default;
    SkinChanger(void*, void*, uintptr_t) {}

    const char* currentSkinName() const {
        if (selectedSkinIdx < 0 || selectedSkinIdx >= g_skinCount) return "None";
        return g_skinList[selectedSkinIdx].name;
    }
    const char* currentWeaponName() const {
        if (selectedSkinIdx < 0 || selectedSkinIdx >= g_skinCount) return "None";
        return g_skinList[selectedSkinIdx].weapon;
    }
};