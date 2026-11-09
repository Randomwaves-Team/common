#include "SyncGraph.h"

#include <stdio.h>

// UI_REDESIGN.md 6. 4 lanes (SYNC IN, SYNC OUT, MIDI IN, MIDI OUT; in CYAN, out WHITE) over one
// bar; pulses end 14 px before x 903, live squares (nodes) at the end.

namespace panel {

const SyncGraph syncGraph;

namespace {

const int16_t kSyncW = kGraphW - 14;

int16_t syncX(float u) { return (int16_t)(kGraphX0 + roundi(u * (float)kSyncW)); }

}  // namespace

void SyncGraph::axes(const View& v, Axes& a) const {
    static const char* const lanes[4] = {"S-I", "S-O", "M-I", "M-O"};
    const float lh = (float)kGraphH / 4.0f;
    for (uint8_t k = 0; k < 4; k++) addRight(a, (int16_t)(kGraphY0 + roundi(((float)k + 0.5f) * lh)), lanes[k], false);
    for (uint8_t b = 0; (b < v.sync.measure) && (a.bottomCount < kMaxLabels); b++) {
        char t[6];
        snprintf(t, sizeof(t), "%u", (unsigned)(b + 1));
        addBottom(a, syncX((float)b / (float)v.sync.measure), t);
    }
}

uint8_t SyncGraph::info(const View& v, char items[kInfoItems][kInfoItemLen]) const {
    static const char* const roles[3] = {"INTERNAL", "MASTER", "SLAVE"};
    uint8_t n = 0;
    const SyncView& s = v.sync;
    snprintf(items[n++], kInfoItemLen, "%s", roles[(s.role < 3) ? s.role : 0]);
    snprintf(items[n++], kInfoItemLen, "%s%u.%u BPM", (s.role == 2) ? "IN " : "", (unsigned)(s.tempo10 / 10), (unsigned)(s.tempo10 % 10));
    if (s.syncIn > 0) {
        snprintf(items[n++], kInfoItemLen, "%u PPQN", (unsigned)s.inPpqn);
    } else if (s.syncOut > 0) {
        snprintf(items[n++], kInfoItemLen, "OUT %u PPQN", (unsigned)s.outPpqn);
    }
    return n;
}

// ruler (beat numbers) follows MEASURE
bool SyncGraph::needsReopen(const View& drawn, const View& v) const { return drawn.sync.measure != v.sync.measure; }

void SyncGraph::begin(Composer& c, const View& v) const {
    const SyncView& s = v.sync;
    const float lh = (float)kGraphH / 4.0f;
    const bool live[4] = {s.liveSync != 0, false, s.liveMidi != 0, false};
    const Color col[4] = {kCyan, kWhite, kCyan, kWhite};
    for (uint8_t k = 0; k < 4; k++) c.addNode(kGraphX1 - kNodeHalf, (int16_t)(kGraphY0 + roundi(((float)k + 0.5f) * lh)), col[k], live[k]);
}

void SyncGraph::column(Composer& c, const View& v, int16_t i) const {
    const SyncView& s = v.sync;
    if (s.measure == 0) return;
    const int16_t x = kGraphX0 + i;
    const int16_t xEnd = kGraphX0 + kSyncW + 1;
    for (uint8_t b = 0; b <= s.measure; b++) {
        if (x == syncX((float)b / (float)s.measure)) c.dotsV(kGraphY0, kGraphY1, 3, kGray35);
    }
    if (x > xEnd) return;
    const float lh = (float)kGraphH / 4.0f;
    const bool on[4] = {s.syncIn > 0, s.syncOut > 0, s.midiIn >= 2, s.midiOut >= 2};
    const uint8_t ppqn[4] = {s.inPpqn, s.outPpqn, 24, 24};
    const Color col[4] = {kCyan, kWhite, kCyan, kWhite};
    for (uint8_t k = 0; k < 4; k++) {
        const int16_t top = (int16_t)(kGraphY0 + roundi((float)k * lh) + 8);
        const int16_t base = (int16_t)(kGraphY0 + roundi((float)(k + 1) * lh) - 8);
        if (!on[k]) {
            if ((i % kGridStep) == 0) c.dot(base, kGray35);
            continue;
        }
        c.dot(base, (col[k] == kCyan) ? kCyanDark : kGray35);
        const int32_t n = (int32_t)(ppqn[k] ? ppqn[k] : 1) * s.measure;
        const float step = (float)kSyncW / (float)n;
        const int32_t j0 = (int32_t)floorf((float)i / step);
        for (int32_t j = j0 - 1; j <= j0 + 1; j++) {
            if ((j < 0) || (j >= n)) continue;
            const int16_t xp = syncX((float)j / (float)n);
            if (k >= 2) {
                if (x != xp) continue;
                const bool tall = ((j % 24) == 0);
                const int16_t hh = tall ? (base - top) : (int16_t)roundi((float)(base - top) * 0.4f);
                c.rows(base - hh, base - 1, tall ? col[k] : kGray75);
            } else {
                const int16_t w = (int16_t)((roundi(step * 0.25f) > 2) ? roundi(step * 0.25f) : 2);
                if ((x >= xp) && (x < xp + w)) c.rows(top + 4, base - 1, col[k]);
            }
        }
    }
}

}  // namespace panel
