#pragma once
#include "../JapaneseAutoAnnotation.h"
#include "../MaterialAnnotationSession.h"
#include "../MaterialPronunciationCatalog.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>

namespace hachi
{
inline bool runJapaneseAutoAnnotationSmoke(const juce::File& output, const juce::File& audio,
    const juce::File& models, const juce::StringArray& correction = {})
{
    if (!output.isDirectory()) return false;
    const auto root = output.getChildFile("japanese-native-" + juce::Uuid().toString());
    if (root.createDirectory().failed()) return false;
    auto report = std::make_unique<juce::DynamicObject>();
    bool ok = true; juce::String error;
    const auto check = [&](const char* key, bool passed)
    { ok &= passed; report->setProperty(key, passed); std::cout << key << '=' << passed << std::endl; };
    const auto finish = [&]
    {
        report->setProperty("error", error); report->setProperty("ok", ok);
        const auto written = root.getChildFile("result.json").replaceWithText(juce::JSON::toString(juce::var(report.release()), true));
        std::cout << "annotation_output=" << root.getFullPathName() << "|error=" << error << std::endl;
        return ok && written;
    };
    const auto sourceHash = juce::SHA256(audio).toHexString();
    JapaneseAutoAnnotation::Result result;
    backend::OrtExecutionConfig execution;
    const bool testDml=juce::SystemStats::getEnvironmentVariable("HACHI_ANNOTATION_SMOKE_DML",{})=="1";
    execution.requested=testDml?backend::InferenceBackend::directML:backend::InferenceBackend::cpu; execution.intraOpThreads = 2;
    const bool inferred = JapaneseAutoAnnotation::preview(audio, models, execution, result, error, {},
        [](const juce::String& stage) { std::cout << "stage=" << stage << std::endl; }, correction);
    check("actual_native_annotation", inferred);
    if (!inferred) return finish();
    report->setProperty("recognizedTokens", result.recognizedTokens.joinIntoString(" "));
    report->setProperty("phones", result.sequence.phones.joinIntoString(" "));
    report->setProperty("asrBackend", result.asrBackend); report->setProperty("tifaBackend", result.alignmentBackend);
    report->setProperty("asrModelFingerprint", result.asrModelFingerprint);
    report->setProperty("tifaModelFingerprint", result.alignmentModelFingerprint);
    report->setProperty("input", audio.getFullPathName()); report->setProperty("sourceSHA256", sourceHash);
    report->setProperty("warnings", result.warnings.joinIntoString("\n"));
    std::cout << "recognized=" << result.recognizedTokens.joinIntoString(" ") << "|phones=" << result.sequence.phones.joinIntoString(" ") << std::endl;
    const auto provider=testDml?juce::String("directml"):juce::String("cpu");
    check("actual_requested_native_provider_not_Python", result.alignmentBackend.containsIgnoreCase(provider)
        && (correction.isEmpty() ? result.asrBackend.containsIgnoreCase(provider) : result.asrBackend == "explicit-user-phone-sequence"));
    check("dictionary_group_count", !result.rows.empty() && result.rows.size() == result.sequence.syllables.size());
    check("TIFA_no_skipped_phones", result.alignment.skippedPhones == 0);
    juce::Array<juce::var> intervals;
    double previous = 0;
    for (const auto& row : result.rows)
    {
        auto item = new juce::DynamicObject(); item->setProperty("alias", row.name);
        item->setProperty("start", row.regionStartSeconds); item->setProperty("end", row.regionEndSeconds);
        item->setProperty("P", row.alignmentSeconds); item->setProperty("C", row.regionStartSeconds + row.fixedDurationSeconds);
        intervals.add(juce::var(item));
        check("positive_ordered_source_interval", row.regionStartSeconds >= previous - 1.e-8 && row.regionEndSeconds > row.regionStartSeconds);
        const auto expectedC=row.pronunciationKind==PronunciationKind::transition?row.regionEndSeconds
            :row.alignmentSeconds+(row.regionEndSeconds-row.alignmentSeconds)/3.0;
        check("reference_fixed_C_and_no_manual_V_inference",std::abs(row.regionStartSeconds+row.fixedDurationSeconds-expectedC)<1.e-9
            &&row.alignmentSeconds>=row.regionStartSeconds&&row.alignmentSeconds<=row.regionEndSeconds
            && !row.simplifiedConsonant && !row.vowelSplitsExplicit);
        previous = row.regionEndSeconds;
    }
    report->setProperty("intervals", intervals);
    const auto folder = root.getChildFile("published"); folder.createDirectory();
    const auto copied = folder.getChildFile(audio.getFileName());
    check("independent_real_audio_copy", audio.copyFileTo(copied));
    const auto manifest = folder.getChildFile("otto.ini"); OttoMaterialIO::Document document;
    check("shared_codec_plan", OttoMaterialIO::replaceAudio(document, manifest, copied, result.rows, error, result.audioId));
    check("shared_codec_atomic_publish", ok && OttoMaterialIO::saveDocument(manifest, document, error));
    auto session = MaterialAnnotationSession::open(copied, manifest, error, result.audioId);
    check("cold_shared_editor_session", session && session->rows().size() == result.rows.size());
    if (session) for (std::size_t i = 0; i < result.rows.size() && i < session->rows().size(); ++i)
        check("cold_alias_and_exact_boundary", session->rows()[i].name == result.rows[i].name
            && session->rows()[i].regionStartSeconds == result.rows[i].regionStartSeconds
            && session->rows()[i].regionEndSeconds == result.rows[i].regionEndSeconds);
    ProjectModel project; const auto track = project.addTypedTrack(); std::vector<juce::String> ids;
    for (const auto& row : result.rows) ids.push_back(row.materialRegionId);
    const auto placed = project.addMaterialPronunciations(copied, result.rows, ids, 0, track, error);
    check("actual_material_placement", placed.isNotEmpty());
    const auto saved = root.getChildFile("annotated.hjpx"); ProjectModel cold;
    check("project_save_cold_reopen", placed.isNotEmpty() && project.save(saved, error) && cold.load(saved, error));
    check("original_and_published_PCM_bytes_unchanged", juce::SHA256(audio).toHexString() == sourceHash && juce::SHA256(copied).toHexString() == sourceHash);
    return finish();
}
}
