#ifndef __PANEL_FILTER_GRAPH_H
#define __PANEL_FILTER_GRAPH_H

#include "Graph.h"

namespace panel {

// FILTER 1 / 2: magnitude response (RBJ biquads), 20 Hz .. 20 kHz, +18 .. -48 dB. View: View::filter (KIND_FILTER).
class FilterGraph : public Graph {
   public:
    void axes(const View& v, Axes& a) const override;
    uint8_t info(const View& v, char items[kInfoItems][kInfoItemLen]) const override;
    void begin(Composer& c, const View& v) const override;
    void column(Composer& c, const View& v, int16_t i) const override;

   private:
    int16_t y(const Composer& c, const View& v, uint8_t which, int16_t i) const;
};

extern const FilterGraph filterGraph;

}  // namespace panel

#endif
