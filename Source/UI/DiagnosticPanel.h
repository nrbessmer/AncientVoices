#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

//==============================================================================
// DiagnosticPanel.h
//
// A collapsible read-only panel showing the processor's runtime diagnostic
// state. All reads are lock-free atomic loads on UI thread timer ticks at
// 10 Hz. Nothing here writes to the processor.
//
// Layout: single column of key/value rows plus a row of five contract
// pass/fail indicators at the bottom. Hidden by default; the parent editor
// toggles visibility via its "D" header button.
//
// Theme matches the spec: slate blue surface, white text, muted label text.
//==============================================================================

namespace AncientVoicesUI
{
    class DiagnosticPanel : public juce::Component,
                            private juce::Timer
    {
    public:
        explicit DiagnosticPanel (const AncientVoicesAudioProcessor& p)
            : processor (p)
        {
            startTimerHz (10);
        }

        ~DiagnosticPanel() override
        {
            stopTimer();
        }

        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat();

            // Panel background
            g.setColour (surfaceColour);
            g.fillRoundedRectangle (bounds, 6.0f);

            g.setColour (borderColour);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

            // Header
            auto area = getLocalBounds().reduced (12, 10);
            g.setColour (textColour);
            g.setFont (juce::Font (14.0f, juce::Font::bold));
            g.drawText ("DIAGNOSTICS", area.removeFromTop (20), juce::Justification::left);
            area.removeFromTop (6);

            // Key/value rows
            g.setFont (juce::Font (12.0f));
            drawRow (g, area, "Sample rate",     juce::String (snapshot.sampleRateHz, 0) + " Hz");
            drawRow (g, area, "Block size",      juce::String (snapshot.blockSize)      + " samples");
            drawRow (g, area, "Voices",          juce::String (snapshot.voiceCount));
            drawRow (g, area, "Mix",             juce::String (snapshot.mix, 3));
            drawRow (g, area, "Dry RMS",         rmsToString (snapshot.dryRms));
            drawRow (g, area, "Wet RMS",         rmsToString (snapshot.wetRms));
            drawRow (g, area, "Wet-Dry diff",    rmsToString (snapshot.wetDryDiff));
            drawRow (g, area, "Output RMS",      rmsToString (snapshot.outputRms));
            drawRow (g, area, "Limiter GR",      juce::String (snapshot.limiterGrDb, 2) + " dB");
            drawRow (g, area, "normGain min",    juce::String (snapshot.normGainMin, 3));
            drawRow (g, area, "normGain max",    juce::String (snapshot.normGainMax, 3));
            drawRow (g, area, "NaN/Inf",         snapshot.nanInf ? "DETECTED" : "clean",
                     snapshot.nanInf ? failColour : okColour);

            // Contract flags row
            area.removeFromTop (6);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.setColour (dimTextColour);
            g.drawText ("CONTRACTS", area.removeFromTop (16), juce::Justification::left);
            g.setFont (juce::Font (10.0f));
            drawContractFlag (g, area, "Stage sep",    snapshot.stageSepOk);
            drawContractFlag (g, area, "Zero-dry",     snapshot.zeroDryOk);
            drawContractFlag (g, area, "Choir async",  snapshot.choirAsyncOk);
            drawContractFlag (g, area, "Preset mix=1", snapshot.presetMixOk);
            drawContractFlag (g, area, "No silence",   snapshot.noSilenceOk);
        }

        void resized() override
        {
            // This panel paints everything directly; no child components
            // to lay out.
        }

    private:
        //----------------------------------------------------------------------
        // Snapshot struct - all the atomics loaded once per timer tick so
        // paint() sees a coherent frame.
        //----------------------------------------------------------------------
        struct Snapshot
        {
            float sampleRateHz = 0.0f;
            int   blockSize    = 0;
            int   voiceCount   = 0;
            float mix          = 1.0f;
            float dryRms       = 0.0f;
            float wetRms       = 0.0f;
            float wetDryDiff   = 0.0f;
            float outputRms    = 0.0f;
            float limiterGrDb  = 0.0f;
            float normGainMin  = 0.0f;
            float normGainMax  = 0.0f;
            bool  nanInf       = false;
            bool  stageSepOk   = true;
            bool  zeroDryOk    = true;
            bool  choirAsyncOk = true;
            bool  presetMixOk  = true;
            bool  noSilenceOk  = true;
        };

        void timerCallback() override
        {
            const auto& d = processor.getDiagnostics();

            snapshot.sampleRateHz = d.sampleRate        .load (std::memory_order_relaxed);
            snapshot.blockSize    = d.blockSize         .load (std::memory_order_relaxed);
            snapshot.voiceCount   = d.voiceCount        .load (std::memory_order_relaxed);
            snapshot.mix          = d.mixParamValue     .load (std::memory_order_relaxed);
            snapshot.dryRms       = d.dryRms            .load (std::memory_order_relaxed);
            snapshot.wetRms       = d.wetRms            .load (std::memory_order_relaxed);
            snapshot.wetDryDiff   = d.wetDryDifference  .load (std::memory_order_relaxed);
            snapshot.outputRms    = d.outputRms         .load (std::memory_order_relaxed);
            snapshot.limiterGrDb  = d.limiterGainReductionDb.load (std::memory_order_relaxed);
            snapshot.normGainMin  = d.normGainMin       .load (std::memory_order_relaxed);
            snapshot.normGainMax  = d.normGainMax       .load (std::memory_order_relaxed);
            snapshot.nanInf       = d.nanInfDetected    .load (std::memory_order_relaxed);
            snapshot.stageSepOk   = d.stageSeparationOk .load (std::memory_order_relaxed);
            snapshot.zeroDryOk    = d.formantZeroDryOk  .load (std::memory_order_relaxed);
            snapshot.choirAsyncOk = d.choirAsyncOk      .load (std::memory_order_relaxed);
            snapshot.presetMixOk  = d.presetMixIsOneOk  .load (std::memory_order_relaxed);
            snapshot.noSilenceOk  = d.noSilenceOk       .load (std::memory_order_relaxed);

            repaint();
        }

        //----------------------------------------------------------------------
        // Painting helpers
        //----------------------------------------------------------------------
        void drawRow (juce::Graphics& g,
                      juce::Rectangle<int>& area,
                      const juce::String& label,
                      const juce::String& value) const
        {
            drawRow (g, area, label, value, textColour);
        }

        void drawRow (juce::Graphics& g,
                      juce::Rectangle<int>& area,
                      const juce::String& label,
                      const juce::String& value,
                      juce::Colour valueColour) const
        {
            auto row = area.removeFromTop (16);
            const int split = row.getWidth() / 2;

            g.setColour (dimTextColour);
            g.drawText (label, row.removeFromLeft (split), juce::Justification::left);

            g.setColour (valueColour);
            g.drawText (value, row, juce::Justification::right);
        }

        void drawContractFlag (juce::Graphics& g,
                               juce::Rectangle<int>& area,
                               const juce::String& label,
                               bool ok) const
        {
            auto row = area.removeFromTop (14);

            // Indicator dot on the left
            const int dotSize = 8;
            auto dotArea = row.removeFromLeft (dotSize + 6).withSizeKeepingCentre (dotSize, dotSize);
            g.setColour (ok ? okColour : failColour);
            g.fillEllipse (dotArea.toFloat());

            g.setColour (dimTextColour);
            g.drawText (label, row, juce::Justification::left);

            g.setColour (ok ? okColour : failColour);
            g.drawText (ok ? "OK" : "FAIL", row, juce::Justification::right);
        }

        static juce::String rmsToString (float rms)
        {
            if (rms < 1.0e-6f)
                return "-inf dB";
            const float db = juce::Decibels::gainToDecibels (rms, -90.0f);
            return juce::String (db, 1) + " dB";
        }

        //----------------------------------------------------------------------
        // State
        //----------------------------------------------------------------------
        const AncientVoicesAudioProcessor& processor;
        Snapshot snapshot;

        // Slate blue theme - matches the spec colour values
        const juce::Colour surfaceColour { juce::Colour::fromString ("FF2A3A56") };
        const juce::Colour borderColour  { juce::Colour::fromString ("FF4A5A7A") };
        const juce::Colour textColour    { juce::Colours::white };
        const juce::Colour dimTextColour { juce::Colour::fromString ("FFCFD8E6") };
        const juce::Colour okColour      { juce::Colour::fromString ("FF7FFF9F") };
        const juce::Colour failColour    { juce::Colour::fromString ("FFFF7F7F") };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiagnosticPanel)
    };
}
