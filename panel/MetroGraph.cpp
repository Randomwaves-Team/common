#include "MetroGraph.h"

#include <stdio.h>

// UI_REDESIGN.md 6. Precount (gray) + one bar of clicks (bar click CYAN), RATE subdivisions GRAY_75,
// click height from VOLUME and ACCENT.

namespace panel {

const MetroGraph metroGraph;

namespace {

uint8_t metroBeats(const MetroView& m) { return (uint8_t)((m.precountBars ? m.measure : 0) + m.measure); }
int16_t metroBeatX(const MetroView& m, float b) { return (int16_t)(kGraphX0 + roundi(b * (float)kGraphW / (float)metroBeats(m))); }

}  // namespace

void MetroGraph::axes(const View& v, Axes& a) const {
    const MetroView& m = v.metro;
    addPercentGrid(a);
    const uint8_t pre = m.precountBars ? m.measure : 0;
    for (uint8_t b = 0; (b < metroBeats(m)) && (a.bottomCount < kMaxLabels); b++) {
        char t[6];
        (b < pre) ? snprintf(t, sizeof(t), "-%u", (unsigned)(pre - b)) : snprintf(t, sizeof(t), "%u", (unsigned)(b - pre + 1));
        addBottom(a, metroBeatX(m, (float)b), t);
    }
}

uint8_t MetroGraph::info(const View& v, char items[kInfoItems][kInfoItemLen]) const {
    static const char* const modes[3] = {"OFF", "RECORD", "ALWAYS"};
    uint8_t n = 0;
    snprintf(items[n++], kInfoItemLen, "%s", v.name1);
    snprintf(items[n++], kInfoItemLen, "%s", modes[(v.metro.mode < 3) ? v.metro.mode : 0]);
    if (v.metro.precountBars) {
        snprintf(items[n++], kInfoItemLen, "COUNT %u %s", (unsigned)v.metro.precountBars, (v.metro.precountBars > 1) ? "BARS" : "BAR");
    } else {
        snprintf(items[n++], kInfoItemLen, "NO COUNT");
    }
    return n;
}

// rulers (beat numbers) follow MEASURE and the precount
bool MetroGraph::needsReopen(const View& drawn, const View& v) const {
    return (drawn.metro.measure != v.metro.measure) || ((drawn.metro.precountBars != 0) != (v.metro.precountBars != 0));
}

void MetroGraph::column(Composer& c, const View& v, int16_t i) const {
    c.grid(i);
    const MetroView& m = v.metro;
    if (m.measure == 0) return;
    const int16_t x = kGraphX0 + i;
    const uint8_t beats = metroBeats(m);
    const uint8_t pre = m.precountBars ? m.measure : 0;
    const float bw = (float)kGraphW / (float)beats;
    const int16_t clickW = (int16_t)((roundi(bw * 0.22f) > 9) ? roundi(bw * 0.22f) : 9);
    const float amp = (float)(kGraphY1 - kGraphCY);
    const uint8_t sub = (m.rateDiv > 0) ? m.rateDiv : 1;
    if (pre && (x == metroBeatX(m, (float)pre))) c.dotsV(kGraphY0, kGraphY1, 2, kGray50);
    const int16_t b0 = (int16_t)floorf((float)i / bw);
    for (int16_t b = b0 - 1; b <= b0 + 1; b++) {
        if ((b < 0) || (b >= beats)) continue;
        const bool isPre = (b < pre);
        const bool first = ((b % m.measure) == 0);
        for (uint8_t s = 0; s < sub; s++) {
            const int16_t xs = (int16_t)roundi((float)metroBeatX(m, (float)b) + (float)s * bw / (float)sub);
            const int16_t d = x - xs;
            if ((d < 0) || (d >= clickW) || ((d % 3) != 0)) continue;
            float level = first ? 1.0f : (1.0f - 0.75f * m.accent);
            if (s > 0) level = (1.0f - 0.75f * m.accent) * 0.55f;
            level *= m.volume;
            const float e = level * expf(-(float)d / ((float)clickW * 0.3f)) * (0.75f + 0.25f * fabsf(sinf((float)d * 1.3f)));
            const int16_t hh = roundi(e * amp);
            Color col = isPre ? ((s == 0) ? kGray50 : kGray35) : ((s > 0) ? kGray75 : (first ? kCyan : kWhite));
            c.rows(kGraphCY - hh, kGraphCY + hh, col);
        }
    }
}

}  // namespace panel
