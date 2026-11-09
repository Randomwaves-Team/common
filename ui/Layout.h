#ifndef __UI_LAYOUT_H
#define __UI_LAYOUT_H

// Screen layout shared by Drumboy Pro and Synthgirl Pro (960 x 540 landscape; docs/shared/ui-language.md "Yerleşim",
// docs/shared/common-code.md): menu tab strips (4 + 4 tabs), tab value bar, status box, center panel area.
// Indices of the tab arrays: 0-3 top row, 4-7 bottom row (= encoders 1-8).

#include <stdint.h>

// clang-format off

/* Menu tabs -----------------------------------------------------------------*/

const uint16_t kMenuLineX[]                         = {130, 305, 480, 655, 130, 305, 480, 655};
const uint16_t kMenuLineY[]                         = {29, 29, 29, 29, 451, 451, 451, 451};
const uint16_t kMenuLineHeight                      = 60;

const uint16_t kMenuHeaderX[]                       = {144, 319, 494, 669, 144, 319, 494, 669};
const uint16_t kMenuHeaderY[]                       = {34, 34, 34, 34, 456, 456, 456, 456};

const uint16_t kMenuDataX[]                         = {293, 468, 643, 818, 293, 468, 643, 818};
const uint16_t kMenuDataY[]                         = {75, 75, 75, 75, 497, 497, 497, 497};

const uint16_t kMenuSelectX[]                       = {144, 319, 494, 669, 144, 319, 494, 669};
const uint16_t kMenuSelectY[]                       = {74, 74, 74, 74, 496, 496, 496, 496};

const uint16_t kMenuEncoderBoxX[]                   = {293 - 8, 468 - 8, 643 - 8, 818 - 8, 293 - 8, 468 - 8, 643 - 8, 818 - 8};
const uint16_t kMenuEncoderBoxY[]                   = {34, 34, 34, 34, 456, 456, 456, 456};

const uint16_t kMenuFileBoxX[]                      = {293 - 25, 469 - 25, 643 - 25, 818 - 25, 293 - 25, 469 - 25, 643 - 25, 818 - 25};
const uint16_t kMenuFileBoxY[]                      = {34, 34, 34, 34, 456, 456, 456, 456};

const uint16_t kMenuTextBoxX[]                      = {293 - 18, 469 - 18, 643 - 18, 818 - 18, 293 - 18, 469 - 18, 643 - 18, 818 - 18};
const uint16_t kMenuTextBoxY[]                      = {34, 34, 34, 34, 456, 456, 456, 456};

const uint16_t kMenuSignBoxX[]                      = {293, 468, 643, 818, 293, 468, 643, 818};
const uint16_t kMenuSignBoxY[]                      = {55, 55, 55, 55, 477, 477, 477, 477};

const uint16_t kMenuNumX                            = 110;
const uint16_t kMenuNumY                            = 34;

const uint16_t kMenuHeaderWidth                     = 90;
const uint16_t kMenuHeaderHeight                    = 9;

const uint16_t kMenuDataWidth                       = 90;
const uint16_t kMenuDataHeight                      = 9;

const uint16_t kMenuEncoderBoxWidth                 = 7;
const uint16_t kMenuEncoderBoxHeight                = 7;

const uint16_t kMenuFileBoxWidth                    = 7;
const uint16_t kMenuFileBoxHeight                   = 7;

const uint16_t kMenuTextBoxWidth                    = 48;
const uint16_t kMenuTextBoxHeight                   = 7;

const uint16_t kMenuSignBoxWidth                    = 56;
const uint16_t kMenuSignBoxHeight                   = 7;

const uint16_t kMenuIconLeftX                       = 75;
const uint16_t kMenuIconLeftY                       = 52;

const uint16_t kMenuIconRightX                      = 886;
const uint16_t kMenuIconRightY                      = 52;

const uint8_t kMinMenu4Tab                          = 0;
const uint8_t kMaxMenu4Tab                          = 3;

const uint8_t kMinMenu8Tab                          = 0;
const uint8_t kMaxMenu8Tab                          = 7;

const uint8_t kMenuHeaderTextSize                   = 10;
const uint8_t kMenuDataTextSize                     = 10;
const uint8_t kMenuTextBoxTextSize                  = 8;
const uint8_t kMenuNumberTextSize                   = 2;

const uint8_t kMenuHeaderShortTextSize              = 4;
const uint8_t kMenuDataShortTextSize                = 4;

const uint8_t kMenuSignTextSize                     = 3;

const uint8_t kTextBoxWidth                         = 29;
const uint8_t kTextBoxHeight                        = 14;

/* Tab value bar (UI_REDESIGN.md section 2) ---------------------------------*/

const uint16_t kMenuBarWidth                        = 56;
const uint16_t kMenuBarHeight                       = 3;
const uint16_t kMenuBarOffsetY                      = 3;                        // y = kMenuDataY + 3
const uint8_t kMenuBarMinSteps                      = 12;                       // fewer steps -> no bar, select arrow instead

/* Status box (top right: CPU / MEM) -----------------------------------------*/

const uint16_t kStatusBoxX                          = 831;
const uint16_t kStatusBoxY                          = 29;
const uint16_t kStatusBoxWidth                      = 109;
const uint16_t kStatusBoxHeight                     = 70;
const uint16_t kStatusBarX                          = 857;
const uint16_t kStatusBarWidth                      = 56;
const uint16_t kStatusBarHeight                     = 3;
const uint16_t kStatusLabelY[2]                     = {38, 63};                 // CPU, MEM
const uint16_t kStatusBarOffsetY                    = 14;                       // bar is 14 px below its label
const uint32_t kStatusUpdateInterval                = 1000;                     // ms

const uint8_t kStatusCpuWhitePercent                = 70;
const uint8_t kStatusCpuMagentaPercent              = 90;

/* Center panel --------------------------------------------------------------*/

const uint16_t kPanelX                              = 300;                      // cleared area: x 300-939, y 131-439
const uint16_t kPanelY                              = 131;                      // (y 440 = layer 10 bottom line stays)
const uint16_t kPanelWidth                          = 640;
const uint16_t kPanelHeight                         = 309;
const uint16_t kPanelDividerX                       = 305;                      // = menu tab 2 left edge line
const uint16_t kPanelDividerGap                     = 10;                       // gap above / below, as the menu tab dividers (y 89-98)
const uint16_t kPanelClearX                         = kPanelDividerX + 1;       // panel_open clears x 300-304 + 306-939 (divider not cleared)
const uint16_t kPanelClearWidth                     = kPanelX + kPanelWidth - kPanelClearX;
const uint16_t kPanelGraphX0                        = 319;                      // = kMenuHeaderX[1]
const uint16_t kPanelGraphX1                        = 903;
const uint16_t kPanelGraphY0                        = 192;                      // layer 02 bottom line
const uint16_t kPanelGraphY1                        = 378;                      // layer 08 bottom line

// Graph geometry, axes and drawing of the panel contents: common/panel/Panel.h / Panel.cpp.
const uint16_t kPanelInfoY                          = 172;                      // = kLayerTextY[1] (layer 02 text row)
const uint16_t kPanelInfoSubGapX                    = 12;                       // gap between title and type text
const uint16_t kPanelInfoValueX                     = 903;                      // right-aligned value

// clang-format on

#endif
