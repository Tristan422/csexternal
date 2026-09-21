#pragma once
#include <cmath>

struct Vec3 { float x,y,z; };
struct ScreenPoint { float x, y, w; bool ok; };

inline ScreenPoint worldToScreen(Vec3 p, float m[16], int w, int h) {
    float cx = p.x*m[0] + p.y*m[1] + p.z*m[2] + m[3];
    float cy = p.x*m[4] + p.y*m[5] + p.z*m[6] + m[7];
    float cw = p.x*m[12] + p.y*m[13] + p.z*m[14] + m[15];
    if (cw < 0.01f) return {0,0,0,false};
    return { (w/2.0f)*((cx/cw)+1.0f), (h/2.0f)*(1.0f-(cy/cw)), cw, true };
}
