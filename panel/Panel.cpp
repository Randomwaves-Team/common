#include "Panel.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// Panel renderer. Drawing model: every column of the graph area (plus the 4 px node margin) is
// composed back to front into a color array; on an update the old and new arrays are compared and
// only differing runs are written. Curves are 1 px column spans (same as Lcd::drawCurve), grids are
// dotted (every 3rd column), nodes are 9x9 squares with a black inside. There is no text inside the
// graph: values sit in the info row, axis labels right of and under the graph.

namespace panel {

namespace {

const float kPi = 3.14159265f;
const float kSampleRate = 44100.0f;
const float kFreqMin = 20.0f;
const float kFreqMax = 20000.0f;
const float kFreqDecades = 3.0f;
const uint8_t kGridStep = 3;

/* Helpers ------------------------------------------------------------------*/

inline float clampf(float v, float lo, float hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); }
inline int16_t clampi(int16_t v, int16_t lo, int16_t hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); }
inline int16_t roundi(float v) { return (int16_t)floorf(v + 0.5f); }
inline float dbOf(float gain) { return 20.0f * log10f((gain > 1.0e-6f) ? gain : 1.0e-6f); }

// Log frequency axis, one entry per graph column (filled once, 7 KB).
float colFreq[kColumns];
float colCos[kColumns];
float colSin[kColumns];
bool colReady = false;

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

int16_t freqToX(float f) {
    f = clampf(f, kFreqMin, kFreqMax);
    return kGraphX0 + roundi(log10f(f / kFreqMin) / kFreqDecades * (float)kGraphW);
}

// y of a dB value on a linear dB axis (top .. bottom), clamped to the graph
int16_t dbToY(float db, float top, float bottom) {
    float t = (top - db) / (top - bottom);
    return clampi(roundi((float)kGraphY0 + t * (float)kGraphH), kGraphY0, kGraphY1);
}

// y of a frequency on a vertical log axis (20 Hz bottom, 20 kHz top)
int16_t freqToY(float f) {
    f = clampf(f, kFreqMin, kFreqMax);
    return roundi((float)kGraphY1 - log10f(f / kFreqMin) / kFreqDecades * (float)kGraphH);
}

/* Biquads ------------------------------------------------------------------*/

struct Biquad {
    float b0, b1, b2, a1, a2;
};

const Biquad kIdentity = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};

Biquad normalize(float b0, float b1, float b2, float a0, float a1, float a2) {
    float inv = 1.0f / a0;
    Biquad q = {b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv};
    return q;
}

// RBJ filters (LP / HP / BP / BS), panel model of FILTER
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

// EQ bands, same formulas as Eq::calculateCoef (Global.h)
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

// complex response at graph column i
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

/* Text helpers -------------------------------------------------------------*/

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

// one decimal, optional plus sign: "-7.5", "+1.0", "1.7"
void fmt1(char* out, size_t n, float v, bool plus) {
    int32_t t = (int32_t)floorf(fabsf(v) * 10.0f + 0.5f);
    const char* sign = (v < 0.0f && t != 0) ? "-" : ((plus && t != 0) ? "+" : "");
    snprintf(out, n, "%s%d.%d", sign, (int)(t / 10), (int)(t % 10));
}

// whole number with sign: "+15", "-6", "00" (menu style)
void fmtDb(char* out, size_t n, float v) {
    int32_t r = (int32_t)floorf(v + 0.5f);
    if (r == 0) {
        snprintf(out, n, "00 DB");
    } else {
        snprintf(out, n, "%s%d DB", (r > 0) ? "+" : "", (int)r);
    }
}

/* Axes ---------------------------------------------------------------------*/

struct Label {
    int16_t pos;            // y (right labels) or x (ruler)
    char text[6];
    bool line;              // right labels: dotted grid line at this y
};

const uint8_t kMaxLabels = 12;

struct Axes {
    Label right[kMaxLabels];
    uint8_t rightCount;
    Label bottom[kMaxLabels];
    uint8_t bottomCount;
    bool bottomCyanLeft;    // LFO: "BAR 1" / "BAR 2" CYAN, left aligned
};

void addRight(Axes& a, int16_t y, const char* t, bool line = true) {
    if (a.rightCount >= kMaxLabels) return;
    a.right[a.rightCount].pos = y;
    a.right[a.rightCount].line = line;
    snprintf(a.right[a.rightCount].text, sizeof(a.right[0].text), "%s", t);
    a.rightCount++;
}

void addBottom(Axes& a, int16_t x, const char* t) {
    if (a.bottomCount >= kMaxLabels) return;
    a.bottom[a.bottomCount].pos = x;
    snprintf(a.bottom[a.bottomCount].text, sizeof(a.bottom[0].text), "%s", t);
    a.bottomCount++;
}

void addFreqRuler(Axes& a) {
    static const uint16_t hz[] = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    for (uint8_t k = 0; k < 10 && a.bottomCount < kMaxLabels; k++) {
        char t[6];
        (hz[k] < 1000) ? snprintf(t, sizeof(t), "%u", hz[k]) : snprintf(t, sizeof(t), "%uK", hz[k] / 1000);
        addBottom(a, freqToX((float)hz[k]), t);
    }
}

void addTimeRuler(Axes& a, int16_t lenMs, int16_t stepMs) {
    for (int16_t m = 0; m <= lenMs; m += stepMs) {
        char t[6];
        snprintf(t, sizeof(t), "%u", (unsigned)(uint16_t)m);  // m >= 0, at most 5 digits
        addBottom(a, kGraphX0 + roundi((float)m / (float)lenMs * (float)kGraphW), t);
    }
}

void addPercentGrid(Axes& a) {
    static const float amp[] = {1.0f, 0.5f, 0.0f, -0.5f, -1.0f};
    static const char* txt[] = {"100", "50", "0", "50", "100"};
    for (uint8_t k = 0; k < 5; k++) addRight(a, roundi((float)kGraphCY - amp[k] * (float)kGraphH * 0.5f), txt[k]);
}

void addDbGrid(Axes& a, float top, float bottom, const int8_t* dbs, uint8_t n) {
    for (uint8_t k = 0; k < n; k++) {
        char t[6];
        (dbs[k] > 0) ? snprintf(t, sizeof(t), "+%d", dbs[k]) : snprintf(t, sizeof(t), "%d", dbs[k]);
        addRight(a, dbToY((float)dbs[k], top, bottom), t);
    }
}

int16_t eqDbToY(float d);
int16_t mixerDbToY(float d);

// RHYTHM: one bar (y 200-330) + song bars (y 348-376); SYNC: 4 lanes, pulses end 14 px before x 903
const int16_t kRhyT = 200, kRhyB = 330, kRhyS0 = 348, kRhyS1 = 376;
const int16_t kSyncW = kGraphW - 14;

int16_t rhyBarX0(const RhythmView& r, int16_t b) { return (int16_t)(kGraphX0 + roundi((float)b * (float)kGraphW / (float)r.bar)); }
int16_t rhyBarX1(const RhythmView& r, int16_t b) { return (int16_t)(rhyBarX0(r, b + 1) - 3); }
uint8_t metroBeats(const MetroView& m) { return (uint8_t)((m.precountBars ? m.measure : 0) + m.measure); }
int16_t metroBeatX(const MetroView& m, float b) { return (int16_t)(kGraphX0 + roundi(b * (float)kGraphW / (float)metroBeats(m))); }
int16_t syncX(float u) { return (int16_t)(kGraphX0 + roundi(u * (float)kSyncW)); }

// DELAY graph length: room for ~8 repeats (500 / 1000 / 2000 / 4000 ms)
int16_t delayWindowMs(float ms) {
    static const int16_t windows[] = {500, 1000, 2000, 4000};
    for (uint8_t k = 0; k < 4; k++) {
        if (8.0f * ms <= (float)windows[k]) return windows[k];
    }
    return 4000;
}

Axes makeAxes(const View& v) {
    Axes a;
    memset(&a, 0, sizeof(a));
    switch (v.kind) {
        case KIND_FILTER: {
            static const int8_t dbs[] = {18, 6, -6, -18, -30, -48};
            addDbGrid(a, 18.0f, -48.0f, dbs, 6);
            addFreqRuler(a);
        } break;
        case KIND_EQ: {
            static const int8_t dbs[] = {12, 6, 0, -12, -24};
            for (uint8_t k = 0; k < 5; k++) {
                char t[6];
                (dbs[k] > 0) ? snprintf(t, sizeof(t), "+%d", dbs[k]) : snprintf(t, sizeof(t), "%d", dbs[k]);
                addRight(a, eqDbToY((float)dbs[k]), t);
            }
            addFreqRuler(a);
        } break;
        case KIND_REVERB: {
            static const int8_t dbs[] = {0, -12, -24, -36, -48, -60};
            addDbGrid(a, 0.0f, -60.0f, dbs, 6);
            addTimeRuler(a, 3000, 500);
        } break;
        case KIND_MIXER: {
            static const int8_t dbs[] = {6, 0, -12, -24, -36};
            for (uint8_t k = 0; k < 5; k++) {
                char t[6];
                (dbs[k] > 0) ? snprintf(t, sizeof(t), "+%d", dbs[k]) : snprintf(t, sizeof(t), "%d", dbs[k]);
                addRight(a, mixerDbToY((float)dbs[k]), t);
            }
            static const int8_t xs[] = {-36, -24, -12, 0, 6};
            for (uint8_t k = 0; k < 5; k++) {
                char t[6];
                (xs[k] > 0) ? snprintf(t, sizeof(t), "+%d", xs[k]) : snprintf(t, sizeof(t), "%d", xs[k]);
                addBottom(a, kGraphX0 + roundi((float)(xs[k] + 36) / 42.0f * (float)kGraphW), t);
            }
        } break;
        case KIND_INST:
            addPercentGrid(a);
            break;
        case KIND_RHYTHM: {
            const RhythmView& r = v.rhythm;
            addRight(a, (kRhyT + kRhyB) / 2, "BAR", false);
            addRight(a, (kRhyS0 + kRhyS1) / 2, "SONG", false);
            for (uint8_t b = 0; (b < r.bar) && (a.bottomCount < kMaxLabels); b++) {
                char t[6];
                snprintf(t, sizeof(t), "%u", (unsigned)(b + 1));
                addBottom(a, (int16_t)((rhyBarX0(r, b) + rhyBarX1(r, b)) / 2), t);
            }
        } break;
        case KIND_METRO: {
            const MetroView& m = v.metro;
            addPercentGrid(a);
            const uint8_t pre = m.precountBars ? m.measure : 0;
            for (uint8_t b = 0; (b < metroBeats(m)) && (a.bottomCount < kMaxLabels); b++) {
                char t[6];
                (b < pre) ? snprintf(t, sizeof(t), "-%u", (unsigned)(pre - b)) : snprintf(t, sizeof(t), "%u", (unsigned)(b - pre + 1));
                addBottom(a, metroBeatX(m, (float)b), t);
            }
        } break;
        case KIND_SYNC: {
            static const char* lanes[4] = {"S-I", "S-O", "M-I", "M-O"};
            const float lh = (float)kGraphH / 4.0f;
            for (uint8_t k = 0; k < 4; k++) addRight(a, (int16_t)(kGraphY0 + roundi(((float)k + 0.5f) * lh)), lanes[k], false);
            for (uint8_t b = 0; (b < v.sync.measure) && (a.bottomCount < kMaxLabels); b++) {
                char t[6];
                snprintf(t, sizeof(t), "%u", (unsigned)(b + 1));
                addBottom(a, syncX((float)b / (float)v.sync.measure), t);
            }
        } break;
        case KIND_SYNTH:
            addPercentGrid(a);
            if (v.synth.windowMs > 0) addTimeRuler(a, (int16_t)v.synth.windowMs, (int16_t)(v.synth.windowMs / 5));
            break;
        case KIND_LFO:
            addPercentGrid(a);
            addBottom(a, kGraphX0, "BAR 1");
            addBottom(a, kGraphX0 + kGraphW / 2, "BAR 2");
            a.bottomCyanLeft = true;
            break;
        case KIND_EFFECT:
            switch (v.effect.type) {
                case FX_DELAY: {
                    static const int8_t dbs[] = {0, -12, -24, -36, -48};
                    addDbGrid(a, 0.0f, -48.0f, dbs, 5);
                    const int16_t w = delayWindowMs(v.effect.a);
                    addTimeRuler(a, w, (int16_t)(w / 4));
                } break;
                case FX_CHORUS: {
                    static const char* t[] = {"100", "75", "50", "25", "0"};
                    for (uint8_t k = 0; k < 5; k++) addRight(a, roundi((float)kGraphY0 + (float)k * 0.25f * (float)kGraphH), t[k]);
                    addTimeRuler(a, 2000, 500);
                } break;
                case FX_FLANGER:
                case FX_PHASER:
                    addRight(a, freqToY(20000.0f), "20K");
                    addRight(a, freqToY(2000.0f), "2K");
                    addRight(a, freqToY(200.0f), "200");
                    addRight(a, freqToY(20.0f), "20");
                    addTimeRuler(a, 2000, 500);
                    break;
                case FX_COMPRESSOR:
                case FX_EXPANDER: {
                    static const int8_t dbs[] = {0, -12, -24, -36, -48};
                    addDbGrid(a, 0.0f, -48.0f, dbs, 5);
                    addTimeRuler(a, 1000, 250);
                } break;
                default:
                    addPercentGrid(a);
                    break;
            }
            break;
        default:
            break;
    }
    return a;
}

int16_t eqDbToY(float d) {
    // 0 dB in the middle: upper half +12 dB, lower half -24 dB
    const float half = (float)kGraphH * 0.5f;
    float y = (d >= 0.0f) ? ((float)kGraphCY - d / 12.0f * half) : ((float)kGraphCY + d / -24.0f * half);
    return clampi(roundi(y), kGraphY0, kGraphY1);
}

int16_t mixerDbToY(float d) {
    // output -36 .. +6 dB, bottom .. top
    d = clampf(d, -36.0f, 6.0f);
    return roundi((float)kGraphY1 - (d + 36.0f) / 42.0f * (float)kGraphH);
}

/* Composer -----------------------------------------------------------------*/

struct Node {
    int16_t x, y;
    Color c;
    bool fill;
};

const uint8_t kMaxNodes = 20;
const uint8_t kMaxCurves = 8;

class Composer {
   public:
    void begin(const View* v);
    void column(int16_t x, Color* out);

   private:
    const View* v_;
    Axes axes_;
    Node nodes_[kMaxNodes];
    uint8_t nodeCount_;
    int16_t prev_[kMaxCurves];
    Color* out_;

    // per kind precomputed values
    Biquad bq_[6];
    bool flag_[6];
    float f_[16];
    // sequential simulations (compressor / drive / bitcrusher)
    float s0_, s1_;
    int32_t simN_;
    int32_t simCnt_;
    float simHeld_;

    void rows(int16_t y0, int16_t y1, Color c);
    void dot(int16_t y, Color c);
    void span(uint8_t id, int16_t y, Color c);
    void addNode(int16_t x, int16_t y, Color c, bool fill = false);
    void drawNodes(int16_t x);
    void grid(int16_t i);

    // per kind
    void beginFilter();
    int16_t filterY(uint8_t which, int16_t i);
    void columnFilter(int16_t i);
    void beginEq();
    float eqTotalDb(int16_t i, float* bands);
    void columnEq(int16_t i);
    void beginReverb();
    void columnReverb(int16_t i);
    void beginMixer();
    void columnMixer(int16_t i);
    void beginLfo();
    int16_t lfoY(int16_t i);
    void columnLfo(int16_t i);
    void beginEffect();
    void columnEffect(int16_t i);
    void beginInst();
    int16_t instBar(int16_t k, bool& on);
    void columnInst(int16_t i);
    void columnRhythm(int16_t i);
    void columnMetro(int16_t i);
    void beginSync();
    void columnSync(int16_t i);
    void dotsV(int16_t y0, int16_t y1, int16_t step, Color c);
    void beginSynth();
    float synthEnv(float tMs) const;
    float synthWave(int16_t i, float tMs) const;
    void columnSynth(int16_t i);
};

void Composer::rows(int16_t y0, int16_t y1, Color c) {
    if (y0 > y1) {
        int16_t t = y0;
        y0 = y1;
        y1 = t;
    }
    if (y0 < kComposeY0) y0 = kComposeY0;
    if (y1 > kComposeY1) y1 = kComposeY1;
    for (int16_t y = y0; y <= y1; y++) out_[y - kComposeY0] = c;
}

void Composer::dot(int16_t y, Color c) {
    if ((y >= kComposeY0) && (y <= kComposeY1)) out_[y - kComposeY0] = c;
}

// 1 px curve column span: from the previous column's y (exclusive) to this y; y < 0 = no curve here
void Composer::span(uint8_t id, int16_t y, Color c) {
    int16_t a = prev_[id];
    prev_[id] = y;
    if (y < 0) return;
    if (a < 0) rows(y, y, c);
    else if (y > a) rows(a + 1, y, c);
    else if (y < a) rows(y, a - 1, c);
    else rows(y, y, c);
}

void Composer::addNode(int16_t x, int16_t y, Color c, bool fill) {
    if (nodeCount_ >= kMaxNodes) return;
    nodes_[nodeCount_].x = x;
    nodes_[nodeCount_].y = y;
    nodes_[nodeCount_].c = c;
    nodes_[nodeCount_].fill = fill;
    nodeCount_++;
}

void Composer::drawNodes(int16_t x) {
    for (uint8_t k = 0; k < nodeCount_; k++) {
        const Node& n = nodes_[k];
        int16_t dx = x - n.x;
        if ((dx < -kNodeHalf) || (dx > kNodeHalf)) continue;
        if ((dx == -kNodeHalf) || (dx == kNodeHalf)) {
            rows(n.y - kNodeHalf, n.y + kNodeHalf, n.c);
        } else {
            rows(n.y - kNodeHalf + 1, n.y + kNodeHalf - 1, n.fill ? n.c : kBlack);
            dot(n.y - kNodeHalf, n.c);
            dot(n.y + kNodeHalf, n.c);
        }
    }
}

void Composer::grid(int16_t i) {
    if ((i % kGridStep) != 0) return;
    for (uint8_t k = 0; k < axes_.rightCount; k++) {
        if (axes_.right[k].line) dot(axes_.right[k].pos, kGray35);
    }
}

void Composer::begin(const View* v) {
    v_ = v;
    nodeCount_ = 0;
    for (uint8_t k = 0; k < kMaxCurves; k++) prev_[k] = -1;
    s0_ = s1_ = 0.0f;
    simN_ = 0;
    simCnt_ = 0;
    simHeld_ = 0.0f;
    if (v_ == nullptr) return;
    axes_ = makeAxes(*v_);
    switch (v_->kind) {
        case KIND_FILTER: beginFilter(); break;
        case KIND_EQ: beginEq(); break;
        case KIND_REVERB: beginReverb(); break;
        case KIND_MIXER: beginMixer(); break;
        case KIND_LFO: beginLfo(); break;
        case KIND_EFFECT: beginEffect(); break;
        case KIND_INST: beginInst(); break;
        case KIND_SYNTH: beginSynth(); break;
        case KIND_SYNC: beginSync(); break;
        default: break;
    }
}

void Composer::column(int16_t x, Color* out) {
    out_ = out;
    for (int16_t r = 0; r < kComposeRows; r++) out[r] = kBlack;
    if (v_ == nullptr) return;
    int16_t i = x - kGraphX0;
    if ((i >= 0) && (i < kColumns)) {
        switch (v_->kind) {
            case KIND_FILTER: columnFilter(i); break;
            case KIND_EQ: columnEq(i); break;
            case KIND_REVERB: columnReverb(i); break;
            case KIND_MIXER: columnMixer(i); break;
            case KIND_LFO: columnLfo(i); break;
            case KIND_EFFECT: columnEffect(i); break;
            case KIND_INST: columnInst(i); break;
            case KIND_SYNTH: columnSynth(i); break;
            case KIND_RHYTHM: columnRhythm(i); break;
            case KIND_METRO: columnMetro(i); break;
            case KIND_SYNC: columnSync(i); break;
            default: break;
        }
    }
    drawNodes(x);
}

/* FILTER -------------------------------------------------------------------*/
// Model (views.js FIL): 06 dB = one-pole (LP / HP), 12 = biquad, 24 = biquad x biquad (Q 0.707);
// BP / BS use max(0.5, Q / 2) (x 0.5 at 06). MIX: (1 - MIX) + MIX * H. ENV: cutoff x 2^(+-3 oct).
// f_: 0 fc, 1 fe, 2 mix; flag_: 0 onePole, 1 envOn; bq_: 0/1 main, 2/3 env.

void Composer::beginFilter() {
    const FilterView& f = v_->filter;
    const bool bw = (f.type == FILTER_BP) || (f.type == FILTER_BS);
    flag_[0] = (f.slope == 0) && !bw;
    flag_[1] = (f.env != 0.0f);
    f_[0] = f.fc;
    f_[1] = clampf(f.fc * powf(2.0f, f.env * 3.0f), kFreqMin, kFreqMax);
    f_[2] = f.mix;
    for (uint8_t w = 0; w < 2; w++) {
        float fc = f_[w];
        bq_[w * 2] = bq_[w * 2 + 1] = kIdentity;
        if (!flag_[0]) {
            float qa = bw ? fmaxf(0.5f, f.q * 0.5f) * ((f.slope == 0) ? 0.5f : 1.0f) : f.q;
            bq_[w * 2] = filterBiquad(f.type, fc, qa);
            if (f.slope == 2) bq_[w * 2 + 1] = filterBiquad(f.type, fc, bw ? fmaxf(0.5f, f.q * 0.5f) : 0.707f);
        }
    }
    int16_t xe = freqToX(f_[1]);
    if (flag_[1]) addNode(xe, filterY(1, xe - kGraphX0), f.active ? kMagenta : kGray50);
    int16_t xc = freqToX(f_[0]);
    addNode(xc, filterY(0, xc - kGraphX0), f.active ? kCyan : kGray50);
}

int16_t Composer::filterY(uint8_t which, int16_t i) {
    const FilterView& f = v_->filter;
    float re, im;
    if (flag_[0]) {
        float r = colFreq[i] / f_[which], d = 1.0f + r * r;
        if (f.type == FILTER_HP) {
            re = (r * r) / d;
            im = r / d;
        } else {
            re = 1.0f / d;
            im = -r / d;
        }
    } else {
        biquadAt(bq_[which * 2], i, re, im);
        if (f.slope == 2) {
            float re2, im2;
            biquadAt(bq_[which * 2 + 1], i, re2, im2);
            float t = re * re2 - im * im2;
            im = re * im2 + im * re2;
            re = t;
        }
    }
    re = (1.0f - f_[2]) + f_[2] * re;
    im = f_[2] * im;
    return dbToY(10.0f * log10f(re * re + im * im + 1.0e-18f), 18.0f, -48.0f);
}

void Composer::columnFilter(int16_t i) {
    const FilterView& f = v_->filter;
    int16_t m = filterY(0, i);
    int16_t e = flag_[1] ? filterY(1, i) : -1;
    if (flag_[1] && (e < m)) rows(e + 1, m, kMagentaDark);
    if (f.active && (m < kGraphY1)) rows(m + 1, kGraphY1, kCyanDark);
    grid(i);
    span(0, e, kMagenta);
    span(1, m, f.active ? kWhite : kGray50);
}

/* EQ -----------------------------------------------------------------------*/
// Sum of the 6 band responses (firmware coefficients). Fill between 0 dB and the sum: boost
// CYAN_DARK, cut MAGENTA_DARK. Each band with gain != 0 as a GRAY_50 curve, sum WHITE.
// flag_[b]: band has gain.

void Composer::beginEq() {
    const EqView& e = v_->eq;
    for (uint8_t b = 0; b < 6; b++) {
        flag_[b] = (e.gainDb[b] != 0.0f);
        bq_[b] = flag_[b] ? eqBiquad(e.band[b], e.freq[b], e.q[b], e.gainDb[b]) : kIdentity;
    }
    for (uint8_t b = 0; b < 6; b++) {
        if ((e.freq[b] < kFreqMin) || (e.freq[b] > kFreqMax)) continue;
        int16_t x = freqToX(e.freq[b]);
        addNode(x, eqDbToY(eqTotalDb(x - kGraphX0, nullptr)), e.active ? kCyan : kGray50);
    }
}

float Composer::eqTotalDb(int16_t i, float* bands) {
    float sum = 0.0f;
    for (uint8_t b = 0; b < 6; b++) {
        float d = flag_[b] ? biquadDbAt(bq_[b], i) : 0.0f;
        if (bands != nullptr) bands[b] = d;
        sum += d;
    }
    return sum;
}

void Composer::columnEq(int16_t i) {
    float bands[6];
    const bool act = v_->eq.active;
    int16_t y = eqDbToY(eqTotalDb(i, bands));
    if (y < kGraphCY) rows(y + 1, kGraphCY, act ? kCyanDark : kGray15);
    else if (y > kGraphCY) rows(kGraphCY + 1, y, act ? kMagentaDark : kGray15);
    grid(i);
    // band curves only where the band does something (keeps the 0 dB line dotted and clean)
    for (uint8_t b = 0; b < 6; b++) span(b, (flag_[b] && (fabsf(bands[b]) >= 0.5f)) ? eqDbToY(bands[b]) : -1, kGray50);
    span(6, y, v_->eq.active ? kWhite : kGray50);
}

/* REVERB -------------------------------------------------------------------*/
// x 0..3000 ms, y 0..-60 dB. Tail = straight line in dB from t0 (PREDELAY + SIZE) at the wet
// level of MIX, -60 dB after RT60 (DECAY 200..3000 ms). DAMPING: steeper MAGENTA line
// (RT60 x (1 - 0.75 DAMPING)). f_: 0 rt, 1 rth, 2 wdb, 3 t0.

void Composer::beginReverb() {
    const ReverbView& r = v_->reverb;
    f_[0] = 200.0f + r.decay * 2800.0f;
    f_[1] = f_[0] * (1.0f - r.damping * 0.75f);
    float wet = sinf(r.mix * kPi * 0.5f);
    f_[2] = fmaxf(-60.0f, dbOf(fmaxf(wet, 1.0e-3f)));
    f_[3] = r.predelayMs + 10.0f + r.size * 60.0f;
    auto X = [](float m) { return (int16_t)(kGraphX0 + roundi(clampf(m, 0.0f, 3000.0f) / 3000.0f * (float)kGraphW)); };
    Color cc = r.active ? kCyan : kGray50;
    addNode(X(f_[3]), dbToY(f_[2], 0.0f, -60.0f), cc);
    int16_t xe = X(f_[3] + f_[0] * (60.0f + f_[2]) / 60.0f);
    if (xe < kGraphX1) addNode(xe, kGraphY1, cc);
    int16_t xh = X(f_[3] + f_[1] * (60.0f + f_[2]) / 60.0f);
    if ((r.damping > 0.0f) && (xh < kGraphX1) && ((xe - xh) >= 24)) addNode(xh, kGraphY1, r.active ? kMagenta : kGray50);
}

void Composer::columnReverb(int16_t i) {
    const ReverbView& r = v_->reverb;
    const bool act = r.active;
    float m = (float)i / (float)kGraphW * 3000.0f;
    int16_t y = -1, yh = -1;
    if (m < f_[3]) {
        rows(dbToY(f_[2], 0.0f, -60.0f), kGraphY1, kGray15);
    } else {
        float d = f_[2] - 60.0f * (m - f_[3]) / f_[0];
        float dh = f_[2] - 60.0f * (m - f_[3]) / f_[1];
        if (d > -60.0f) y = dbToY(d, 0.0f, -60.0f);
        if (dh > -60.0f) yh = dbToY(dh, 0.0f, -60.0f);
    }
    if (y >= 0) {
        int16_t h = ((r.damping > 0.0f) && (yh >= 0)) ? yh : kGraphY1;
        if (h > y) rows(y + 1, h, act ? ((r.damping > 0.0f) ? kMagentaDark : kCyanDark) : kGray15);
        if (h < kGraphY1) rows(h + 1, kGraphY1, act ? kCyanDark : kGray15);
    }
    grid(i);
    span(0, (r.damping > 0.0f) ? yh : -1, act ? kMagenta : kGray35);
    span(1, y, act ? kWhite : kGray50);
}

/* MIXER --------------------------------------------------------------------*/
// x = layer sum -36..+6 dB, y = output -36..+6 dB. Output = input + MASTER, flat at the LIMITER
// ceiling. Above the ceiling GRAY_15, cut part MAGENTA_DARK, under the curve CYAN_DARK, ceiling
// GRAY_50, identity diagonal dotted GRAY_35, PAN (GRAY_50), HEADPHONE (MAGENTA), LINE OUT (GRAY_75).

void Composer::beginMixer() {
    const MixerView& mx = v_->mixer;
    auto X = [](float d) { return (int16_t)(kGraphX0 + roundi((d + 36.0f) / 42.0f * (float)kGraphW)); };
    int16_t xk = X(mx.ceilDb - mx.masterDb);
    if ((xk >= kGraphX0) && (xk <= kGraphX1)) addNode(xk, mixerDbToY(mx.ceilDb), kCyan);
    addNode(X(0.0f), mixerDbToY(fminf(mx.masterDb, mx.ceilDb)), kCyan);
}

void Composer::columnMixer(int16_t i) {
    const MixerView& mx = v_->mixer;
    float d = -36.0f + (float)i / (float)kGraphW * 42.0f;
    float o = d + mx.masterDb;
    float lim = fminf(o, mx.ceilDb);
    int16_t yc = mixerDbToY(mx.ceilDb);
    int16_t ys = (lim >= -36.0f) ? mixerDbToY(lim) : -1;  // a curve ends where it leaves the graph
    int16_t yu = mixerDbToY(o);
    auto curveY = [](float db) { return (db >= -36.0f) ? mixerDbToY(db) : (int16_t)-1; };
    if (yc > kGraphY0) rows(kGraphY0, yc - 1, kGray15);
    if ((ys >= 0) && (yu < ys)) rows(yu, ys - 1, kMagentaDark);
    if ((ys >= 0) && (ys < kGraphY1)) rows(ys + 1, kGraphY1, kCyanDark);
    grid(i);
    rows(yc, yc, kGray50);
    if ((i % kGridStep) == 0) dot(mixerDbToY(d), kGray35);
    span(0, (mx.panLDb != 0.0f) ? curveY(lim + mx.panLDb) : -1, kGray50);
    span(1, (mx.panRDb != 0.0f) ? curveY(lim + mx.panRDb) : -1, kGray50);
    span(2, (mx.loDb != 0.0f) ? curveY(lim + mx.loDb) : -1, kGray75);
    span(3, (mx.hpDb != 0.0f) ? curveY(lim + mx.hpDb) : -1, kMagenta);
    span(4, ys, kWhite);
}

/* LFO ----------------------------------------------------------------------*/
// 2 bars, firmware waveform table, SHAPE = piecewise linear phase warp (0.5 neutral), PHASE,
// INVERT, DEPTH. 16 read points (eighths) as nodes; JITTER range as GRAY_50 vertical lines and a
// fixed pseudo random offset of the node inside it. f_: 0 amp, 1 skew, 2 jitter px.

int16_t Composer::lfoY(int16_t i) {
    const LfoView& l = v_->lfo;
    float u = (float)i / (float)kGraphW * 2.0f * l.cyclesPerBar + l.phase;
    u -= floorf(u);
    if (l.holdSteps > 0) u = (floorf(u * (float)l.holdSteps) + 0.5f) / (float)l.holdSteps;
    float sk = f_[1];
    u = (u < sk) ? (0.5f * u / sk) : (0.5f + 0.5f * (u - sk) / (1.0f - sk));
    float w = 0.0f;
    if ((l.table != nullptr) && (l.tableLen > 0)) {
        int32_t k = (int32_t)(u * (float)l.tableLen);
        if (k >= l.tableLen) k = l.tableLen - 1;
        if (k < 0) k = 0;
        w = l.table[k] * 2.0f - 1.0f;
    }
    if (l.invert) w = -w;
    return clampi(roundi((float)kGraphCY - w * f_[0]), kGraphY0, kGraphY1);
}

void Composer::beginLfo() {
    const LfoView& l = v_->lfo;
    f_[0] = (float)(kGraphY1 - kGraphCY) * l.depth;
    f_[1] = clampf(l.shape, 0.02f, 0.98f);
    f_[2] = (float)roundi(l.jitter * f_[0]);
    uint64_t r = 11;
    for (uint8_t k = 0; k < 16; k++) {
        int16_t x = kGraphX0 + roundi((float)k / 16.0f * (float)kGraphW);
        r = (r * 16807ULL) % 2147483647ULL;
        float rnd = (float)r / 2147483647.0f;
        int16_t y = lfoY(x - kGraphX0);
        addNode(x, clampi(roundi((float)y + (rnd * 2.0f - 1.0f) * f_[2]), kGraphY0, kGraphY1), l.active ? kCyan : kGray50);
    }
}

void Composer::columnLfo(int16_t i) {
    const LfoView& l = v_->lfo;
    int16_t y = lfoY(i);
    Color fill = l.active ? kCyanDark : kGray15;
    if (y < kGraphCY) rows(y + 1, kGraphCY, fill);
    else if (y > kGraphCY) rows(kGraphCY, y - 1, fill);
    grid(i);
    span(0, y, l.active ? kWhite : kGray50);
    int16_t j = (int16_t)f_[2];
    if ((j > 0) && l.active) {
        for (uint8_t k = 0; k < nodeCount_; k++) {
            if (nodes_[k].x - kGraphX0 == i) rows(clampi(y - j, kGraphY0, kGraphY1), clampi(y + j, kGraphY0, kGraphY1), kGray50);
        }
    }
}

/* EFFECT -------------------------------------------------------------------*/
// View values per type (a, b, c, d):
//   DELAY      time ms, level 0..1, feedback 0..1, damping 0..1
//   CHORUS     time ms, feedback 0..1, rate Hz, depth 0..1
//   FLANGER    time ms, feedback 0..1, rate Hz, width 0..1
//   PHASER     start Hz, end Hz, rate Hz, feedback 0..1
//   COMP / EXP threshold dB, ratio, attack ms, release ms
//   DRIVE      gain dB, threshold dB, tone Hz, color / bias 0..1
//   BITCRUSHER bits, divider N, threshold dB, smooth 0..100

void Composer::beginEffect() {
    const EffectView& e = v_->effect;
    const Color cc = e.active ? kCyan : kGray50;
    switch (e.type) {
        case FX_DELAY: {
            // f_: 0 ms, 1 l0, 2 fb, 3 fbh
            f_[0] = fmaxf(1.0f, e.a);
            f_[1] = dbOf(fmaxf(0.004f, e.b * e.mix));
            f_[2] = dbOf(fmaxf(0.004f, e.c));
            f_[3] = f_[2] - 6.0f * e.d;
            f_[4] = (float)delayWindowMs(e.a);
            const float win = f_[4];
            auto X = [win](float m) { return (int16_t)(kGraphX0 + roundi(clampf(m, 0.0f, win) / win * (float)kGraphW)); };
            addNode(X(f_[0]), dbToY(f_[1], 0.0f, -48.0f), cc);
            float me = f_[0] * (1.0f + (-48.0f - f_[1]) / f_[2]);
            if ((f_[2] < 0.0f) && (me < win)) addNode(X(me), kGraphY1, cc);
            float mh = f_[0] * (1.0f + (-48.0f - f_[1]) / f_[3]);
            if ((e.d > 0.0f) && (f_[3] < 0.0f) && (mh < win) && ((X(me) - X(mh)) >= 12)) addNode(X(mh), kGraphY1, e.active ? kMagenta : kGray50);
        } break;
        case FX_PHASER: {
            // f_: 0 l1, 1 l2, 2 sp
            float f1 = e.a, f2 = e.b;
            if (f2 < f1) {
                float t = f1;
                f1 = f2;
                f2 = t;
            }
            f1 = fmaxf(f1, kFreqMin);
            f2 = fmaxf(f2, f1 * 1.05f);
            f_[0] = log10f(f1);
            f_[1] = log10f(f2);
            f_[2] = (f_[1] - f_[0]) * 0.3f;
        } break;
        default:
            break;
    }
}

void Composer::columnEffect(int16_t i) {
    const EffectView& e = v_->effect;
    const bool act = e.active;
    const Color CW = act ? kWhite : kGray50;
    const Color CM = act ? kMagenta : kGray50;
    const Color FD = act ? kCyanDark : kGray15;
    const Color FM = act ? kMagentaDark : kGray15;
    const float t = (float)i / (float)kGraphW;  // 0..1 across the graph
    const float amp = (float)kGraphH * 0.5f;

    switch (e.type) {
        case FX_DELAY: {
            const float ms = f_[0], l0 = f_[1], fb = f_[2], fbh = f_[3];
            const float win = f_[4];
            float m = t * win;
            auto X = [win](float mm) { return (int16_t)(kGraphX0 + roundi(clampf(mm, 0.0f, win) / win * (float)kGraphW)); };
            int16_t y = -1, yh = -1;
            if (kGraphX0 + i < X(ms)) rows(dbToY(l0, 0.0f, -48.0f), kGraphY1, kGray15);
            if (m >= ms) {
                float a = l0 + (m / ms - 1.0f) * fb, b = l0 + (m / ms - 1.0f) * fbh;
                if (a >= -48.0f) y = dbToY(a, 0.0f, -48.0f);
                if (b >= -48.0f) yh = dbToY(b, 0.0f, -48.0f);
            }
            if (y >= 0) {
                int16_t h = ((e.d > 0.0f) && (yh >= 0)) ? yh : kGraphY1;
                if (h > y) rows(y + 1, h, (e.d > 0.0f) ? FM : FD);
                if (h < kGraphY1) rows(h + 1, kGraphY1, FD);
            }
            grid(i);
            span(0, (e.d > 0.0f) ? yh : -1, CM);
            span(1, y, CW);
            int16_t k = roundi(m / ms);
            if ((k >= 1) && (X((float)k * ms) == kGraphX0 + i) && (kGraphX0 + i < kGraphX1)) {
                float d = l0 + (float)(k - 1) * fb;
                if (d >= -48.0f) rows(dbToY(d, 0.0f, -48.0f), kGraphY1, CW);
            }
        } break;

        case FX_CHORUS: {
            const float base = e.a, dep = e.d * base * 0.5f, hz = e.c;
            const int16_t nfb = roundi(e.b * 3.0f);
            auto Ym = [](float mm) { return (int16_t)roundi((float)kGraphY1 - clampf(mm, 0.0f, 100.0f) / 100.0f * (float)kGraphH); };
            float dl = base + dep * sinf(t * 2.0f * hz * 2.0f * kPi);
            int16_t yb = Ym(base), y = Ym(dl);
            if (y < yb) rows(y + 1, yb, FD);
            else if (y > yb) rows(yb, y - 1, FD);
            grid(i);
            for (int16_t k = 2; k <= 4; k++) {
                float mm = (float)k * dl;
                span((uint8_t)k, ((k <= nfb + 1) && (mm <= 100.0f)) ? Ym(mm) : -1, CM);
            }
            span(0, y, CW);
        } break;

        case FX_FLANGER:
        case FX_PHASER: {
            const float ph = sinf(t * 2.0f * e.c * 2.0f * kPi);
            float notch[3], peak;
            uint8_t n;
            int16_t npk;
            if (e.type == FX_FLANGER) {
                float d0 = e.a / 1000.0f, w = 0.2f + e.d * 0.75f, dl = d0 * (1.0f + w * ph);
                n = 3;
                for (uint8_t k = 0; k < 3; k++) notch[k] = (float)(2 * (k + 1) - 1) / (2.0f * dl);
                peak = 1.0f / dl;
                npk = roundi(e.b * 3.0f);
            } else {
                float cen = f_[0] + 0.5f * f_[2] + (f_[1] - f_[0] - f_[2]) * (0.5f + 0.5f * ph);
                n = 2;
                for (uint8_t k = 0; k < 2; k++) notch[k] = powf(10.0f, cen + ((float)(k + 1) - 1.5f) * f_[2]);
                notch[2] = 0.0f;
                peak = powf(10.0f, cen);
                npk = roundi(e.d * 2.0f);
            }
            auto Yf = [](float f) { return ((f >= kFreqMin) && (f <= kFreqMax)) ? freqToY(f) : (int16_t)-1; };
            int16_t y1 = freqToY(notch[0]);
            if (y1 < kGraphY1) rows(y1 + 1, kGraphY1, FD);
            grid(i);
            if (e.type == FX_PHASER) {
                rows(freqToY(powf(10.0f, f_[1])), freqToY(powf(10.0f, f_[1])), kGray50);
                rows(freqToY(powf(10.0f, f_[0])), freqToY(powf(10.0f, f_[0])), kGray50);
            }
            span(0, (npk > 0) ? Yf(peak) : -1, CM);
            for (int8_t k = (int8_t)n - 1; k >= 1; k--) span((uint8_t)(k + 1), Yf(notch[k]), kGray50);
            span(1, Yf(notch[0]), CW);
        } break;

        case FX_COMPRESSOR:
        case FX_EXPANDER: {
            const bool comp = (e.type == FX_COMPRESSOR);
            const float thr = e.a, ratio = e.b, atk = fmaxf(0.1f, e.c), rl = fmaxf(0.1f, e.d);
            const float dt = 1000.0f / (float)kGraphW;
            float m = (float)i * dt;
            auto hit = [](float u) { return (u < 0.0f) ? -80.0f : ((u < 2.0f) ? (-3.0f - (2.0f - u) * 10.0f) : (-3.0f - u * 0.11f)); };
            float din = fmaxf(hit(m), hit(m - 500.0f));
            float tg = comp ? ((din > thr) ? (din - thr) * (1.0f - 1.0f / ratio) : 0.0f) : ((din < thr) ? fminf(40.0f, (thr - din) * (ratio - 1.0f)) : 0.0f);
            float tc = (tg > s0_) ? (comp ? atk : rl) : (comp ? rl : atk);
            s0_ += (tg - s0_) * (1.0f - expf(-dt / tc));
            int16_t yi = dbToY(din, 0.0f, -48.0f), yo = dbToY(din - s0_, 0.0f, -48.0f);
            if (yo > yi) rows(yi + 1, yo, FM);
            int16_t top = (yo > yi) ? yo : yi;
            if (top < kGraphY1) rows(top + 1, kGraphY1, FD);
            grid(i);
            int16_t yt = dbToY(thr, 0.0f, -48.0f);
            rows(yt, yt, kGray50);
            span(0, yi, kGray50);
            span(1, yo, CW);
        } break;

        case FX_OVERDRIVE:
        case FX_DISTORTION: {
            const float gain = powf(10.0f, e.a / 20.0f), th = powf(10.0f, e.b / 20.0f), cb = e.d;
            const float dt = 0.02f / (float)kGraphW, al = 1.0f - expf(-2.0f * kPi * e.c * dt);
            float s = sinf(t * 4.0f * kPi);
            float sh;
            if (e.type == FX_OVERDRIVE) sh = tanhf((s + cb * 0.3f) * gain / th) * th;  // COLOR shifts the input: asymmetric, stays within +-THRESHOLD
            else sh = clampf(s * gain + cb * 0.5f * th, -th, th);
            s0_ += (sh - s0_) * al;
            int16_t yi = roundi((float)kGraphCY - s * amp), y = roundi((float)kGraphCY - clampf(s0_, -1.0f, 1.0f) * amp);
            if (y < kGraphCY) rows(y + 1, kGraphCY, FD);
            else if (y > kGraphCY) rows(kGraphCY, y - 1, FD);
            grid(i);
            span(0, yi, kGray50);
            span(1, y, CW);
        } break;

        case FX_BITCRUSHER: {
            // firmware DSP: hold every N-th sample, fold above the threshold, 24-bit mask, one-pole smooth
            const int16_t bits = (int16_t)clampf(e.a, 1.0f, 24.0f);
            const int32_t div = (int32_t)fmaxf(1.0f, e.b);
            const float lim = powf(10.0f, e.c / 20.0f);
            const float smf = (100.0f - e.d) / 100.0f;
            const float al = fminf(1.0f, (smf * smf > 0.0f) ? smf * smf : 0.0001f);
            const int32_t NS = 882;  // 20 ms @ 44.1 kHz
            int32_t n = (int32_t)floorf((float)i * (float)NS / (float)kGraphW);
            if (n > NS - 1) n = NS - 1;
            auto fold = [lim](float x) {
                for (uint8_t k = 0; (k < 64) && ((x > lim) || (x < -lim)); k++) {
                    if (x > lim) x = lim - (x - lim);
                    if (x < -lim) x = -lim - (x + lim);
                }
                return x;
            };
            auto q = [bits](float x) {
                uint32_t nn = (uint32_t)floorf((x + 1.0f) * 0.5f * 16777215.0f);
                uint32_t mm = nn & ((((uint32_t)1 << bits) - 1) << (24 - bits));
                return (float)mm / 16777215.0f * 2.0f - 1.0f;
            };
            while (simN_ <= n) {
                float s = sinf(2.0f * kPi * 100.0f * (float)simN_ / kSampleRate) * 0.95f;
                simCnt_++;
                if (simCnt_ >= div) {
                    simCnt_ = 0;
                    simHeld_ = (fabsf(s) < 0.002f) ? 0.0f : q(fold(s));
                }
                s0_ += al * (simHeld_ - s0_);
                simN_++;
            }
            float s = sinf(2.0f * kPi * 100.0f * (float)n / kSampleRate) * 0.95f;
            int16_t yi = roundi((float)kGraphCY - s * amp), y = roundi((float)kGraphCY - clampf(s0_, -1.0f, 1.0f) * amp);
            if (y < kGraphCY) rows(y + 1, kGraphCY, FD);
            else if (y > kGraphCY) rows(kGraphCY, y - 1, FD);
            grid(i);
            span(0, yi, kGray50);
            span(1, y, CW);
        } break;

        default:
            grid(i);
            break;
    }
}

/* INST ---------------------------------------------------------------------*/
// Sample waveform as 1 px vertical bars every 3 px (2 px gap; +- peak around the center line), scaled by LEVEL.
// Inside START-END: WHITE with the ATTACK fade-in / DECAY fade-out (%); outside: GRAY_50, no envelope.
// DIRECTION BACK / PONG draws the waveform reversed. START and END: 9x9 nodes on the bar tops.

const int16_t kInstBarStep = 3;                       // 1 px bar + 2 px gap
const int16_t kInstBars = kGraphW / kInstBarStep + 1;  // 195

int16_t Composer::instBar(int16_t k, bool& on) {
    const InstView& in = v_->inst;
    float t = (float)k / (float)(kInstBars - 1);
    float tt = in.reverse ? (1.0f - t) : t;
    float pos = tt * 59.0f;
    int16_t p0 = (int16_t)pos;
    if (p0 > 58) p0 = 58;
    float fr = pos - (float)p0;
    float a = ((1.0f - fr) * (float)in.peaks[p0] + fr * (float)in.peaks[p0 + 1]) / 30.0f;
    if (a > 1.0f) a = 1.0f;
    // the x axis is play time: reversed, START-END of the sample is at 1-END .. 1-START
    const float s0 = in.reverse ? (1.0f - in.end) : in.start;
    const float s1 = in.reverse ? (1.0f - in.start) : in.end;
    on = (t >= s0) && (t <= s1);
    float e = 1.0f;
    if (on) {
        // fade-in over the first ATTACK fraction, fade-out over the last DECAY fraction
        float len = fmaxf(0.001f, s1 - s0);
        float u = (t - s0) / len;
        if ((in.attack > 0.0f) && (u < in.attack)) e = u / in.attack;
        if ((in.decay > 0.0f) && (u > 1.0f - in.decay)) e *= fmaxf(0.0f, (1.0f - u) / in.decay);
    }
    const float amp = (float)(kGraphY1 - kGraphCY);
    int16_t h = roundi(a * amp * in.level * e);
    return (h > (int16_t)amp) ? (int16_t)amp : h;
}

void Composer::beginInst() {
    const InstView& in = v_->inst;
    if (!in.hasSample) return;
    bool on;
    int16_t ks = roundi((in.reverse ? (1.0f - in.end) : in.start) * (float)(kInstBars - 1));
    int16_t ke = roundi((in.reverse ? (1.0f - in.start) : in.end) * (float)(kInstBars - 1));
    addNode(kGraphX0 + kInstBarStep * ks, kGraphCY - instBar(ks, on), kCyan);
    addNode(kGraphX0 + kInstBarStep * ke, kGraphCY - instBar(ke, on), kCyan);
}

void Composer::columnInst(int16_t i) {
    grid(i);
    if (!v_->inst.hasSample || ((i % kInstBarStep) != 0)) return;
    bool on;
    int16_t h = instBar(i / kInstBarStep, on);
    rows(kGraphCY - h, kGraphCY + h, on ? kWhite : kGray50);
}

/* RHYTHM / METRONOME / SYNC -----------------------------------------------*/
// UI_REDESIGN.md 6. RHYTHM: one bar (beats WHITE, QUANTIZE grid dotted, swung subdivisions CYAN with
// the shift CYAN_DARK) and the song bars (LOOP CYAN_DARK + CYAN frame). METRONOME: precount (gray)
// + one bar of clicks (bar click CYAN). SYNC: 4 lanes (in CYAN, out WHITE), live squares at the end.

void Composer::dotsV(int16_t y0, int16_t y1, int16_t step, Color c) {
    for (int16_t y = y0; y < y1; y += step) dot(y, c);
}

void Composer::columnRhythm(int16_t i) {
    const RhythmView& r = v_->rhythm;
    const int16_t x = kGraphX0 + i;
    if ((r.measure == 0) || (r.bar == 0)) return;
    const float W = (float)kGraphW;
    auto X = [&](float u) -> int16_t { return (int16_t)(kGraphX0 + roundi(u * W)); };

    // one bar: baseline, QUANTIZE grid
    if ((i % kGridStep) == 0) dot(kRhyB, kGray35);
    const int32_t barTicks = (int32_t)r.measure * 800;
    const int32_t nq = (r.quantTicks > 0) ? ((barTicks / r.quantTicks) > 0 ? (barTicks / r.quantTicks) : 1) : 1;
    {
        const int32_t k0 = (int32_t)floorf((float)i * (float)nq / W);
        for (int32_t k = k0 - 1; k <= k0 + 1; k++) {
            if ((k >= 0) && (k <= nq) && (X((float)k / (float)nq) == x)) dotsV(kRhyT, kRhyB, 3, kGray35);
        }
    }
    // SWING GRID subdivisions
    const int32_t div = (r.swingTicks > 0) ? (800 / r.swingTicks) : 2;
    const int32_t n = div * r.measure;
    const float g = 1.0f / (float)n;
    const float sw = (float)r.swing / 100.0f;
    const int32_t k0 = (int32_t)floorf((float)i * (float)n / W);
    for (int32_t k = k0 - 2; k <= k0 + 1; k++) {
        if ((k < 0) || (k >= n)) continue;
        const float u = (float)k * g;
        // height from the note value: beat full, 8th 88, 16th 56, 32nd 36
        const int32_t pos = ((int32_t)k * r.swingTicks) % 800;
        const int16_t h = (pos == 0) ? (kRhyB - kRhyT) : (((pos % 400) == 0) ? 88 : (((pos % 200) == 0) ? 56 : 36));
        if ((k % 2) == 1) {
            const int16_t xa = X(u), xb = X(u - g + sw * 2.0f * g);
            if ((x >= xa) && (x < xb)) rows(kRhyB - h, kRhyB - 1, kCyanDark);
            if (x == xa) dotsV(kRhyB - h, kRhyB, 2, kGray50);
            if (x == xb) rows(kRhyB - h, kRhyB - 1, kCyan);
        } else if (X(u) == x) {
            // the beat being played: MAGENTA
            const bool now = (pos == 0) && (r.playBeat >= 0) && ((k / div) == (r.playBeat % r.measure));
            rows(kRhyB - h, kRhyB - 1, now ? kMagenta : kWhite);
        }
    }
    if (x == kGraphX1) rows(kRhyT, kRhyB - 1, kWhite);

    // song bars, LOOP
    int16_t b0 = (int16_t)floorf((float)i * (float)r.bar / W);
    for (int16_t b = b0 - 1; b <= b0 + 1; b++) {
        if ((b < 0) || (b >= r.bar)) continue;
        const int16_t xa = rhyBarX0(r, b), xb = rhyBarX1(r, b);
        if ((x < xa) || (x > xb)) continue;
        const bool inLoop = ((b + 1) >= r.loopStart) && ((b + 1) <= r.loopEnd);
        const Color frame = inLoop ? kCyan : kGray35;
        if (inLoop) rows(kRhyS0, kRhyS1, kCyanDark);
        dot(kRhyS0, frame);
        dot(kRhyS1, frame);
        if ((x == xa) || (x == xb)) rows(kRhyS0, kRhyS1, frame);
        for (int16_t m = 1; m < r.measure; m++) {
            if (x == (int16_t)(xa + roundi((float)m * (float)(xb - xa) / (float)r.measure))) rows(kRhyS1 - 6, kRhyS1 - 1, frame);
        }
    }
    // play cursor: one MAGENTA line on the beat being played
    if (r.playBeat >= 0) {
        const int16_t total = (int16_t)r.measure * r.bar;
        const int16_t b = r.playBeat / r.measure, m = r.playBeat % r.measure;
        if ((r.playBeat < total) && (x == (int16_t)(rhyBarX0(r, b) + roundi((float)m * (float)(rhyBarX1(r, b) - rhyBarX0(r, b)) / (float)r.measure)))) rows(kRhyS0 - 4, kRhyS1 + 4, kMagenta);
    }
}

void Composer::columnMetro(int16_t i) {
    grid(i);
    const MetroView& m = v_->metro;
    if (m.measure == 0) return;
    const int16_t x = kGraphX0 + i;
    const uint8_t beats = metroBeats(m);
    const uint8_t pre = m.precountBars ? m.measure : 0;
    const float bw = (float)kGraphW / (float)beats;
    const int16_t clickW = (int16_t)((roundi(bw * 0.22f) > 9) ? roundi(bw * 0.22f) : 9);
    const float amp = (float)(kGraphY1 - kGraphCY);
    const uint8_t sub = (m.rateDiv > 0) ? m.rateDiv : 1;
    if (pre && (x == metroBeatX(m, (float)pre))) dotsV(kGraphY0, kGraphY1, 2, kGray50);
    const int16_t b0 = (int16_t)floorf((float)i / bw);
    for (int16_t b = b0 - 1; b <= b0 + 1; b++) {
        if ((b < 0) || (b >= beats)) continue;
        const bool isPre = (b < pre);
        const bool first = ((b % m.measure) == 0);
        for (uint8_t s = 0; s < sub; s++) {
            const int16_t xs = (int16_t)roundi((float)metroBeatX(m, (float)b) + (float)s * bw / (float)sub);
            const int16_t d = x - xs;
            if ((d < 0) || (d >= clickW) || ((d % 3) != 0)) continue;
            float level = first ? 1.0f : (1.0f - 0.75f * m.accent);
            if (s > 0) level = (1.0f - 0.75f * m.accent) * 0.55f;
            level *= m.volume;
            const float e = level * expf(-(float)d / ((float)clickW * 0.3f)) * (0.75f + 0.25f * fabsf(sinf((float)d * 1.3f)));
            const int16_t hh = roundi(e * amp);
            Color c = isPre ? ((s == 0) ? kGray50 : kGray35) : ((s > 0) ? kGray75 : (first ? kCyan : kWhite));
            rows(kGraphCY - hh, kGraphCY + hh, c);
        }
    }
}

void Composer::beginSync() {
    const SyncView& s = v_->sync;
    const float lh = (float)kGraphH / 4.0f;
    const bool live[4] = {s.liveSync != 0, false, s.liveMidi != 0, false};
    const Color col[4] = {kCyan, kWhite, kCyan, kWhite};
    for (uint8_t k = 0; k < 4; k++) addNode(kGraphX1 - kNodeHalf, (int16_t)(kGraphY0 + roundi(((float)k + 0.5f) * lh)), col[k], live[k]);
}

void Composer::columnSync(int16_t i) {
    const SyncView& s = v_->sync;
    if (s.measure == 0) return;
    const int16_t x = kGraphX0 + i;
    const int16_t xEnd = kGraphX0 + kSyncW + 1;
    for (uint8_t b = 0; b <= s.measure; b++) {
        if (x == syncX((float)b / (float)s.measure)) dotsV(kGraphY0, kGraphY1, 3, kGray35);
    }
    if (x > xEnd) return;
    const float lh = (float)kGraphH / 4.0f;
    const bool on[4] = {s.syncIn > 0, s.syncOut > 0, s.midiIn >= 2, s.midiOut >= 2};
    const uint8_t ppqn[4] = {s.inPpqn, s.outPpqn, 24, 24};
    const Color col[4] = {kCyan, kWhite, kCyan, kWhite};
    for (uint8_t k = 0; k < 4; k++) {
        const int16_t top = (int16_t)(kGraphY0 + roundi((float)k * lh) + 8);
        const int16_t base = (int16_t)(kGraphY0 + roundi((float)(k + 1) * lh) - 8);
        if (!on[k]) {
            if ((i % kGridStep) == 0) dot(base, kGray35);
            continue;
        }
        dot(base, (col[k] == kCyan) ? kCyanDark : kGray35);
        const int32_t n = (int32_t)(ppqn[k] ? ppqn[k] : 1) * s.measure;
        const float step = (float)kSyncW / (float)n;
        const int32_t j0 = (int32_t)floorf((float)i / step);
        for (int32_t j = j0 - 1; j <= j0 + 1; j++) {
            if ((j < 0) || (j >= n)) continue;
            const int16_t xp = syncX((float)j / (float)n);
            if (k >= 2) {
                if (x != xp) continue;
                const bool tall = ((j % 24) == 0);
                const int16_t hh = tall ? (base - top) : (int16_t)roundi((float)(base - top) * 0.4f);
                rows(base - hh, base - 1, tall ? col[k] : kGray75);
            } else {
                const int16_t w = (int16_t)((roundi(step * 0.25f) > 2) ? roundi(step * 0.25f) : 2);
                if ((x >= xp) && (x < xp + w)) rows(top + 4, base - 1, col[k]);
            }
        }
    }
}

/* SYNTH --------------------------------------------------------------------*/
// Graph: DRUM = the whole hit (windowMs), MELODIC = the first 8 periods of the note; y +-100 %.
// Wave WHITE with CYAN_DARK between it and 0, AD envelope MAGENTA, 9x9 MAGENTA node on its peak.
// Noise is a hash of the column, so the same view always gives the same pixels (diff redraw).
// f_: 0 window ms, 1 amp px

const float kTwoPi = 6.2831853f;

float hashNoise(int32_t n) {
    uint32_t x = (uint32_t)n * 747796405u + 2891336453u;
    x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    x = (x >> 22u) ^ x;
    return (float)(x & 0xFFFF) / 32767.5f - 1.0f;
}

float smoothNoise(float pos) {
    // value noise: linear interpolation between hashed points (pos in points)
    int32_t k = (int32_t)floorf(pos);
    float u = pos - (float)k;
    return hashNoise(k) * (1.0f - u) + hashNoise(k + 1) * u;
}

float foldUnit(float x) {
    // triangle fold into -1..1
    float y = fmodf(x + 1.0f, 4.0f);
    if (y < 0.0f) y += 4.0f;
    return (y < 2.0f) ? (y - 1.0f) : (3.0f - y);
}

float waveOf(uint8_t wave, float ph, float pw) {
    ph -= floorf(ph);
    switch (wave) {
        case 0: return 2.0f * ph - 1.0f;                                          // SAW
        case 1: return (ph < pw) ? 1.0f : -1.0f;                                  // SQUARE
        case 2: return (ph < 0.5f) ? (4.0f * ph - 1.0f) : (3.0f - 4.0f * ph);     // TRIANGLE
        default: return sinf(kTwoPi * ph);                                        // SINE
    }
}

float driveOf(float x, float d) {
    if (d <= 0.0f) return x;
    float g = 1.0f + 6.0f * d;
    return tanhf(x * g) / tanhf(g);
}

void Composer::beginSynth() {
    const SynthView& s = v_->synth;
    f_[0] = (s.windowMs > 0) ? (float)s.windowMs : (8000.0f / fmaxf(1.0f, s.noteHz));
    f_[1] = (float)(kGraphY1 - kGraphCY);
    if (s.attackMs <= f_[0]) {
        int16_t x = kGraphX0 + roundi(s.attackMs / f_[0] * (float)kGraphW);
        addNode(x, kGraphCY - roundi(s.level * f_[1]), kMagenta);
    }
}

float Composer::synthEnv(float tMs) const {
    const SynthView& s = v_->synth;
    if (tMs < s.attackMs) return tMs / fmaxf(0.001f, s.attackMs);
    float u = (tMs - s.attackMs) / fmaxf(1.0f, s.decayMs);
    return (u >= 1.0f) ? 0.0f : expf(-4.6f * u) * (1.0f - u * u * u);  // ~-40 dB at DECAY, 0 after it
}

float Composer::synthWave(int16_t i, float tMs) const {
    const SynthView& s = v_->synth;
    const float t = tMs * 0.001f;
    const float f = s.noteHz;
    float y = 0.0f;
    switch (s.engine) {
        case SY_KICK:
        case SY_TOM: {
            // pitch sweep: f(t) = f0 (1 + 8 SWEEP e^(-t / tau)); phase is its integral
            const float f0 = f * ((s.engine == SY_KICK) ? 0.25f : 0.5f);
            const float tau = 0.002f + s.b * 0.1f;
            const float ph = f0 * t + f0 * 8.0f * s.a * tau * (1.0f - expf(-t / tau));
            y = sinf(kTwoPi * ph);
            if (s.engine == SY_KICK) {
                y += s.c * hashNoise(i) * expf(-tMs / 2.0f);   // CLICK
            } else {
                y = y * (1.0f - 0.5f * s.c) + s.c * smoothNoise((float)i * 0.7f);  // NOISE
            }
            y = driveOf(y, s.d);
        } break;
        case SY_SNARE: {
            const float f0 = f * 0.75f;
            const float tau = 0.01f;
            const float ph = f0 * t + f0 * 4.0f * s.d * tau * (1.0f - expf(-t / tau));
            const float body = sinf(kTwoPi * ph);
            const float noise = smoothNoise((float)i * (0.3f + 1.7f * s.c));     // COLOR: brighter = denser
            y = s.a * body + (1.0f - s.a) * (0.3f + 0.7f * s.b) * noise;         // TONE, SNAP
        } break;
        case SY_HAT: {
            static const float r[6] = {2.0f, 3.0f, 4.16f, 5.43f, 6.79f, 8.21f};
            float m = 0.0f;
            for (uint8_t k = 0; k < 6; k++) m += waveOf(1, f * r[k] * t, 0.5f);
            m *= (1.0f / 6.0f);
            const float noise = hashNoise(i);
            y = s.a * m + (1.0f - s.a) * noise;                                  // METAL
            y = y * (0.4f + 0.6f * s.b) + s.c * 0.5f * sinf(kTwoPi * f * 8.0f * t); // COLOR, RESONANCE
            y = driveOf(y, s.d);
        } break;
        case SY_CLAP: {
            // 3 short bursts SPREAD apart, then a TAIL
            const float gap = 4.0f + s.a * 20.0f;
            float burst = 0.0f;
            for (uint8_t k = 0; k < 3; k++) {
                float dt = tMs - gap * (float)k;
                if (dt >= 0.0f) burst = fmaxf(burst, expf(-dt / 3.0f));
            }
            float tail = (tMs >= 2.0f * gap) ? s.c : 0.0f;
            float noise = smoothNoise((float)i * (0.3f + 1.7f * s.b));
            y = noise * fmaxf(burst, tail);
            y = driveOf(y, s.d);
        } break;
        case SY_ANALOG: {
            const uint8_t wave = (uint8_t)s.a;
            const float pw = 0.5f - 0.45f * s.b;
            float o1 = waveOf(wave, f * t, pw);
            float o2 = waveOf(wave, f * powf(2.0f, s.d / 1200.0f) * t, pw);
            float o = 0.5f * (o1 + o2);
            if (wave != 1) o = (1.0f - s.b) * o + s.b * foldUnit(o * (1.0f + 3.0f * s.b));  // SHAPE: wavefold
            y = (1.0f - 0.5f * s.c) * o + 0.5f * s.c * waveOf(1, 0.5f * f * t, 0.5f);       // SUB
        } break;
        case SY_FM: {
            const float e = synthEnv(tMs);
            const float index = s.b * 5.0f * ((1.0f - s.d) + s.d * e);
            const float mph = kTwoPi * s.a * f * t;
            const float mod = sinf(mph + s.c * 1.5f * sinf(mph));
            y = sinf(kTwoPi * f * t + index * mod);
        } break;
        case SY_AM: {
            const float car = waveOf((uint8_t)s.c, f * t, 0.5f);
            const float mod = sinf(kTwoPi * s.a * f * t);
            y = car * ((1.0f - s.b) + s.b * mod);
            y = (1.0f - 0.5f * s.d) * y + 0.5f * s.d * waveOf(1, 0.5f * f * t, 0.5f);
        } break;
        case SY_FOLD: {
            const float e = synthEnv(tMs);
            const float gain = 1.0f + 6.0f * s.a * ((1.0f - s.d) + s.d * e);
            y = foldUnit((sinf(kTwoPi * f * t) + s.b) * gain);
            y = (1.0f - 0.5f * s.c) * y + 0.5f * s.c * waveOf(1, 0.5f * f * t, 0.5f);
        } break;
        case SY_PLUCK: {
            // Karplus-Strong look: one noise period repeated, quieter and smoother every period
            const float periods = f * t;
            const float n = floorf(periods);
            const float phase = periods - n;
            const float points = 4.0f + 28.0f * s.b * powf(1.0f - 0.6f * s.a, n);
            const float string = smoothNoise(phase * points) * powf(1.0f - 0.5f * s.a, n);
            y = (1.0f - s.c) * string + s.c * sinf(kTwoPi * phase);
            y += s.d * hashNoise(i) * expf(-periods * 1.5f);
        } break;
        default:
            break;
    }
    return clampf(y, -1.0f, 1.0f);
}

void Composer::columnSynth(int16_t i) {
    grid(i);
    const SynthView& s = v_->synth;
    const float tMs = (float)i / (float)kGraphW * f_[0];
    const float e = synthEnv(tMs);
    const int16_t y = kGraphCY - roundi(synthWave(i, tMs) * e * s.level * f_[1]);
    if (y != kGraphCY) rows((y < kGraphCY) ? y + 1 : kGraphCY, (y < kGraphCY) ? kGraphCY : y - 1, kCyanDark);
    span(0, y, kWhite);
    span(1, (tMs <= s.attackMs + s.decayMs) ? (int16_t)(kGraphCY - roundi(e * s.level * f_[1])) : (int16_t)-1, kMagenta);  // ends with the decay
}

/* Info row -----------------------------------------------------------------*/

// items after the title, by kind (UI_REDESIGN.md: info row = title + gray items, equal gaps)
uint8_t infoItems(const View& v, char items[3][32]) {
    uint8_t n = 0;
    switch (v.kind) {
        case KIND_FILTER: {
            static const char* slopes[] = {"06 DB", "12 DB", "24 DB"};
            snprintf(items[n++], 32, "%s", v.name1);
            snprintf(items[n++], 32, "%s", slopes[(v.filter.slope < 3) ? v.filter.slope : 1]);
            fmtHz(items[n++], 32, v.filter.fc);
        } break;
        case KIND_REVERB: {
            char t[16];
            fmt1(t, sizeof(t), (200.0f + v.reverb.decay * 2800.0f) / 1000.0f, false);
            snprintf(items[n++], 32, "RT60 %s S", t);
            snprintf(items[n++], 32, "PRE %d MS", (int)roundi(v.reverb.predelayMs));
            if (v.reverb.lowCutHz > 0.0f) {
                char f[16];
                fmtHz(f, sizeof(f), v.reverb.lowCutHz);
                snprintf(items[n++], 32, "CUT %s", f);
            }
        } break;
        case KIND_MIXER: {
            char t[16];
            fmt1(t, sizeof(t), fminf(v.mixer.masterDb, v.mixer.ceilDb), true);
            snprintf(items[n++], 32, "OUT %s DB", (v.mixer.masterDb < -90.0f) ? "--" : t);
            fmt1(t, sizeof(t), v.mixer.ceilDb, true);
            snprintf(items[n++], 32, "LIMIT %s DB", t);
        } break;
        case KIND_LFO:
            snprintf(items[n++], 32, "%s", v.name1);
            snprintf(items[n++], 32, "%s BAR", v.name2);
            break;
        case KIND_INST:
            if (v.inst.hasSample) {
                snprintf(items[n++], 32, "%s", v.name1);  // instrument
                snprintf(items[n++], 32, "%s", v.name2);  // sample
                snprintf(items[n++], 32, "%d MS", (int)roundi(v.inst.lengthMs));
            } else {
                snprintf(items[n++], 32, "----");         // nothing loaded: a single dash field
            }
            break;
        case KIND_RHYTHM: {
            const RhythmView& r = v.rhythm;
            snprintf(items[n++], 32, "%u.%u BPM", (unsigned)(r.tempo10 / 10), (unsigned)(r.tempo10 % 10));
            snprintf(items[n++], 32, "%u/4", (unsigned)r.measure);
            snprintf(items[n++], 32, "%u BARS  SWING %u %%", (unsigned)r.bar, (unsigned)r.swing);
        } break;
        case KIND_METRO: {
            static const char* modes[3] = {"OFF", "RECORD", "ALWAYS"};
            snprintf(items[n++], 32, "%s", v.name1);
            snprintf(items[n++], 32, "%s", modes[(v.metro.mode < 3) ? v.metro.mode : 0]);
            if (v.metro.precountBars) {
                snprintf(items[n++], 32, "COUNT %u %s", (unsigned)v.metro.precountBars, (v.metro.precountBars > 1) ? "BARS" : "BAR");
            } else {
                snprintf(items[n++], 32, "NO COUNT");
            }
        } break;
        case KIND_SYNC: {
            static const char* roles[3] = {"INTERNAL", "MASTER", "SLAVE"};
            const SyncView& s = v.sync;
            snprintf(items[n++], 32, "%s", roles[(s.role < 3) ? s.role : 0]);
            snprintf(items[n++], 32, "%s%u.%u BPM", (s.role == 2) ? "IN " : "", (unsigned)(s.tempo10 / 10), (unsigned)(s.tempo10 % 10));
            if (s.syncIn > 0) {
                snprintf(items[n++], 32, "%u PPQN", (unsigned)s.inPpqn);
            } else if (s.syncOut > 0) {
                snprintf(items[n++], 32, "OUT %u PPQN", (unsigned)s.outPpqn);
            }
        } break;
        case KIND_SYNTH:
            snprintf(items[n++], 32, "%s", v.name1);  // engine
            snprintf(items[n++], 32, "%s", v.name2);  // note
            break;
        case KIND_EFFECT: {
            const EffectView& e = v.effect;
            snprintf(items[n++], 32, "%s", v.name1);
            char t[40];
            switch (e.type) {
                case FX_DELAY: snprintf(t, sizeof(t), "%d MS", (int)roundi(e.a)); break;
                case FX_CHORUS: {
                    char f[16];
                    fmt1(f, sizeof(f), e.a, false);
                    snprintf(t, sizeof(t), "%s MS", f);
                } break;
                case FX_FLANGER: {
                    char f[16];
                    fmt1(f, sizeof(f), e.a, false);
                    snprintf(t, sizeof(t), "%s MS", f);
                } break;
                case FX_PHASER: {
                    char a[16], b[16];
                    fmtHz(a, sizeof(a), fminf(e.a, e.b));
                    fmtHz(b, sizeof(b), fmaxf(e.a, e.b));
                    snprintf(t, sizeof(t), "%s - %s", a, b);
                } break;
                case FX_COMPRESSOR:
                case FX_EXPANDER: fmtDb(t, sizeof(t), e.a); break;
                case FX_OVERDRIVE:
                case FX_DISTORTION: fmtDb(t, sizeof(t), e.a); break;
                case FX_BITCRUSHER: snprintf(t, sizeof(t), "%d BIT", (int)roundi(e.a)); break;
                default: t[0] = '\0'; break;
            }
            snprintf(items[n++], 32, "%s", t);
        } break;
        default:
            break;
    }
    return n;
}

}  // namespace

/* Public -------------------------------------------------------------------*/

void clearView(View& v) { memset(&v, 0, sizeof(View)); }

bool Renderer::same(const View& v) const { return hasDrawn_ && (memcmp(&drawn_, &v, sizeof(View)) == 0); }

bool Renderer::needsReopen(const View& v) const {
    if (!hasDrawn_) return true;
    if (drawn_.kind != v.kind) return true;
    if (strncmp(drawn_.title, v.title, sizeof(v.title)) != 0) return true;
    if ((v.kind == KIND_EFFECT) && (drawn_.effect.type != v.effect.type)) return true;
    if ((v.kind == KIND_EFFECT) && (v.effect.type == FX_DELAY) && (delayWindowMs(drawn_.effect.a) != delayWindowMs(v.effect.a))) return true;  // time ruler
    if ((v.kind == KIND_SYNTH) && (drawn_.synth.windowMs != v.synth.windowMs)) return true;  // time ruler
    // rulers (bar / beat numbers) follow MEASURE, BAR and the precount
    if ((v.kind == KIND_RHYTHM) && ((drawn_.rhythm.bar != v.rhythm.bar) || (drawn_.rhythm.measure != v.rhythm.measure))) return true;
    if ((v.kind == KIND_METRO) && ((drawn_.metro.measure != v.metro.measure) || ((drawn_.metro.precountBars != 0) != (v.metro.precountBars != 0)))) return true;
    if ((v.kind == KIND_SYNC) && (drawn_.sync.measure != v.sync.measure)) return true;
    return false;
}

void Renderer::drawInfoItems(const View& v) {
    char items[3][32];
    uint8_t n = infoItems(v, items);
    char line[kInfoItemChars + 1];
    uint8_t p = 0;
    for (uint8_t k = 0; k < n; k++) {
        if (k > 0) {
            for (uint8_t g = 0; (g < 2) && (p < kInfoItemChars); g++) line[p++] = ' ';
        }
        for (const char* c = items[k]; (*c != '\0') && (p < kInfoItemChars); c++) line[p++] = *c;
    }
    while (p < kInfoItemChars) line[p++] = ' ';
    line[kInfoItemChars] = '\0';
    // visible gap after the title = 2 px glyph spacing + 14 px = 16 px, same as between items
    int16_t x = kGraphX0 + (int16_t)strnlen(v.title, sizeof(v.title)) * 9 + 14;
    surface_.text(FONT_SMALL, ALIGN_LEFT, kGray50, line, kInfoItemChars, x, kInfoY + 1);
}

void Renderer::drawStatic(const View& v) {
    surface_.text(FONT_MEDIUM, ALIGN_LEFT, kCyan, v.title, (uint16_t)strnlen(v.title, sizeof(v.title)), kGraphX0, kInfoY);
    drawInfoItems(v);
    Axes a = makeAxes(v);
    for (uint8_t k = 0; k < a.rightCount; k++) {
        surface_.text(FONT_SMALL, ALIGN_LEFT, kGray50, a.right[k].text, (uint16_t)strlen(a.right[k].text), kAxisLabelX, a.right[k].pos - 3);
    }
    for (uint8_t k = 0; k < a.bottomCount; k++) {
        uint16_t len = (uint16_t)strlen(a.bottom[k].text);
        if (a.bottomCyanLeft) {
            surface_.text(FONT_SMALL, ALIGN_LEFT, kCyan, a.bottom[k].text, len, a.bottom[k].pos, kRulerY);
        } else {
            int16_t half = (int16_t)(len * 7) / 2;
            int16_t x = clampi(a.bottom[k].pos, kGraphX0 + half, kGraphX1 - half);
            surface_.text(FONT_SMALL, ALIGN_CENTER, kGray50, a.bottom[k].text, len, x, kRulerY);
        }
    }
}

void Renderer::compose(const View* oldView, const View& newView) {
    static Color oldCol[kComposeRows];
    static Color newCol[kComposeRows];
    static Composer a, b;
    columnsInit();
    a.begin(oldView);
    b.begin(&newView);
    for (int16_t x = kComposeX0; x <= kComposeX1; x++) {
        a.column(x, oldCol);
        b.column(x, newCol);
        int16_t start = -1;
        for (int16_t r = 0; r <= kComposeRows; r++) {
            bool need = (r < kComposeRows) && (oldCol[r] != newCol[r]);
            if (need && (start < 0)) start = r;
            if (!need && (start >= 0)) {
                surface_.column(x, kComposeY0 + start, &newCol[start], (uint16_t)(r - start));
                start = -1;
            }
        }
    }
}

void Renderer::open(const View& v) {
    drawStatic(v);
    compose(nullptr, v);
    memcpy(&drawn_, &v, sizeof(View));
    hasDrawn_ = true;
}

void Renderer::update(const View& v) {
    if (!hasDrawn_) {
        open(v);
        return;
    }
    char oldItems[3][32], newItems[3][32];
    memset(oldItems, 0, sizeof(oldItems));
    memset(newItems, 0, sizeof(newItems));
    uint8_t no = infoItems(drawn_, oldItems), nn = infoItems(v, newItems);
    if ((no != nn) || (memcmp(oldItems, newItems, sizeof(oldItems)) != 0)) drawInfoItems(v);
    compose(&drawn_, v);
    memcpy(&drawn_, &v, sizeof(View));
}

}  // namespace panel
