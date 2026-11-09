#include "LfoGraph.h"

#include <stdio.h>

// 2 bars, firmware waveform table, SHAPE = piecewise linear phase warp (0.5 neutral), PHASE,
// INVERT, DEPTH. 16 read points (eighths) as nodes; JITTER range as GRAY_50 vertical lines and a
// fixed pseudo random offset of the node inside it. Composer scratch: f 0 amp, 1 skew, 2 jitter px.

namespace panel {

const LfoGraph lfoGraph;

void LfoGraph::axes(const View& v, Axes& a) const {
    (void)v;
    addPercentGrid(a);
    addBottom(a, kGraphX0, "BAR 1");
    addBottom(a, kGraphX0 + kGraphW / 2, "BAR 2");
    a.bottomCyanLeft = true;
}

uint8_t LfoGraph::info(const View& v, char items[kInfoItems][kInfoItemLen]) const {
    uint8_t n = 0;
    snprintf(items[n++], kInfoItemLen, "%s", v.name1);
    snprintf(items[n++], kInfoItemLen, "%s BAR", v.name2);
    return n;
}

int16_t LfoGraph::y(const Composer& c, const View& v, int16_t i) const {
    const LfoView& l = v.lfo;
    float u = (float)i / (float)kGraphW * 2.0f * l.cyclesPerBar + l.phase;
    u -= floorf(u);
    if (l.holdSteps > 0) u = (floorf(u * (float)l.holdSteps) + 0.5f) / (float)l.holdSteps;
    float sk = c.f[1];
    u = (u < sk) ? (0.5f * u / sk) : (0.5f + 0.5f * (u - sk) / (1.0f - sk));
    float w = 0.0f;
    if ((l.table != nullptr) && (l.tableLen > 0)) {
        int32_t k = (int32_t)(u * (float)l.tableLen);
        if (k >= l.tableLen) k = l.tableLen - 1;
        if (k < 0) k = 0;
        w = l.table[k] * 2.0f - 1.0f;
    }
    if (l.invert) w = -w;
    return clampi(roundi((float)kGraphCY - w * c.f[0]), kGraphY0, kGraphY1);
}

void LfoGraph::begin(Composer& c, const View& v) const {
    const LfoView& l = v.lfo;
    c.f[0] = (float)(kGraphY1 - kGraphCY) * l.depth;
    c.f[1] = clampf(l.shape, 0.02f, 0.98f);
    c.f[2] = (float)roundi(l.jitter * c.f[0]);
    uint64_t r = 11;
    for (uint8_t k = 0; k < 16; k++) {
        int16_t x = kGraphX0 + roundi((float)k / 16.0f * (float)kGraphW);
        r = (r * 16807ULL) % 2147483647ULL;
        float rnd = (float)r / 2147483647.0f;
        int16_t yk = y(c, v, x - kGraphX0);
        c.addNode(x, clampi(roundi((float)yk + (rnd * 2.0f - 1.0f) * c.f[2]), kGraphY0, kGraphY1), l.active ? kCyan : kGray50);
    }
}

void LfoGraph::column(Composer& c, const View& v, int16_t i) const {
    const LfoView& l = v.lfo;
    int16_t yi = y(c, v, i);
    Color fill = l.active ? kCyanDark : kGray15;
    if (yi < kGraphCY) c.rows(yi + 1, kGraphCY, fill);
    else if (yi > kGraphCY) c.rows(kGraphCY, yi - 1, fill);
    c.grid(i);
    c.span(0, yi, l.active ? kWhite : kGray50);
    int16_t j = (int16_t)c.f[2];
    if ((j > 0) && l.active) {
        for (uint8_t k = 0; k < c.nodeCount(); k++) {
            if (c.node(k).x - kGraphX0 == i) c.rows(clampi(yi - j, kGraphY0, kGraphY1), clampi(yi + j, kGraphY0, kGraphY1), kGray50);
        }
    }
}

}  // namespace panel
