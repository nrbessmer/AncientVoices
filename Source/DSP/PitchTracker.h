#pragma once
#include <JuceHeader.h>
#include <vector>
#include <numeric>

// ─────────────────────────────────────────────────────────────────────────────
//  PITCH TRACKER  — YIN algorithm
//
//  YIN is the standard approach for monophonic pitch detection on vocals.
//  It computes a difference function, normalises it (CMNDF), and finds the
//  first minimum below a confidence threshold.
//
//  Output: detected frequency in Hz (0 = unvoiced / no pitch detected)
//
//  Usage: call process(buffer, numSamples) once per block.
//         call getFrequency() to get latest result.
//         call getConfidence() to check voicing (0.0 = unvoiced, 1.0 = certain)
// ─────────────────────────────────────────────────────────────────────────────

class PitchTracker {
public:
    static constexpr int kMinPeriod = 32;   // ~1378 Hz at 44100
    static constexpr int kMaxPeriod = 2048; // ~21.5 Hz at 44100

    void prepare(double sampleRate, int blockSize) {
        sr_         = sampleRate;
        bufSize_    = kMaxPeriod * 2;
        buf_.assign(size_t(bufSize_), 0.f);
        diff_.assign(size_t(kMaxPeriod), 0.f);
        cmDiff_.assign(size_t(kMaxPeriod), 0.f);
        writePos_   = 0;
        freq_       = 0.f;
        confidence_ = 0.f;
    }

    void process(const float* samples, int numSamples) {
        // Fill circular buffer
        for (int i = 0; i < numSamples; ++i) {
            buf_[size_t(writePos_)] = samples[i];
            writePos_ = (writePos_ + 1) % bufSize_;
        }
        // Run YIN on latest window
        computeYIN();
    }

    float getFrequency()  const { return freq_;       }
    float getConfidence() const { return confidence_; }
    bool  isVoiced()      const { return freq_ > 40.f && confidence_ > 0.15f; }

private:
    double sr_      = 44100.0;
    int    bufSize_ = 4096;
    int    writePos_= 0;

    std::vector<float> buf_;
    std::vector<float> diff_;
    std::vector<float> cmDiff_;

    float freq_       = 0.f;
    float confidence_ = 0.f;

    void computeYIN() {
        int W = kMaxPeriod;

        // Step 1: Difference function
        for (int tau = 0; tau < W; ++tau) {
            float sum = 0.f;
            for (int j = 0; j < W; ++j) {
                int i0 = (writePos_ - W + j + bufSize_) % bufSize_;
                int i1 = (writePos_ - W + j + tau + bufSize_) % bufSize_;
                float d = buf_[size_t(i0)] - buf_[size_t(i1)];
                sum += d * d;
            }
            diff_[size_t(tau)] = sum;
        }

        // Step 2: Cumulative mean normalised difference
        cmDiff_[0] = 1.f;
        float runningSum = 0.f;
        for (int tau = 1; tau < W; ++tau) {
            runningSum += diff_[size_t(tau)];
            cmDiff_[size_t(tau)] = diff_[size_t(tau)] * float(tau) / runningSum;
        }

        // Step 3: Find first minimum below threshold
        static constexpr float kThreshold = 0.15f;
        int tauMin = -1;

        for (int tau = kMinPeriod; tau < W - 1; ++tau) {
            if (cmDiff_[size_t(tau)] < kThreshold) {
                // Parabolic interpolation for sub-sample accuracy
                float prev = cmDiff_[size_t(tau - 1)];
                float curr = cmDiff_[size_t(tau)];
                float next = cmDiff_[size_t(tau + 1)];
                float offset = (next - prev) / (2.f * (2.f * curr - prev - next));
                tauMin = tau;
                float tauInterp = float(tau) + offset;
                freq_       = float(sr_) / tauInterp;
                confidence_ = 1.f - curr;
                return;
            }
        }

        // No voiced pitch found
        freq_       = 0.f;
        confidence_ = 0.f;
    }
};
