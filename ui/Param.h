#ifndef __UI_PARAM_H
#define __UI_PARAM_H

// Menu parameters (docs/shared/common-code.md, step C). A menu page is 8 parameter ids (one per tab / encoder)
// and a ParamSource that owns the values. ParamDef is a const table in Flash: header, unit, range and flags.
// The tab rules of docs/shared/ui-language.md live here and in TabStrip, not in each menu:
// - value bar only on amount values: !PARAM_LIST && max - min >= kMenuBarMinSteps; bipolar when zero >= 0
// - encoder step: clamp at min / max, or wrap with PARAM_WRAP

#include <stdint.h>

#include "ui/Layout.h"

namespace ui {

const uint8_t kNoParam                              = 0xFF;     // empty tab
const uint8_t kFreeTab                              = 0xFE;     // tab area drawn by the product (e.g. the SONG fill graph): TabStrip leaves it alone

enum ParamFlags : uint8_t {
    PARAM_WRAP                                      = 0x01,     // step past max -> min (lists, ON / OFF)
    PARAM_LIST                                      = 0x02,     // list / identity value: no bar even with >= 12 steps
    PARAM_ALT                                       = 0x04      // ADD + encoder changes a 2nd value (LFO / MOD assignment, EQ frequency ...)
};

struct ParamDef {
    const char* header;                             // 10 characters, no abbreviations
    const char* unit;                               // 3 characters, sign row right (CYAN); ParamSource::unit() may vary it
    int16_t min;
    int16_t max;
    int16_t zero;                                   // >= 0: bipolar bar filled from this value; -1: from min
    uint8_t flags;                                  // ParamFlags
};

inline bool paramHasBar(const ParamDef& d) { return !(d.flags & PARAM_LIST) && ((d.max - d.min) >= kMenuBarMinSteps); }

// One encoder detent: clamp, or wrap with PARAM_WRAP.
inline int16_t paramStep(const ParamDef& d, int16_t value, int8_t dir) {
    int16_t v = value + dir;
    if (v > d.max) return (d.flags & PARAM_WRAP) ? d.min : d.max;
    if (v < d.min) return (d.flags & PARAM_WRAP) ? d.max : d.min;
    return v;
}

// Owner of the values of a page (product side: a module adapter of the Controller).
class ParamSource {
   public:
    virtual ~ParamSource() {}

    virtual const ParamDef& def(uint8_t id) const = 0;
    virtual int16_t get(uint8_t id) const = 0;
    virtual void set(uint8_t id, int16_t value) = 0;                    // applies the value (audio, file, other screens)
    // Value text, right aligned at the value position; returns its length (10 = whole field, padded with spaces;
    // shorter texts leave the left part of the field as it is).
    virtual uint8_t format(uint8_t id, int16_t value, char text[11]) const = 0;
    // Text right aligned at the end of the sign row: the unit (3 characters) or a longer detail up to 11 characters
    // (INST sample format "44.1K-16-S", "READ ERROR"), in unitColor() (CYAN unless the menu says otherwise).
    virtual const char* unit(uint8_t id, int16_t value) const { return def(id).unit; }
    virtual uint16_t unitColor(uint8_t id, int16_t value) const { return 0x07FF; }
    // Header when it depends on the state (EFFECT A-D per type); default: the table.
    virtual const char* header(uint8_t id) const { return def(id).header; }
    // Header color: CYAN, or GRAY_50 for a tab whose function is switched off (GLOBAL encoder set to OFF).
    virtual uint16_t headerColor(uint8_t id) const { return 0x07FF; }
    // Value color (RGB565): WHITE, or GRAY_50 when the value does not apply (e.g. PPQN while SYNC is off).
    virtual uint16_t valueColor(uint8_t id, int16_t value) const { return 0xFFFF; }
    // Small label right of the header (Font_05x07, right aligned at the text box: "MUTE", "LOCK", "SLAVE").
    // An "off" label is returned in BLACK so the cell is erased. len <= 8.
    virtual bool badge(uint8_t id, char text[9], uint8_t& len, uint16_t& color) const { return false; }

    // ADD 2nd value, shown left aligned under the header on the sign row (Font_05x07). color: GRAY_75 when set,
    // GRAY_50 when unassigned ("LFO --"). Only for PARAM_ALT parameters.
    virtual bool alt(uint8_t id, char label[11], uint16_t& color) const { return false; }
    virtual void stepAlt(uint8_t id, int8_t dir) {}

    // One encoder detent on a parameter; no set() when the value does not change (at a clamp).
    void step(uint8_t id, int8_t dir) {
        const int16_t v = get(id);
        const int16_t n = paramStep(def(id), v, dir);
        if (n != v) set(id, n);
    }
};

struct Page {
    uint8_t param[8];                               // tab 0-3 top row, 4-7 bottom row; kNoParam = empty
    ParamSource* src;
};

}  // namespace ui

#endif
