#include "ReverbGraph.h"

#include <stdio.h>

// x 0..3000 ms, y 0..-60 dB. Tail = straight line in dB from t0 (PREDELAY + SIZE) at the wet
// level of MIX, -60 dB after RT60 (DECAY 200..3000 ms). DAMPING: steeper MAGENTA line
// (RT60 x (1 - 0.75 DAMPING)). Composer scratch: f 0 rt, 1 rth, 2 wdb, 3 t0.

namespace panel {

const ReverbGraph reverbGraph;

void ReverbGraph::axes(const View& v, Axes& a) const {
    (void)v;
    static const int8_t dbs[] = {0, -12, -24, -36, -48, -60};
    addDbGrid(a, 0.0f, -60.0f, dbs, 6);
    addTimeRuler(a, 3000, 500);
}

uint8_t ReverbGraph::info(const View& v, char items[kInfoItems][kInfoItemLen]) const {
    uint8_t n = 0;
    char t[16];
    fmt1(t, sizeof(t), (200.0f + v.reverb.decay * 2800.0f) / 1000.0f, false);
    snprintf(items[n++], kInfoItemLen, "RT60 %s S", t);
    snprintf(items[n++], kInfoItemLen, "PRE %d MS", (int)roundi(v.reverb.predelayMs));
    if (v.reverb.lowCutHz > 0.0f) {
        char f[16];
        fmtHz(f, sizeof(f), v.reverb.lowCutHz);
        snprintf(items[n++], kInfoItemLen, "CUT %s", f);
    }
    return n;
}

void ReverbGraph::begin(Composer& c, const View& v) const {
    const ReverbView& r = v.reverb;
    c.f[0] = 200.0f + r.decay * 2800.0f;
    c.f[1] = c.f[0] * (1.0f - r.damping * 0.75f);
    float wet = sinf(r.mix * kPi * 0.5f);
    c.f[2] = fmaxf(-60.0f, dbOf(fmaxf(wet, 1.0e-3f)));
    c.f[3] = r.predelayMs + 10.0f + r.size * 60.0f;
    auto X = [](float m) { return (int16_t)(kGraphX0 + roundi(clampf(m, 0.0f, 3000.0f) / 3000.0f * (float)kGraphW)); };
    Color cc = r.active ? kCyan : kGray50;
    c.addNode(X(c.f[3]), dbToY(c.f[2], 0.0f, -60.0f), cc);
    int16_t xe = X(c.f[3] + c.f[0] * (60.0f + c.f[2]) / 60.0f);
    if (xe < kGraphX1) c.addNode(xe, kGraphY1, cc);
    int16_t xh = X(c.f[3] + c.f[1] * (60.0f + c.f[2]) / 60.0f);
    if ((r.damping > 0.0f) && (xh < kGraphX1) && ((xe - xh) >= 24)) c.addNode(xh, kGraphY1, r.active ? kMagenta : kGray50);
}

void ReverbGraph::column(Composer& c, const View& v, int16_t i) const {
    const ReverbView& r = v.reverb;
    const bool act = r.active;
    float m = (float)i / (float)kGraphW * 3000.0f;
    int16_t y = -1, yh = -1;
    if (m < c.f[3]) {
        c.rows(dbToY(c.f[2], 0.0f, -60.0f), kGraphY1, kGray15);
    } else {
        float d = c.f[2] - 60.0f * (m - c.f[3]) / c.f[0];
        float dh = c.f[2] - 60.0f * (m - c.f[3]) / c.f[1];
        if (d > -60.0f) y = dbToY(d, 0.0f, -60.0f);
        if (dh > -60.0f) yh = dbToY(dh, 0.0f, -60.0f);
    }
    if (y >= 0) {
        int16_t h = ((r.damping > 0.0f) && (yh >= 0)) ? yh : kGraphY1;
        if (h > y) c.rows(y + 1, h, act ? ((r.damping > 0.0f) ? kMagentaDark : kCyanDark) : kGray15);
        if (h < kGraphY1) c.rows(h + 1, kGraphY1, act ? kCyanDark : kGray15);
    }
    c.grid(i);
    c.span(0, (r.damping > 0.0f) ? yh : -1, act ? kMagenta : kGray35);
    c.span(1, y, act ? kWhite : kGray50);
}

}  // namespace panel
