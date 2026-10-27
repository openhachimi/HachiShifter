#pragma once
#include "ProjectMaterialRegistrationModelSmoke.h"
#include "../ProjectMaterialRegistrationFacts.h"
#include "../MaterialPronunciationCatalog.h"
#include "../AudioEngine.h"
#include "../NativeTransitionDetection.h"
#include "../backend/NsfHifiganRenderer.h"
#include <map>
#include <set>
#include <tuple>

namespace hachi
{
namespace teioRegistrationAudioDetail
{
inline juce::String requestFingerprint(const ProjectData& project)
{
    juce::MemoryOutputStream bytes;
    const auto curve = [&](const std::vector<float>& values)
    { bytes.writeInt64(juce::int64(values.size())); for (const auto value : values) bytes.writeFloat(value); };
    for (std::size_t ti = 0; ti < project.tracks.size(); ++ti)
    {
        const auto& track = project.tracks[ti]; bytes.writeString(track.id);
        for (std::size_t ci = 0; ci < track.clips.size(); ++ci)
        {
            const auto& clip = track.clips[ci]; bytes.writeString(clip.id);
            const bool trace=juce::SystemStats::getEnvironmentVariable("HACHI_PROFILE_REQUEST_FINGERPRINT",{}).isNotEmpty();
            const auto started=juce::Time::getMillisecondCounterHiRes();
            if(trace)std::cout<<"fingerprint clip="<<ci<<" file="<<clip.sourceFile.getFileName()<<" request BEGIN"<<std::endl;
            const auto request = AudioEngine::diagnosticNativeRequest(clip, track);
            if(trace)std::cout<<"fingerprint clip="<<ci<<" request END ms="<<(juce::Time::getMillisecondCounterHiRes()-started)<<" target BEGIN"<<std::endl;
            // Deliberately exclude ONLY physical sourceFile reference. No request
            // clock/contour/gain/synthesis setting is fitted, changed or omitted.
            bytes.writeDouble(request.sourceOffsetSeconds); bytes.writeDouble(request.sourceDurationSeconds);
            bytes.writeDouble(request.targetDurationSeconds); bytes.writeDouble(request.framePeriodMs);
            curve(request.sourceMidi); curve(request.targetMidi); curve(request.formantSemitones);
            curve(request.noteGain); curve(request.tension); curve(request.breath); curve(request.robustPitchCurve);
            bytes.writeInt64(juce::int64(request.timeMap.size()));
            for (const auto& p : request.timeMap) { bytes.writeDouble(p.targetSeconds); bytes.writeDouble(p.sourceSeconds); }
            bytes.writeString(request.hifiganModelDirectory.getFullPathName());
            bytes.writeInt(int(request.inference.requested)); bytes.writeInt(request.inference.deviceIndex);
            bytes.writeInt(request.inference.intraOpThreads);
            bytes.writeInt(int(request.pitchBackend)); bytes.writeInt(request.stretchAlgorithm);
            bytes.writeBool(request.normalizeVolume); bytes.writeBool(request.nsfSmoothPitchTransitions);
            bytes.writeBool(request.nsfNoiseProtection); bytes.writeBool(request.matchNsfSourceLevel);
            bytes.writeBool(request.preserveUneditedSource); bytes.writeBool(request.nsfIncrementalRender);
            bytes.writeFloat(request.neuralGuardStartSeconds); bytes.writeFloat(request.neuralGuardEndSeconds);
            bytes.writeFloat(request.neighborEdgeF0Start); bytes.writeFloat(request.neighborEdgeF0End);
            bytes.writeBool(request.isGlideMerged); bytes.writeInt(int(request.exportComponent));
            curve(AudioEngine::diagnosticNativeTargetMidi(project, int(ti), int(ci)));
            if(trace)std::cout<<"fingerprint clip="<<ci<<" target END total_ms="<<(juce::Time::getMillisecondCounterHiRes()-started)<<std::endl;
        }
    }
    return juce::SHA256(bytes.getData(), bytes.getDataSize()).toHexString();
}
inline std::map<juce::String, juce::String> modelHashes(const juce::File& folder)
{
    std::map<juce::String, juce::String> result;
    for (const auto& entry : juce::RangedDirectoryIterator(folder, true, "*", juce::File::findFiles))
    {
        const auto file = entry.getFile();
        result[file.getRelativePathFrom(folder)] = juce::SHA256(file).toHexString();
    }
    return result;
}
}

// CLI-ready real acceptance entry. The confirmed policy was explicitly selected
// by the user; this is not automatic phonetic annotation. Original project track
// modes/order/tempo/solo/noise/NSF/amplitude settings are NEVER overridden here.
inline bool runTeioProjectMaterialRegistrationAudioGate(const juce::File& originalProject,
    const juce::File& outputPrefix, const juce::File& nsfModel,bool autoTransitions=false)
{
    using namespace teioRegistrationAudioDetail;
    using F = ProjectMaterialRegistrationFacts;
    if (!originalProject.existsAsFile() || !outputPrefix.isDirectory()) return false;
    const auto reportRoot = outputPrefix.getChildFile("teio-report-" + juce::Uuid().toString());
    if (reportRoot.exists() || reportRoot.createDirectory().failed()) return false;
    auto report = std::make_unique<juce::DynamicObject>();
    juce::Array<juce::var> checks, renders;
    bool ok = true; juce::String error;
    const auto check = [&](const char* name, bool pass, const juce::String& detail = {})
    {
        ok &= pass; std::cout << name << '=' << pass;
        if (detail.isNotEmpty()) std::cout << '|' << detail;
        std::cout << std::endl;
        auto item = std::make_unique<juce::DynamicObject>(); item->setProperty("name", name);
        item->setProperty("pass", pass); item->setProperty("detail", detail); checks.add(juce::var(item.release()));
    };
    const auto finish = [&]()
    {
        report->setProperty("ok", ok); report->setProperty("error", error);
        report->setProperty("checks", juce::var(checks)); report->setProperty("renders", juce::var(renders));
        const auto text = juce::JSON::toString(juce::var(report.release()));
        juce::FileOutputStream output(reportRoot.getChildFile("verification.json"));
        const bool written = output.openedOk() && output.writeText(text, false, false, "\n");
        output.flush(); return ok && written && output.getStatus().wasOk();
    };
    report->setProperty("project", originalProject.getFullPathName());
    report->setProperty("projectSHA256", juce::SHA256(originalProject).toHexString());
    report->setProperty("nsfModel", nsfModel.getFullPathName());
    report->setProperty("inference", "CPU, no fallback; no NSF diagnostic or musical setting overrides");
    report->setProperty("annotationPolicy", "USER-CONFIRMED carried placements as independent unnamed variants; not phonetic recovery");
    report->setProperty("pcmGate", "Exact decoded float PCM memcmp, identical headers/length, non-silent baseline; no offset/gain fitting");
    check("real_nsf_model_available", backend::NsfHifiganRenderer::modelAvailable(nsfModel));
    if (!ok) return finish();
    const auto resources = modelHashes(nsfModel);
    check("model_bytes_snapshotted", !resources.empty());
    juce::Array<juce::var> modelFiles;
    for (const auto& entry : resources)
    { auto item = std::make_unique<juce::DynamicObject>(); item->setProperty("relativePath", entry.first); item->setProperty("sha256", entry.second); modelFiles.add(juce::var(item.release())); }
    report->setProperty("modelFiles", juce::var(modelFiles));
    ProjectModel inspected;
    check("original_project_read_only_load", inspected.load(originalProject, error) && error.isEmpty(), error);
    if (!ok) return finish();
    auto original = inspected.snapshot();
    if(autoTransitions)
    {
        const auto before=requestFingerprint(original);const auto candidates=detectMelodyneTransitions(original);
        report->setProperty("autoTransitionCandidates",int(candidates.size()));report->setProperty("autoTransitionsAnnotated",annotateMelodyneTransitions(original,candidates));
        check("transition_identity_annotation_preserves_exact_native_request",requestFingerprint(original)==before);
    }
    const auto facts = F::derive(original, F::Policy::ConfirmedCarriedPlacements);
    check("confirmed_carried_facts_no_inferred_phonetics", facts.ready, facts.diagnostics.joinIntoString("\n"));
    if (!ok) return finish();
    std::size_t originalNotes = 0; bool nativeNsf = false;
    juce::Array<juce::var> trackSettings;
    for (const auto& track : original.tracks)
    {
        nativeNsf |= track.pitchAlgorithm == PitchAlgorithm::nsfHifigan && !trackUsesVoicebankSynthesis(track);
        auto item = std::make_unique<juce::DynamicObject>(); item->setProperty("id", track.id); item->setProperty("name", track.name);
        item->setProperty("muted", track.muted); item->setProperty("solo", track.solo);
        item->setProperty("pitchAlgorithm", int(track.pitchAlgorithm)); item->setProperty("renderOrder", int(track.renderOrder));
        item->setProperty("stretchAlgorithm", int(track.stretchAlgorithm)); item->setProperty("mergeContiguousSource", track.mergeContiguousSource);
        item->setProperty("nsfNoiseProtection", track.nsfNoiseProtection); item->setProperty("nsfSmoothPitchTransitions", track.nsfSmoothPitchTransitions);
        item->setProperty("normalizeVolume", track.normalizeVolume); item->setProperty("flags", track.utauGlobalFlags);
        trackSettings.add(juce::var(item.release()));
        for (const auto& clip : track.clips) originalNotes += clip.notes.size();
    }
    report->setProperty("tracks", juce::var(trackSettings)); report->setProperty("bpm", original.bpm);
    report->setProperty("windowFromSeconds", 0); report->setProperty("windowToSeconds", original.durationSeconds());
    report->setProperty("originalNotes", int(originalNotes)); report->setProperty("confirmedVariants", int(facts.variants.size()));
    check("actual_teio_81_carried_placements", originalNotes == 81 && facts.variants.size() == 81 && nativeNsf);
    if (!ok) return finish();
    const auto expectedMusic = ProjectModel::registrationMusicSourceFingerprint(original);
    const auto expectedRequest = requestFingerprint(original);
    report->setProperty("musicSourceFingerprint", expectedMusic); report->setProperty("nativeRequestFingerprint", expectedRequest);
    const auto previousRoots = SampleSettings::materialRoots();
    struct RestoreRoots
    {
        juce::StringArray old;
        ~RestoreRoots() { SampleSettings::setMaterialRoots(old); }
    } restore {previousRoots};
    using Registration=ProjectMaterialRegistration;
    using BindingKey=std::tuple<juce::String,juce::String,juce::String,juce::String>;
    const auto key=[](const Registration::Locator& where){return BindingKey{where.trackId,where.parentClipId,where.sourcePartId,where.noteId};};
    std::unique_ptr<Registration::Plan> expectedPlan;
    std::map<BindingKey,const Registration::Binding*> expectedBindings;
    std::size_t expectedCatalogEntries=0;
    int phase = 0;
    const RegistrationRealRender renderer = [&](const ProjectData& data, const juce::File& file, juce::String& renderError)
    {
        const auto tag = phase == 0 ? juce::String("before") : phase == 1 ? juce::String("after-adoption") : juce::String("cold-reopen");
        ++phase;
        const auto request = requestFingerprint(data);
        check((tag + "_exact_native_request_source_clock_F0_arrays").toRawUTF8(), request == expectedRequest, request);
        check((tag + "_exact_music_source_snapshot").toRawUTF8(), ProjectModel::registrationMusicSourceFingerprint(data) == expectedMusic);
        if (!ok) { renderError = "Actual render request or music/source authority changed"; return false; }
        if (phase > 1)
        {
            // Register ONLY in the isolated process-local source-root service;
            // no persistent user prefs and no source-update/reanalysis call.
            const auto copiedRoot = data.tracks.front().clips.front().sourceFile.getParentDirectory();
            SampleSettings::setMaterialRoots(juce::StringArray{copiedRoot.getFullPathName()});
            OttoMaterialIO::Document document;
            bool sourcesOk = OttoMaterialIO::loadDocument(copiedRoot.getChildFile("otto.ini"), document, renderError);
            std::set<juce::String> physicalPaths;
            for (const auto& audio : document.audio) physicalPaths.insert(audio.relativePath.toLowerCase());
            report->setProperty("copiedPhysicalRecordings", int(physicalPaths.size()));
            report->setProperty("publishedAnnotationVariants", int(document.audio.size()));
            juce::String actualDocument,plannedDocument;
            sourcesOk &= expectedPlan&&physicalPaths.size()==expectedPlan->recordings.size()
                &&document.audio.size()==expectedPlan->document.audio.size();
            if(expectedPlan)sourcesOk &= OttoMaterialIO::encode(document,actualDocument,renderError)
                &&OttoMaterialIO::encode(expectedPlan->document,plannedDocument,renderError)&&actualDocument==plannedDocument;
            const auto catalog = MaterialPronunciationCatalog::index({{copiedRoot, UtauMode::classic}});
            sourcesOk &= catalog.ok() && catalog.warnings.isEmpty() && catalog.entries.size()==expectedCatalogEntries;
            int checked = 0;
            std::set<BindingKey> checkedBindings;
            for (const auto& track : data.tracks) for (const auto& clip : track.clips) for (const auto& note : clip.notes)
            {
                const Registration::Locator where{track.id,clip.id,note.clipPartId,note.id};
                const auto binding=expectedBindings.find(key(where));
                juce::File ownerFile=clip.sourceFile;
                if(!note.clipPartId.isEmpty())
                {ownerFile=juce::File{};for(const auto& part:clip.parts)if(part.id==note.clipPartId)ownerFile=part.sourceFile;}
                sourcesOk &= binding!=expectedBindings.end()&&checkedBindings.insert(key(where)).second;
                if(binding!=expectedBindings.end())sourcesOk &= note.materialAudioId==binding->second->audioId
                    &&note.materialRegionId==binding->second->regionId&&ownerFile==binding->second->copiedSource;
                int matches = 0;
                for (const auto& entry : catalog.entries)
                    if (entry.audio == ownerFile && entry.audioId == note.materialAudioId && entry.regionId == note.materialRegionId)
                    {
                        MaterialPronunciationCatalog::Entry resolved;
                        if (entry.alias == "-" && note.materialLocalOverride
                            && MaterialPronunciationCatalog::resolve(entry, resolved, renderError)
                            && resolved.audioId == note.materialAudioId && resolved.regionId == note.materialRegionId) ++matches;
                    }
                sourcesOk &= matches == 1; ++checked;
            }
            sourcesOk &= checked==int(originalNotes)&&checkedBindings.size()==expectedBindings.size();
            check((tag + "_isolated_catalog_copied_audio_OTTO_explicit_variant_readback").toRawUTF8(), sourcesOk,
                "physical=" + juce::String(int(physicalPaths.size())) + ", variants=" + juce::String(int(document.audio.size()))
                + ", boundNotes=" + juce::String(checked) + ", " + renderError);
            if (!sourcesOk) { if (renderError.isEmpty()) renderError = "Copied source/variant catalog readback failed"; return false; }
        }
        AudioEngine engine(false);
        engine.setHifiganModelDirectory(nsfModel); engine.setInferenceConfiguration(backend::InferenceBackend::cpu, -1);
        engine.prepareToPlay(512, 48000); engine.selectEveryUtauNote(data); engine.syncProject(data);
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 1800000.0;
        while (engine.renderProgress().has_value() && juce::Time::getMillisecondCounterHiRes() < deadline) juce::Thread::sleep(10);
        auto result = std::make_unique<juce::DynamicObject>(); result->setProperty("phase", tag); result->setProperty("wav", file.getFullPathName());
        result->setProperty("backend", engine.activeRenderBackends()); result->setProperty("warnings", engine.activeRenderWarnings());
        result->setProperty("nativeRequestFingerprint", request);
        const auto backend = engine.activeRenderBackends();
        const bool real = !engine.renderProgress().has_value() && engine.hasCurrentRenderedAudio()
            && backend.containsIgnoreCase("nsf-hifigan-onnx") && backend.containsIgnoreCase("cpu")
            && !backend.containsIgnoreCase("fallback") && !backend.containsIgnoreCase("world") && engine.activeRenderWarnings().isEmpty();
        check((tag + "_actual_CPU_ONNX_no_fallback_or_warning").toRawUTF8(), real, backend + " | " + engine.activeRenderWarnings());
        WavExportOptions options; options.sampleRate = 48000; options.channels = 2; options.bitDepth = 32;
        const bool exported = real && engine.exportWav(file, renderError, {}, 0, original.durationSeconds(), options);
        bool finiteAudible = false;
        if (exported)
        {
            juce::AudioFormatManager formats; formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
            if (reader)
            {
                result->setProperty("sampleRate", reader->sampleRate); result->setProperty("channels", int(reader->numChannels));
                result->setProperty("samples", reader->lengthInSamples); result->setProperty("floatWav", reader->usesFloatingPointData);
                bool finite = reader->sampleRate == 48000 && reader->numChannels == 2 && reader->lengthInSamples > 0
                    && reader->bitsPerSample == 32 && reader->usesFloatingPointData;
                juce::AudioBuffer<float> decoded(2, 8192); float peak = 0;
                for (juce::int64 offset = 0; finite && offset < reader->lengthInSamples; offset += 8192)
                {
                    const int count = int(std::min<juce::int64>(8192, reader->lengthInSamples - offset));
                    finite = reader->read(&decoded, 0, count, offset, true, true);
                    for (int channel = 0; finite && channel < 2; ++channel) for (int i = 0; i < count; ++i)
                    { const auto sample = decoded.getSample(channel, i); finite &= std::isfinite(sample); peak = std::max(peak, std::abs(sample)); }
                }
                finiteAudible = finite && peak > 1.e-6f; result->setProperty("peak", peak);
            }
        }
        const bool noDevice = engine.devices().getCurrentAudioDevice() == nullptr && !engine.isPlaying();
        result->setProperty("exported", exported); result->setProperty("finiteAudible", finiteAudible); result->setProperty("noAudioDevice", noDevice);
        result->setProperty("error", renderError); if (exported) result->setProperty("wavSHA256", juce::SHA256(file).toHexString());
        renders.add(juce::var(result.release())); engine.releaseResources();
        check((tag + "_real_offline_export").toRawUTF8(), exported && finiteAudible && noDevice, renderError);
        if ((!real || !finiteAudible) && renderError.isEmpty()) renderError = "Real CPU NSF render failed/timed out, was non-finite/silent, or reported fallback/warnings";
        return exported && finiteAudible && noDevice;
    };
    // Generic gate creates its own unique child DIRECTLY under the prefix, not
    // under reportRoot: extra nesting would exceed Windows file/path limits.
    const bool accepted = runProjectMaterialRegistrationRealProjectGate(originalProject, outputPrefix, facts.variants, renderer, error,
        [&](const Registration::Plan& prepared,juce::String& prepareError)
        {
            expectedPlan=std::make_unique<Registration::Plan>(prepared);
            std::set<BindingKey> originalKeys;
            for(const auto& track:original.tracks)for(const auto& clip:track.clips)for(const auto& note:clip.notes)
                originalKeys.insert(BindingKey{track.id,clip.id,note.clipPartId,note.id});
            bool exact=prepared.ready&&prepared.recordings.size()==31&&prepared.bindings.size()==originalNotes;
            for(const auto& binding:expectedPlan->bindings)
                exact &= originalKeys.contains(key(binding.where))&&expectedBindings.emplace(key(binding.where),&binding).second;
            exact &= expectedBindings.size()==originalKeys.size();
            std::set<juce::String> canonicalIds;
            for(const auto& audio:prepared.document.audio)
            {
                exact &= canonicalIds.insert(audio.id).second;
                for(const auto& row:audio.rows)expectedCatalogEntries+=std::size_t(1+row.additionalAliases.size());
            }
            report->setProperty("expectedCanonicalVariants",int(prepared.document.audio.size()));
            report->setProperty("expectedCanonicalCatalogEntries",int(expectedCatalogEntries));
            check("prepared_canonical_document_covers_all_original_notes",exact,
                "physical="+juce::String(int(prepared.recordings.size()))+", uniqueVariants="+juce::String(int(prepared.document.audio.size()))+", bindings="+juce::String(int(prepared.bindings.size())));
            if(!exact)prepareError="Canonical registration plan does not cover every original note exactly once";
            return exact;
        });
    check("teio_before_adoption_and_cold_reopen_PCM_exact", accepted, error);
    check("all_three_real_render_phases_ran", phase == 3);
    check("model_config_bytes_unchanged", modelHashes(nsfModel) == resources);
    SampleSettings::setMaterialRoots(previousRoots);
    check("isolated_material_roots_restored", SampleSettings::materialRoots() == previousRoots);
    return finish();
}
}
