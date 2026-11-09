#include "Panel.h"

#include <string.h>

#include "EffectGraph.h"
#include "EqGraph.h"
#include "FilterGraph.h"
#include "Graph.h"
#include "LfoGraph.h"
#include "MetroGraph.h"
#include "MixerGraph.h"
#include "ReverbGraph.h"
#include "RhythmGraph.h"
#include "SyncGraph.h"

// Panel renderer: static layer (title, info row, axis labels) and the column diff of the graph.
// What a kind draws comes from its Graph (Graph.h); the renderer itself does not know the kinds.

namespace panel {

namespace {

// Shared kinds, indexed by Kind (Flash)
const Graph* const kSharedGraphs[KIND_SHARED_END] = {
    nullptr,        // KIND_NONE
    &filterGraph,   // KIND_FILTER
    &eqGraph,       // KIND_EQ
    &reverbGraph,   // KIND_REVERB
    &mixerGraph,    // KIND_MIXER
    &lfoGraph,      // KIND_LFO
    &effectGraph,   // KIND_EFFECT
    nullptr,        // 7 (product)
    nullptr,        // 8 (product)
    &rhythmGraph,   // KIND_RHYTHM
    &metroGraph,    // KIND_METRO
    &syncGraph,     // KIND_SYNC
};

uint8_t infoItems(const Graph* g, const View& v, char items[kInfoItems][kInfoItemLen]) { return (g != nullptr) ? g->info(v, items) : 0; }

}  // namespace

/* Public -------------------------------------------------------------------*/

void clearView(View& v) { memset(&v, 0, sizeof(View)); }

const Graph* Renderer::graphFor(uint8_t kind) const {
    if ((kind < KIND_SHARED_END) && (kSharedGraphs[kind] != nullptr)) return kSharedGraphs[kind];
    if (product_ != nullptr) {
        for (uint8_t k = 0; k < product_->count; k++) {
            if (product_->entries[k].kind == kind) return product_->entries[k].graph;
        }
    }
    return nullptr;
}

bool Renderer::same(const View& v) const { return hasDrawn_ && (memcmp(&drawn_, &v, sizeof(View)) == 0); }

bool Renderer::needsReopen(const View& v) const {
    if (!hasDrawn_) return true;
    if (drawn_.kind != v.kind) return true;
    if (strncmp(drawn_.title, v.title, sizeof(v.title)) != 0) return true;
    const Graph* g = graphFor(v.kind);
    return (g != nullptr) && g->needsReopen(drawn_, v);  // static labels of the graph
}

void Renderer::drawInfoItems(const View& v) {
    char items[kInfoItems][kInfoItemLen];
    uint8_t n = infoItems(graphFor(v.kind), v, items);
    char line[kInfoItemChars + 1];
    uint8_t p = 0;
    for (uint8_t k = 0; k < n; k++) {
        if (k > 0) {
            for (uint8_t g = 0; (g < 2) && (p < kInfoItemChars); g++) line[p++] = ' ';
        }
        for (const char* c = items[k]; (*c != '\0') && (p < kInfoItemChars); c++) line[p++] = *c;
    }
    while (p < kInfoItemChars) line[p++] = ' ';
    line[kInfoItemChars] = '\0';
    // visible gap after the title = 2 px glyph spacing + 14 px = 16 px, same as between items
    int16_t x = kGraphX0 + (int16_t)strnlen(v.title, sizeof(v.title)) * 9 + 14;
    surface_.text(FONT_SMALL, ALIGN_LEFT, kGray50, line, kInfoItemChars, x, kInfoY + 1);
}

void Renderer::drawStatic(const View& v) {
    surface_.text(FONT_MEDIUM, ALIGN_LEFT, kCyan, v.title, (uint16_t)strnlen(v.title, sizeof(v.title)), kGraphX0, kInfoY);
    drawInfoItems(v);
    Axes a;
    memset(&a, 0, sizeof(a));
    const Graph* g = graphFor(v.kind);
    if (g != nullptr) g->axes(v, a);
    for (uint8_t k = 0; k < a.rightCount; k++) {
        surface_.text(FONT_SMALL, ALIGN_LEFT, kGray50, a.right[k].text, (uint16_t)strlen(a.right[k].text), kAxisLabelX, a.right[k].pos - 3);
    }
    for (uint8_t k = 0; k < a.bottomCount; k++) {
        uint16_t len = (uint16_t)strlen(a.bottom[k].text);
        if (a.bottomCyanLeft) {
            surface_.text(FONT_SMALL, ALIGN_LEFT, kCyan, a.bottom[k].text, len, a.bottom[k].pos, kRulerY);
        } else {
            int16_t half = (int16_t)(len * 7) / 2;
            int16_t x = clampi(a.bottom[k].pos, kGraphX0 + half, kGraphX1 - half);
            surface_.text(FONT_SMALL, ALIGN_CENTER, kGray50, a.bottom[k].text, len, x, kRulerY);
        }
    }
}

void Renderer::compose(const View* oldView, const View& newView) {
    static Color oldCol[kComposeRows];
    static Color newCol[kComposeRows];
    static Composer a, b;
    columnsInit();
    a.begin(oldView, (oldView != nullptr) ? graphFor(oldView->kind) : nullptr);
    b.begin(&newView, graphFor(newView.kind));
    for (int16_t x = kComposeX0; x <= kComposeX1; x++) {
        a.column(x, oldCol);
        b.column(x, newCol);
        int16_t start = -1;
        for (int16_t r = 0; r <= kComposeRows; r++) {
            bool need = (r < kComposeRows) && (oldCol[r] != newCol[r]);
            if (need && (start < 0)) start = r;
            if (!need && (start >= 0)) {
                surface_.column(x, kComposeY0 + start, &newCol[start], (uint16_t)(r - start));
                start = -1;
            }
        }
    }
}

void Renderer::open(const View& v) {
    drawStatic(v);
    compose(nullptr, v);
    memcpy(&drawn_, &v, sizeof(View));
    hasDrawn_ = true;
}

void Renderer::update(const View& v) {
    if (!hasDrawn_) {
        open(v);
        return;
    }
    char oldItems[kInfoItems][kInfoItemLen], newItems[kInfoItems][kInfoItemLen];
    memset(oldItems, 0, sizeof(oldItems));
    memset(newItems, 0, sizeof(newItems));
    uint8_t no = infoItems(graphFor(drawn_.kind), drawn_, oldItems), nn = infoItems(graphFor(v.kind), v, newItems);
    if ((no != nn) || (memcmp(oldItems, newItems, sizeof(oldItems)) != 0)) drawInfoItems(v);
    compose(&drawn_, v);
    memcpy(&drawn_, &v, sizeof(View));
}

}  // namespace panel
