#ifndef __UI_ENCODER_INPUT_H
#define __UI_ENCODER_INPUT_H

// Quadrature decoding and turn acceleration of the 8 tab encoders (docs/shared/ui-language.md, "Encoder and ADD";
// docs/shared/common-code.md). capture() runs in the EXTI interrupt, poll() in the main loop. What a step does
// (encoder function, resolution, which tabs accelerate) stays in the product.

#include <stdint.h>

#include "main.h"

namespace ui {

const uint32_t kEncoderAccelFastUs                  = 25000;                    // detents closer than 25 ms -> 4 steps
const uint32_t kEncoderAccelMidUs                   = 50000;                    // closer than 50 ms -> 2 steps
const uint32_t kEncoderIdleUs                       = 2000000;                  // no edge for 2 s: direction history restarts
const uint32_t kEncoderDebounceUs                   = 700;                      // H723 + 10 nF caps

class EncoderInput {
   public:
    // Pins of channel A / B; also clears all state.
    void attach(GPIO_TypeDef* aPort, uint16_t aPin, GPIO_TypeDef* bPort, uint16_t bPin);
    // Latches the current AB phase and clears partial steps (after a function / menu change: no false delta).
    void sync();
    // Clears partial steps only.
    void clearSteps() { incCount_ = 0; decCount_ = 0; }
    // Interrupt: snapshot of AB, decoded later by poll().
    void capture();

    // Main loop: decodes the captured edge. A direction reversal needs 2 edges (kills 15 -> 14 -> 15 jitter).
    // Returns +1 (CW) / -1 (CCW) when `resolution` edges in one direction completed a step, else 0.
    int8_t poll(uint32_t nowUs, uint8_t resolution);

    // Values per step from the turning speed of the last step: 1, 2 or 4 (a reversal starts at 1).
    uint8_t accel() const { return accel_; }

   private:
    GPIO_TypeDef* aPort_                            = nullptr;
    uint16_t aPin_                                  = 0;
    GPIO_TypeDef* bPort_                            = nullptr;
    uint16_t bPin_                                  = 0;

    volatile uint8_t captured_                      = 0;
    volatile bool pending_                          = false;

    uint8_t abPrevious_                             = 0;
    uint32_t edgeUs_                                = 0;        // time of the last accepted edge
    int8_t confirmedDir_                            = 0;        // 1 CW, -1 CCW, 0 none
    uint8_t reversalCount_                          = 0;
    uint8_t incCount_                               = 0;
    uint8_t decCount_                               = 0;

    uint32_t stepUs_                                = 0;        // time of the last step (acceleration)
    uint8_t accel_                                  = 1;
    int8_t accelDir_                                = 0;

    uint8_t readPins() const;
};

}  // namespace ui

#endif
