#ifndef __PANEL_SYNC_GRAPH_H
#define __PANEL_SYNC_GRAPH_H

#include "Graph.h"

namespace panel {

// SYNC: 4 lanes (SYNC IN / OUT, MIDI IN / OUT) with pulses per beat. View: View::sync (KIND_SYNC).
class SyncGraph : public Graph {
   public:
    void axes(const View& v, Axes& a) const override;
    uint8_t info(const View& v, char items[kInfoItems][kInfoItemLen]) const override;
    bool needsReopen(const View& drawn, const View& v) const override;
    void begin(Composer& c, const View& v) const override;
    void column(Composer& c, const View& v, int16_t i) const override;
};

extern const SyncGraph syncGraph;

}  // namespace panel

#endif
