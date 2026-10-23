#pragma once
#include "../OtoWaveformEditorComponent.h"
#include "../MaterialAnnotationEditor.h"
#include "../OttoMaterialIO.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include <functional>
#include <iostream>

namespace hachi
{
// Offline component diagnostic only: no application window, audio device, or real
// user assets. Invoke from a diagnostic branch on JUCE's message thread.
inline bool runUnifiedMaterialEditorSmoke(const juce::File& output)
{
    int total = 0, failed = 0;
    juce::String log, error;
    const auto root = output.getChildFile("unified-material-" + juce::Uuid().toString());
    const auto check = [&](const juce::String& name, bool success)
    {
        ++total; if (!success) ++failed;
        log += name + "=" + (success ? "PASS" : "FAIL")
            + (success || error.isEmpty() ? juce::String{} : " | " + error) + "\n";
    };
    if (!root.createDirectory().wasOk()) return false;
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        root.getChildFile("verification.txt").replaceWithText("FAIL: requires JUCE message thread\n");
        return false;
    }
    const auto near = [](double a, double b) { return std::abs(a - b) < 1.e-8; };
    const auto finish = [&]
    {
        const bool written = root.getChildFile("verification.txt").replaceWithText(log);
        std::cout << "unified_material_passed=" << total - failed << "|total=" << total
                  << "|fixture=" << root.getFullPathName() << std::endl;
        if (failed) std::cerr << log << std::endl;
        return failed == 0 && written;
    };
    I18n strings; strings.setLanguage(I18n::Language::enUS);
    HachiLookAndFeel look;
    // Restore every mutable palette colour when the offline diagnostic exits.
    struct PaletteRestore
    {
        std::vector<std::pair<juce::Colour*, juce::Colour>> colours;
        PaletteRestore()
        {
            for (auto* p : {&Palette::background, &Palette::base, &Palette::panel, &Palette::panelRaised,
                &Palette::button, &Palette::buttonHover, &Palette::border, &Palette::graphBackground,
                &Palette::clipBackground, &Palette::grid, &Palette::beatGrid, &Palette::accent,
                &Palette::accentLight, &Palette::noteFill, &Palette::noteLight, &Palette::noteEdge,
                &Palette::pitchLine, &Palette::playhead, &Palette::text, &Palette::textMuted, &Palette::scrollThumb})
                colours.emplace_back(p, *p);
        }
        ~PaletteRestore() { for (const auto& p : colours) *p.first = p.second; }
    } restore;
    const auto audio = root.getChildFile("source.wav");
    const auto manifest = root.getChildFile("otto.ini");
    {
        juce::AudioBuffer<float> pcm(1, 96000);
        for (int i = 0; i < pcm.getNumSamples(); ++i)
            pcm.setSample(0, i, .15f * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 220 * i / 48000.0)));
        auto stream = audio.createOutputStream();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(stream
            ? format.createWriterFor(stream.release(), 48000, 1, 16, {}, 0) : nullptr);
        const bool ok = writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples());
        check("own_two_second_wav_fixture", ok);
        if (!ok) return finish();
    }
    juce::MemoryBlock sourceBefore; check("read_original_pcm", audio.loadFileAsData(sourceBefore));
    int callbacks = 0, validations = 0;
    std::vector<SampleRegionSetting> published;
    auto editor = OtoWaveformEditorComponent::forMaterial(audio, manifest, strings,
        [&](const auto& rows) { ++callbacks; published = rows; }, false, &error);
    check("actual_precise_factory", editor != nullptr);
    if (!editor) return finish();
    editor->setLookAndFeel(&look);
    editor->setVisible(true);
    check("bare_audio_starts_in_native_overview", editor->diagnosticMaterialOverview()
        && editor->diagnosticMaterialRows().empty() && !manifest.exists());

    const auto overviewOf = [](OtoWaveformEditorComponent& owner) -> MaterialAnnotationEditor*
    {
        for (int i = 0; i < owner.getNumChildComponents(); ++i)
            if (auto* c = dynamic_cast<MaterialAnnotationEditor*>(owner.getChildComponent(i))) return c;
        return nullptr;
    };
    const auto button = [](juce::Component& owner, const juce::String& text) -> juce::Button*
    {
        for (int i = 0; i < owner.getNumChildComponents(); ++i)
            if (auto* b = dynamic_cast<juce::Button*>(owner.getChildComponent(i)))
                if (b->getButtonText() == text) return b;
        return nullptr;
    };
    const auto click = [&](juce::Component& owner, const juce::String& text)
    {
        auto* b = button(owner, text);
        if (!b || !b->isVisible() || !b->isEnabled() || !b->onClick) return false;
        b->onClick(); return true;
    };
    const auto mouse = [](juce::Component& view, juce::Point<float> at, juce::Point<float> down, int clicks = 1)
    {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 0, 0, 0, 0, 0,
            &view, &view, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), clicks, at != down);
    };
    const auto visibleCombos = [](juce::Component& owner)
    {
        int count = 0;
        for (int i = 0; i < owner.getNumChildComponents(); ++i)
            if (auto* c = dynamic_cast<juce::ComboBox*>(owner.getChildComponent(i)); c && c->isVisible()) ++count;
        return count;
    };
    bool nativeDefault = false;
    for (int i = 0; i < editor->getNumChildComponents(); ++i)
        if (auto* c = dynamic_cast<juce::ComboBox*>(editor->getChildComponent(i)); c && c->isVisible())
            nativeDefault = c->getSelectedId() == 1 && c->getNumItems() == 4;
    check("native_default_and_segment_selector_hidden_in_overview", nativeDefault && visibleCombos(*editor) == 1);
    // Check only application-owned controls; JUCE's internal text/viewport
    // children may intentionally extend outside their clipped parent.
    const auto layout = [&](OtoWaveformEditorComponent& owner, const juce::String& tag)
    {
        bool fits = true; int controls = 0;
        const auto inspect = [&](juce::Component& parent)
        {
            for (int i = 0; i < parent.getNumChildComponents(); ++i)
            {
                auto* c = parent.getChildComponent(i);
                if (!c->isVisible()) continue;
                if (!(dynamic_cast<juce::Button*>(c) || dynamic_cast<juce::ComboBox*>(c)
                    || dynamic_cast<juce::TextEditor*>(c))) continue;
                ++controls;
                if (c->getWidth() <= 0 || c->getHeight() <= 0 || !parent.getLocalBounds().contains(c->getBounds()))
                {
                    fits = false;
                    log += tag + " bad_control=" + c->getName() + " bounds=" + c->getBounds().toString() + "\n";
                }
            }
        };
        inspect(owner);
        if (auto* overview = overviewOf(owner); overview && overview->isVisible()) inspect(*overview);
        check(tag + "_visible_control_bounds", fits && controls >= 4);
    };
    const auto snapshot = [&](OtoWaveformEditorComponent& owner, const juce::String& tag)
    {
        const auto image = owner.createComponentSnapshot(owner.getLocalBounds(), true, 1.f);
        const auto file = root.getChildFile(tag + ".png");
        auto stream = file.createOutputStream(); juce::PNGImageFormat png;
        check(tag + "_snapshot", image.isValid() && image.getWidth() == owner.getWidth()
            && stream && png.writeImageToStream(image, *stream));
    };
    // A real pending mouse selection is cancelled through the actual overview
    // button; no draft rows or files should be created by a cancelled range.
    auto* overview = overviewOf(*editor);
    check("factory_embeds_shared_overview", overview != nullptr);
    if (overview)
    {
        juce::Component* wave = nullptr;
        for (int i = 0; i < overview->getNumChildComponents(); ++i)
        {
            auto* c = overview->getChildComponent(i);
            if (!dynamic_cast<juce::Label*>(c) && !dynamic_cast<juce::TextEditor*>(c)
                && !dynamic_cast<juce::Button*>(c) && !dynamic_cast<juce::ListBox*>(c)) { wave = c; break; }
        }
        check("overview_waveform_available", wave != nullptr);
        if (wave)
        {
            const juce::Point<float> from(60.f, 60.f), to(230.f, 60.f);
            wave->mouseDown(mouse(*wave, from, from)); wave->mouseDrag(mouse(*wave, to, from));
            wave->mouseUp(mouse(*wave, to, from));
            check("pending_range_cancel_uses_real_button", click(*overview, "Remove current alias")
                && editor->diagnosticMaterialRows().empty() && !manifest.exists() && callbacks == 0);
        }
    }
    check("create_first_pronunciation_via_factory", editor->diagnosticCreateMaterialAlias(.1, .9, "ka", error));
    check("create_second_pronunciation_via_factory", editor->diagnosticCreateMaterialAlias(1.1, 1.9, "ki", error));
    auto rows = editor->diagnosticMaterialRows();
    check("two_distinct_authored_ranges", rows.size() == 2 && near(rows[0].regionStartSeconds, .1)
        && near(rows[0].regionEndSeconds, .9) && near(rows[1].regionStartSeconds, 1.1));
    if (rows.size() != 2) return finish();
    const auto first = rows[0].materialRegionId, second = rows[1].materialRegionId;
    check("same_range_additional_alias", editor->diagnosticCreateMaterialAlias(.1, .9, "ga", error)
        && editor->diagnosticMaterialRows().size() == 2
        && editor->diagnosticMaterialRows()[0].additionalAliases.contains("ga"));
    check("remove_additional_alias", editor->diagnosticRemoveMaterialAlias(first, "ga", error)
        && editor->diagnosticMaterialRows()[0].additionalAliases.isEmpty());
    check("create_temporary_alias", editor->diagnosticCreateMaterialAlias(.92, 1.02, "temporary", error));
    rows = editor->diagnosticMaterialRows();
    juce::String temporary;
    for (const auto& row : rows) if (row.name == "temporary") temporary = row.materialRegionId;
    check("remove_only_alias_removes_pronunciation", temporary.isNotEmpty()
        && editor->diagnosticRemoveMaterialAlias(temporary, "temporary", error)
        && editor->diagnosticMaterialRows().size() == 2);
    check("invalid_creation_changes_nothing", !editor->diagnosticCreateMaterialAlias(.5, .4, "bad", error)
        && editor->diagnosticMaterialRows().size() == 2);
    error.clear();

    for (const auto theme : {juce::String("light"), juce::String("dark")})
    {
        Palette::applyTheme(theme, restore.colours[11].second, restore.colours[12].second, restore.colours[13].second);
        look.refreshColours();
        editor->sendLookAndFeelChange();
        for (const int width : {900, 1000})
        {
            editor->setSize(width, 780);
            const auto tag = "precise-" + theme + "-" + juce::String(width) + "-overview";
            layout(*editor, tag); snapshot(*editor, tag);
        }
    }
    check("select_overview_row_does_not_open_detail", editor->selectMaterialPronunciation(first, false)
        && editor->diagnosticMaterialOverview());
    // Invoke the actual overview waveform's double-click handler, not a second
    // editor or parallel model. The point is inside the first pronunciation.
    bool doubled = false;
    if (overview)
        for (int i = 0; i < overview->getNumChildComponents(); ++i)
        {
            auto* c = overview->getChildComponent(i);
            if (dynamic_cast<juce::Label*>(c) || dynamic_cast<juce::TextEditor*>(c)
                || dynamic_cast<juce::Button*>(c) || dynamic_cast<juce::ListBox*>(c)) continue;
            const juce::Point<float> at(10.f + (c->getWidth() - 20.f) * .25f, 60.f);
            c->mouseDoubleClick(mouse(*c, at, at, 2)); doubled = !editor->diagnosticMaterialOverview(); break;
        }
    check("doubleclick_opens_existing_oto_detail", doubled);
    check("select_detail_via_factory", editor->selectMaterialPronunciation(first, true));
    check("native_segment_selectors_appear_in_detail_only", visibleCombos(*editor) == 3);
    check("multiple_pronunciations_lock_outer_range", editor->diagnosticOuterRangeLocked());
    const auto offset = editor->diagnosticParameterText(0), cutoff = editor->diagnosticParameterText(2);
    editor->diagnosticTypeParameter(0, "300"); editor->diagnosticTypeParameter(2, "200");
    editor->diagnosticDragOffsetTo(350);
    // Actual mouse cutoff drag (disabled handles must not be hittable).
    {
        auto& view = editor->diagnosticWaveformComponent();
        const float y = static_cast<float>(editor->diagnosticWaveformBounds().getCentreY());
        const juce::Point<float> from(editor->diagnosticXForTime(900), y), to(editor->diagnosticXForTime(700), y);
        view.mouseDown(mouse(view, from, from)); view.mouseDrag(mouse(view, to, from)); view.mouseUp(mouse(view, to, from));
    }
    check("typed_and_drag_outer_parameters_unchanged", editor->diagnosticParameterText(0) == offset
        && editor->diagnosticParameterText(2) == cutoff);
    check("named_vowel_split_opens_without_automatic_cut", click(*editor, "Split named vowel")
        && editor->diagnosticMaterialVowelItems().size() == 1);
    check("split_internal_vowel_requires_manual_names", editor->diagnosticConfirmMaterialVowelSplit("an", "ng"));
    editor->diagnosticDragMaterialBoundary(0, .43);
    check("commit_internal_boundary_on_return", editor->diagnosticMaterialBack());
    rows = editor->diagnosticMaterialRows();
    check("two_internal_vowels_and_dragged_shared_boundary", rows.size() == 2 && rows[0].segments.size() == 2
        && rows[0].segments[0].role == NativeSegmentRole::vowel && rows[0].segments[1].role == NativeSegmentRole::vowel
        && rows[0].vowelSplitsExplicit && rows[0].segments[0].alias == "an" && rows[0].segments[1].alias == "ng"
        && near(rows[0].segments[0].sourceEndSeconds, .43) && near(rows[0].segments[1].sourceStartSeconds, .43)
        && near(rows[0].regionStartSeconds, .1) && near(rows[0].regionEndSeconds, .9));
    check("return_preserves_second_pronunciation", rows.size() == 2 && rows[1].materialRegionId == second
        && rows[1].name == "ki" && near(rows[1].regionEndSeconds, 1.9));

    // Encode the real rows to compare all persistent metadata, IDs, and segments
    // after view-only mode switches, rather than comparing a few handpicked fields.
    const auto encodeRows = [&](const std::vector<SampleRegionSetting>& value)
    {
        OttoMaterialIO::Document doc;
        doc.audio.push_back({value.empty() ? juce::String("audio") : value.front().materialAudioId, "source.wav", value});
        juce::String text, problem;
        if (!OttoMaterialIO::encode(doc, text, problem)) { error = problem; return juce::String{}; }
        return text;
    };
    const auto beforeModes = encodeRows(rows);
    check("native_draft_encodes_before_view_switches", beforeModes.isNotEmpty());
    for (const int mode : {2, 3, 4, 1})
    {
        check("mode_" + juce::String(mode) + "_detail_selection", editor->selectMaterialPronunciation(first, true));
        editor->diagnosticMaterialMode(mode);
        for (const int width : {900, 1000})
        {
            editor->setSize(width, 780);
            layout(*editor, "precise-mode-" + juce::String(mode) + "-" + juce::String(width));
        }
        check("mode_" + juce::String(mode) + "_return", editor->diagnosticMaterialBack());
        check("mode_" + juce::String(mode) + "_no_native_data_loss", encodeRows(editor->diagnosticMaterialRows()) == beforeModes);
    }
    editor->setPublicationValidator([&](const auto& value, juce::String& reason)
    { ++validations; reason = "Intentional smoke preflight refusal"; return value.empty(); });
    editor->diagnosticSave();
    check("publication_preflight_prevents_first_write", validations == 1 && callbacks == 0 && !manifest.exists());
    editor->setPublicationValidator([&](const auto& value, juce::String&)
    { ++validations; return value.size() == 2; });
    editor->diagnosticSave();
    check("default_save_is_otto_and_callback_has_real_rows", manifest.existsAsFile() && callbacks == 1
        && validations == 2 && encodeRows(published) == beforeModes);
    check("no_legacy_annotation_files_written", !root.getChildFile("oto.ini").exists()
        && !root.getChildFile("oto.jie.ini").exists() && !root.getChildFile("oto4.ini").exists()
        && !root.getChildFile("otomou.ini").exists() && !SampleSettings::sidecarFor(audio).exists());
    const auto savedBytes = manifest.loadFileAsString();
    check("saved_manifest_nonempty", savedBytes.isNotEmpty());
    // Quick-wrench and precise routes instantiate exactly the same factory class.
    int quickCallbacks = 0;
    auto quick = OtoWaveformEditorComponent::forMaterial(audio, manifest, strings,
        [&](const auto&) { ++quickCallbacks; }, true, &error);
    check("actual_quick_factory_reopens_saved_rows", quick && quick->diagnosticMaterialOverview()
        && encodeRows(quick->diagnosticMaterialRows()) == beforeModes);
    if (quick)
    {
        quick->setLookAndFeel(&look); quick->setVisible(true);
        for (const auto theme : {juce::String("light"), juce::String("dark")})
        {
            Palette::applyTheme(theme, restore.colours[11].second, restore.colours[12].second, restore.colours[13].second);
            look.refreshColours();
            quick->sendLookAndFeelChange();
            for (const int width : {900, 1000})
            {
                quick->setSize(width, 780);
                const auto tag = "quick-" + theme + "-" + juce::String(width);
                layout(*quick, tag + "-overview"); snapshot(*quick, tag + "-overview");
                check(tag + "_detail_select", quick->selectMaterialPronunciation(first, true));
                layout(*quick, tag + "-detail"); snapshot(*quick, tag + "-detail");
                check(tag + "_back", quick->diagnosticMaterialBack());
                check(tag + "_no_view_loss", encodeRows(quick->diagnosticMaterialRows()) == beforeModes);
            }
        }
        check("quick_cancel_draft_created", quick->diagnosticCreateMaterialAlias(.94, 1.04, "cancelled", error));
        int quickPreflights = 0;
        quick->setPublicationValidator([&](const auto& value, juce::String& reason)
        { ++quickPreflights; reason = "Intentional existing-file preflight refusal"; return value.empty(); });
        quick->diagnosticSave();
        check("existing_manifest_preflight_preserves_exact_bytes", quickPreflights == 1
            && quickCallbacks == 0 && manifest.loadFileAsString() == savedBytes);
        quick.reset();
        check("closing_quick_draft_never_saves", quickCallbacks == 0 && manifest.loadFileAsString() == savedBytes);
    }
    check("precise_detail_for_light_dark_capture", editor->selectMaterialPronunciation(first, true));
    for (const auto theme : {juce::String("light"), juce::String("dark")})
    {
        Palette::applyTheme(theme, restore.colours[11].second, restore.colours[12].second, restore.colours[13].second);
        look.refreshColours();
        editor->sendLookAndFeelChange();
        for (const int width : {900, 1000})
        {
            editor->setSize(width, 780);
            snapshot(*editor, "precise-" + theme + "-" + juce::String(width) + "-detail");
        }
    }
    editor->diagnosticTypeParameter(4, "25");
    editor.reset();
    check("closing_precise_unsaved_detail_never_saves", callbacks == 1 && manifest.loadFileAsString() == savedBytes);
    auto reopened = OtoWaveformEditorComponent::forMaterial(audio, manifest, strings, {}, false, &error);
    check("reopen_keeps_saved_only_not_cancelled_changes", reopened && encodeRows(reopened->diagnosticMaterialRows()) == beforeModes);
    juce::MemoryBlock sourceAfter;
    check("source_wav_bytes_exactly_unchanged", audio.loadFileAsData(sourceAfter) && sourceAfter == sourceBefore);
    return finish();
}
}
