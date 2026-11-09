#include "EncoderInput.h"

namespace ui {

// AB transition table, index (previous AB << 2) | current AB: CW = 1 <-> 2, CCW = 0 <-> 3.
static const int8_t kEncoderLut[16] = {
    0, 0, 0, -1,
    0, 0, 1, 0,
    0, 1, 0, 0,
    -1, 0, 0, 0};

void EncoderInput::attach(GPIO_TypeDef* aPort, uint16_t aPin, GPIO_TypeDef* bPort, uint16_t bPin) {
    aPort_ = aPort;
    aPin_ = aPin;
    bPort_ = bPort;
    bPin_ = bPin;
    pending_ = false;
    confirmedDir_ = 0;
    reversalCount_ = 0;
    sync();
}

uint8_t EncoderInput::readPins() const {
    uint8_t a = (aPort_->IDR & aPin_) ? 2 : 0;
    uint8_t b = (bPort_->IDR & bPin_) ? 1 : 0;
    return a | b;
}

void EncoderInput::sync() {
    abPrevious_ = readPins();
    clearSteps();
}

void EncoderInput::capture() {
    captured_ = readPins();
    pending_ = true;
}

int8_t EncoderInput::poll(uint32_t nowUs, uint8_t resolution) {
    if (!pending_) return 0;

    uint8_t currentAB = captured_;
    pending_ = false;

    int8_t result = 0;
    uint32_t rawDiff = nowUs - edgeUs_;

    // Idle: forget the direction history
    if (rawDiff > kEncoderIdleUs) {
        abPrevious_ = currentAB;
        confirmedDir_ = 0;
        reversalCount_ = 0;
    }

    // Debounce
    if (rawDiff > kEncoderDebounceUs) {
        int8_t direction = kEncoderLut[(abPrevious_ << 2) | currentAB];

        if (direction != 0) {
            // Same direction: at once. Reversal (or first edge after idle): needs a 2nd edge.
            bool processStep = false;
            if (direction == confirmedDir_) {
                processStep = true;
                reversalCount_ = 0;
            } else if (++reversalCount_ >= 2) {
                processStep = true;
                confirmedDir_ = direction;
                reversalCount_ = 0;
            }

            if (processStep) {
                bool step = false;
                if (direction == 1) {
                    decCount_ = 0;
                    if (++incCount_ >= resolution) {
                        incCount_ = 0;
                        step = true;
                    }
                } else {
                    incCount_ = 0;
                    if (++decCount_ >= resolution) {
                        decCount_ = 0;
                        step = true;
                    }
                }

                if (step) {
                    // Acceleration from the time since the previous step
                    uint32_t dt = nowUs - stepUs_;
                    stepUs_ = nowUs;
                    accel_ = (dt < kEncoderAccelFastUs) ? 4 : ((dt < kEncoderAccelMidUs) ? 2 : 1);
                    if (direction != accelDir_) accel_ = 1;
                    accelDir_ = direction;
                    result = direction;
                }
            }
        }

        edgeUs_ = nowUs;
        abPrevious_ = currentAB;
    }

    return result;
}

}  // namespace ui
