#include "RhythmGraph.h"

#include <stdio.h>

// UI_REDESIGN.md 6. One bar (y 200-330: beats WHITE, QUANTIZE grid dotted, swung subdivisions CYAN
// with the shift CYAN_DARK) and the song bars (y 348-376: LOOP CYAN_DARK + CYAN frame), play cursor
// MAGENTA.

namespace panel {

const RhythmGraph rhythmGraph;

namespace {

const int16_t kRhyT = 200, kRhyB = 330, kRhyS0 = 348, kRhyS1 = 376;

int16_t rhyBarX0(const RhythmView& r, int16_t b) { return (int16_t)(kGraphX0 + roundi((float)b * (float)kGraphW / (float)r.bar)); }
int16_t rhyBarX1(const RhythmView& r, int16_t b) { return (int16_t)(rhyBarX0(r, b + 1) - 3); }

}  // namespace

void RhythmGraph::axes(const View& v, Axes& a) const {
    const RhythmView& r = v.rhythm;
    addRight(a, (kRhyT + kRhyB) / 2, "BAR", false);
    addRight(a, (kRhyS0 + kRhyS1) / 2, "SONG", false);
    for (uint8_t b = 0; (b < r.bar) && (a.bottomCount < kMaxLabels); b++) {
        char t[6];
        snprintf(t, sizeof(t), "%u", (unsigned)(b + 1));
        addBottom(a, (int16_t)((rhyBarX0(r, b) + rhyBarX1(r, b)) / 2), t);
    }
}

uint8_t RhythmGraph::info(const View& v, char items[kInfoItems][kInfoItemLen]) const {
    uint8_t n = 0;
    const RhythmView& r = v.rhythm;
    snprintf(items[n++], kInfoItemLen, "%u.%u BPM", (unsigned)(r.tempo10 / 10), (unsigned)(r.tempo10 % 10));
    snprintf(items[n++], kInfoItemLen, "%u/4", (unsigned)r.measure);
    snprintf(items[n++], kInfoItemLen, "%u BARS  SWING %u %%", (unsigned)r.bar, (unsigned)r.swing);
    return n;
}

// rulers (bar numbers) follow MEASURE and BAR
bool RhythmGraph::needsReopen(const View& drawn, const View& v) const { return (drawn.rhythm.bar != v.rhythm.bar) || (drawn.rhythm.measure != v.rhythm.measure); }

void RhythmGraph::column(Composer& c, const View& v, int16_t i) const {
    const RhythmView& r = v.rhythm;
    const int16_t x = kGraphX0 + i;
    if ((r.measure == 0) || (r.bar == 0)) return;
    const float W = (float)kGraphW;
    auto X = [&](float u) -> int16_t { return (int16_t)(kGraphX0 + roundi(u * W)); };

    // one bar: baseline, QUANTIZE grid
    if ((i % kGridStep) == 0) c.dot(kRhyB, kGray35);
    const int32_t barTicks = (int32_t)r.measure * 800;
    const int32_t nq = (r.quantTicks > 0) ? ((barTicks / r.quantTicks) > 0 ? (barTicks / r.quantTicks) : 1) : 1;
    {
        const int32_t k0 = (int32_t)floorf((float)i * (float)nq / W);
        for (int32_t k = k0 - 1; k <= k0 + 1; k++) {
            if ((k >= 0) && (k <= nq) && (X((float)k / (float)nq) == x)) c.dotsV(kRhyT, kRhyB, 3, kGray35);
        }
    }
    // SWING GRID subdivisions
    const int32_t div = (r.swingTicks > 0) ? (800 / r.swingTicks) : 2;
    const int32_t n = div * r.measure;
    const float g = 1.0f / (float)n;
    const float sw = (float)r.swing / 100.0f;
    const int32_t k0 = (int32_t)floorf((float)i * (float)n / W);
    for (int32_t k = k0 - 2; k <= k0 + 1; k++) {
        if ((k < 0) || (k >= n)) continue;
        const float u = (float)k * g;
        // height from the note value: beat full, 8th 88, 16th 56, 32nd 36
        const int32_t pos = ((int32_t)k * r.swingTicks) % 800;
        const int16_t h = (pos == 0) ? (kRhyB - kRhyT) : (((pos % 400) == 0) ? 88 : (((pos % 200) == 0) ? 56 : 36));
        if ((k % 2) == 1) {
            const int16_t xa = X(u), xb = X(u - g + sw * 2.0f * g);
            if ((x >= xa) && (x < xb)) c.rows(kRhyB - h, kRhyB - 1, kCyanDark);
            if (x == xa) c.dotsV(kRhyB - h, kRhyB, 2, kGray50);
            if (x == xb) c.rows(kRhyB - h, kRhyB - 1, kCyan);
        } else if (X(u) == x) {
            // the beat being played: MAGENTA
            const bool now = (pos == 0) && (r.playBeat >= 0) && ((k / div) == (r.playBeat % r.measure));
            c.rows(kRhyB - h, kRhyB - 1, now ? kMagenta : kWhite);
        }
    }
    if (x == kGraphX1) c.rows(kRhyT, kRhyB - 1, kWhite);

    // song bars, LOOP
    int16_t b0 = (int16_t)floorf((float)i * (float)r.bar / W);
    for (int16_t b = b0 - 1; b <= b0 + 1; b++) {
        if ((b < 0) || (b >= r.bar)) continue;
        const int16_t xa = rhyBarX0(r, b), xb = rhyBarX1(r, b);
        if ((x < xa) || (x > xb)) continue;
        const bool inLoop = ((b + 1) >= r.loopStart) && ((b + 1) <= r.loopEnd);
        const Color frame = inLoop ? kCyan : kGray35;
        if (inLoop) c.rows(kRhyS0, kRhyS1, kCyanDark);
        c.dot(kRhyS0, frame);
        c.dot(kRhyS1, frame);
        if ((x == xa) || (x == xb)) c.rows(kRhyS0, kRhyS1, frame);
        for (int16_t m = 1; m < r.measure; m++) {
            if (x == (int16_t)(xa + roundi((float)m * (float)(xb - xa) / (float)r.measure))) c.rows(kRhyS1 - 6, kRhyS1 - 1, frame);
        }
    }
    // play cursor: one MAGENTA line on the beat being played
    if (r.playBeat >= 0) {
        const int16_t total = (int16_t)r.measure * r.bar;
        const int16_t b = r.playBeat / r.measure, m = r.playBeat % r.measure;
        if ((r.playBeat < total) && (x == (int16_t)(rhyBarX0(r, b) + roundi((float)m * (float)(rhyBarX1(r, b) - rhyBarX0(r, b)) / (float)r.measure)))) c.rows(kRhyS0 - 4, kRhyS1 + 4, kMagenta);
    }
}

}  // namespace panel
