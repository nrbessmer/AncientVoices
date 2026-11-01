// PluginEditor.cpp — Ancient Voices
// (c)(p) 2026 Tully EDM Vibe

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Data/Presets.h"
#include "TullyLogoData.h"
#include <juce_cryptography/juce_cryptography.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <vector>
#include <string>

using namespace avui;

//============================================================================== LookAndFeel
LnF::LnF()
{
    setColour (juce::ComboBox::backgroundColourId, Col::panelHi);
    setColour (juce::ComboBox::outlineColourId, Col::line);
    setColour (juce::ComboBox::textColourId, Col::text);
    setColour (juce::ComboBox::arrowColourId, Col::dim);
    setColour (juce::PopupMenu::backgroundColourId, Col::panelHi);
    setColour (juce::PopupMenu::textColourId, Col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Col::line);
    setColour (juce::PopupMenu::highlightedTextColourId, Col::cyan);
    setColour (juce::PopupMenu::headerTextColourId, Col::pink);
    setColour (juce::Label::textColourId, Col::text);
    setColour (juce::Slider::textBoxTextColourId, Col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId, Col::panelHi);
    setColour (juce::TooltipWindow::textColourId, Col::text);
    setColour (juce::TooltipWindow::outlineColourId, Col::line);
}

juce::Label* LnF::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::FontOptions (11.0f));
    l->setColour (juce::Label::textColourId, Col::dim);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

void LnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto colour = s.findColour (juce::Slider::rotarySliderFillColourId);
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = start + pos * (end - start);
    const float thick = juce::jmax (2.5f, r * 0.14f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - thick, r - thick, 0.0f, start, end, true);
    g.setColour (Col::line);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // bipolar knobs (pitch) fill from the centre
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    juce::Path val;
    val.addCentredArc (c.x, c.y, r - thick, r - thick, 0.0f, juce::jmin (from, ang), juce::jmax (from, ang), true);
    if (s.isMouseOverOrDragging())   // hover / drag glow: instant visual feedback
    {
        g.setColour (colour.withAlpha (0.22f));
        g.strokePath (val, juce::PathStrokeType (thick * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (colour);
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = r - thick * 2.2f;
    g.setColour (Col::panelHi);
    g.fillEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2);
    g.setColour (Col::line);
    g.drawEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2, 1.0f);

    juce::Path ptr;
    ptr.addRoundedRectangle (-1.5f, -inner + 2.0f, 3.0f, inner * 0.55f, 1.5f);
    g.setColour (Col::text);
    g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void LnF::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.isMouseOver (true) ? Col::dim : Col::line);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (ax - 5.0f, ay - 2.5f, ax + 5.0f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (Col::dim);
    g.fillPath (arrow);
}

//============================================================================== Scope
void Scope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Col::line.withAlpha (0.6f));
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

    auto drawWave = [&] (const std::vector<float>& d, juce::Colour c, float thickness)
    {
        if (d.size() < 2) return;
        juce::Path p;
        const float mid = r.getCentreY(), amp = r.getHeight() * 0.45f;
        const int n = (int) d.size();
        const int step = juce::jmax (1, n / juce::jmax (1, (int) r.getWidth()));
        for (int i = 0; i < n; i += step)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
            const float y = mid - juce::jlimit (-1.0f, 1.0f, d[(size_t) i]) * amp;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved));
    };
    drawWave (input,  Col::dim.withAlpha (0.55f), 1.0f);
    drawWave (output, Col::cyan, 1.6f);

    // legend
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    auto lg = getLocalBounds().removeFromTop (16).removeFromRight (150);
    g.setColour (Col::dim);
    g.fillRect (lg.getX(), lg.getCentreY() - 1, 12, 2);
    g.drawText ("INPUT", lg.getX() + 16, lg.getY(), 50, 16, juce::Justification::left);
    g.setColour (Col::cyan);
    g.fillRect (lg.getX() + 70, lg.getCentreY() - 1, 12, 2);
    g.drawText ("VOICE", lg.getX() + 86, lg.getY(), 60, 16, juce::Justification::left);
}

//============================================================================== licence helpers (unchanged)
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


void AncientVoicesAudioProcessorEditor::configureKnob(juce::Slider& s, juce::Label& lbl, const juce::String& name,
                                                     juce::Colour colour, const juce::String& tip)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setColour(juce::Slider::textBoxTextColourId,       Col::dim);
    s.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxHighlightColourId,  colour.withAlpha(0.35f));
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 16);
    s.setColour(juce::Slider::rotarySliderFillColourId, colour);
    s.setTooltip(tip);
    addAndMakeVisible(s);
    lbl.setText(name, juce::dontSendNotification);
    lbl.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lbl.setColour(juce::Label::textColourId, Col::dim);
    lbl.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lbl);
}

AncientVoicesAudioProcessorEditor::AncientVoicesAudioProcessorEditor(AncientVoicesAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), tooltipWindow(this, 450)
{
    // No license prompt: Ancient Voices is sold as a one-time purchase with no
    // serial keys (customers never receive a User ID / License Key).
    setLookAndFeel(&lnf);
    tooltipWindow.setLookAndFeel(&lnf);

    logo = juce::ImageCache::getFromMemory(TullyLogoData::png, TullyLogoData::pngSize);

    presetCombo.setJustificationType(juce::Justification::centredLeft);
    presetCombo.setTextWhenNothingSelected("Choose a voice");
    presetCombo.setTooltip("Choose a voice. Each one is a complete character: choir size, era, room and tone.");
    populatePresetCombo();
    presetCombo.onChange = [this] { onPresetComboChanged(); };
    addAndMakeVisible(presetCombo);

    profileDisplayLabel.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    profileDisplayLabel.setColour(juce::Label::textColourId, Col::text);
    profileDisplayLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(profileDisplayLabel);

    descriptionLabel.setFont(juce::FontOptions(13.0f));
    descriptionLabel.setColour(juce::Label::textColourId, Col::dim);
    descriptionLabel.setJustificationType(juce::Justification::topLeft);
    descriptionLabel.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(descriptionLabel);

    configureKnob(pitchSlider,  pitchLabel,  "PITCH",  Col::pink, "Shift the voice up or down, in semitones.");
    configureKnob(reverbSlider, reverbLabel, "REVERB", Col::cyan, "How much of the voice's space (temple, hall, chamber) you hear.");
    configureKnob(delaySlider,  delayLabel,  "DELAY",  Col::cyan, "Echo amount.");
    configureKnob(driveSlider,  driveLabel,  "DRIVE",  Col::cyan, "Warmth and grit.");
    configureKnob(outputSlider, outputLabel, "OUTPUT", Col::pink, "Output level.");
    configureKnob(mixSlider,    mixLabel,    "MIX",    Col::pink, "Blend between your dry voice (left) and the transformed voice (right).");

    auto& state = audioProcessor.getAPVTS();
    reverbAtt = std::make_unique<SA>(state, "USER_REVERB", reverbSlider);
    delayAtt  = std::make_unique<SA>(state, "USER_DELAY",  delaySlider);
    driveAtt  = std::make_unique<SA>(state, "USER_DRIVE",  driveSlider);
    outputAtt = std::make_unique<SA>(state, "USER_OUTPUT", outputSlider);
    mixAtt    = std::make_unique<SA>(state, "MIX",         mixSlider);
    pitchAtt  = std::make_unique<SA>(state, "USER_PITCH",  pitchSlider);
    for (auto* s : { &reverbSlider, &delaySlider, &driveSlider, &outputSlider, &mixSlider })
        s->setNumDecimalPlacesToDisplay(2);
    pitchSlider.setNumDecimalPlacesToDisplay(1);
    pitchSlider.setTextValueSuffix(" st");

    // snappier knobs: less travel per turn, double-click resets, shift-drag is fine control
    {
        const std::pair<juce::Slider*, const char*> knobs[] = { { &reverbSlider, "USER_REVERB" }, { &delaySlider, "USER_DELAY" },
            { &driveSlider, "USER_DRIVE" }, { &outputSlider, "USER_OUTPUT" }, { &mixSlider, "MIX" }, { &pitchSlider, "USER_PITCH" } };
        for (auto& [sl, id] : knobs)
        {
            sl->setMouseDragSensitivity (180);
            if (auto* prm = state.getParameter (id))
                sl->setDoubleClickReturnValue (true, (double) state.getParameterRange (id).convertFrom0to1 (prm->getDefaultValue()));
            sl->setTooltip (sl->getTooltip() + "  (double-click: reset, shift-drag: fine)");
        }
    }
    scope.setTooltip ("Live picture of your input (grey) and the transformed voice (cyan).");

    syncComboFromParameter();

    addAndMakeVisible(scope);

    footerLabel.setText(juce::String::fromUTF8("\xc2\xa9 \xe2\x84\x97 2026 Tully EDM Vibe  \xe2\x80\x94  "), juce::dontSendNotification);
    footerLabel.setFont(juce::FontOptions(10.5f));
    footerLabel.setColour(juce::Label::textColourId, Col::dim);
    footerLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(footerLabel);

    footerEmailLink.setButtonText("info@tullyedmvibe.com");
    footerEmailLink.setURL(juce::URL("mailto:info@tullyedmvibe.com"));
    footerEmailLink.setFont(juce::FontOptions(10.5f), false, juce::Justification::centredLeft);
    footerEmailLink.setColour(juce::HyperlinkButton::textColourId, Col::cyan);
    footerEmailLink.setTooltip("Email info@tullyedmvibe.com");
    addAndMakeVisible(footerEmailLink);

    startTimerHz(30);
    setSize(900, 620);
}

AncientVoicesAudioProcessorEditor::~AncientVoicesAudioProcessorEditor()
{
    stopTimer();
    tooltipWindow.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void AncientVoicesAudioProcessorEditor::populatePresetCombo()
{
    using namespace AncientVoicesPresets;
    presetCombo.clear(juce::dontSendNotification);
    presetCombo.addSectionHeading("MisterMultiVox Originals");
    for (int i = 0; i < kMmvProfileCount; ++i) {
        juce::String itemText = juce::String(getMmvProfileName(i))
                              + juce::String::fromUTF8("  \xe2\x80\x94  ")
                              + getMmvProfileDescription(i);
        presetCombo.addItem(itemText, i + 1);
    }
    Category lastCategory = Category::Chant;
    bool firstInGroup = true;
    for (int i = 0; i < kAvPresetCount; ++i) {
        const auto& pr = getPreset(i);
        if (firstInGroup || pr.category != lastCategory) {
            presetCombo.addSectionHeading(getCategoryName(pr.category));
            lastCategory = pr.category;
            firstInGroup = false;
        }
        juce::String itemText = juce::String(pr.name)
                              + juce::String::fromUTF8("  \xe2\x80\x94  ")
                              + getPresetDescription(i);
        presetCombo.addItem(itemText, pr.globalIndex + 1);
    }
}

void AncientVoicesAudioProcessorEditor::showProfile(int g)
{
    using namespace AncientVoicesPresets;
    juce::String name = "Alpha", desc;
    if (g >= 0 && g < kMmvProfileCount)
    {
        name = getMmvProfileName(g);
        desc = getMmvProfileDescription(g);
    }
    else if (g >= kMmvProfileCount && g < kTotalProfiles)
    {
        const int i = g - kMmvProfileCount;
        name = getPreset(i).name;
        desc = juce::String(getCategoryName(getPreset(i).category)) + juce::String::fromUTF8("  \xc2\xb7  ") + getPresetDescription(i);
    }
    profileDisplayLabel.setText(name, juce::dontSendNotification);
    descriptionLabel.setText(desc, juce::dontSendNotification);
}

void AncientVoicesAudioProcessorEditor::onPresetComboChanged()
{
    const int id = presetCombo.getSelectedId();
    if (id <= 0) return;
    const int g = id - 1;
    if (auto* param = audioProcessor.getAPVTS().getParameter("PROFILE_INDEX"))
        param->setValueNotifyingHost(param->convertTo0to1((float) g));
    showProfile(g);
}

void AncientVoicesAudioProcessorEditor::syncComboFromParameter()
{
    if (auto* v = audioProcessor.getAPVTS().getRawParameterValue("PROFILE_INDEX")) {
        const int g = (int) v->load();
        presetCombo.setSelectedId(g + 1, juce::dontSendNotification);
        showProfile(g);
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
    // one shared scale so the input/voice levels stay comparable
    float peak = 0.0f;
    for (auto s : inData)  peak = std::max(peak, std::abs(s));
    for (auto s : outData) peak = std::max(peak, std::abs(s));
    const float k = peak > 0.0001f ? 1.0f / peak : 1.0f;
    for (auto& s : inData)  s *= k;
    for (auto& s : outData) s *= k;
    scope.setData(std::move(inData), std::move(outData));
}

void AncientVoicesAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);

    headerArea = area.removeFromTop(58);
    logoArea   = headerArea.withWidth(58).withSizeKeepingCentre(58, 56);
    area.removeFromTop(10);

    auto footerRow = area.removeFromBottom(18);
    const int copyrightWidth = 180, emailWidth = 150;
    footerRow.removeFromLeft(juce::jmax(0, (footerRow.getWidth() - copyrightWidth - emailWidth) / 2));
    footerLabel.setBounds(footerRow.removeFromLeft(copyrightWidth));
    footerEmailLink.setBounds(footerRow.removeFromLeft(emailWidth));
    area.removeFromBottom(8);

    // VOICE panel
    voiceArea = area.removeFromTop(150);
    {
        auto r = voiceArea.reduced(16, 0).withTrimmedTop(38).withTrimmedBottom(12);
        presetCombo.setBounds(r.removeFromTop(32));
        r.removeFromTop(8);
        profileDisplayLabel.setBounds(r.removeFromTop(34));
        descriptionLabel.setBounds(r);
    }
    area.removeFromTop(10);

    // CONTROLS panel: 6 knobs across
    controlsArea = area.removeFromTop(160);
    {
        auto r = controlsArea.reduced(12, 0).withTrimmedTop(38).withTrimmedBottom(10);
        const int w = r.getWidth() / 6;
        auto place = [&](juce::Slider& s, juce::Label& lbl) {
            auto cell = r.removeFromLeft(w).reduced(8, 0);
            lbl.setBounds(cell.removeFromTop(16));
            s.setBounds(cell);
        };
        place(pitchSlider,  pitchLabel);
        place(reverbSlider, reverbLabel);
        place(delaySlider,  delayLabel);
        place(driveSlider,  driveLabel);
        place(outputSlider, outputLabel);
        place(mixSlider,    mixLabel);
    }
    area.removeFromTop(10);

    scopeArea = area;
    scope.setBounds(scopeArea.reduced(16, 0).withTrimmedTop(36).withTrimmedBottom(12));
}

void AncientVoicesAudioProcessorEditor::drawPanel(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour c) const
{
    g.setColour(Col::panel);
    g.fillRoundedRectangle(r.toFloat(), 8.0f);
    g.setColour(Col::line);
    g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 8.0f, 1.0f);
    g.setColour(c);
    g.fillRoundedRectangle((float) r.getX() + 12.0f, (float) r.getY() + 12.0f, 4.0f, 14.0f, 2.0f);
    g.setColour(Col::text);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(title, r.getX() + 22, r.getY() + 9, 300, 20, juce::Justification::left);
}

void AncientVoicesAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(Col::bg);

    if (logo.isValid())
        g.drawImage(logo, logoArea.toFloat(), juce::RectanglePlacement::centred);

    const int tx = logoArea.getRight() + 12;
    g.setColour(Col::text);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText("ANCIENT VOICES", tx, headerArea.getY() + 4, 300, 30, juce::Justification::left);
    g.setColour(Col::dim);
    g.setFont(juce::FontOptions(11.5f));
    g.drawText(juce::String::fromUTF8("Vocal transformer  \xc2\xb7  Sumerian, Egyptian and Mesopotamian voices and choirs"),
               tx, headerArea.getY() + 32, 520, 16, juce::Justification::left);

    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(Col::dim);
    g.drawText("TULLY EDM VIBE", headerArea.getRight() - 200, headerArea.getY() + 10, 200, 16, juce::Justification::right);
    g.setFont(juce::FontOptions(10.5f));
    g.drawText(juce::String("v") + JucePlugin_VersionString, headerArea.getRight() - 200, headerArea.getY() + 28, 200, 14,
               juce::Justification::right);

    drawPanel(g, voiceArea,    "VOICE",    Col::pink);
    drawPanel(g, controlsArea, "SHAPE",    Col::cyan);
    drawPanel(g, scopeArea,    "SIGNAL",   Col::cyan);
}

void AncientVoicesAudioProcessorEditor::timerCallback()
{
    updateWaveforms();
    if (auto* v = audioProcessor.getAPVTS().getRawParameterValue("PROFILE_INDEX")) {
        const int cur = (int) v->load();
        if (presetCombo.getSelectedId() - 1 != cur)
            syncComboFromParameter();
    }
}
