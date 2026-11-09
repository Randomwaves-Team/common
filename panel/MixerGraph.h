#ifndef __PANEL_MIXER_GRAPH_H
#define __PANEL_MIXER_GRAPH_H

#include "Graph.h"

namespace panel {

// MIXER: output level over the layer sum (-36 .. +6 dB both axes), LIMITER. View: View::mixer (KIND_MIXER).
class MixerGraph : public Graph {
   public:
    void axes(const View& v, Axes& a) const override;
    uint8_t info(const View& v, char items[kInfoItems][kInfoItemLen]) const override;
    void begin(Composer& c, const View& v) const override;
    void column(Composer& c, const View& v, int16_t i) const override;
};

extern const MixerGraph mixerGraph;

}  // namespace panel

#endif
