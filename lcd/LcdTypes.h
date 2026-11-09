#ifndef __LCD_TYPES_H
#define __LCD_TYPES_H

// Display types, colors and LCD bus macros shared by Drumboy Pro and Synthgirl Pro
// (docs/shared/common-code.md). No hardware header here: the bus macros expand at their use
// sites in Lcd.h / Lcd.cpp, which include the product's CubeMX main.h (same pin names in both products).

#include <stdint.h>

#ifndef ALWAYSINLINE
#if defined(__GNUC__)
#define ALWAYSINLINE                                __attribute__((always_inline)) inline
#else
#define ALWAYSINLINE __forceinline
#endif
#endif

// clang-format off

#define LCD_DATA_GPIO_Port                          GPIOB

#define LCD_CS_HIGH                                 LCD_CS_GPIO_Port->BSRR = LCD_CS_Pin
#define LCD_CS_LOW                                  LCD_CS_GPIO_Port->BSRR = LCD_CS_Pin << 16U

#define LCD_RS_HIGH                                 LCD_RS_GPIO_Port->BSRR = LCD_RS_Pin
#define LCD_RS_LOW                                  LCD_RS_GPIO_Port->BSRR = LCD_RS_Pin << 16U

#define LCD_WR_HIGH                                 LCD_WR_GPIO_Port->BSRR = LCD_WR_Pin
#define LCD_WR_LOW                                  LCD_WR_GPIO_Port->BSRR = LCD_WR_Pin << 16U

#define LCD_RD_HIGH                                 LCD_RD_GPIO_Port->BSRR = LCD_RD_Pin
#define LCD_RD_LOW                                  LCD_RD_GPIO_Port->BSRR = LCD_RD_Pin << 16U

#define LCD_RESET_HIGH                              LCD_RESET_GPIO_Port->BSRR = LCD_RESET_Pin
#define LCD_RESET_LOW                               LCD_RESET_GPIO_Port->BSRR = LCD_RESET_Pin << 16U

#define LCD_BL_HIGH                                 LCD_BL_GPIO_Port->BSRR = LCD_BL_Pin
#define LCD_BL_LOW                                  LCD_BL_GPIO_Port->BSRR = LCD_BL_Pin << 16U

#define LCD_RS_DATA                                 LCD_RS_GPIO_Port->BSRR = LCD_RS_Pin
#define LCD_RS_CMD                                  LCD_RS_GPIO_Port->BSRR = LCD_RS_Pin << 16U

#define LCD_WR_STROBE \
    {                 \
        LCD_WR_LOW;   \
        LCD_WR_HIGH;  \
    }

/*----------------------------------------------------------------------------*/

#define MADCTL_MY                                   0x80                        // Bottom to top
#define MADCTL_MX                                   0x40                        // Right to left
#define MADCTL_MV                                   0x20                        // Row/Column exchange
#define MADCTL_ML                                   0x10                        // LCD refresh Bottom to top
#define MADCTL_RGB                                  0x00                        // Red-Green-Blue pixel order
#define MADCTL_BGR                                  0x08                        // Blue-Green-Red pixel order
#define MADCTL_MH                                   0x04                        // LCD refresh right to left

/*----------------------------------------------------------------------------*/

const uint16_t kLCD_WIDTH                           = 540;
const uint16_t kLCD_HEIGHT                          = 960;

// R: 5 bit, G: 6 bit, B: 5bit
// https://lvgl.io/tools/imageconverter
// http://www.rinkydinkelectronics.com/calc_rgb565.php

typedef enum {
    RGB16 = 0x00,
    RGB24 = 0x01,
} RGBMode;

typedef uint16_t RGB16Color;
typedef uint32_t RGB24Color;

struct RGB24RawColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

/*----------------------------------------------------------------------------*/

const RGB16Color WHITE                              = 0xFFFF;
const RGB16Color BLACK                              = 0x0000;
const RGB16Color GRAY                               = 0x8C51;
const RGB16Color YELLOW                             = 0xFFE0;
const RGB16Color GREEN                              = 0x07E0;
const RGB16Color CYAN                               = 0x07FF;
const RGB16Color MAGENTA                            = 0xF81F;
const RGB16Color BLUE                               = 0x001F;
const RGB16Color RED                                = 0xF800;
const RGB16Color ORANGE                             = 0xFD20;
const RGB16Color PINK                               = 0xF81F;

const RGB16Color GRAY_05                            = 0x0861;
const RGB16Color GRAY_10                            = 0x18C3;
const RGB16Color GRAY_15                            = 0x2124;
const RGB16Color GRAY_20                            = 0x3186;
const RGB16Color GRAY_25                            = 0x4208;
const RGB16Color GRAY_30                            = 0x4A69;
const RGB16Color GRAY_35                            = 0x5ACB;
const RGB16Color GRAY_40                            = 0x632C;
const RGB16Color GRAY_45                            = 0x738E;
const RGB16Color GRAY_50                            = 0x8410;
const RGB16Color GRAY_55                            = 0x8C71;
const RGB16Color GRAY_60                            = 0x9CD3;
const RGB16Color GRAY_65                            = 0xA534;
const RGB16Color GRAY_70                            = 0xB596;
const RGB16Color GRAY_75                            = 0xBDF7;
const RGB16Color GRAY_80                            = 0xCE79;
const RGB16Color GRAY_85                            = 0xDEDB;
const RGB16Color GRAY_90                            = 0xE73C;
const RGB16Color GRAY_95                            = 0xF79E;

const RGB16Color CYAN_DARK                          = 0x0166;                   // panel: fill under curve
const RGB16Color MAGENTA_DARK                       = 0x4009;                   // panel: env / eq cut / predelay fill

/*----------------------------------------------------------------------------*/

typedef enum {
    PORTRAIT_0                                      = 0x00,
    PORTRAIT_1                                      = 0x02,
    LANDSCAPE_0                                     = 0x01,
    LANDSCAPE_1                                     = 0x03
} LcdRotation;

/*----------------------------------------------------------------------------*/

typedef enum {
    FONT_05x07                                      = 0x00,
    FONT_07x09                                      = 0x01,
    FONT_10x14                                      = 0x02,
    FONT_14x18                                      = 0x03
} LcdFont;

/*----------------------------------------------------------------------------*/

typedef enum {
    LEFT                                            = 0x00,
    RIGHT                                           = 0x01,
    CENTER                                          = 0x02
} LcdAlign;

// clang-format on

#endif
