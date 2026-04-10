#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include "../PluginProcessor.h"

//==============================================================================
// SpectrumAnalyser.h
//
// A collapsible panel showing overlaid dry and wet spectra on a log
// frequency axis. Reads the processor's spectrum FIFO through
// tryReadSpectrumSnapshot, which is a SpinLock::ScopedTryLock internally -
// if the audio thread is mid-push we skip the frame and keep showing the
// previous one.
//
// Frequency axis: 20 Hz .. 20 kHz log-spaced
// Magnitude axis: -90 dBFS .. 0 dBFS linear
// Dry trace: grey
// Wet trace: white on slate blue
// Update rate: 20 Hz
//
// FFT size matches the processor's kSpectrumFftSize (2048 points, order 11).
// All working buffers are members sized at construction - no allocation on
// the timer tick.
//==============================================================================

namespace AncientVoicesUI
{
    class SpectrumAnalyser : public juce::Component,
                             private juce::Timer
    {
    public:
        static constexpr int kFftOrder = AncientVoicesAudioProcessor::kSpectrumFftOrder;
        static constexpr int kFftSize  = AncientVoicesAudioProcessor::kSpectrumFftSize;
        static constexpr int kNumBins  = kFftSize / 2;

        explicit SpectrumAnalyser (const AncientVoicesAudioProcessor& p)
            : processor (p),
              fft (kFftOrder),
              window (kFftSize, juce::dsp::WindowingFunction<float>::hann)
        {
            for (auto& v : dryDbSmoothed) v = kMinDb;
            for (auto& v : wetDbSmoothed) v = kMinDb;

            startTimerHz (20);
        }

        ~SpectrumAnalyser() override
        {
            stopTimer();
        }

        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat();

            g.setColour (surfaceColour);
            g.fillRoundedRectangle (bounds, 6.0f);

            g.setColour (borderColour);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

            // Header
            auto area = getLocalBounds().reduced (12, 10);
            g.setColour (textColour);
            g.setFont (juce::Font (14.0f, juce::Font::bold));
            g.drawText ("SPECTRUM",  area.removeFromTop (20), juce::Justification::left);
            area.removeFromTop (4);

            // Plot area
            const auto plotArea = area.toFloat();
            if (plotArea.getWidth() < 10.0f || plotArea.getHeight() < 10.0f)
                return;

            drawGrid (g, plotArea);
            drawTrace (g, plotArea, dryDbSmoothed, dryTraceColour, 1.0f);
            drawTrace (g, plotArea, wetDbSmoothed, wetTraceColour, 1.5f);
            drawLegend (g, plotArea);
        }

        void resized() override
        {
            // Paints directly; no child components.
        }

    private:
        static constexpr float kMinDb = -90.0f;
        static constexpr float kMaxDb =   0.0f;
        static constexpr float kMinHz =  20.0f;
        static constexpr float kMaxHz = 20000.0f;

        //----------------------------------------------------------------------
        // Timer tick: read the snapshot, FFT, smooth, repaint
        //----------------------------------------------------------------------
        void timerCallback() override
        {
            if (! processor.tryReadSpectrumSnapshot (dryTimeData.data(), wetTimeData.data()))
                return;  // Audio thread is pushing; skip this frame

            computeSpectrum (dryTimeData, dryFftBuffer, dryDbSmoothed);
            computeSpectrum (wetTimeData, wetFftBuffer, wetDbSmoothed);

            repaint();
        }

        void computeSpectrum (std::array<float, kFftSize>& timeIn,
                              std::array<float, kFftSize * 2>& fftScratch,
                              std::array<float, kNumBins>& smoothedOut)
        {
            // Copy windowed time data into the scratch buffer. performFrequency-
            // OnlyForwardTransform needs a buffer of size 2*N: input in the
            // first N slots, output magnitudes written back into the first N.
            for (int i = 0; i < kFftSize; ++i)
                fftScratch[(size_t) i] = timeIn[(size_t) i];
            for (int i = kFftSize; i < kFftSize * 2; ++i)
                fftScratch[(size_t) i] = 0.0f;

            window.multiplyWithWindowingTable (fftScratch.data(), (size_t) kFftSize);
            fft.performFrequencyOnlyForwardTransform (fftScratch.data());

            // Normalize and convert to dB. A 2048-point FFT of a full-scale
            // sine puts ~N/2 at its peak bin, so we divide by N/2 to land on
            // 0 dBFS for full-scale input.
            constexpr float normalizer = 2.0f / (float) kFftSize;
            constexpr float alpha      = 0.3f;

            for (int bin = 0; bin < kNumBins; ++bin)
            {
                const float mag = fftScratch[(size_t) bin] * normalizer;
                const float db  = juce::Decibels::gainToDecibels (mag, kMinDb);

                // Exponential smoothing toward the new value
                smoothedOut[(size_t) bin] = alpha * db + (1.0f - alpha) * smoothedOut[(size_t) bin];
            }
        }

        //----------------------------------------------------------------------
        // Coordinate mapping
        //----------------------------------------------------------------------
        float freqToX (float hz, juce::Rectangle<float> plotArea) const noexcept
        {
            const float clamped = juce::jlimit (kMinHz, kMaxHz, hz);
            const float norm    = std::log (clamped / kMinHz) / std::log (kMaxHz / kMinHz);
            return plotArea.getX() + norm * plotArea.getWidth();
        }

        float dbToY (float db, juce::Rectangle<float> plotArea) const noexcept
        {
            const float clamped = juce::jlimit (kMinDb, kMaxDb, db);
            const float norm    = (clamped - kMinDb) / (kMaxDb - kMinDb);
            return plotArea.getBottom() - norm * plotArea.getHeight();
        }

        float binToFreq (int bin) const noexcept
        {
            const float sr = juce::jmax (1.0f,
                processor.getDiagnostics().sampleRate.load (std::memory_order_relaxed));
            return ((float) bin * sr) / (float) kFftSize;
        }

        //----------------------------------------------------------------------
        // Painting
        //----------------------------------------------------------------------
        void drawGrid (juce::Graphics& g, juce::Rectangle<float> plotArea) const
        {
            g.setColour (gridColour);

            // Vertical lines at decade boundaries: 20, 100, 1k, 10k, 20k
            static constexpr float decades[] = { 20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f };
            for (float f : decades)
            {
                const float x = freqToX (f, plotArea);
                g.drawLine (x, plotArea.getY(), x, plotArea.getBottom(), 0.5f);
            }

            // Horizontal lines every 20 dB
            for (float db = kMaxDb; db >= kMinDb; db -= 20.0f)
            {
                const float y = dbToY (db, plotArea);
                g.drawLine (plotArea.getX(), y, plotArea.getRight(), y, 0.5f);
            }

            // Frequency labels along the bottom
            g.setFont (juce::Font (9.0f));
            g.setColour (dimTextColour);
            drawFreqLabel (g, plotArea,   100.0f, "100");
            drawFreqLabel (g, plotArea,  1000.0f, "1k");
            drawFreqLabel (g, plotArea, 10000.0f, "10k");
        }

        void drawFreqLabel (juce::Graphics& g,
                            juce::Rectangle<float> plotArea,
                            float hz,
                            const juce::String& label) const
        {
            const float x = freqToX (hz, plotArea);
            g.drawText (label,
                        juce::Rectangle<float> (x - 15.0f, plotArea.getBottom() - 12.0f, 30.0f, 12.0f),
                        juce::Justification::centred);
        }

        void drawTrace (juce::Graphics& g,
                        juce::Rectangle<float> plotArea,
                        const std::array<float, kNumBins>& smoothed,
                        juce::Colour colour,
                        float lineWidth) const
        {
            juce::Path path;
            bool pathStarted = false;

            // Walk across the plot width in pixel steps, finding the nearest
            // bin at each X position. This gives a clean log-axis line without
            // aliasing from iterating bins directly.
            const int pixelWidth = (int) plotArea.getWidth();
            if (pixelWidth < 2)
                return;

            const float sr = juce::jmax (1.0f,
                processor.getDiagnostics().sampleRate.load (std::memory_order_relaxed));
            const float binHz = sr / (float) kFftSize;

            for (int px = 0; px < pixelWidth; ++px)
            {
                const float xNorm = (float) px / (float) (pixelWidth - 1);
                const float hz    = kMinHz * std::pow (kMaxHz / kMinHz, xNorm);
                const int   bin   = juce::jlimit (1, kNumBins - 1, (int) std::round (hz / binHz));
                const float db    = smoothed[(size_t) bin];

                const float x = plotArea.getX() + (float) px;
                const float y = dbToY (db, plotArea);

                if (! pathStarted)
                {
                    path.startNewSubPath (x, y);
                    pathStarted = true;
                }
                else
                {
                    path.lineTo (x, y);
                }
            }

            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (lineWidth));
        }

        void drawLegend (juce::Graphics& g, juce::Rectangle<float> plotArea) const
        {
            g.setFont (juce::Font (10.0f));

            auto legend = plotArea.removeFromTop (14).reduced (6.0f, 0.0f);

            g.setColour (dryTraceColour);
            g.drawText ("dry", legend.removeFromLeft (28), juce::Justification::left);

            g.setColour (wetTraceColour);
            g.drawText ("wet", legend.removeFromLeft (28), juce::Justification::left);
        }

        //----------------------------------------------------------------------
        // State
        //----------------------------------------------------------------------
        const AncientVoicesAudioProcessor& processor;

        juce::dsp::FFT                       fft;
        juce::dsp::WindowingFunction<float>  window;

        std::array<float, kFftSize>     dryTimeData {};
        std::array<float, kFftSize>     wetTimeData {};
        std::array<float, kFftSize * 2> dryFftBuffer {};
        std::array<float, kFftSize * 2> wetFftBuffer {};
        std::array<float, kNumBins>     dryDbSmoothed {};
        std::array<float, kNumBins>     wetDbSmoothed {};

        // Slate blue theme
        const juce::Colour surfaceColour    { juce::Colour::fromString ("FF2A3A56") };
        const juce::Colour borderColour     { juce::Colour::fromString ("FF4A5A7A") };
        const juce::Colour gridColour       { juce::Colour::fromString ("FF3A4A6B") };
        const juce::Colour textColour       { juce::Colours::white };
        const juce::Colour dimTextColour    { juce::Colour::fromString ("FFCFD8E6") };
        const juce::Colour dryTraceColour   { juce::Colour::fromString ("FFA0A8B8") };
        const juce::Colour wetTraceColour   { juce::Colours::white };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyser)
    };
}
