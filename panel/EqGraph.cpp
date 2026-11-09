#include "EqGraph.h"

#include <stdio.h>

// Sum of the 6 band responses (firmware coefficients). Fill between 0 dB and the sum: boost
// CYAN_DARK, cut MAGENTA_DARK. Each band with gain != 0 as a GRAY_50 curve, sum WHITE.
// Composer scratch: flag[b] band has gain, bq[b] band biquad.

namespace panel {

const EqGraph eqGraph;

namespace {

int16_t eqDbToY(float d) {
    // 0 dB in the middle: upper half +12 dB, lower half -24 dB
    const float half = (float)kGraphH * 0.5f;
    float y = (d >= 0.0f) ? ((float)kGraphCY - d / 12.0f * half) : ((float)kGraphCY + d / -24.0f * half);
    return clampi(roundi(y), kGraphY0, kGraphY1);
}

}  // namespace

void EqGraph::axes(const View& v, Axes& a) const {
    (void)v;
    static const int8_t dbs[] = {12, 6, 0, -12, -24};
    for (uint8_t k = 0; k < 5; k++) {
        char t[6];
        (dbs[k] > 0) ? snprintf(t, sizeof(t), "+%d", dbs[k]) : snprintf(t, sizeof(t), "%d", dbs[k]);
        addRight(a, eqDbToY((float)dbs[k]), t);
    }
    addFreqRuler(a);
}

uint8_t EqGraph::info(const View& v, char items[kInfoItems][kInfoItemLen]) const {
    (void)v;
    (void)items;
    return 0;
}

void EqGraph::begin(Composer& c, const View& v) const {
    const EqView& e = v.eq;
    for (uint8_t b = 0; b < 6; b++) {
        c.flag[b] = (e.gainDb[b] != 0.0f);
        c.bq[b] = c.flag[b] ? eqBiquad(e.band[b], e.freq[b], e.q[b], e.gainDb[b]) : kIdentity;
    }
    for (uint8_t b = 0; b < 6; b++) {
        if ((e.freq[b] < kFreqMin) || (e.freq[b] > kFreqMax)) continue;
        int16_t x = freqToX(e.freq[b]);
        c.addNode(x, eqDbToY(totalDb(c, x - kGraphX0, nullptr)), e.active ? kCyan : kGray50);
    }
}

float EqGraph::totalDb(const Composer& c, int16_t i, float* bands) const {
    float sum = 0.0f;
    for (uint8_t b = 0; b < 6; b++) {
        float d = c.flag[b] ? biquadDbAt(c.bq[b], i) : 0.0f;
        if (bands != nullptr) bands[b] = d;
        sum += d;
    }
    return sum;
}

void EqGraph::column(Composer& c, const View& v, int16_t i) const {
    float bands[6];
    const bool act = v.eq.active;
    int16_t y = eqDbToY(totalDb(c, i, bands));
    if (y < kGraphCY) c.rows(y + 1, kGraphCY, act ? kCyanDark : kGray15);
    else if (y > kGraphCY) c.rows(kGraphCY + 1, y, act ? kMagentaDark : kGray15);
    c.grid(i);
    // band curves only where the band does something (keeps the 0 dB line dotted and clean)
    for (uint8_t b = 0; b < 6; b++) c.span(b, (c.flag[b] && (fabsf(bands[b]) >= 0.5f)) ? eqDbToY(bands[b]) : -1, kGray50);
    c.span(6, y, v.eq.active ? kWhite : kGray50);
}

}  // namespace panel
