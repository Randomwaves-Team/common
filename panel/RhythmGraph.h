#ifndef __PANEL_RHYTHM_GRAPH_H
#define __PANEL_RHYTHM_GRAPH_H

#include "Graph.h"

namespace panel {

// RHYTHM: one bar (beats, QUANTIZE grid, SWING) and the song bars with LOOP. View: View::rhythm (KIND_RHYTHM).
class RhythmGraph : public Graph {
   public:
    void axes(const View& v, Axes& a) const override;
    uint8_t info(const View& v, char items[kInfoItems][kInfoItemLen]) const override;
    bool needsReopen(const View& drawn, const View& v) const override;
    void column(Composer& c, const View& v, int16_t i) const override;
};

extern const RhythmGraph rhythmGraph;

}  // namespace panel

#endif
