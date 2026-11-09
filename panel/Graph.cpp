#include "Graph.h"

#include <stdio.h>
#include <string.h>

// Drawing model: every column of the graph area (plus the 4 px node margin) is composed back to
// front into a color array; the Renderer compares the old and new arrays and writes only differing
// runs. Curves are 1 px column spans (same as Lcd::drawCurve), grids are dotted (every 3rd column),
// nodes are 9x9 squares with a black inside. There is no text inside the graph.

namespace panel {

/* Axes ---------------------------------------------------------------------*/

void addRight(Axes& a, int16_t y, const char* t, bool line) {
    if (a.rightCount >= kMaxLabels) return;
    a.right[a.rightCount].pos = y;
    a.right[a.rightCount].line = line;
    snprintf(a.right[a.rightCount].text, sizeof(a.right[0].text), "%s", t);
    a.rightCount++;
}

void addBottom(Axes& a, int16_t x, const char* t) {
    if (a.bottomCount >= kMaxLabels) return;
    a.bottom[a.bottomCount].pos = x;
    snprintf(a.bottom[a.bottomCount].text, sizeof(a.bottom[0].text), "%s", t);
    a.bottomCount++;
}

void addFreqRuler(Axes& a) {
    static const uint16_t hz[] = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    for (uint8_t k = 0; k < 10 && a.bottomCount < kMaxLabels; k++) {
        char t[6];
        (hz[k] < 1000) ? snprintf(t, sizeof(t), "%u", hz[k]) : snprintf(t, sizeof(t), "%uK", hz[k] / 1000);
        addBottom(a, freqToX((float)hz[k]), t);
    }
}

void addTimeRuler(Axes& a, int16_t lenMs, int16_t stepMs) {
    for (int16_t m = 0; m <= lenMs; m += stepMs) {
        char t[6];
        snprintf(t, sizeof(t), "%u", (unsigned)(uint16_t)m);  // m >= 0, at most 5 digits
        addBottom(a, kGraphX0 + roundi((float)m / (float)lenMs * (float)kGraphW), t);
    }
}

void addPercentGrid(Axes& a) {
    static const float amp[] = {1.0f, 0.5f, 0.0f, -0.5f, -1.0f};
    static const char* const txt[] = {"100", "50", "0", "50", "100"};
    for (uint8_t k = 0; k < 5; k++) addRight(a, roundi((float)kGraphCY - amp[k] * (float)kGraphH * 0.5f), txt[k]);
}

void addDbGrid(Axes& a, float top, float bottom, const int8_t* dbs, uint8_t n) {
    for (uint8_t k = 0; k < n; k++) {
        char t[6];
        (dbs[k] > 0) ? snprintf(t, sizeof(t), "+%d", dbs[k]) : snprintf(t, sizeof(t), "%d", dbs[k]);
        addRight(a, dbToY((float)dbs[k], top, bottom), t);
    }
}

/* Composer -----------------------------------------------------------------*/

void Composer::rows(int16_t y0, int16_t y1, Color c) {
    if (y0 > y1) {
        int16_t t = y0;
        y0 = y1;
        y1 = t;
    }
    if (y0 < kComposeY0) y0 = kComposeY0;
    if (y1 > kComposeY1) y1 = kComposeY1;
    for (int16_t y = y0; y <= y1; y++) out_[y - kComposeY0] = c;
}

void Composer::dot(int16_t y, Color c) {
    if ((y >= kComposeY0) && (y <= kComposeY1)) out_[y - kComposeY0] = c;
}

void Composer::dotsV(int16_t y0, int16_t y1, int16_t step, Color c) {
    for (int16_t y = y0; y < y1; y += step) dot(y, c);
}

void Composer::span(uint8_t id, int16_t y, Color c) {
    int16_t a = prev_[id];
    prev_[id] = y;
    if (y < 0) return;
    if (a < 0) rows(y, y, c);
    else if (y > a) rows(a + 1, y, c);
    else if (y < a) rows(y, a - 1, c);
    else rows(y, y, c);
}

void Composer::addNode(int16_t x, int16_t y, Color c, bool fill) {
    if (nodeCount_ >= kMaxNodes) return;
    nodes_[nodeCount_].x = x;
    nodes_[nodeCount_].y = y;
    nodes_[nodeCount_].c = c;
    nodes_[nodeCount_].fill = fill;
    nodeCount_++;
}

void Composer::drawNodes(int16_t x) {
    for (uint8_t k = 0; k < nodeCount_; k++) {
        const Node& n = nodes_[k];
        int16_t dx = x - n.x;
        if ((dx < -kNodeHalf) || (dx > kNodeHalf)) continue;
        if ((dx == -kNodeHalf) || (dx == kNodeHalf)) {
            rows(n.y - kNodeHalf, n.y + kNodeHalf, n.c);
        } else {
            rows(n.y - kNodeHalf + 1, n.y + kNodeHalf - 1, n.fill ? n.c : kBlack);
            dot(n.y - kNodeHalf, n.c);
            dot(n.y + kNodeHalf, n.c);
        }
    }
}

void Composer::grid(int16_t i) {
    if ((i % kGridStep) != 0) return;
    for (uint8_t k = 0; k < axes_.rightCount; k++) {
        if (axes_.right[k].line) dot(axes_.right[k].pos, kGray35);
    }
}

void Composer::begin(const View* v, const Graph* g) {
    v_ = v;
    g_ = (v != nullptr) ? g : nullptr;
    nodeCount_ = 0;
    for (uint8_t k = 0; k < kMaxCurves; k++) prev_[k] = -1;
    s0 = s1 = 0.0f;
    simN = 0;
    simCnt = 0;
    simHeld = 0.0f;
    if (v_ == nullptr) return;
    memset(&axes_, 0, sizeof(axes_));
    if (g_ == nullptr) return;
    g_->axes(*v_, axes_);
    g_->begin(*this, *v_);
}

void Composer::column(int16_t x, Color* out) {
    out_ = out;
    for (int16_t r = 0; r < kComposeRows; r++) out[r] = kBlack;
    if (g_ == nullptr) return;
    int16_t i = x - kGraphX0;
    if ((i >= 0) && (i < kColumns)) g_->column(*this, *v_, i);
    drawNodes(x);
}

}  // namespace panel
