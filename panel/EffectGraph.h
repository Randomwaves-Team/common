#ifndef __PANEL_EFFECT_GRAPH_H
#define __PANEL_EFFECT_GRAPH_H

#include "Graph.h"

namespace panel {

// EFFECT 1 / 2: one model per type (DELAY ... BITCRUSHER). View: View::effect (KIND_EFFECT).
class EffectGraph : public Graph {
   public:
    void axes(const View& v, Axes& a) const override;
    uint8_t info(const View& v, char items[kInfoItems][kInfoItemLen]) const override;
    bool needsReopen(const View& drawn, const View& v) const override;
    void begin(Composer& c, const View& v) const override;
    void column(Composer& c, const View& v, int16_t i) const override;
};

extern const EffectGraph effectGraph;

}  // namespace panel

#endif
