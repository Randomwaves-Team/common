#include "FilterGraph.h"

#include <stdio.h>

// Model (views.js FIL): 06 dB = one-pole (LP / HP), 12 = biquad, 24 = biquad x biquad (Q 0.707);
// BP / BS use max(0.5, Q / 2) (x 0.5 at 06). MIX: (1 - MIX) + MIX * H. ENV: cutoff x 2^(+-3 oct).
// Composer scratch: f 0 fc, 1 fe, 2 mix; flag 0 onePole, 1 envOn; bq 0/1 main, 2/3 env.

namespace panel {

const FilterGraph filterGraph;

void FilterGraph::axes(const View& v, Axes& a) const {
    (void)v;
    static const int8_t dbs[] = {18, 6, -6, -18, -30, -48};
    addDbGrid(a, 18.0f, -48.0f, dbs, 6);
    addFreqRuler(a);
}

uint8_t FilterGraph::info(const View& v, char items[kInfoItems][kInfoItemLen]) const {
    static const char* const slopes[] = {"06 DB", "12 DB", "24 DB"};
    uint8_t n = 0;
    snprintf(items[n++], kInfoItemLen, "%s", v.name1);
    snprintf(items[n++], kInfoItemLen, "%s", slopes[(v.filter.slope < 3) ? v.filter.slope : 1]);
    fmtHz(items[n++], kInfoItemLen, v.filter.fc);
    return n;
}

void FilterGraph::begin(Composer& c, const View& v) const {
    const FilterView& f = v.filter;
    const bool bw = (f.type == FILTER_BP) || (f.type == FILTER_BS);
    c.flag[0] = (f.slope == 0) && !bw;
    c.flag[1] = (f.env != 0.0f);
    c.f[0] = f.fc;
    c.f[1] = clampf(f.fc * powf(2.0f, f.env * 3.0f), kFreqMin, kFreqMax);
    c.f[2] = f.mix;
    for (uint8_t w = 0; w < 2; w++) {
        float fc = c.f[w];
        c.bq[w * 2] = c.bq[w * 2 + 1] = kIdentity;
        if (!c.flag[0]) {
            float qa = bw ? fmaxf(0.5f, f.q * 0.5f) * ((f.slope == 0) ? 0.5f : 1.0f) : f.q;
            c.bq[w * 2] = filterBiquad(f.type, fc, qa);
            if (f.slope == 2) c.bq[w * 2 + 1] = filterBiquad(f.type, fc, bw ? fmaxf(0.5f, f.q * 0.5f) : 0.707f);
        }
    }
    int16_t xe = freqToX(c.f[1]);
    if (c.flag[1]) c.addNode(xe, y(c, v, 1, xe - kGraphX0), f.active ? kMagenta : kGray50);
    int16_t xc = freqToX(c.f[0]);
    c.addNode(xc, y(c, v, 0, xc - kGraphX0), f.active ? kCyan : kGray50);
}

int16_t FilterGraph::y(const Composer& c, const View& v, uint8_t which, int16_t i) const {
    const FilterView& f = v.filter;
    float re, im;
    if (c.flag[0]) {
        float r = colFreq[i] / c.f[which], d = 1.0f + r * r;
        if (f.type == FILTER_HP) {
            re = (r * r) / d;
            im = r / d;
        } else {
            re = 1.0f / d;
            im = -r / d;
        }
    } else {
        biquadAt(c.bq[which * 2], i, re, im);
        if (f.slope == 2) {
            float re2, im2;
            biquadAt(c.bq[which * 2 + 1], i, re2, im2);
            float t = re * re2 - im * im2;
            im = re * im2 + im * re2;
            re = t;
        }
    }
    re = (1.0f - c.f[2]) + c.f[2] * re;
    im = c.f[2] * im;
    return dbToY(10.0f * log10f(re * re + im * im + 1.0e-18f), 18.0f, -48.0f);
}

void FilterGraph::column(Composer& c, const View& v, int16_t i) const {
    const FilterView& f = v.filter;
    int16_t m = y(c, v, 0, i);
    int16_t e = c.flag[1] ? y(c, v, 1, i) : -1;
    if (c.flag[1] && (e < m)) c.rows(e + 1, m, kMagentaDark);
    if (f.active && (m < kGraphY1)) c.rows(m + 1, kGraphY1, kCyanDark);
    c.grid(i);
    c.span(0, e, kMagenta);
    c.span(1, m, f.active ? kWhite : kGray50);
}

}  // namespace panel
