// AVVideo: renders a demo video of Ancient Voices offline: the real plugin editor
// (snapshotted at 30 fps) + the real processor output, voice changes on a script.
#include <JuceHeader.h>
#include <vector>
#include <map>
#include <mutex>
#include <cstdio>
#include "../Source/DSPModules.h"
#define private public
#define protected public
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#undef private
#undef protected

struct Step { double t; int profile; };

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double SR = 48000.0; const int FPS = 30; const int spf = (int) SR / FPS; // 1600
    const juce::File in (argv[1]), outDir (argv[2]);
    const float scale = argc > 3 ? (float) atof (argv[3]) : 2.0f;
    outDir.createDirectory();

    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (in));
    const int L = (int) r->lengthInSamples;
    juce::AudioBuffer<float> voice (2, L); r->read (&voice, 0, L, 0, true, true);

    const std::vector<Step> steps = { {0.0,10}, {5.8,24}, {11.6,13}, {17.4,26}, {23.2,17}, {29.0,8}, {34.8,53} };
    const double total = 40.6;
    const int nFrames = (int) (total * FPS);

    AncientVoicesAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, SR, 512);
    proc.prepareToPlay (SR, 512);
    auto& apvts = proc.getAPVTS();
    auto setP = [&] (const char* id, float v) { if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    setP ("PROFILE_INDEX", 10); setP ("MIX", 0.0f);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* av = static_cast<AncientVoicesAudioProcessorEditor*> (ed.get());
    ed->setVisible (true);

    const int lat = proc.getLatencySamples();
    const int totalSamples = nFrames * spf + lat;
    juce::AudioBuffer<float> out (2, totalSamples); out.clear();
    juce::AudioBuffer<float> blk (2, 400); juce::MidiBuffer midi;
    int pos = 0, frame = 0;
    // run the audio, snapshot the editor once per video frame; frames are taken at
    // audio time t, and audio is later shifted by the latency so picture and sound line up
    while (pos < totalSamples)
    {
        const double t = (double) pos / SR;
        int prof = steps.front().profile;
        for (auto& s : steps) if (t >= s.t) prof = s.profile;
        if ((int) apvts.getRawParameterValue ("PROFILE_INDEX")->load() != prof) setP ("PROFILE_INDEX", (float) prof);
        // knob moves: MIX 0 -> 1 over 0.6..2.2 s, REVERB 0.5 -> 0.9 -> 0.5 in the Ziggurat section
        setP ("MIX", (float) juce::jlimit (0.0, 1.0, (t - 0.6) / 1.6));
        float rev = 0.5f;
        if (t > 24.0 && t < 28.6) rev = (float) (0.5 + 0.4 * std::sin (juce::MathConstants<double>::pi * (t - 24.0) / 4.6));
        setP ("USER_REVERB", rev);

        for (int k = 0; k < spf; k += 400)
        {
            blk.clear();
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 400; ++i)
                {
                    const int s = pos + i;
                    blk.setSample (ch, i, s < nFrames * spf ? voice.getSample (ch, s % L) : 0.0f);
                }
            proc.processBlock (blk, midi);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, blk, ch, 0, juce::jmin (400, totalSamples - pos));
            pos += 400;
            if (pos >= totalSamples) break;
        }
        // the editor's 20 Hz timer, driven by hand (scope + preset display)
        av->timerCallback();
        // frames lag the audio by the plugin latency so that what you hear matches what you see
        const int frameAudioPos = pos - lat;
        if (frameAudioPos >= 0 && frame < nFrames)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
            juce::File f = outDir.getChildFile (juce::String::formatted ("f%05d.png", frame));
            f.deleteFile();
            juce::FileOutputStream fos (f);
            juce::PNGImageFormat().writeImageToStream (img, fos);
            ++frame;
        }
    }
    // write audio (latency removed)
    juce::AudioBuffer<float> aligned (2, nFrames * spf);
    for (int ch = 0; ch < 2; ++ch) aligned.copyFrom (ch, 0, out, ch, lat, nFrames * spf);
    juce::WavAudioFormat wav;
    auto wf = outDir.getChildFile ("audio.wav"); wf.deleteFile();
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (wf), SR, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (aligned, 0, aligned.getNumSamples());
    printf ("frames %d latency %d peak %.3f\n", frame, lat, aligned.getMagnitude (0, aligned.getNumSamples()));
    ed = nullptr;
    return 0;
}
