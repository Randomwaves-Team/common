#ifndef __UI_MENU_H
#define __UI_MENU_H

// A menu screen (docs/shared/common-code.md, step C): its current page of 8 tabs, menu-specific keys and the
// center panel. The product keeps the menu list, keys and panel contents; TabStrip draws the tabs of page().

#include <stdint.h>

#include "panel/Panel.h"
#include "ui/Param.h"

namespace ui {

class Menu {
   public:
    virtual ~Menu() {}
    virtual const Page& page() const = 0;                               // paged menus (INST-1 / 2, OSC A-D) switch here
    virtual bool onKey(uint8_t key) { return false; }                   // LEFT / RIGHT / CLEAR / COPY / PASTE; true = used
    virtual bool buildPanel(panel::View& v) const { return false; }     // center panel contents; false = no panel
};

}  // namespace ui

#endif
