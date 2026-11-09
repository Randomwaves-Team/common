#include "GraphMath.h"

#include <stdio.h>

namespace panel {

/* Column tables ------------------------------------------------------------*/

float colFreq[kColumns];
float colCos[kColumns];
float colSin[kColumns];

namespace {
bool colReady = false;
}

void columnsInit() {
    if (colReady) return;
    for (int16_t i = 0; i < kColumns; i++) {
        float f = kFreqMin * powf(10.0f, kFreqDecades * (float)i / (float)kGraphW);
        float w = 2.0f * kPi * f / kSampleRate;
        colFreq[i] = f;
        colCos[i] = cosf(w);
        colSin[i] = sinf(w);
    }
    colReady = true;
}

/* Biquads ------------------------------------------------------------------*/

const Biquad kIdentity = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};

Biquad normalize(float b0, float b1, float b2, float a0, float a1, float a2) {
    float inv = 1.0f / a0;
    Biquad q = {b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv};
    return q;
}

Biquad filterBiquad(uint8_t type, float f0, float q) {
    float w = 2.0f * kPi * f0 / kSampleRate;
    float c = cosf(w), s = sinf(w), al = s / (2.0f * q);
    switch (type) {
        case FILTER_HP: return normalize((1.0f + c) * 0.5f, -(1.0f + c), (1.0f + c) * 0.5f, 1.0f + al, -2.0f * c, 1.0f - al);
        case FILTER_BP: return normalize(al, 0.0f, -al, 1.0f + al, -2.0f * c, 1.0f - al);
        case FILTER_BS: return normalize(1.0f, -2.0f * c, 1.0f, 1.0f + al, -2.0f * c, 1.0f - al);
        case FILTER_LP:
        default: return normalize((1.0f - c) * 0.5f, 1.0f - c, (1.0f - c) * 0.5f, 1.0f + al, -2.0f * c, 1.0f - al);
    }
}

Biquad eqBiquad(uint8_t band, float f0, float q, float gainDb) {
    float A = powf(10.0f, gainDb * 0.025f);
    float w = 2.0f * kPi * f0 / kSampleRate;
    float c = cosf(w), s = sinf(w), al = s / (2.0f * q);
    float sq = sqrtf(A), ap = A + 1.0f, am = A - 1.0f, be = 2.0f * sq * al;
    switch (band) {
        case EQ_BAND_LOWSHELF:
            return normalize(A * (ap - am * c + be), 2.0f * A * (am - ap * c), A * (ap - am * c - be), ap + am * c + be, -2.0f * (am + ap * c), ap + am * c - be);
        case EQ_BAND_HIGHSHELF:
            return normalize(A * (ap + am * c + be), -2.0f * A * (am + ap * c), A * (ap + am * c - be), ap - am * c + be, 2.0f * (am - ap * c), ap - am * c - be);
        case EQ_BAND_PEAK:
        default: return normalize(1.0f + al * A, -2.0f * c, 1.0f - al * A, 1.0f + al / A, -2.0f * c, 1.0f - al / A);
    }
}

/* Text ---------------------------------------------------------------------*/

void fmtHz(char* out, size_t n, float hz) {
    int32_t f = (int32_t)(hz + 0.5f);
    if (f < 1000) {
        snprintf(out, n, "%d HZ", (int)f);
    } else if (f < 2000) {
        int32_t hundredths = (f + 5) / 10;  // 1.05 KHZ: same as the menu list (50 Hz steps)
        snprintf(out, n, "%d.%02d KHZ", (int)(hundredths / 100), (int)(hundredths % 100));
    } else {
        int32_t tenths = (f + 50) / 100;
        snprintf(out, n, "%d.%d KHZ", (int)(tenths / 10), (int)(tenths % 10));
    }
}

void fmt1(char* out, size_t n, float v, bool plus) {
    int32_t t = (int32_t)floorf(fabsf(v) * 10.0f + 0.5f);
    const char* sign = (v < 0.0f && t != 0) ? "-" : ((plus && t != 0) ? "+" : "");
    snprintf(out, n, "%s%d.%d", sign, (int)(t / 10), (int)(t % 10));
}

void fmtDb(char* out, size_t n, float v) {
    int32_t r = (int32_t)floorf(v + 0.5f);
    if (r == 0) {
        snprintf(out, n, "00 DB");
    } else {
        snprintf(out, n, "%s%d DB", (r > 0) ? "+" : "", (int)r);
    }
}

}  // namespace panel
