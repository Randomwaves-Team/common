#ifndef __UI_TAB_STRIP_H
#define __UI_TAB_STRIP_H

// The 8 menu tabs (4 top, 4 bottom; docs/shared/ui-language.md "Tab rules", docs/shared/common-code.md step C).
//
// Two layers:
// - Value bars, used by every menu: 56 x 3 px CYAN over GRAY_25 under the value, bipolar values fill from a GRAY_50
//   zero tick; only the changed columns are written. After a menu change (beginMenu) the bars the new menu does not
//   draw are blacked out by sweepBars() in the main loop.
// - Pages: open() draws a Page (headers, values, units, bars, badges, ADD labels); refresh() in the main loop builds
//   the same items again, compares them with what is on screen and redraws only what changed, so changes from
//   encoders, GLOBAL, MIDI, sync or file loads need no drawing hooks.
// The select arrow (FILE / DRUMKIT lists) stays in the product: it is an image loaded from the SD card.

#include <stdint.h>

#include "lcd/Lcd.h"
#include "ui/Layout.h"
#include "ui/Param.h"

namespace ui {

class TabStrip {
   public:
    explicit TabStrip(Lcd& lcd);

    /* Value bars ------------------------------------------------------------ */

    // Draws (or updates) the bar of a tab; marks the tab as used by the current menu.
    void drawBar(uint8_t tab, int16_t value, int16_t lo, int16_t hi, int16_t zero);
    // The tab has no bar: blacks out a leftover one. Returns true when a bar was there.
    bool clearBar(uint8_t tab);
    bool barDrawn(uint8_t tab) const { return (tab < 8) && bar_[tab].drawn; }
    // The caller blacked out these tabs itself: their bars are gone.
    void forgetBars(uint8_t firstTab, uint8_t lastTab);
    // Blacks out every bar.
    void clearBars();
    // Menu change: bars belong to the old menu until the new one draws (or declines) them.
    void beginMenu();
    // Main loop: blacks out the bars of the old menu that the new menu did not use.
    void sweepBars();

    /* Pages ----------------------------------------------------------------- */

    void open(const Page& page);
    void refresh(const Page& page);         // no-op unless `page` is the one on screen
    bool isOpen(const Page& page) const { return opened_ == &page; }

   private:
    struct BarState {
        bool drawn;
        bool touched;                       // drawn or declined since the last menu change
        int16_t x0, x1;                     // fill interval [x0, x1) relative to the bar start
        int16_t zero;                       // zero tick x (-1 = none)
    };
    struct TabCache {
        const char* header;
        uint16_t headerColor;
        bool bar;                           // bar parameters below are valid
        int16_t value, lo, hi, zero;
        char text[11];
        uint8_t textLen;
        uint16_t textColor;
        char unit[12];
        uint8_t unitLen;
        uint16_t unitColor;
        char alt[11];
        uint16_t altColor;
        char badge[9];
        uint8_t badgeLen;
        uint16_t badgeColor;
    };

    Lcd& lcd_;
    BarState bar_[8];
    bool sweep_;
    const Page* opened_;
    TabCache tab_[8];

    void segment(uint8_t tab, int16_t x0, int16_t x1, RGB16Color color);
    void drawTab(const Page& page, uint8_t tab, bool force);
    void eraseExtras(uint8_t tab, bool badge, bool alt);
    void text(uint8_t tab, const char* s, RGB16Color color, LcdFont font, LcdAlign align, uint16_t len, uint16_t x, uint16_t y);
};

}  // namespace ui

#endif
