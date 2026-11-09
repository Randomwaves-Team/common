#include "EffectGraph.h"

#include <stdio.h>

// View values per type (a, b, c, d):
//   DELAY      time ms, level 0..1, feedback 0..1, damping 0..1
//   CHORUS     time ms, feedback 0..1, rate Hz, depth 0..1
//   FLANGER    time ms, feedback 0..1, rate Hz, width 0..1
//   PHASER     start Hz, end Hz, rate Hz, feedback 0..1
//   COMP / EXP threshold dB, ratio, attack ms, release ms
//   DRIVE      gain dB, threshold dB, tone Hz, color / bias 0..1
//   BITCRUSHER bits, divider N, threshold dB, smooth 0..100
// Composer scratch: DELAY f 0 ms, 1 l0, 2 fb, 3 fbh, 4 window ms; PHASER f 0 l1, 1 l2, 2 sp;
// COMP / EXP, DRIVE, BITCRUSHER: sequential simulation in s0 / simN / simCnt / simHeld.

namespace panel {

const EffectGraph effectGraph;

namespace {

// DELAY graph length: room for ~8 repeats (500 / 1000 / 2000 / 4000 ms)
int16_t delayWindowMs(float ms) {
    static const int16_t windows[] = {500, 1000, 2000, 4000};
    for (uint8_t k = 0; k < 4; k++) {
        if (8.0f * ms <= (float)windows[k]) return windows[k];
    }
    return 4000;
}

}  // namespace

void EffectGraph::axes(const View& v, Axes& a) const {
    switch (v.effect.type) {
        case FX_DELAY: {
            static const int8_t dbs[] = {0, -12, -24, -36, -48};
            addDbGrid(a, 0.0f, -48.0f, dbs, 5);
            const int16_t w = delayWindowMs(v.effect.a);
            addTimeRuler(a, w, (int16_t)(w / 4));
        } break;
        case FX_CHORUS: {
            static const char* const t[] = {"100", "75", "50", "25", "0"};
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
}

uint8_t EffectGraph::info(const View& v, char items[kInfoItems][kInfoItemLen]) const {
    uint8_t n = 0;
    const EffectView& e = v.effect;
    snprintf(items[n++], kInfoItemLen, "%s", v.name1);
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
    snprintf(items[n++], kInfoItemLen, "%s", t);
    return n;
}

bool EffectGraph::needsReopen(const View& drawn, const View& v) const {
    if (drawn.effect.type != v.effect.type) return true;
    if ((v.effect.type == FX_DELAY) && (delayWindowMs(drawn.effect.a) != delayWindowMs(v.effect.a))) return true;  // time ruler
    return false;
}

void EffectGraph::begin(Composer& c, const View& v) const {
    const EffectView& e = v.effect;
    const Color cc = e.active ? kCyan : kGray50;
    switch (e.type) {
        case FX_DELAY: {
            // scratch f: 0 ms, 1 l0, 2 fb, 3 fbh
            c.f[0] = fmaxf(1.0f, e.a);
            c.f[1] = dbOf(fmaxf(0.004f, e.b * e.mix));
            c.f[2] = dbOf(fmaxf(0.004f, e.c));
            c.f[3] = c.f[2] - 6.0f * e.d;
            c.f[4] = (float)delayWindowMs(e.a);
            const float win = c.f[4];
            auto X = [win](float m) { return (int16_t)(kGraphX0 + roundi(clampf(m, 0.0f, win) / win * (float)kGraphW)); };
            c.addNode(X(c.f[0]), dbToY(c.f[1], 0.0f, -48.0f), cc);
            float me = c.f[0] * (1.0f + (-48.0f - c.f[1]) / c.f[2]);
            if ((c.f[2] < 0.0f) && (me < win)) c.addNode(X(me), kGraphY1, cc);
            float mh = c.f[0] * (1.0f + (-48.0f - c.f[1]) / c.f[3]);
            if ((e.d > 0.0f) && (c.f[3] < 0.0f) && (mh < win) && ((X(me) - X(mh)) >= 12)) c.addNode(X(mh), kGraphY1, e.active ? kMagenta : kGray50);
        } break;
        case FX_PHASER: {
            // scratch f: 0 l1, 1 l2, 2 sp
            float f1 = e.a, f2 = e.b;
            if (f2 < f1) {
                float t = f1;
                f1 = f2;
                f2 = t;
            }
            f1 = fmaxf(f1, kFreqMin);
            f2 = fmaxf(f2, f1 * 1.05f);
            c.f[0] = log10f(f1);
            c.f[1] = log10f(f2);
            c.f[2] = (c.f[1] - c.f[0]) * 0.3f;
        } break;
        default:
            break;
    }
}

void EffectGraph::column(Composer& c, const View& v, int16_t i) const {
    const EffectView& e = v.effect;
    const bool act = e.active;
    const Color CW = act ? kWhite : kGray50;
    const Color CM = act ? kMagenta : kGray50;
    const Color FD = act ? kCyanDark : kGray15;
    const Color FM = act ? kMagentaDark : kGray15;
    const float t = (float)i / (float)kGraphW;  // 0..1 across the graph
    const float amp = (float)kGraphH * 0.5f;

    switch (e.type) {
        case FX_DELAY: {
            const float ms = c.f[0], l0 = c.f[1], fb = c.f[2], fbh = c.f[3];
            const float win = c.f[4];
            float m = t * win;
            auto X = [win](float mm) { return (int16_t)(kGraphX0 + roundi(clampf(mm, 0.0f, win) / win * (float)kGraphW)); };
            int16_t y = -1, yh = -1;
            if (kGraphX0 + i < X(ms)) c.rows(dbToY(l0, 0.0f, -48.0f), kGraphY1, kGray15);
            if (m >= ms) {
                float a = l0 + (m / ms - 1.0f) * fb, b = l0 + (m / ms - 1.0f) * fbh;
                if (a >= -48.0f) y = dbToY(a, 0.0f, -48.0f);
                if (b >= -48.0f) yh = dbToY(b, 0.0f, -48.0f);
            }
            if (y >= 0) {
                int16_t h = ((e.d > 0.0f) && (yh >= 0)) ? yh : kGraphY1;
                if (h > y) c.rows(y + 1, h, (e.d > 0.0f) ? FM : FD);
                if (h < kGraphY1) c.rows(h + 1, kGraphY1, FD);
            }
            c.grid(i);
            c.span(0, (e.d > 0.0f) ? yh : -1, CM);
            c.span(1, y, CW);
            int16_t k = roundi(m / ms);
            if ((k >= 1) && (X((float)k * ms) == kGraphX0 + i) && (kGraphX0 + i < kGraphX1)) {
                float d = l0 + (float)(k - 1) * fb;
                if (d >= -48.0f) c.rows(dbToY(d, 0.0f, -48.0f), kGraphY1, CW);
            }
        } break;

        case FX_CHORUS: {
            const float base = e.a, dep = e.d * base * 0.5f, hz = e.c;
            const int16_t nfb = roundi(e.b * 3.0f);
            auto Ym = [](float mm) { return (int16_t)roundi((float)kGraphY1 - clampf(mm, 0.0f, 100.0f) / 100.0f * (float)kGraphH); };
            float dl = base + dep * sinf(t * 2.0f * hz * 2.0f * kPi);
            int16_t yb = Ym(base), y = Ym(dl);
            if (y < yb) c.rows(y + 1, yb, FD);
            else if (y > yb) c.rows(yb, y - 1, FD);
            c.grid(i);
            for (int16_t k = 2; k <= 4; k++) {
                float mm = (float)k * dl;
                c.span((uint8_t)k, ((k <= nfb + 1) && (mm <= 100.0f)) ? Ym(mm) : -1, CM);
            }
            c.span(0, y, CW);
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
                float cen = c.f[0] + 0.5f * c.f[2] + (c.f[1] - c.f[0] - c.f[2]) * (0.5f + 0.5f * ph);
                n = 2;
                for (uint8_t k = 0; k < 2; k++) notch[k] = powf(10.0f, cen + ((float)(k + 1) - 1.5f) * c.f[2]);
                notch[2] = 0.0f;
                peak = powf(10.0f, cen);
                npk = roundi(e.d * 2.0f);
            }
            auto Yf = [](float f) { return ((f >= kFreqMin) && (f <= kFreqMax)) ? freqToY(f) : (int16_t)-1; };
            int16_t y1 = freqToY(notch[0]);
            if (y1 < kGraphY1) c.rows(y1 + 1, kGraphY1, FD);
            c.grid(i);
            if (e.type == FX_PHASER) {
                c.rows(freqToY(powf(10.0f, c.f[1])), freqToY(powf(10.0f, c.f[1])), kGray50);
                c.rows(freqToY(powf(10.0f, c.f[0])), freqToY(powf(10.0f, c.f[0])), kGray50);
            }
            c.span(0, (npk > 0) ? Yf(peak) : -1, CM);
            for (int8_t k = (int8_t)n - 1; k >= 1; k--) c.span((uint8_t)(k + 1), Yf(notch[k]), kGray50);
            c.span(1, Yf(notch[0]), CW);
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
            float tc = (tg > c.s0) ? (comp ? atk : rl) : (comp ? rl : atk);
            c.s0 += (tg - c.s0) * (1.0f - expf(-dt / tc));
            int16_t yi = dbToY(din, 0.0f, -48.0f), yo = dbToY(din - c.s0, 0.0f, -48.0f);
            if (yo > yi) c.rows(yi + 1, yo, FM);
            int16_t top = (yo > yi) ? yo : yi;
            if (top < kGraphY1) c.rows(top + 1, kGraphY1, FD);
            c.grid(i);
            int16_t yt = dbToY(thr, 0.0f, -48.0f);
            c.rows(yt, yt, kGray50);
            c.span(0, yi, kGray50);
            c.span(1, yo, CW);
        } break;

        case FX_OVERDRIVE:
        case FX_DISTORTION: {
            const float gain = powf(10.0f, e.a / 20.0f), th = powf(10.0f, e.b / 20.0f), cb = e.d;
            const float dt = 0.02f / (float)kGraphW, al = 1.0f - expf(-2.0f * kPi * e.c * dt);
            float s = sinf(t * 4.0f * kPi);
            float sh;
            if (e.type == FX_OVERDRIVE) sh = tanhf((s + cb * 0.3f) * gain / th) * th;  // COLOR shifts the input: asymmetric, stays within +-THRESHOLD
            else sh = clampf(s * gain + cb * 0.5f * th, -th, th);
            c.s0 += (sh - c.s0) * al;
            int16_t yi = roundi((float)kGraphCY - s * amp), y = roundi((float)kGraphCY - clampf(c.s0, -1.0f, 1.0f) * amp);
            if (y < kGraphCY) c.rows(y + 1, kGraphCY, FD);
            else if (y > kGraphCY) c.rows(kGraphCY, y - 1, FD);
            c.grid(i);
            c.span(0, yi, kGray50);
            c.span(1, y, CW);
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
            while (c.simN <= n) {
                float s = sinf(2.0f * kPi * 100.0f * (float)c.simN / kSampleRate) * 0.95f;
                c.simCnt++;
                if (c.simCnt >= div) {
                    c.simCnt = 0;
                    c.simHeld = (fabsf(s) < 0.002f) ? 0.0f : q(fold(s));
                }
                c.s0 += al * (c.simHeld - c.s0);
                c.simN++;
            }
            float s = sinf(2.0f * kPi * 100.0f * (float)n / kSampleRate) * 0.95f;
            int16_t yi = roundi((float)kGraphCY - s * amp), y = roundi((float)kGraphCY - clampf(c.s0, -1.0f, 1.0f) * amp);
            if (y < kGraphCY) c.rows(y + 1, kGraphCY, FD);
            else if (y > kGraphCY) c.rows(kGraphCY, y - 1, FD);
            c.grid(i);
            c.span(0, yi, kGray50);
            c.span(1, y, CW);
        } break;

        default:
            c.grid(i);
            break;
    }
}

}  // namespace panel
