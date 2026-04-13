// PluginEditor.cpp — Ancient Voices

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Data/Presets.h"
#include <juce_cryptography/juce_cryptography.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <vector>
#include <string>

namespace AVTheme
{
    static const juce::Colour bgTop        = juce::Colour::fromRGB(245, 250, 247);
    static const juce::Colour bgBottom     = juce::Colour::fromRGB(220, 238, 228);
    static const juce::Colour accentGreen  = juce::Colour::fromRGB( 20, 160,  90);
    static const juce::Colour accentDark   = juce::Colour::fromRGB( 10, 100,  55);
    static const juce::Colour accentLight  = juce::Colour::fromRGB( 80, 210, 140);
    static const juce::Colour textDark     = juce::Colour::fromRGB( 25,  45,  35);
    static const juce::Colour panelWhite   = juce::Colours::white;
    static const juce::Colour panelBorder  = juce::Colour::fromRGB(200, 220, 210);
}

static const char* publicKeyPEM = R"(
-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyzN/UB8uXAusctvP6bi/
ZXjOQgwrLyOqQRS9hnwBRo+8OAxzKOrsWZPaV30ornhoai/Y91V6Io2utbJT3ifw
fesSgrXhjQxbjy/z0kwLmy763IUlFQCpA2LxAM0ygNW3pOdEgpyH9y7qRV0XiwvL
ERXZKT+Z6ZZXgt0AddVIWNytrxjq1VJ4yf2BDDLgr6hFY5sXGxNJYLHk2FCD7ouv
d7gYDqzaNkmrIpFrSYgmC0eYSIROnsqHRgOrecD81bsLvoVMH4MTfc3lXidOGk2M
wWECqb74JhlHQTnxW4UGe1TNpnf+scMjQspOIWoO0qkh8L2gqER+ozPzNb9Sv7/q
nwIDAQAB
-----END PUBLIC KEY-----
)";
static const std::string appKey = "fa05f2e9f19c930dd3ae310c33adf5b334dbdea56f8cefb5a10101d61e7469c9";
static const juce::File licenseFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
    .getChildFile("AncientVoices").getChildFile("license.dat");
static void saveLicenseStatus(bool v) { licenseFile.create(); licenseFile.replaceWithText(v ? "valid" : "invalid"); }
static RSA* loadPublicKey() { BIO* b = BIO_new_mem_buf(publicKeyPEM, -1); RSA* k = PEM_read_bio_RSA_PUBKEY(b, NULL, NULL, NULL); BIO_free(b); return k; }
static std::vector<unsigned char> decodeBase64Impl(const std::string& s) {
    BIO* bio = BIO_new_mem_buf(s.data(), -1);
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    RSA* pk = loadPublicKey();
    std::vector<unsigned char> out(pk ? RSA_size(pk) : 256);
    int n = BIO_read(bio, out.data(), (int)out.size());
    if (n > 0) out.resize(n); else out.clear();
    BIO_free_all(bio);
    if (pk) RSA_free(pk);
    return out;
}
static bool verifySignature(const std::string& data, const std::string& sig) {
    RSA* pk = loadPublicKey();
    if (!pk) return false;
    auto bin = decodeBase64Impl(sig);
    if (bin.empty()) { RSA_free(pk); return false; }
    EVP_PKEY* pkey = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(pkey, pk);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX* pctx = NULL;
    bool ok = false;
    if (EVP_DigestVerifyInit(ctx, &pctx, EVP_sha256(), NULL, pkey) == 1 &&
        EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) == 1 &&
        EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, -1) == 1 &&
        EVP_DigestVerifyUpdate(ctx, data.c_str(), data.size()) == 1)
        ok = (EVP_DigestVerifyFinal(ctx, bin.data(), bin.size()) == 1);
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ok;
}

void AncientVoicesAudioProcessorEditor::configureKnob(juce::Slider& s, juce::Label& lbl, const juce::String& name)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 16);
    s.setColour(juce::Slider::rotarySliderFillColourId,    AVTheme::accentGreen);
    s.setColour(juce::Slider::rotarySliderOutlineColourId, AVTheme::panelBorder);
    s.setColour(juce::Slider::thumbColourId,               AVTheme::accentDark);
    s.setColour(juce::Slider::textBoxTextColourId,         AVTheme::textDark);
    s.setColour(juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    addAndMakeVisible(s);
    lbl.setText(name, juce::dontSendNotification);
    lbl.setFont(juce::Font("Helvetica Neue", 11.0f, juce::Font::bold));
    lbl.setColour(juce::Label::textColourId, AVTheme::accentDark);
    lbl.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lbl);
}

AncientVoicesAudioProcessorEditor::AncientVoicesAudioProcessorEditor(AncientVoicesAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), tooltipWindow(this)
{
    if (!licenseFile.existsAsFile()) {
        getSerialKeyFromUser([this](juce::String u, juce::String k) {
            if (!u.isEmpty() && !k.isEmpty()) {
                if (!validateLicenseKey(u, k))
                    getSerialKeyFromUser([this](juce::String u2, juce::String k2){ validateLicenseKey(u2, k2); }, true);
            }
        }, false);
    }

    titleLabel.setText("ANCIENT VOICES", juce::dontSendNotification);
    titleLabel.setFont(juce::Font("Helvetica Neue", 30.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, AVTheme::accentDark);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Vocal Transformer  /  Sumerian  /  Egyptian  /  Mesopotamian",
                          juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font("Helvetica Neue", 12.0f, juce::Font::plain));
    subtitleLabel.setColour(juce::Label::textColourId, AVTheme::textDark.withAlpha(0.7f));
    subtitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(subtitleLabel);

    presetLabel.setText("PRESET", juce::dontSendNotification);
    presetLabel.setFont(juce::Font("Helvetica Neue", 11.0f, juce::Font::bold));
    presetLabel.setColour(juce::Label::textColourId, AVTheme::accentDark);
    presetLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetLabel);

    presetCombo.setColour(juce::ComboBox::backgroundColourId, AVTheme::panelWhite);
    presetCombo.setColour(juce::ComboBox::textColourId,       AVTheme::textDark);
    presetCombo.setColour(juce::ComboBox::outlineColourId,    AVTheme::accentGreen);
    presetCombo.setColour(juce::ComboBox::arrowColourId,      AVTheme::accentGreen);
    presetCombo.setColour(juce::ComboBox::buttonColourId,     AVTheme::accentLight);
    presetCombo.setJustificationType(juce::Justification::centredLeft);
    populatePresetCombo();
    presetCombo.onChange = [this] { onPresetComboChanged(); };
    addAndMakeVisible(presetCombo);

    profileDisplayLabel.setFont(juce::Font("Helvetica Neue", 26.0f, juce::Font::bold));
    profileDisplayLabel.setColour(juce::Label::textColourId, AVTheme::accentGreen);
    profileDisplayLabel.setJustificationType(juce::Justification::centred);
    profileDisplayLabel.setText("Alpha", juce::dontSendNotification);
    addAndMakeVisible(profileDisplayLabel);

    configureKnob(reverbSlider, reverbLabel, "REVERB");
    configureKnob(delaySlider,  delayLabel,  "DELAY");
    configureKnob(driveSlider,  driveLabel,  "DRIVE");
    configureKnob(outputSlider, outputLabel, "OUTPUT");
    configureKnob(mixSlider,    mixLabel,    "MIX");

    auto& state = audioProcessor.getAPVTS();
    reverbAtt = std::make_unique<SA>(state, "USER_REVERB", reverbSlider);
    delayAtt  = std::make_unique<SA>(state, "USER_DELAY",  delaySlider);
    driveAtt  = std::make_unique<SA>(state, "USER_DRIVE",  driveSlider);
    outputAtt = std::make_unique<SA>(state, "USER_OUTPUT", outputSlider);
    mixAtt    = std::make_unique<SA>(state, "MIX",         mixSlider);

    syncComboFromParameter();

    waveformDisplay = std::make_unique<CustomAudioVisualiserComponent>(2);
    waveformDisplay->setBufferSize(2048);
    waveformDisplay->setSamplesPerBlock(512);
    waveformDisplay->setColours(AVTheme::panelWhite, AVTheme::accentGreen);
    addAndMakeVisible(*waveformDisplay);

    footerLabel.setText("(c)(p) 2026 Tully EDM Vibe", juce::dontSendNotification);
    footerLabel.setFont(juce::Font("Helvetica Neue", 10.0f, juce::Font::plain));
    footerLabel.setColour(juce::Label::textColourId, AVTheme::textDark.withAlpha(0.5f));
    footerLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(footerLabel);

    startTimerHz(20);
    setSize(900, 680);
}

AncientVoicesAudioProcessorEditor::~AncientVoicesAudioProcessorEditor() { stopTimer(); }

void AncientVoicesAudioProcessorEditor::populatePresetCombo()
{
    using namespace AncientVoicesPresets;
    presetCombo.clear(juce::dontSendNotification);
    presetCombo.addSectionHeading("MisterMultiVox Originals");
    for (int i = 0; i < kMmvProfileCount; ++i)
        presetCombo.addItem(getMmvProfileName(i), i + 1);
    Category lastCategory = Category::Chant;
    bool firstInGroup = true;
    for (int i = 0; i < kAvPresetCount; ++i) {
        const auto& pr = getPreset(i);
        if (firstInGroup || pr.category != lastCategory) {
            presetCombo.addSectionHeading(getCategoryName(pr.category));
            lastCategory = pr.category;
            firstInGroup = false;
        }
        presetCombo.addItem(pr.name, pr.globalIndex + 1);
    }
}

void AncientVoicesAudioProcessorEditor::onPresetComboChanged()
{
    const int id = presetCombo.getSelectedId();
    if (id <= 0) return;
    const int g = id - 1;
    if (auto* param = audioProcessor.getAPVTS().getParameter("PROFILE_INDEX"))
        param->setValueNotifyingHost(param->convertTo0to1((float) g));
    juce::String name = (g < AncientVoicesPresets::kMmvProfileCount)
        ? AncientVoicesPresets::getMmvProfileName(g)
        : AncientVoicesPresets::getPreset(g - AncientVoicesPresets::kMmvProfileCount).name;
    profileDisplayLabel.setText(name, juce::dontSendNotification);
}

void AncientVoicesAudioProcessorEditor::syncComboFromParameter()
{
    if (auto* v = audioProcessor.getAPVTS().getRawParameterValue("PROFILE_INDEX")) {
        const int g = (int) v->load();
        presetCombo.setSelectedId(g + 1, juce::dontSendNotification);
        juce::String name;
        if (g < AncientVoicesPresets::kMmvProfileCount)
            name = AncientVoicesPresets::getMmvProfileName(g);
        else if (g < AncientVoicesPresets::kTotalProfiles)
            name = AncientVoicesPresets::getPreset(g - AncientVoicesPresets::kMmvProfileCount).name;
        else
            name = "Alpha";
        profileDisplayLabel.setText(name, juce::dontSendNotification);
    }
}

bool AncientVoicesAudioProcessorEditor::isLicenseValid()
{
    juce::File lf = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("AncientVoices").getChildFile("license.dat");
    return lf.existsAsFile() && lf.loadFileAsString().trim() == "valid";
}
bool AncientVoicesAudioProcessorEditor::validateLicenseKey(const juce::String& u, const juce::String& k)
{
    if (isLicenseValid()) return true;
    bool ok = verifySignature(appKey + u.toStdString(), k.trim().toStdString());
    if (ok) { saveLicenseStatus(true); audioProcessor.setLicenseStatus(true); refreshUIAfterLicenseValidation(); }
    else    { saveLicenseStatus(false); }
    return ok;
}
void AncientVoicesAudioProcessorEditor::getSerialKeyFromUser(std::function<void(juce::String, juce::String)> cb, bool isRetry)
{
    auto aw = std::make_unique<juce::AlertWindow>("Ancient Voices License",
        isRetry ? "Invalid. Please re-enter:" : "Please enter User ID and License Key:",
        juce::AlertWindow::NoIcon);
    aw->addTextEditor("userIDEditor", "", "User ID:");
    aw->addTextEditor("licenseKeyEditor", "", "License Key:");
    aw->addButton("OK", 1); aw->addButton("Cancel", 0);
    auto* raw = aw.get();
    raw->setBounds(getScreenX() + 20, getScreenY() + getHeight() / 2 - 125, 400, 250);
    raw->enterModalState(true, juce::ModalCallbackFunction::create(
        [aw = std::move(aw), cb](int r) {
            if (r == 1) cb(aw->getTextEditorContents("userIDEditor"), aw->getTextEditorContents("licenseKeyEditor"));
            else cb("", "");
        }));
}
void AncientVoicesAudioProcessorEditor::refreshUIAfterLicenseValidation() { if (isLicenseValid()) repaint(); }
bool AncientVoicesAudioProcessorEditor::hasValidSerialStored()
{ return audioProcessor.getApplicationProperties().getUserSettings()->getBoolValue("isValidSerial", false); }
void AncientVoicesAudioProcessorEditor::storeValidSerial()
{ audioProcessor.getApplicationProperties().getUserSettings()->setValue("isValidSerial", true);
  audioProcessor.getApplicationProperties().getUserSettings()->saveIfNeeded(); }
std::vector<unsigned char> AncientVoicesAudioProcessorEditor::decodeBase64(const juce::String& s)
{ return decodeBase64Impl(s.toStdString()); }

void AncientVoicesAudioProcessorEditor::updateWaveforms()
{
    auto inData  = audioProcessor.getLastInputSamples();
    auto outData = audioProcessor.getLastOutputSamples();
    if (inData.empty() || outData.empty() || inData.size() != outData.size()) return;
    float mi = 0, mo = 0;
    for (auto s : inData)  mi = std::max(mi, std::abs(s));
    for (auto s : outData) mo = std::max(mo, std::abs(s));
    if (mi < 0.0001f) mi = 1.0f;
    if (mo < 0.0001f) mo = 1.0f;
    for (auto& s : inData)  s /= mi;
    for (auto& s : outData) s /= mo;
    juce::AudioBuffer<float> buf(2, (int)inData.size());
    buf.copyFrom(0, 0, inData.data(),  (int)inData.size());
    buf.copyFrom(1, 0, outData.data(), (int)outData.size());
    if (waveformDisplay) waveformDisplay->pushBuffer(buf);
}

void AncientVoicesAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(24);

    titleLabel.setBounds(area.removeFromTop(40));
    subtitleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(16);

    auto presetRow = area.removeFromTop(40);
    presetLabel.setBounds(presetRow.removeFromLeft(70));
    presetCombo.setBounds(presetRow.reduced(4, 2));
    area.removeFromTop(8);

    profileDisplayLabel.setBounds(area.removeFromTop(42));
    area.removeFromTop(8);

    // 5 KNOBS across
    auto knobRow = area.removeFromTop(120);
    const int knobW = knobRow.getWidth() / 5;
    auto placeKnob = [&](juce::Slider& s, juce::Label& lbl) {
        auto cell = knobRow.removeFromLeft(knobW).reduced(6, 0);
        lbl.setBounds(cell.removeFromTop(18));
        s.setBounds(cell);
    };
    placeKnob(reverbSlider, reverbLabel);
    placeKnob(delaySlider,  delayLabel);
    placeKnob(driveSlider,  driveLabel);
    placeKnob(outputSlider, outputLabel);
    placeKnob(mixSlider,    mixLabel);

    area.removeFromTop(8);

    footerLabel.setBounds(area.removeFromBottom(18));
    area.removeFromBottom(6);

    if (waveformDisplay != nullptr)
        waveformDisplay->setBounds(area);
}

void AncientVoicesAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(AVTheme::bgTop, 0.0f, 0.0f,
                            AVTheme::bgBottom, 0.0f, (float)getHeight(), false);
    g.setGradientFill(bg);
    g.fillAll();

    g.setColour(AVTheme::accentGreen);
    g.fillRect(0, 0, getWidth(), 5);
    g.setColour(AVTheme::accentGreen.withAlpha(0.5f));
    g.fillRect(0, getHeight() - 2, getWidth(), 2);

    if (waveformDisplay != nullptr) {
        auto wb = waveformDisplay->getBounds().expanded(5);
        g.setColour(AVTheme::panelWhite);
        g.fillRoundedRectangle(wb.toFloat(), 6.0f);
        g.setColour(AVTheme::panelBorder);
        g.drawRoundedRectangle(wb.toFloat(), 6.0f, 1.0f);
    }

    auto headerCard = juce::Rectangle<int>(24, 24, getWidth() - 48, 70);
    g.setColour(AVTheme::panelWhite.withAlpha(0.5f));
    g.fillRoundedRectangle(headerCard.toFloat(), 8.0f);
    g.setColour(AVTheme::panelBorder);
    g.drawRoundedRectangle(headerCard.toFloat(), 8.0f, 1.0f);
}

void AncientVoicesAudioProcessorEditor::timerCallback()
{
    if (waveformDisplay != nullptr) {
        updateWaveforms();
        if (auto* v = audioProcessor.getAPVTS().getRawParameterValue("PROFILE_INDEX")) {
            const int cur = (int) v->load();
            if (presetCombo.getSelectedId() - 1 != cur)
                syncComboFromParameter();
        }
    }
}
