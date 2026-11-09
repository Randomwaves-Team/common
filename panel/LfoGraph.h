#ifndef __PANEL_LFO_GRAPH_H
#define __PANEL_LFO_GRAPH_H

#include "Graph.h"

namespace panel {

// LFO: two bars of the firmware waveform with SHAPE, PHASE, INVERT, DEPTH, JITTER. View: View::lfo (KIND_LFO).
class LfoGraph : public Graph {
   public:
    void axes(const View& v, Axes& a) const override;
    uint8_t info(const View& v, char items[kInfoItems][kInfoItemLen]) const override;
    void begin(Composer& c, const View& v) const override;
    void column(Composer& c, const View& v, int16_t i) const override;

   private:
    int16_t y(const Composer& c, const View& v, int16_t i) const;
};

extern const LfoGraph lfoGraph;

}  // namespace panel

#endif
