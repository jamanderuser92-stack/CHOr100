// Chor100 - Klang-Engine (unabhaengig von JUCE)
// 1) Tonhoehenerkennung (YIN) auf der Eingangsstimme
// 2) 12 Kernstimmen: TD-PSOLA mit Formantverschiebung (andere "Saenger"), eigene Verstimmung + Vibrato
// 3) Bis zu 100 Klone der Kernstimmen: eigene Verzoegerung, Mini-Tonhoehendrift, Klangfilter, Panorama
#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace choir {

constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline int nextPow2(int v) { int p = 1; while (p < v) p <<= 1; return p; }

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float)(next() >> 8) * (1.0f / 16777216.0f); } // [0,1)
    float bi() { return uni() * 2.0f - 1.0f; }                           // [-1,1)
};

// schneller Sinus fuer Phase in [0, 2pi)
inline float fastSin(float ph) {
    float x = ph - kPi;                         // [-pi, pi)
    float y = 1.27323954f * x - 0.405284735f * x * std::fabs(x);
    y = 0.225f * (y * std::fabs(y) - y) + y;
    return -y;
}

inline float readCubic(const float* b, int64_t mask, double idx) {
    const int64_t i = (int64_t)std::floor(idx);
    const float t = (float)(idx - (double)i);
    const float xm1 = b[(i - 1) & mask], x0 = b[i & mask], x1 = b[(i + 1) & mask], x2 = b[(i + 2) & mask];
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

// ---------------------------------------------------------------- Tonhoehenerkennung
class PitchDetector {
public:
    void prepare(double sr) {
        decim = std::max(1, (int)std::lround(sr / 24000.0));
        const double dsr = sr / decim;
        tauMin = std::max(2, (int)(dsr / 1000.0));
        tauMax = (int)(dsr / 70.0) + 2;
        W = 512;
        span = W + tauMax + 2;
        const int rs = nextPow2(span * 2);
        ring.assign(rs, 0.0f); rMask = rs - 1; rPos = 0;
        scratch.assign(span, 0.0f);
        d.assign(tauMax + 2, 0.0f);
        cmnd.assign(tauMax + 2, 1.0f);
        hopDec = 128; hopOrig = hopDec * decim;
        sinceHop = 0; acc = 0.0f; accN = 0; sampleCount = 0;
        minP = sr / 1000.0; maxP = sr / 70.0;
        fallbackP = 0.005 * sr;
        hist.fill(fallbackP);
        vhist.fill(0.0f);
        med = { fallbackP, fallbackP, fallbackP };
        lastBlk = 0;
    }

    void push(float x) {
        acc += x;
        if (++accN == decim) {
            ring[(size_t)(rPos++ & rMask)] = acc / (float)decim;
            acc = 0.0f; accN = 0;
            if (++sinceHop >= hopDec) { sinceHop = 0; analyse(); }
        }
        ++sampleCount;
    }

    // Periode (in Samples) der Eingangsstimme zum Zeitpunkt t
    double periodAt(double t) const {
        int64_t blk = (int64_t)std::floor(t / (double)hopOrig);
        if (blk > lastBlk) blk = lastBlk;
        if (blk < lastBlk - 200) blk = lastBlk - 200;
        if (blk < 0) return fallbackP;
        return hist[(size_t)(blk & 255)];
    }

    // 1 = klar gesungener Ton, 0 = Rauschen/Konsonant/Stille
    float voicedAt(double t) const {
        int64_t blk = (int64_t)std::floor(t / (double)hopOrig);
        if (blk > lastBlk) blk = lastBlk;
        if (blk < lastBlk - 200) blk = lastBlk - 200;
        if (blk < 0) return 0.0f;
        return vhist[(size_t)(blk & 255)];
    }

    double maxPeriod() const { return maxP; }

private:
    void analyse() {
        if (rPos < span) return;
        for (int i = 0; i < span; ++i) scratch[i] = ring[(size_t)((rPos - span + i) & rMask)];

        double energy = 0.0;
        for (int j = 0; j < W; ++j) energy += scratch[j] * scratch[j];
        double P = fallbackP;
        float voiced = 0.0f;

        if (energy / W > 1.0e-7) {
            for (int tau = 1; tau <= tauMax; ++tau) {
                float s = 0.0f;
                const float* a = scratch.data();
                const float* b = scratch.data() + tau;
                for (int j = 0; j < W; ++j) { const float df = a[j] - b[j]; s += df * df; }
                d[tau] = s;
            }
            float running = 0.0f;
            cmnd[0] = 1.0f;
            for (int tau = 1; tau <= tauMax; ++tau) {
                running += d[tau];
                cmnd[tau] = running > 0.0f ? d[tau] * (float)tau / running : 1.0f;
            }
            int best = -1;
            for (int tau = tauMin; tau < tauMax; ++tau) {
                if (cmnd[tau] < 0.15f) {
                    while (tau + 1 < tauMax && cmnd[tau + 1] < cmnd[tau]) ++tau;
                    best = tau; break;
                }
            }
            if (best < 0) {
                best = tauMin;
                for (int tau = tauMin; tau < tauMax; ++tau) if (cmnd[tau] < cmnd[best]) best = tau;
            }
            if (cmnd[best] < 0.3f) {
                double tb = best;
                if (best > 1 && best < tauMax) {
                    const float y0 = cmnd[best - 1], y1 = cmnd[best], y2 = cmnd[best + 1];
                    const float den = y0 - 2.0f * y1 + y2;
                    if (std::fabs(den) > 1e-9f) tb += 0.5 * (y0 - y2) / den;
                }
                const double raw = std::clamp(tb * decim, minP, maxP);
                med[0] = med[1]; med[1] = med[2]; med[2] = raw;
                P = std::max(std::min(med[0], med[1]), std::min(std::max(med[0], med[1]), med[2]));
                voiced = std::clamp((0.3f - cmnd[best]) / 0.15f, 0.0f, 1.0f);
            }
        }

        const double centre = (double)sampleCount - 0.5 * span * decim;
        const int64_t blk = (int64_t)std::floor(centre / (double)hopOrig);
        if (blk > lastBlk) {
            int64_t from = std::max(lastBlk + 1, blk - 255);
            for (int64_t b = from; b <= blk; ++b) { hist[(size_t)(b & 255)] = P; vhist[(size_t)(b & 255)] = voiced; }
            lastBlk = blk;
        } else {
            hist[(size_t)(lastBlk & 255)] = P;
            vhist[(size_t)(lastBlk & 255)] = voiced;
        }
    }

    int decim = 1, tauMin = 2, tauMax = 100, W = 512, span = 600;
    std::vector<float> ring, scratch, d, cmnd;
    int64_t rMask = 0, rPos = 0;
    int hopDec = 128, hopOrig = 128, sinceHop = 0, accN = 0;
    float acc = 0.0f;
    int64_t sampleCount = 0, lastBlk = 0;
    double minP = 48, maxP = 686, fallbackP = 240;
    std::array<double, 256> hist{};
    std::array<float, 256> vhist{};
    std::array<double, 3> med{};
};

// ---------------------------------------------------------------- Engine
struct Params {
    int   voices   = 60;     // 1..100
    float detune   = 14.0f;  // Cent
    float timingMs = 25.0f;  // ms
    float vibrato  = 0.3f;   // 0..1
    float spread   = 0.6f;   // 0..1  Klangfarben-Streuung
    float tone     = 0.0f;   // -1..1 dunkel..hell
    float width    = 0.8f;   // 0..1
};

class ChoirEngine {
public:
    static constexpr int kCore = 16;
    static constexpr int kClones = 100;

    void prepare(double sr) {
        sampleRate = sr;
        det.prepare(sr);
        const double pMax = det.maxPeriod();
        latency = (int)std::ceil(2.0 * pMax) + 64;

        const int inSize = nextPow2(std::max(1 << 16, (int)(latency + 8 * pMax)));
        inBuf.assign(inSize, 0.0f); inMask = inSize - 1;

        const int coreSize = nextPow2((int)(0.12 * sr) + 64);
        coreMask = coreSize - 1;

        Rng rng(0xC40B100u);
        // Kernstimmen: Klangfarben gleichmaessig verteilt, Reihenfolge gemischt
        std::array<float, kCore> fn{};
        for (int v = 0; v < kCore; ++v) fn[v] = (float)v / (kCore - 1) * 2.0f - 1.0f;
        for (int v = kCore - 1; v > 0; --v) std::swap(fn[v], fn[rng.next() % (v + 1)]);

        for (int v = 0; v < kCore; ++v) {
            Core& c = cores[v];
            c.buf.assign(coreSize, 0.0f);
            c.formantNorm = fn[v];
            c.detuneNorm = rng.bi();
            c.vibRate = 4.4f + 1.8f * rng.uni();
            c.vibDepth = 0.5f + 0.5f * rng.uni();
            c.vibPhase = kTwoPi * rng.uni();
            c.driftRate = 0.05f + 0.25f * rng.uni();
            c.driftPhase = kTwoPi * rng.uni();
            c.nextGrain = 0.0;
            c.lastMark = -1.0e18;
            for (auto& g : c.g) g.active = false;
        }

        for (int k = 0; k < kClones; ++k) {
            Clone& cl = clones[k];
            cl.core = k % kCore;
            const int perCore = (kClones + kCore - 1) / kCore;
            const int j = k / kCore;
            cl.baseDelay = (k < kCore) ? 0.2f * rng.uni() : ((float)j + 0.8f * rng.uni()) / (float)perCore;
            cl.pan = (k == 0) ? 0.0f : rng.bi();
            const float fc = 3000.0f + 11000.0f * rng.uni();
            cl.lpCoef = 1.0f - std::exp(-kTwoPi * fc / (float)sr);
            cl.modRate = 0.4f + 1.2f * rng.uni();
            cl.modPhase = rng.uni();
            cl.modFrom = rng.uni(); cl.modTo = rng.uni();
            cl.modDepth = 0.4f + 0.6f * rng.uni();
            cl.gainVar = 0.7f + 0.3f * rng.uni();
            cl.g = 0.0f; cl.lp = 0.0f;
        }
        timingSm = -1.0f; detuneSm = -1.0f; widthSm = -1.0f;
        rng = Rng(0x5EED1234u);
        T = 0;
    }

    int getLatency() const { return latency; }

    void process(const float* in, float* outL, float* outR, int n, const Params& p) {
        const float sr = (float)sampleRate;
        if (timingSm < 0.0f) { timingSm = p.timingMs; detuneSm = p.detune; widthSm = p.width; }
        const float smooth = 1.0f - std::exp(-1.0f / (0.05f * sr));      // ~50 ms
        const float gSmooth = 1.0f - std::exp(-1.0f / (0.02f * sr));     // ~20 ms
        const float vSmooth = 1.0f - std::exp(-1.0f / (0.012f * sr));    // ~12 ms
        const int voices = std::clamp(p.voices, 1, kClones);

        widthSm += (p.width - widthSm) * std::min(1.0f, smooth * n);
        for (auto& cl : clones) {
            const float pan = cl.pan * widthSm;
            const float a = (pan + 1.0f) * 0.25f * kPi;
            cl.gl = std::cos(a) * 1.41421356f;
            cl.gr = std::sin(a) * 1.41421356f;
        }

        for (int i = 0; i < n; ++i) {
            const float x = in[i];
            inBuf[(size_t)(T & inMask)] = x;
            det.push(x);
            timingSm += (p.timingMs - timingSm) * smooth;
            detuneSm += (p.detune - detuneSm) * smooth;

            // ---- Kernstimmen
            for (int v = 0; v < kCore; ++v) {
                Core& c = cores[v];
                c.vibPhase += kTwoPi * c.vibRate / sr;   if (c.vibPhase >= kTwoPi) c.vibPhase -= kTwoPi;
                c.driftPhase += kTwoPi * c.driftRate / sr; if (c.driftPhase >= kTwoPi) c.driftPhase -= kTwoPi;

                int guard = 0;
                while ((double)T >= c.nextGrain && guard++ < 4) spawnGrain(c, p);

                float out = 0.0f;
                for (auto& g : c.g) {
                    if (!g.active) continue;
                    const double idx = g.centre + ((double)g.pos - 0.5 * g.len) * g.f;
                    const float w = 0.5f - 0.5f * std::cos(kTwoPi * g.pos / g.len);
                    out += readCubic(inBuf.data(), inMask, idx) * w * g.gain;
                    g.pos += 1.0f;
                    if (g.pos >= g.len) g.active = false;
                }
                // Konsonanten/Atem nicht zerhacken: dort die Originalstimme durchlassen
                const float vt = det.voicedAt((double)T - latency);
                c.voicedSm += (vt - c.voicedSm) * vSmooth;
                const float direct = inBuf[(size_t)((T - latency) & inMask)];
                c.buf[(size_t)(T & coreMask)] = out * c.voicedSm + direct * (1.0f - c.voicedSm);
            }

            // ---- Klone
            float L = 0.0f, R = 0.0f, sumG2 = 0.0f;
            const float modMs = 0.6f + 2.4f * detuneSm / 50.0f;
            for (int k = 0; k < kClones; ++k) {
                Clone& cl = clones[k];
                const float target = (k < voices) ? 1.0f : 0.0f;
                cl.g += (target - cl.g) * gSmooth;
                if (cl.g < 1.0e-5f && target == 0.0f) { cl.g = 0.0f; continue; }

                cl.modPhase += cl.modRate / sr;
                if (cl.modPhase >= 1.0f) { cl.modPhase -= 1.0f; cl.modFrom = cl.modTo; cl.modTo = rng.uni(); }
                const float sm = cl.modPhase * cl.modPhase * (3.0f - 2.0f * cl.modPhase);
                const float wander = cl.modFrom + (cl.modTo - cl.modFrom) * sm;   // 0..1, zufaellige Drift
                const float dMs = timingSm * cl.baseDelay + modMs * cl.modDepth * wander;
                const double dSamp = (double)dMs * 0.001 * sampleRate + 2.0;
                const float y = readCubic(cores[cl.core].buf.data(), coreMask, (double)T - dSamp);
                cl.lp += cl.lpCoef * (y - cl.lp);
                const float gg = cl.g * cl.gainVar;
                const float s = cl.lp * gg;
                L += s * cl.gl; R += s * cl.gr;
                sumG2 += gg * gg;
            }
            const float norm = 1.0f / std::sqrt(std::max(0.5f, sumG2));
            outL[i] = L * norm;
            outR[i] = R * norm;
            ++T;
        }
    }

private:
    struct Grain { double centre = 0; float len = 1, pos = 0, f = 1, gain = 1; bool active = false; };
    struct Core {
        std::array<Grain, 10> g{};
        std::vector<float> buf;
        float formantNorm = 0, detuneNorm = 0, vibRate = 5, vibDepth = 1, vibPhase = 0, driftRate = 0.1f, driftPhase = 0;
        float voicedSm = 0;
        double nextGrain = 0, lastMark = -1.0e18;
    };
    struct Clone {
        int core = 0;
        float baseDelay = 0, pan = 0, lpCoef = 1, modRate = 0.2f, modPhase = 0, modDepth = 1, gainVar = 1;
        float modFrom = 0, modTo = 0;
        float g = 0, lp = 0, gl = 1, gr = 1;
    };

    void spawnGrain(Core& c, const Params& p) {
        const double P = det.periodAt((double)T - latency);
        const float f = std::clamp(std::pow(2.0f, 0.4f * p.tone + 0.35f * p.spread * c.formantNorm), 0.6f, 1.65f);
        const float cents = detuneSm * c.detuneNorm
                          + p.vibrato * 25.0f * c.vibDepth * std::sin(c.vibPhase)
                          + detuneSm * 0.3f * std::sin(c.driftPhase);
        const double ratio = std::pow(2.0, cents / 1200.0);
        const float len = (float)(2.0 * P / f);

        const double target = (double)T + 0.5 * len - latency;
        double m = c.lastMark;
        if (std::fabs(target - m) > 4.0 * P) m = target;
        else while (m < target - 0.5 * P) m += P;
        c.lastMark = m;

        Grain* slot = nullptr;
        for (auto& g : c.g) if (!g.active) { slot = &g; break; }
        if (slot != nullptr) {
            slot->active = true;
            slot->centre = m;
            slot->len = std::max(4.0f, len);
            slot->pos = 0.0f;
            slot->f = f;
            slot->gain = (float)(f / ratio) * (1.0f + 0.08f * rng.bi());
        }
        c.nextGrain += (P / ratio) * (1.0 + 0.015 * rng.bi());
        if (c.nextGrain < (double)T) c.nextGrain = (double)T + P / ratio;
    }

    double sampleRate = 48000.0;
    int latency = 1500;
    PitchDetector det;
    std::vector<float> inBuf;
    int64_t inMask = 0, coreMask = 0;
    std::array<Core, kCore> cores;
    std::array<Clone, kClones> clones;
    float timingSm = -1, detuneSm = -1, widthSm = -1;
    int64_t T = 0;
    Rng rng{1u};
};

} // namespace choir
