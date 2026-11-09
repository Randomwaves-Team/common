#include "MixerGraph.h"

#include <stdio.h>

// x = layer sum -36..+6 dB, y = output -36..+6 dB. Output = input + MASTER, flat at the LIMITER
// ceiling. Above the ceiling GRAY_15, cut part MAGENTA_DARK, under the curve CYAN_DARK, ceiling
// GRAY_50, identity diagonal dotted GRAY_35, PAN (GRAY_50), HEADPHONE (MAGENTA), LINE OUT (GRAY_75).

namespace panel {

const MixerGraph mixerGraph;

namespace {

int16_t mixerDbToY(float d) {
    // output -36 .. +6 dB, bottom .. top
    d = clampf(d, -36.0f, 6.0f);
    return roundi((float)kGraphY1 - (d + 36.0f) / 42.0f * (float)kGraphH);
}

}  // namespace

void MixerGraph::axes(const View& v, Axes& a) const {
    (void)v;
    static const int8_t dbs[] = {6, 0, -12, -24, -36};
    for (uint8_t k = 0; k < 5; k++) {
        char t[6];
        (dbs[k] > 0) ? snprintf(t, sizeof(t), "+%d", dbs[k]) : snprintf(t, sizeof(t), "%d", dbs[k]);
        addRight(a, mixerDbToY((float)dbs[k]), t);
    }
    static const int8_t xs[] = {-36, -24, -12, 0, 6};
    for (uint8_t k = 0; k < 5; k++) {
        char t[6];
        (xs[k] > 0) ? snprintf(t, sizeof(t), "+%d", xs[k]) : snprintf(t, sizeof(t), "%d", xs[k]);
        addBottom(a, kGraphX0 + roundi((float)(xs[k] + 36) / 42.0f * (float)kGraphW), t);
    }
}

uint8_t MixerGraph::info(const View& v, char items[kInfoItems][kInfoItemLen]) const {
    uint8_t n = 0;
    char t[16];
    fmt1(t, sizeof(t), fminf(v.mixer.masterDb, v.mixer.ceilDb), true);
    snprintf(items[n++], kInfoItemLen, "OUT %s DB", (v.mixer.masterDb < -90.0f) ? "--" : t);
    fmt1(t, sizeof(t), v.mixer.ceilDb, true);
    snprintf(items[n++], kInfoItemLen, "LIMIT %s DB", t);
    return n;
}

void MixerGraph::begin(Composer& c, const View& v) const {
    const MixerView& mx = v.mixer;
    auto X = [](float d) { return (int16_t)(kGraphX0 + roundi((d + 36.0f) / 42.0f * (float)kGraphW)); };
    int16_t xk = X(mx.ceilDb - mx.masterDb);
    if ((xk >= kGraphX0) && (xk <= kGraphX1)) c.addNode(xk, mixerDbToY(mx.ceilDb), kCyan);
    c.addNode(X(0.0f), mixerDbToY(fminf(mx.masterDb, mx.ceilDb)), kCyan);
}

void MixerGraph::column(Composer& c, const View& v, int16_t i) const {
    const MixerView& mx = v.mixer;
    float d = -36.0f + (float)i / (float)kGraphW * 42.0f;
    float o = d + mx.masterDb;
    float lim = fminf(o, mx.ceilDb);
    int16_t yc = mixerDbToY(mx.ceilDb);
    int16_t ys = (lim >= -36.0f) ? mixerDbToY(lim) : -1;  // a curve ends where it leaves the graph
    int16_t yu = mixerDbToY(o);
    auto curveY = [](float db) { return (db >= -36.0f) ? mixerDbToY(db) : (int16_t)-1; };
    if (yc > kGraphY0) c.rows(kGraphY0, yc - 1, kGray15);
    if ((ys >= 0) && (yu < ys)) c.rows(yu, ys - 1, kMagentaDark);
    if ((ys >= 0) && (ys < kGraphY1)) c.rows(ys + 1, kGraphY1, kCyanDark);
    c.grid(i);
    c.rows(yc, yc, kGray50);
    if ((i % kGridStep) == 0) c.dot(mixerDbToY(d), kGray35);
    c.span(0, (mx.panLDb != 0.0f) ? curveY(lim + mx.panLDb) : -1, kGray50);
    c.span(1, (mx.panRDb != 0.0f) ? curveY(lim + mx.panRDb) : -1, kGray50);
    c.span(2, (mx.loDb != 0.0f) ? curveY(lim + mx.loDb) : -1, kGray75);
    c.span(3, (mx.hpDb != 0.0f) ? curveY(lim + mx.hpDb) : -1, kMagenta);
    c.span(4, ys, kWhite);
}

}  // namespace panel
