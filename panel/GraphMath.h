#ifndef __PANEL_GRAPH_MATH_H
#define __PANEL_GRAPH_MATH_H

// Shared math of the panel graphs: rounding / clamping, the per-column log frequency table, freq /
// dB axis mapping, biquad responses and info row number formatting. Tables live in GraphMath.cpp
// (one copy); the small hot helpers are inline here.

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "Panel.h"

namespace panel {

const float kPi = 3.14159265f;
const float kTwoPi = 6.2831853f;
const float kSampleRate = 44100.0f;
const float kFreqMin = 20.0f;
const float kFreqMax = 20000.0f;
const float kFreqDecades = 3.0f;
const uint8_t kGridStep = 3;                          // dotted grid: every 3rd column

/* Helpers ------------------------------------------------------------------*/

inline float clampf(float v, float lo, float hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); }
inline int16_t clampi(int16_t v, int16_t lo, int16_t hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); }
inline int16_t roundi(float v) { return (int16_t)floorf(v + 0.5f); }
inline float dbOf(float gain) { return 20.0f * log10f((gain > 1.0e-6f) ? gain : 1.0e-6f); }

/* Column tables ------------------------------------------------------------*/

// Log frequency axis, one entry per graph column (filled once by columnsInit, 7 KB RAM).
extern float colFreq[kColumns];
extern float colCos[kColumns];
extern float colSin[kColumns];
void columnsInit();

/* Axis mapping -------------------------------------------------------------*/

inline int16_t freqToX(float f) {
    f = clampf(f, kFreqMin, kFreqMax);
    return kGraphX0 + roundi(log10f(f / kFreqMin) / kFreqDecades * (float)kGraphW);
}

// y of a dB value on a linear dB axis (top .. bottom), clamped to the graph
inline int16_t dbToY(float db, float top, float bottom) {
    float t = (top - db) / (top - bottom);
    return clampi(roundi((float)kGraphY0 + t * (float)kGraphH), kGraphY0, kGraphY1);
}

// y of a frequency on a vertical log axis (20 Hz bottom, 20 kHz top)
inline int16_t freqToY(float f) {
    f = clampf(f, kFreqMin, kFreqMax);
    return roundi((float)kGraphY1 - log10f(f / kFreqMin) / kFreqDecades * (float)kGraphH);
}

/* Biquads ------------------------------------------------------------------*/

struct Biquad {
    float b0, b1, b2, a1, a2;
};

extern const Biquad kIdentity;

Biquad normalize(float b0, float b1, float b2, float a0, float a1, float a2);
// RBJ filters (LP / HP / BP / BS), panel model of FILTER
Biquad filterBiquad(uint8_t type, float f0, float q);
// EQ bands, same formulas as Eq::calculateCoef (Drumboy Global.h)
Biquad eqBiquad(uint8_t band, float f0, float q, float gainDb);

// complex response at graph column i (columnsInit must have run)
inline void biquadAt(const Biquad& q, int16_t i, float& re, float& im) {
    float c = colCos[i], s = colSin[i];
    float c2 = 2.0f * c * c - 1.0f, s2 = 2.0f * s * c;
    float nr = q.b0 + q.b1 * c + q.b2 * c2, ni = -(q.b1 * s + q.b2 * s2);
    float dr = 1.0f + q.a1 * c + q.a2 * c2, di = -(q.a1 * s + q.a2 * s2);
    float d = dr * dr + di * di;
    re = (nr * dr + ni * di) / d;
    im = (ni * dr - nr * di) / d;
}

inline float biquadDbAt(const Biquad& q, int16_t i) {
    float re, im;
    biquadAt(q, i, re, im);
    return 10.0f * log10f(re * re + im * im + 1.0e-18f);
}

/* Text ---------------------------------------------------------------------*/

// "930 HZ", "1.05 KHZ", "12.5 KHZ"
void fmtHz(char* out, size_t n, float hz);
// one decimal, optional plus sign: "-7.5", "+1.0", "1.7"
void fmt1(char* out, size_t n, float v, bool plus);
// whole number with sign: "+15 DB", "-6 DB", "00 DB" (menu style)
void fmtDb(char* out, size_t n, float v);

}  // namespace panel

#endif
