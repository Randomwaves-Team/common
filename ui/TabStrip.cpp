#include "TabStrip.h"

#include <string.h>

namespace ui {

static const char kBlank10[] = "          ";

TabStrip::TabStrip(Lcd& lcd) : lcd_(lcd), sweep_(false), opened_(nullptr) {
    for (uint8_t i = 0; i < 8; i++) {
        bar_[i].drawn = false;
        bar_[i].touched = false;
        bar_[i].x0 = 0;
        bar_[i].x1 = 0;
        bar_[i].zero = -1;
    }
    memset(tab_, 0, sizeof(tab_));
}

/* Value bars ----------------------------------------------------------------*/

void TabStrip::segment(uint8_t tab, int16_t x0, int16_t x1, RGB16Color color) {
    // Bar columns [x0, x1) of one tab, relative to the bar start.
    if (x1 <= x0) return;
    lcd_.setForeColor(color);
    lcd_.fillRect(kMenuHeaderX[tab] + x0, kMenuDataY[tab] + kMenuBarOffsetY, x1 - x0, kMenuBarHeight);
}

bool TabStrip::clearBar(uint8_t tab) {
    if (tab > 7) return false;
    bar_[tab].touched = true;
    if (!bar_[tab].drawn) return false;
    lcd_.setBackColor(BLACK);
    lcd_.clearRect(kMenuHeaderX[tab], kMenuDataY[tab] + kMenuBarOffsetY - 2, kMenuBarWidth, kMenuBarHeight + 4);
    bar_[tab].drawn = false;
    bar_[tab].zero = -1;
    return true;
}

void TabStrip::drawBar(uint8_t tab, int16_t value, int16_t lo, int16_t hi, int16_t zero) {
    if (tab > 7) return;
    BarState& st = bar_[tab];
    st.touched = true;

    // 1. FILL INTERVAL [a, b)
    const int16_t range = hi - lo;
    if (range <= 0) return;
    if (value < lo) value = lo;
    if (value > hi) value = hi;
    int16_t xv = (int16_t)(((int32_t)kMenuBarWidth * (value - lo) + (range / 2)) / range);
    int16_t xz = -1;
    int16_t a, b;
    if (zero >= 0) {
        xz = (int16_t)(((int32_t)kMenuBarWidth * (zero - lo) + (range / 2)) / range);
        a = (xv < xz) ? xv : xz;
        b = (xv < xz) ? xz : xv;
    } else {
        a = 0;
        b = (xv < 1) ? 1 : xv;
    }

    // 2. DRAW: full on first draw, otherwise only the difference to the previous interval
    if (!st.drawn) {
        segment(tab, 0, kMenuBarWidth, GRAY_25);
        segment(tab, a, b, CYAN);
    } else {
        const int16_t oa = st.x0;
        const int16_t ob = st.x1;
        // new \ old -> CYAN
        segment(tab, a, (b < oa) ? b : oa, CYAN);
        segment(tab, (a > ob) ? a : ob, b, CYAN);
        // old \ new -> GRAY_25
        segment(tab, oa, (ob < a) ? ob : a, GRAY_25);
        segment(tab, (oa > b) ? oa : b, ob, GRAY_25);
    }

    // 3. ZERO TICK (bipolar): 7 px GRAY_50, the fill covers it inside the bar when it starts there.
    // A tick left at another place (other menu / range) is removed without blacking out the bar.
    const int16_t oz = st.zero;
    const uint16_t barX = kMenuHeaderX[tab];
    const uint16_t barY = kMenuDataY[tab] + kMenuBarOffsetY;
    if ((oz >= 0) && (oz != xz)) {
        lcd_.setBackColor(BLACK);
        lcd_.clearRect(barX + oz, barY - 2, 1, 2);
        lcd_.clearRect(barX + oz, barY + kMenuBarHeight, 1, 2);
        segment(tab, oz, oz + 1, ((oz >= a) && (oz < b)) ? CYAN : GRAY_25);
    }
    if (xz >= 0) {
        if (oz != xz) {
            lcd_.setForeColor(GRAY_50);
            lcd_.drawVLine(barX + xz, barY - 2, kMenuBarHeight + 4);
        }
        segment(tab, xz, xz + 1, ((xz >= a) && (xz < b)) ? CYAN : GRAY_50);
    }

    st.x0 = a;
    st.x1 = b;
    st.zero = xz;
    st.drawn = true;
}

void TabStrip::forgetBars(uint8_t firstTab, uint8_t lastTab) {
    for (uint8_t i = firstTab; (i <= lastTab) && (i < 8); i++) {
        bar_[i].drawn = false;
        bar_[i].zero = -1;
    }
}

void TabStrip::clearBars() {
    for (uint8_t i = 0; i < 8; i++) {
        const bool touched = bar_[i].touched;
        clearBar(i);
        bar_[i].touched = touched;
    }
}

void TabStrip::beginMenu() {
    for (uint8_t i = 0; i < 8; i++) bar_[i].touched = false;
    sweep_ = true;
    opened_ = nullptr;
}

void TabStrip::sweepBars() {
    if (!sweep_) return;
    sweep_ = false;
    for (uint8_t i = 0; i < 8; i++) {
        if (bar_[i].drawn && !bar_[i].touched) {
            clearBar(i);
            bar_[i].touched = false;
        }
    }
}

/* Pages ---------------------------------------------------------------------*/

void TabStrip::text(uint8_t tab, const char* s, RGB16Color color, LcdFont font, LcdAlign align, uint16_t len, uint16_t x, uint16_t y) {
    lcd_.setForeColor(color);
    lcd_.setBackColor(BLACK);
    lcd_.setFont(font);
    lcd_.setAlignment(align);
    lcd_.drawText(s, len, x, y);
}

void TabStrip::open(const Page& page) {
    opened_ = &page;
    for (uint8_t t = 0; t < 8; t++) drawTab(page, t, true);
}

void TabStrip::refresh(const Page& page) {
    if (opened_ != &page) return;
    for (uint8_t t = 0; t < 8; t++) drawTab(page, t, false);
}

void TabStrip::drawTab(const Page& page, uint8_t t, bool force) {
    TabCache& c = tab_[t];
    const uint8_t id = page.param[t];

    if (id == kFreeTab) return;  // the product draws this tab area itself

    if (id == kNoParam) {
        if (force) {
            text(t, kBlank10, CYAN, FONT_07x09, LEFT, kMenuHeaderTextSize, kMenuHeaderX[t], kMenuHeaderY[t]);
            clearBar(t);
            text(t, "   ", CYAN, FONT_05x07, RIGHT, kMenuSignTextSize, kMenuSignBoxX[t], kMenuSignBoxY[t]);
            text(t, kBlank10, WHITE, FONT_07x09, RIGHT, kMenuDataTextSize, kMenuDataX[t], kMenuDataY[t]);
            c.header = kBlank10;
            c.headerColor = CYAN;
            c.bar = false;
        }
        return;
    }

    const ParamSource& src = *page.src;
    const ParamDef& d = src.def(id);
    const int16_t v = src.get(id);

    // header
    const char* h = src.header(id);
    const uint16_t hcolor = src.headerColor(id);
    if (force || (h != c.header) || (hcolor != c.headerColor)) {
        text(t, h, hcolor, FONT_07x09, LEFT, kMenuHeaderTextSize, kMenuHeaderX[t], kMenuHeaderY[t]);
        c.header = h;
        c.headerColor = hcolor;
    }

    // value bar (or none)
    if (paramHasBar(d)) {
        if (force || !c.bar || (v != c.value) || (d.min != c.lo) || (d.max != c.hi) || (d.zero != c.zero)) {
            drawBar(t, v, d.min, d.max, d.zero);
            c.bar = true;
            c.value = v;
            c.lo = d.min;
            c.hi = d.max;
            c.zero = d.zero;
        }
    } else if (force || c.bar) {
        clearBar(t);
        c.bar = false;
    }

    // unit
    const char* u = src.unit(id, v);
    uint8_t ulen = (uint8_t)strnlen(u, 11);
    if (ulen < kMenuSignTextSize) ulen = kMenuSignTextSize;      // units are 3 characters (space padded)
    const uint16_t ucolor = src.unitColor(id, v);
    if (force || (ulen != c.unitLen) || (ucolor != c.unitColor) || (memcmp(u, c.unit, ulen) != 0)) {
        text(t, u, ucolor, FONT_05x07, RIGHT, ulen, kMenuSignBoxX[t], kMenuSignBoxY[t]);
        memcpy(c.unit, u, ulen);
        c.unit[ulen] = '\0';
        c.unitLen = ulen;
        c.unitColor = ucolor;
    }

    // value
    char s[11];
    memset(s, ' ', 10);
    s[10] = '\0';
    uint8_t len = src.format(id, v, s);
    if (len > 10) len = 10;
    const uint16_t color = src.valueColor(id, v);
    if (force || (len != c.textLen) || (color != c.textColor) || (memcmp(s, c.text, len) != 0)) {
        text(t, s, color, FONT_07x09, RIGHT, len, kMenuDataX[t], kMenuDataY[t]);
        memcpy(c.text, s, 11);
        c.textLen = len;
        c.textColor = color;
    }

    // badge right of the header
    char b[9];
    memset(b, ' ', 8);
    b[8] = '\0';
    uint8_t blen = 0;
    uint16_t bcolor = BLACK;
    if (src.badge(id, b, blen, bcolor)) {
        if (blen > 8) blen = 8;
        if (force || (blen != c.badgeLen) || (bcolor != c.badgeColor) || (memcmp(b, c.badge, blen) != 0)) {
            text(t, b, bcolor, FONT_05x07, RIGHT, blen, kMenuTextBoxX[t], kMenuTextBoxY[t]);
            memcpy(c.badge, b, 9);
            c.badgeLen = blen;
            c.badgeColor = bcolor;
        }
    }

    // ADD 2nd value: left aligned under the header on the sign row
    if (d.flags & PARAM_ALT) {
        char label[11];
        memset(label, ' ', 10);
        label[10] = '\0';
        uint16_t acolor = GRAY_50;
        if (src.alt(id, label, acolor) && (force || (acolor != c.altColor) || (strncmp(label, c.alt, 10) != 0))) {
            text(t, label, acolor, FONT_05x07, LEFT, kMenuHeaderTextSize, kMenuHeaderX[t], kMenuSignBoxY[t]);
            memcpy(c.alt, label, 11);
            c.altColor = acolor;
        }
    }
}

}  // namespace ui
