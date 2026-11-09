#ifndef __PANEL_H
#define __PANEL_H

// Center panel renderer (UI_REDESIGN.md sections 1, 11 and the panel specs).
//
// Hardware-free on purpose: it only needs a Surface (column + text output), so the same code runs
// on the device (Surface = Lcd) and on a PC (Tools/panel_preview, Surface = framebuffer -> PNG).
// The caller describes the panel with a View (plain numbers and short strings, already converted
// from the firmware tables); the renderer draws it and, on later calls, rewrites only the pixels
// that changed (old and new column colors are composed and compared).
//
// What a kind draws (labels, info row, columns) is a Graph (Graph.h): the shared kinds are in this
// folder (FilterGraph ... SyncGraph), product kinds live in the product and are passed to the
// Renderer as a GraphList (Drumboy: INST, SYNTH in Core/Library/Panel).

#include <stdint.h>

namespace panel {

typedef uint16_t Color;  // RGB565

// Colors (same values as Global.h)
const Color kBlack = 0x0000;
const Color kWhite = 0xFFFF;
const Color kCyan = 0x07FF;
const Color kMagenta = 0xF81F;
const Color kGray15 = 0x2124;
const Color kGray35 = 0x5ACB;
const Color kGray50 = 0x8410;
const Color kGray75 = 0xBDF7;
const Color kCyanDark = 0x0166;
const Color kMagentaDark = 0x4009;

// Geometry (UI_REDESIGN.md 12, "Panel ortak ölçüleri")
const int16_t kGraphX0 = 319;                         // = kMenuHeaderX[1]
const int16_t kGraphX1 = 903;
const int16_t kGraphY0 = 192;                         // layer 02 bottom line
const int16_t kGraphY1 = 378;                         // layer 08 bottom line
const int16_t kGraphW = kGraphX1 - kGraphX0;          // 584
const int16_t kGraphH = kGraphY1 - kGraphY0;          // 186
const int16_t kGraphCY = (kGraphY0 + kGraphY1) / 2;   // 285
const int16_t kColumns = kGraphW + 1;                 // 585
const int16_t kNodeHalf = 4;                          // 9x9 node square
const int16_t kComposeX0 = kGraphX0 - kNodeHalf;
const int16_t kComposeX1 = kGraphX1 + kNodeHalf;
const int16_t kComposeY0 = kGraphY0 - kNodeHalf;
const int16_t kComposeY1 = kGraphY1 + kNodeHalf;
const int16_t kComposeRows = kComposeY1 - kComposeY0 + 1;
const int16_t kInfoY = 172;                           // layer 02 text row
const int16_t kAxisLabelX = 910;                      // labels right of the graph
const int16_t kRulerY = 390;                          // layer 09 text row (under the graph)
const uint8_t kInfoItemChars = 36;                    // padded length of the info items string

enum Font : uint8_t { FONT_SMALL = 0, FONT_MEDIUM = 1 };  // Font_05x07 (7 px pitch), Font_07x09 (9 px pitch)
enum Align : uint8_t { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2 };

class Surface {
   public:
    virtual ~Surface() {}
    // n pixels of individual colors in column x, rows y .. y + n - 1
    virtual void column(int16_t x, int16_t y, const Color* colors, uint16_t n) = 0;
    // text on a black background (each glyph cell is fully written)
    virtual void text(Font font, Align align, Color color, const char* text, uint16_t len, int16_t x, int16_t y) = 0;
};

// Kinds drawn by the shared graphs (common/panel/*Graph). Numbers not listed here are free for
// product kinds (Drumboy: 7 INST, 8 SYNTH); new product kinds start at KIND_PRODUCT. A product
// passes its graphs to the Renderer in a GraphList (below).
enum Kind : uint8_t {
    KIND_NONE = 0,
    KIND_FILTER = 1,
    KIND_EQ = 2,
    KIND_REVERB = 3,
    KIND_MIXER = 4,
    KIND_LFO = 5,
    KIND_EFFECT = 6,
    KIND_RHYTHM = 9,
    KIND_METRO = 10,
    KIND_SYNC = 11,
    KIND_SHARED_END = 12,   // shared kinds are < KIND_SHARED_END
    KIND_PRODUCT = 16
};

enum FilterShape : uint8_t { FILTER_LP = 1, FILTER_HP = 2, FILTER_BP = 3, FILTER_BS = 4 };
enum EqBand : uint8_t { EQ_BAND_LOWSHELF = 0, EQ_BAND_HIGHSHELF = 1, EQ_BAND_PEAK = 2 };
enum EffectType : uint8_t {
    FX_DELAY = 0,
    FX_CHORUS = 1,
    FX_FLANGER = 2,
    FX_PHASER = 3,
    FX_COMPRESSOR = 4,
    FX_EXPANDER = 5,
    FX_OVERDRIVE = 6,
    FX_DISTORTION = 7,
    FX_BITCRUSHER = 8
};

struct FilterView {
    uint8_t active;
    uint8_t type;           // FilterShape
    uint8_t slope;          // 0 = 6 dB, 1 = 12 dB, 2 = 24 dB
    float fc;               // Hz
    float q;
    float mix;              // 0..1
    float env;              // -1..1 (+-3 octaves)
};

struct EqView {
    uint8_t active;
    uint8_t band[6];        // EqBand
    float freq[6];          // Hz
    float gainDb[6];
    float q[6];
};

struct ReverbView {
    uint8_t active;
    float size;             // 0..1
    float decay;            // 0..1 (RT60 200..3000 ms)
    float damping;          // 0..1
    float predelayMs;
    float mix;              // 0..1
    float lowCutHz;         // input high-pass, 0 = OFF (info row only: the graph is time / dB)
};

struct MixerView {
    float masterDb;         // MASTER gain, dB (-99 = off)
    float ceilDb;           // LIMITER ceiling
    float hpDb;             // HEADPHONE gain (0 = not drawn)
    float loDb;             // LINE OUT gain (0 = not drawn)
    float panLDb;           // channel gains from PAN (0 = not drawn)
    float panRDb;
};

struct LfoView {
    uint8_t active;
    uint8_t invert;
    float cyclesPerBar;
    float phase;            // 0..1 of a cycle
    float shape;            // 0..1 (0.5 = neutral)
    float jitter;           // 0..1
    float depth;            // 0..1
    uint8_t holdSteps;      // 0 = continuous; n = sample and hold, n steps per cycle (RANDOM)
    const float* table;     // firmware waveform (0..1, center 0.5)
    uint16_t tableLen;
};

struct EffectView {
    uint8_t active;
    uint8_t type;           // EffectType
    float a, b, c, d;       // physical values, see EffectGraph.cpp (per type)
    float mix;              // 0..1
};

// RHYTHM / METRONOME / SYNC (UI_REDESIGN.md 6)
struct RhythmView {
    uint8_t measure;        // beats per bar
    uint8_t bar;            // bars in the song
    uint8_t swing;          // 50..75 %
    uint8_t loopStart;      // 1-based bars
    uint8_t loopEnd;
    uint16_t swingTicks;    // SWING GRID in ticks (800 = 1 beat)
    uint16_t quantTicks;    // QUANTIZE grid in ticks
    uint16_t tempo10;       // BPM x 10
    int16_t playBeat;       // beat of the song being played (-1 = stopped)
};

struct MetroView {
    uint8_t mode;           // 0 OFF, 1 RECORD, 2 ALWAYS
    uint8_t precountBars;   // 0 = no precount
    uint8_t measure;
    uint8_t rateDiv;        // clicks per beat
    float volume;           // 0..1
    float accent;           // 0..1 (other clicks quieter than the bar click)
};

struct SyncView {
    uint8_t syncIn, syncOut, midiIn, midiOut;   // modes (0 = OFF; MIDI >= 2 carries clock)
    uint8_t inPpqn, outPpqn;                    // pulses per beat
    uint8_t measure;
    uint8_t role;           // 0 INTERNAL, 1 MASTER, 2 SLAVE
    uint8_t liveSync;       // pulse in the last ~150 ms
    uint8_t liveMidi;       // MIDI clock running
    uint16_t tempo10;       // BPM x 10 (measured when SLAVE)
};

// Raw bytes for product kinds (Drumboy InstView / SynthView): products keep their view structs
// in their own code and reach them with productView<T>(v). Raise it when a product view grows.
const uint16_t kProductViewBytes = 96;

struct View {
    uint8_t kind;           // Kind (or a product kind)
    char title[12];         // "FILTER 1"
    char name1[12];         // table names used in the info row (type, rate ...)
    char name2[12];
    union {                 // only the member of 'kind' is used; clearView zeroes all of it
        FilterView filter;
        EqView eq;
        ReverbView reverb;
        MixerView mixer;
        LfoView lfo;
        EffectView effect;
        RhythmView rhythm;
        MetroView metro;
        SyncView sync;
        uint8_t product[kProductViewBytes];
    };
};

// Typed access to the product block (T: plain struct, all zero after clearView)
template <typename T>
inline T& productView(View& v) {
    static_assert(sizeof(T) <= kProductViewBytes, "product view larger than kProductViewBytes");
    static_assert(alignof(T) <= alignof(View), "product view alignment");
    return *reinterpret_cast<T*>(v.product);
}

template <typename T>
inline const T& productView(const View& v) {
    static_assert(sizeof(T) <= kProductViewBytes, "product view larger than kProductViewBytes");
    static_assert(alignof(T) <= alignof(View), "product view alignment");
    return *reinterpret_cast<const T*>(v.product);
}

void clearView(View& v);    // zero everything (also padding) before filling

class Graph;

// Product graphs: {kind, graph} pairs in Flash, given to the Renderer (no registration at run time).
// A product kind must not use a shared Kind number (the shared graph would win).
struct GraphEntry {
    uint8_t kind;
    const Graph* graph;
};

struct GraphList {
    const GraphEntry* entries;
    uint8_t count;
};

class Renderer {
   public:
    explicit Renderer(Surface& surface, const GraphList* productGraphs = nullptr) : surface_(surface), product_(productGraphs), hasDrawn_(false) {}

    // Full draw on a cleared panel area (caller clears and draws the x 305 divider).
    void open(const View& v);
    // Redraws only what changed since the last open / update.
    void update(const View& v);
    // True when v equals what is on screen (nothing to do).
    bool same(const View& v) const;
    // True when v needs a full open (other kind, title or a graph's static labels change).
    bool needsReopen(const View& v) const;
    // Graph of a kind: shared kinds first, then the product list (nullptr = nothing drawn).
    const Graph* graphFor(uint8_t kind) const;

   private:
    Surface& surface_;
    const GraphList* product_;
    View drawn_;
    bool hasDrawn_;

    void drawStatic(const View& v);
    void drawInfoItems(const View& v);
    void compose(const View* oldView, const View& newView);
};

}  // namespace panel

#endif
