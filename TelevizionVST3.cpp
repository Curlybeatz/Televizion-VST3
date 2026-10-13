/*
  ==============================================================================
    پلاگین "تلویزیون" (Televizion) — Retro & Vintage Persian Lo-Fi Audio Plugin
    Format: VST3 / AU / Standalone (JUCE C++17 Framework)
    Target Genres: Hip-Hop, Trap, R&B, Lo-Fi Beats

    DSP Signal Chain:
      1. Stereo Tape/VHS Pitch Wow & Flutter (Dual LFO Modulated Fractional Delay)
      2. Asymmetric Vacuum Tube Saturation & Warmth (Hyperbolic Tangent + 2nd Harmonics)
      3. Vintage CRT Television Speaker Cabinet (Morphing High-Pass, Box Peak & Low-Pass)
      4. CRT Snow Static, 50Hz Mains Hum & Vinyl Dust Crackle Generator
      5. Golden-Era Persian Hall / Spring Reverb (Stereo Damped Reverb Tank)
      6. Dry/Wet Parallel Mix & Output Trim
  ==============================================================================
*/

#include <JuceHeader.h>
#include <cmath>
#include <vector>
#include <random>

//==============================================================================
// Persian Factory Preset Structure
//==============================================================================
struct TelevizionPreset
{
    juce::String name;
    juce::String channelTitle;
    float wowFlutter;
    float tubeDrive;
    float tvSpeaker;
    float staticNoise;
    float vintageReverb;
    float mix;
    float outputGainDb;
};

static const std::vector<TelevizionPreset> kFactoryPresets = {
    { juce::String::fromUTF8("دهه ۶۰ (Dahe 60)"),
      juce::String::fromUTF8("کانال ۰۱ : دهه ۶۰"),
      38.0f, 45.0f, 52.0f, 24.0f, 30.0f, 85.0f, 0.0f },

    { juce::String::fromUTF8("بانو هایده (Banoo Hayedeh)"),
      juce::String::fromUTF8("کانال ۰۲ : بانو هایده"),
      26.0f, 54.0f, 34.0f, 14.0f, 74.0f, 82.0f, 1.0f },

    { juce::String::fromUTF8("ترپ زیرزمینی تهران (Tehran Trap)"),
      juce::String::fromUTF8("کانال ۰۳ : ترپ زیرزمینی"),
      22.0f, 78.0f, 28.0f, 19.0f, 18.0f, 76.0f, -1.0f },

    { juce::String::fromUTF8("آر اند بی مخملی (Velvet R&B)"),
      juce::String::fromUTF8("کانال ۰۴ : آر اند بی مخملی"),
      58.0f, 32.0f, 44.0f, 16.0f, 62.0f, 80.0f, 0.5f },

    { juce::String::fromUTF8("برفک آخر شب (Late Night Static)"),
      juce::String::fromUTF8("کانال ۰۵ : برفک آخر شب"),
      84.0f, 62.0f, 76.0f, 72.0f, 55.0f, 94.0f, -1.5f },

    { juce::String::fromUTF8("کاست نوار قلبی (Heart Cassette)"),
      juce::String::fromUTF8("کانال ۰۶ : کاست نوار قلبی"),
      65.0f, 50.0f, 68.0f, 36.0f, 22.0f, 90.0f, 0.0f },

    { juce::String::fromUTF8("تلویزیون پارس الکتریک (Pars Electric CRT)"),
      juce::String::fromUTF8("کانال ۰۷ : پارس الکتریک"),
      18.0f, 60.0f, 92.0f, 30.0f, 15.0f, 100.0f, 1.5f },

    { juce::String::fromUTF8("لالایی مهتاب (Moonlight Lullaby)"),
      juce::String::fromUTF8("کانال ۰۸ : لالایی مهتاب"),
      72.0f, 28.0f, 48.0f, 28.0f, 88.0f, 88.0f, 0.5f }
};

//==============================================================================
// Audio Processor Class
//==============================================================================
class TelevizionAudioProcessor : public juce::AudioProcessor
{
public:
    TelevizionAudioProcessor()
        : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
    {
        loadPreset (0);
    }

    ~TelevizionAudioProcessor() override = default;

    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        params.push_back (std::make_unique<juce::AudioParameterFloat>(
            "wowFlutter", juce::String::fromUTF8("موج و لرزش (Wow & Flutter)"),
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 38.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat>(
            "tubeDrive", juce::String::fromUTF8("گرمای لامپ (Tube Drive)"),
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 45.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat>(
            "tvSpeaker", juce::String::fromUTF8("جعبه تلویزیون (TV Speaker)"),
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 52.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat>(
            "staticNoise", juce::String::fromUTF8("برفک و خش (Static & Crackle)"),
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 24.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat>(
            "vintageReverb", juce::String::fromUTF8("پژواک تالار (Retro Reverb)"),
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 30.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat>(
            "mix", juce::String::fromUTF8("میکس افکت (Dry/Wet)"),
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 85.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat>(
            "outputGain", juce::String::fromUTF8("ولوم خروجی (Output Gain)"),
            juce::NormalisableRange<float>(-18.0f, 6.0f, 0.1f), 0.0f));

        params.push_back (std::make_unique<juce::AudioParameterBool>(
            "powerOn", juce::String::fromUTF8("روشن/خاموش (TV Power)"), true));

        return { params.begin(), params.end() };
    }

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        currentSampleRate = sampleRate;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
        spec.numChannels = 2;

        hpFilter.prepare (spec);
        hpFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);

        lpFilter.prepare (spec);
        lpFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

        reverb.prepare (spec);

        // Allocate circular fractional delay buffer (max 100ms)
        delayBufferLen = static_cast<int> (sampleRate * 0.1);
        delayBuffer.setSize (2, delayBufferLen);
        delayBuffer.clear();
        writePos = 0;
        wowPhase = 0.0;
        flutterPhase = 0.0;
        humPhase = 0.0;
    }

    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    //==========================================================================
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        const bool isPowerOn = *apvts.getRawParameterValue ("powerOn") > 0.5f;
        if (!isPowerOn)
        {
            pushVisualSamples (buffer);
            return;
        }

        const float wowNorm   = *apvts.getRawParameterValue ("wowFlutter") / 100.0f;
        const float driveNorm = *apvts.getRawParameterValue ("tubeDrive") / 100.0f;
        const float spkNorm   = *apvts.getRawParameterValue ("tvSpeaker") / 100.0f;
        const float statNorm  = *apvts.getRawParameterValue ("staticNoise") / 100.0f;
        const float revNorm   = *apvts.getRawParameterValue ("vintageReverb") / 100.0f;
        const float mixNorm   = *apvts.getRawParameterValue ("mix") / 100.0f;
        const float outGain   = juce::Decibels::decibelsToGain (
                                    apvts.getRawParameterValue ("outputGain")->load());

        // Copy dry signal for parallel mixing
        juce::AudioBuffer<float> dryBuffer;
        dryBuffer.makeCopyOf (buffer);

        // Configure CRT Cabinet Filters
        const float hpCutoff = 35.0f * std::pow (420.0f / 35.0f, spkNorm);
        const float lpCutoff = 16000.0f * std::pow (2300.0f / 16000.0f, spkNorm);
        hpFilter.setCutoffFrequency (hpCutoff);
        lpFilter.setCutoffFrequency (lpCutoff);

        const float driveAmount = 1.0f + driveNorm * 12.0f;
        const float normFactor  = 1.0f / std::tanh (driveAmount);

        const double wowInc     = (0.85 + wowNorm * 1.4) / currentSampleRate;
        const double flutterInc = 6.8 / currentSampleRate;
        const double humInc     = 50.0 / currentSampleRate;

        // Per-sample Wow/Flutter + Tube Saturation + Static/Vinyl Crackle
        for (int i = 0; i < numSamples; ++i)
        {
            wowPhase     = std::fmod (wowPhase + wowInc, 1.0);
            flutterPhase = std::fmod (flutterPhase + flutterInc, 1.0);
            humPhase     = std::fmod (humPhase + humInc, 1.0);

            const float wowMod = std::sin (static_cast<float>(wowPhase * juce::MathConstants<double>::twoPi));
            const float fltMod = std::sin (static_cast<float>(flutterPhase * juce::MathConstants<double>::twoPi));

            // Generate subtle CRT static + vinyl crackle + 50Hz hum
            float white = (rng.nextFloat() * 2.0f - 1.0f) * 0.12f;
            if (rng.nextFloat() < 0.0012f)
                white += (rng.nextFloat() * 2.0f - 1.0f) * 0.9f; // Vinyl dust pop
            const float hum = 0.03f * std::sin (static_cast<float>(humPhase * juce::MathConstants<double>::twoPi));
            const float staticSample = (white + hum) * std::pow (statNorm, 1.5f) * 0.25f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* chData = buffer.getWritePointer (ch);
                float* dBuf   = delayBuffer.getWritePointer (ch % 2);

                const float inSample = chData[i];
                dBuf[writePos] = inSample;

                const float chSign = (ch == 0) ? 1.0f : -0.9f;
                const float modDelaySec = 0.015f
                                        + (wowMod * chSign * wowNorm * 0.0032f)
                                        + (fltMod * wowNorm * 0.0005f);
                const float delaySamples = juce::jlimit (1.0f, static_cast<float>(delayBufferLen - 4),
                                                         modDelaySec * static_cast<float>(currentSampleRate));

                // Linear fractional delay interpolation
                float readPtr = static_cast<float>(writePos) - delaySamples;
                if (readPtr < 0.0f) readPtr += static_cast<float>(delayBufferLen);
                const int idx0 = static_cast<int>(readPtr) % delayBufferLen;
                const int idx1 = (idx0 + 1) % delayBufferLen;
                const float frac = readPtr - std::floor (readPtr);
                const float delayed = dBuf[idx0] + frac * (dBuf[idx1] - dBuf[idx0]);

                // Asymmetric vacuum tube soft-clipping
                const float asym = delayed + 0.12f * delayed * delayed;
                const float saturated = std::tanh (driveAmount * asym) * normFactor;

                chData[i] = saturated + staticSample;
            }

            writePos = (writePos + 1) % delayBufferLen;
        }

        // Apply CRT Cabinet High-Pass & Low-Pass Filters
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        hpFilter.process (ctx);
        lpFilter.process (ctx);

        // Apply Vintage Hall / Spring Reverb
        juce::dsp::Reverb::Parameters revParams;
        revParams.roomSize   = 0.45f + revNorm * 0.45f;
        revParams.damping    = 0.55f;
        revParams.wetLevel   = revNorm * 0.55f;
        revParams.dryLevel   = 1.0f - revNorm * 0.25f;
        revParams.width      = 0.85f;
        reverb.setParameters (revParams);
        reverb.process (ctx);

        // Dry / Wet Mix & Master Output Gain
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* wetPtr = buffer.getWritePointer (ch);
            const float* dryPtr = dryBuffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const float mixed = dryPtr[i] * (1.0f - mixNorm) + wetPtr[i] * mixNorm;
                wetPtr[i] = mixed * outGain;
            }
        }

        pushVisualSamples (buffer);
    }

    //==========================================================================
    void loadPreset (int index)
    {
        if (index < 0 || index >= static_cast<int>(kFactoryPresets.size()))
            return;

        currentPresetIndex = index;
        const auto& p = kFactoryPresets[static_cast<size_t>(index)];

        apvts.getParameter ("wowFlutter")->setValueNotifyingHost (p.wowFlutter / 100.0f);
        apvts.getParameter ("tubeDrive")->setValueNotifyingHost (p.tubeDrive / 100.0f);
        apvts.getParameter ("tvSpeaker")->setValueNotifyingHost (p.tvSpeaker / 100.0f);
        apvts.getParameter ("staticNoise")->setValueNotifyingHost (p.staticNoise / 100.0f);
        apvts.getParameter ("vintageReverb")->setValueNotifyingHost (p.vintageReverb / 100.0f);
        apvts.getParameter ("mix")->setValueNotifyingHost (p.mix / 100.0f);

        auto* outParam = apvts.getParameter ("outputGain");
        outParam->setValueNotifyingHost (outParam->convertTo0to1 (p.outputGainDb));
    }

    int getNumPrograms() override { return static_cast<int>(kFactoryPresets.size()); }
    int getCurrentProgram() override { return currentPresetIndex; }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override
    {
        return kFactoryPresets[static_cast<size_t>(index)].name;
    }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
        if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    }

    void pushVisualSamples (const juce::AudioBuffer<float>& buf)
    {
        const int n = juce::jmin (buf.getNumSamples(), 256);
        const float* r = buf.getReadPointer (0);
        for (int i = 0; i < n; ++i)
            scopeFifo[static_cast<size_t>(i)] = r[i];
    }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    juce::AudioProcessorValueTreeState apvts;
    std::array<float, 256> scopeFifo { 0.0f };
    int currentPresetIndex = 0;

private:
    double currentSampleRate = 44100.0;
    juce::AudioBuffer<float> delayBuffer;
    int delayBufferLen = 4410;
    int writePos = 0;
    double wowPhase = 0.0;
    double flutterPhase = 0.0;
    double humPhase = 0.0;

    juce::dsp::StateVariableTPTFilter<float> hpFilter;
    juce::dsp::StateVariableTPTFilter<float> lpFilter;
    juce::dsp::Reverb reverb;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TelevizionAudioProcessor)
};

//==============================================================================
// Custom Vintage CRT Television LookAndFeel & Editor
//==============================================================================
class CRTTelevisionLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override
    {
        const float radius = static_cast<float>(juce::jmin (width / 2, height / 2)) - 6.0f;
        const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Outer Bakelite Skirt
        g.setColour (juce::Colour (0xff1c1815));
        g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (juce::Colour (0xff8c7120));
        g.drawEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 2.0f);

        // Inner Brass Cap
        const float innerR = radius * 0.74f;
        juce::ColourGradient grad (juce::Colour (0xffd6be83), centreX - innerR * 0.4f, centreY - innerR * 0.4f,
                                   juce::Colour (0xff5c4518), centreX + innerR, centreY + innerR, true);
        g.setGradientFill (grad);
        g.fillEllipse (centreX - innerR, centreY - innerR, innerR * 2.0f, innerR * 2.0f);

        // Pointer Notch
        juce::Path p;
        p.addRoundedRectangle (-2.0f, -innerR + 3.0f, 4.0f, innerR * 0.55f, 1.5f);
        g.setColour (juce::Colour (0xff17120b));
        g.fillPath (p, juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    }
};

class TelevizionAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit TelevizionAudioProcessorEditor (TelevizionAudioProcessor& p)
        : AudioProcessorEditor (&p), processor (p)
    {
        setLookAndFeel (&crtLnf);
        setSize (860, 520);

        // Preset Selector ComboBox
        for (int i = 0; i < static_cast<int>(kFactoryPresets.size()); ++i)
            presetBox.addItem (kFactoryPresets[static_cast<size_t>(i)].name, i + 1);

        presetBox.setSelectedId (processor.currentPresetIndex + 1, juce::dontSendNotification);
        presetBox.onChange = [this]()
        {
            processor.loadPreset (presetBox.getSelectedId() - 1);
            repaint();
        };
        addAndMakeVisible (presetBox);

        setupKnob (wowKnob,     "wowFlutter",    juce::String::fromUTF8("موج و لرزش"));
        setupKnob (driveKnob,   "tubeDrive",     juce::String::fromUTF8("گرمای لامپ"));
        setupKnob (speakerKnob, "tvSpeaker",     juce::String::fromUTF8("جعبه تلویزیون"));
        setupKnob (staticKnob,  "staticNoise",   juce::String::fromUTF8("برفک و خش"));
        setupKnob (reverbKnob,  "vintageReverb", juce::String::fromUTF8("پژواک تالار"));
        setupKnob (mixKnob,     "mix",           juce::String::fromUTF8("میکس افکت"));

        startTimerHz (30);
    }

    ~TelevizionAudioProcessorEditor() override
    {
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        // Outer Walnut Wood Cabinet
        g.fillAll (juce::Colour (0xff3a1d0d));
        g.setColour (juce::Colour (0xff5c3018));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (8.0f), 24.0f);

        // Inner Matte Bezel
        auto inner = getLocalBounds().reduced (22);
        g.setColour (juce::Colour (0xff1c1917));
        g.fillRoundedRectangle (inner.toFloat(), 18.0f);
        g.setColour (juce::Colour (0xffd4af37));
        g.drawRoundedRectangle (inner.toFloat(), 18.0f, 2.5f);

        // Left Side: Curved CRT Screen
        juce::Rectangle<float> crtRect (42.0f, 44.0f, 430.0f, 360.0f);
        g.setColour (juce::Colour (0xff070b08));
        g.fillRoundedRectangle (crtRect, 28.0f);
        g.setColour (juce::Colour (0xff33302c));
        g.drawRoundedRectangle (crtRect, 28.0f, 4.0f);

        // Channel OSD Header inside CRT
        g.setColour (juce::Colour (0xff4eff8b));
        g.setFont (juce::Font (18.0f, juce::Font::bold));
        const auto& preset = kFactoryPresets[static_cast<size_t>(processor.currentPresetIndex)];
        g.drawText (preset.channelTitle, crtRect.reduced (20.0f).removeFromTop (28.0f),
                    juce::Justification::centredRight);

        // Oscilloscope Waveform
        juce::Path wavePath;
        const float w = crtRect.getWidth() - 32.0f;
        const float midY = crtRect.getCentreY();
        for (size_t i = 0; i < processor.scopeFifo.size(); ++i)
        {
            const float x = crtRect.getX() + 16.0f + (static_cast<float>(i) / 255.0f) * w;
            const float y = midY + processor.scopeFifo[i] * 110.0f;
            if (i == 0) wavePath.startNewSubPath (x, y);
            else        wavePath.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xff4eff8b));
        g.strokePath (wavePath, juce::PathStrokeType (2.4f));

        // CRT Snow Static Particles
        const float staticAmt = *processor.apvts.getRawParameterValue ("staticNoise") / 100.0f;
        const int numFlakes = static_cast<int>(staticAmt * 380.0f);
        g.setColour (juce::Colours::white.withAlpha (0.65f));
        for (int i = 0; i < numFlakes; ++i)
        {
            const float sx = crtRect.getX() + 14.0f + rng.nextFloat() * (crtRect.getWidth() - 28.0f);
            const float sy = crtRect.getY() + 14.0f + rng.nextFloat() * (crtRect.getHeight() - 28.0f);
            g.fillRect (sx, sy, 3.0f, 1.5f);
        }

        // Brand Plate below CRT
        g.setColour (juce::Colour (0xffd4af37));
        g.setFont (juce::Font (22.0f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8("تلویزیون — پارس آکوستیک مدل ۱۳۶۰"),
                    42, 420, 430, 40, juce::Justification::centred);
    }

    void resized() override
    {
        presetBox.setBounds (495, 52, 325, 36);

        const int startX = 495;
        const int startY = 115;
        const int colW   = 110;
        const int rowH   = 150;

        layoutKnob (wowKnob,     startX,            startY);
        layoutKnob (driveKnob,   startX + colW,     startY);
        layoutKnob (speakerKnob, startX + colW * 2, startY);

        layoutKnob (staticKnob,  startX,            startY + rowH);
        layoutKnob (reverbKnob,  startX + colW,     startY + rowH);
        layoutKnob (mixKnob,     startX + colW * 2, startY + rowH);
    }

private:
    struct AttachedKnob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (AttachedKnob& k, const juce::String& paramID, const juce::String& title)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        addAndMakeVisible (k.slider);

        k.label.setText (title, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, paramID, k.slider);
    }

    void layoutKnob (AttachedKnob& k, int x, int y)
    {
        k.label.setBounds (x, y, 100, 22);
        k.slider.setBounds (x, y + 22, 100, 110);
    }

    void timerCallback() override { repaint(); }

    TelevizionAudioProcessor& processor;
    CRTTelevisionLookAndFeel crtLnf;
    juce::ComboBox presetBox;
    AttachedKnob wowKnob, driveKnob, speakerKnob, staticKnob, reverbKnob, mixKnob;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TelevizionAudioProcessorEditor)
};

juce::AudioProcessorEditor* TelevizionAudioProcessor::createEditor()
{
    return new TelevizionAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TelevizionAudioProcessor();
}
