#ifndef __PANEL_REVERB_GRAPH_H
#define __PANEL_REVERB_GRAPH_H

#include "Graph.h"

namespace panel {

// REVERB: decay tail, 0 .. 3000 ms, 0 .. -60 dB. View: View::reverb (KIND_REVERB).
class ReverbGraph : public Graph {
   public:
    void axes(const View& v, Axes& a) const override;
    uint8_t info(const View& v, char items[kInfoItems][kInfoItemLen]) const override;
    void begin(Composer& c, const View& v) const override;
    void column(Composer& c, const View& v, int16_t i) const override;
};

extern const ReverbGraph reverbGraph;

}  // namespace panel

#endif
