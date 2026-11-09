#ifndef __PANEL_EQ_GRAPH_H
#define __PANEL_EQ_GRAPH_H

#include "Graph.h"

namespace panel {

// PARAM EQ: sum of the 6 band responses, 20 Hz .. 20 kHz, +12 / -24 dB. View: View::eq (KIND_EQ).
class EqGraph : public Graph {
   public:
    void axes(const View& v, Axes& a) const override;
    uint8_t info(const View& v, char items[kInfoItems][kInfoItemLen]) const override;
    void begin(Composer& c, const View& v) const override;
    void column(Composer& c, const View& v, int16_t i) const override;

   private:
    float totalDb(const Composer& c, int16_t i, float* bands) const;
};

extern const EqGraph eqGraph;

}  // namespace panel

#endif
