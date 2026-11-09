#ifndef __PANEL_GRAPH_H
#define __PANEL_GRAPH_H

// One panel kind = one Graph. The Renderer (Panel.cpp) asks the graph of the view's kind for
//   - axes():        the static layer (labels right of the graph, optional dotted grid line at
//                    their y, ruler under the graph), drawn once on open,
//   - info():        the gray items after the title in the info row,
//   - needsReopen(): changes that move static labels (beyond kind and title),
//   - begin():       per view precompute and the 9x9 nodes,
//   - column():      one graph column, composed back to front into the Composer.
// Graphs are stateless const objects (vtable only, in Flash); per view values go into the
// Composer scratch (bq / flag / f / sim*), whose meaning each graph documents. No heap.
//
// Shared graphs: FilterGraph ... SyncGraph in this folder. A product adds kinds by defining Graph
// subclasses in its own code and passing a GraphList (Panel.h) to the Renderer; its view data sits
// in View::product (productView<T>).

#include <stdint.h>

#include "GraphMath.h"
#include "Panel.h"

namespace panel {

/* Axes ---------------------------------------------------------------------*/

struct Label {
    int16_t pos;            // y (right labels) or x (ruler)
    char text[6];
    bool line;              // right labels: dotted grid line at this y
};

const uint8_t kMaxLabels = 12;

struct Axes {
    Label right[kMaxLabels];
    uint8_t rightCount;
    Label bottom[kMaxLabels];
    uint8_t bottomCount;
    bool bottomCyanLeft;    // LFO: "BAR 1" / "BAR 2" CYAN, left aligned
};

void addRight(Axes& a, int16_t y, const char* t, bool line = true);
void addBottom(Axes& a, int16_t x, const char* t);
void addFreqRuler(Axes& a);                                  // 20 .. 20K under a log frequency axis
void addTimeRuler(Axes& a, int16_t lenMs, int16_t stepMs);   // 0 .. lenMs ms
void addPercentGrid(Axes& a);                                // 100 50 0 50 100 (+-100 % around the center)
void addDbGrid(Axes& a, float top, float bottom, const int8_t* dbs, uint8_t n);  // linear dB axis (dbToY)

/* Composer -----------------------------------------------------------------*/

struct Node {
    int16_t x, y;
    Color c;
    bool fill;
};

const uint8_t kMaxNodes = 20;
const uint8_t kMaxCurves = 8;
const uint8_t kInfoItems = 3;       // info row: at most 3 items after the title
const uint8_t kInfoItemLen = 32;

class Graph;

// Column builder: one color per row of the compose area (graph + 4 px node margin). Graphs draw
// with the primitives below; nodes are drawn over every column after Graph::column.
class Composer {
   public:
    void begin(const View* v, const Graph* g);
    void column(int16_t x, Color* out);

    // rows y0..y1 (either order), clipped to the compose area
    void rows(int16_t y0, int16_t y1, Color c);
    void dot(int16_t y, Color c);
    // dots y0, y0 + step ... (< y1)
    void dotsV(int16_t y0, int16_t y1, int16_t step, Color c);
    // 1 px curve column span: from the previous column's y of curve id (exclusive) to y; y < 0 = no curve here
    void span(uint8_t id, int16_t y, Color c);
    // 9x9 node square (BLACK inside unless fill), drawn on top of the columns
    void addNode(int16_t x, int16_t y, Color c, bool fill = false);
    // dotted horizontal grid at the right labels with line = true (every kGridStep-th column)
    void grid(int16_t i);

    uint8_t nodeCount() const { return nodeCount_; }
    const Node& node(uint8_t k) const { return nodes_[k]; }

    // Per view scratch for the graph (set in Graph::begin, meaning per graph)
    Biquad bq[6];
    bool flag[6];
    float f[16];
    // sequential simulations (compressor / drive / bitcrusher), reset to 0 in begin
    float s0, s1;
    int32_t simN;
    int32_t simCnt;
    float simHeld;

   private:
    const View* v_;
    const Graph* g_;
    Axes axes_;
    Node nodes_[kMaxNodes];
    uint8_t nodeCount_;
    int16_t prev_[kMaxCurves];
    Color* out_;

    void drawNodes(int16_t x);
};

/* Graph --------------------------------------------------------------------*/

class Graph {
   public:
    // Static labels; a is zeroed before the call.
    virtual void axes(const View& v, Axes& a) const = 0;
    // Info row items after the title; returns the count (<= kInfoItems).
    virtual uint8_t info(const View& v, char items[kInfoItems][kInfoItemLen]) const = 0;
    // Same kind and title: true when static labels change from drawn to v (full open needed).
    virtual bool needsReopen(const View& drawn, const View& v) const {
        (void)drawn;
        (void)v;
        return false;
    }
    // Per view precompute into the Composer scratch, nodes. Axes are ready (Composer::grid).
    virtual void begin(Composer& c, const View& v) const {
        (void)c;
        (void)v;
    }
    // Graph column i = 0 .. kColumns - 1 (x = kGraphX0 + i), columns run left to right.
    virtual void column(Composer& c, const View& v, int16_t i) const = 0;
    // no destructor: graphs are static const objects, never deleted (keeps them constant-initialized)
};

}  // namespace panel

#endif
