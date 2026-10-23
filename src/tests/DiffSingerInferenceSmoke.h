#pragma once
#include "../SettingsComponent.h"
#include "../backend/DiffSingerRenderer.h"
#include "../AudioEngine.h"
#include <iostream>

namespace hachi
{
inline juce::Component* dsFindControl(juce::Component& root, const juce::String& id)
{
    if (root.getComponentID() == id) return &root;
    for (auto* child : root.getChildren()) if (auto* found = dsFindControl(*child, id)) return found;
    return nullptr;
}
inline bool runDiffSingerInferenceSmoke(const juce::File& bank, const juce::File& folder)
{
    folder.createDirectory();
    int checks = 0;
    const auto check = [&](bool value, const char* name)
    {
        std::cout << name << "=" << value << std::endl;
        if (value) ++checks;
        return value;
    };
    juce::PropertiesFile::Options po;
    po.applicationName = "DS-GPU-quality-smoke"; po.filenameSuffix = "settings";
    po.folderName = folder.getFullPathName(); po.storageFormat = juce::PropertiesFile::storeAsXML;
    juce::PropertiesFile properties(po);
    properties.clear();
    I18n strings; juce::AudioDeviceManager devices;
    {
        SettingsComponent page(strings, devices, properties, [&] { backend::DiffSingerRenderer::configure(backend::DiffSingerOptions::read(properties)); });
        page.selectPage(5); page.setSize(820, 640);
        auto* backendBox = dynamic_cast<juce::ComboBox*>(dsFindControl(page, "ds.backend"));
        auto* preview = dynamic_cast<juce::ComboBox*>(dsFindControl(page, "ds.preview"));
        auto* exporting = dynamic_cast<juce::ComboBox*>(dsFindControl(page, "ds.export"));
        auto* steps = dynamic_cast<juce::Slider*>(dsFindControl(page, "ds.pitch"));
        auto* depth = dynamic_cast<juce::Slider*>(dsFindControl(page, "ds.depth"));
        auto* apply = dynamic_cast<juce::TextButton*>(dsFindControl(page, "settings.apply"));
        if (!check(backendBox && preview && exporting && steps && depth && apply && page.diagnosticTabCount()==6, "settings_controls")) return false;
        if (!check(!steps->isEnabled() && preview->getSelectedId()==2 && exporting->getSelectedId()==3, "default_standard_and_high")) return false;
        preview->setSelectedId(4, juce::sendNotificationSync);
        if (!check(steps->isEnabled(), "custom_controls_enabled")) return false;
        backendBox->setSelectedId(3, juce::sendNotificationSync); steps->setValue(7); depth->setValue(50); apply->onClick();
        auto opts = backend::DiffSingerOptions::read(properties);
        if (!check(opts.backend==3 && opts.preview==4 && opts.pitchSteps==7 && opts.depth==.5, "saved_settings")) return false;
        preview->setSelectedId(1, juce::sendNotificationSync); apply->onClick();
        if (auto stream=folder.getChildFile("DS-GPU-quality-settings.png").createOutputStream())
            juce::PNGImageFormat().writeImageToStream(page.createComponentSnapshot(page.getLocalBounds()), *stream);
    }
    juce::PropertiesFile reopened(po);
    auto settings = backend::DiffSingerOptions::read(reopened);
    if (!check(settings.backend==3 && settings.preview==1 && settings.exportQuality==3, "settings_reload")) return false;
    backend::DiffSingerRenderer::configure(settings);
    auto previewOptions = backend::DiffSingerRenderer::inferenceOptions();
    auto exportOptions = backend::DiffSingerRenderer::inferenceOptions(true);
    if (!check((int)previewOptions["acoustic_steps"]==8 && (int)exportOptions["acoustic_steps"]==50, "separate_profiles")) return false;
    ProjectData data; TrackData track; track.id="ds-quality-track"; track.name="DS quality";
    track.pitchAlgorithm=PitchAlgorithm::utau; track.voicebankDirectory=bank; track.compose=true; track.utauMode=UtauMode::mou;
    ClipData clip; clip.id="ds-quality-clip"; clip.durationSeconds=1.8;
    NoteData note; note.id="ds-quality-note"; note.label="ni"; note.startSeconds=.3; note.durationSeconds=1.2; note.midiNote=60;
    note.pitchControlPoints={{0,60},{.4,61},{1.2,60}}; clip.notes.push_back(note); track.clips.push_back(clip);data.tracks.push_back(track);
    AudioEngine engine; engine.prepareToPlay(512,44100); engine.selectEveryUtauNote(data);
    const auto awaitRender = [&]
    {
        for(int i=0;i<3600 && !engine.hasCurrentRenderedAudio();++i)
        {
            if(i>60 && !engine.renderProgress())break;
            juce::Thread::sleep(50);
        }
        return engine.hasCurrentRenderedAudio() && engine.activeRenderWarnings().isEmpty();
    };
    engine.syncProject(data);
    if (!check(awaitRender(), "gpu_preview_render")) { std::cout<<engine.activeRenderWarnings()<<std::endl;return false; }
    auto report = backend::DiffSingerRenderer::inferenceReport();
    if (!check(report["backend"].toString().contains("DirectML") && report["options"]["quality"].toString()=="fast", "gpu_preview_profile"))return false;
    engine.syncProject(data, true);
    if (!check(awaitRender(), "gpu_export_rerender"))return false;
    report=backend::DiffSingerRenderer::inferenceReport();
    if (!check(report["options"]["quality"].toString()=="high" && (int)report["sampling"]["acoustic"]["steps"][0]==50, "export_uses_high_quality_cache"))return false;
    juce::String error;
    if (!check(engine.exportWav(folder.getChildFile("GPU-high-quality-export.wav"), error), "wav_export")) { std::cout<<error<<std::endl;return false; }
    if (!check(std::equal(note.pitchControlPoints.begin(), note.pitchControlPoints.end(), data.tracks.front().clips.front().notes.front().pitchControlPoints.begin(), [](const auto& a, const auto& b) { return a.timeSeconds==b.timeSeconds && a.targetMidi==b.targetMidi; }), "export_preserves_pitch"))return false;
    settings.preview=4; settings.acousticSteps=12; settings.varianceSteps=9;settings.depth=.5;
    backend::DiffSingerRenderer::configure(settings); engine.syncProject(data);
    if (!check(awaitRender(), "changed_settings_rerender")) return false;
    report=backend::DiffSingerRenderer::inferenceReport();
    if (!check((int)report["sampling"]["acoustic"]["steps"][0]==12 && report["options"]["quality"].toString()=="custom", "custom_invalidates_audio_cache"))return false;
    folder.getChildFile("inference-report.json").replaceWithText(juce::JSON::toString(report));
    std::cout<<"checks="<<checks<<std::endl;
    return true;
}
}
