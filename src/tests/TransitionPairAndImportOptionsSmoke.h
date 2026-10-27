#pragma once
#include "../MelodyneTrackSelector.h"
#include "../NativeVirtualRenderPlan.h"
#include "../AudioEngine.h"
#include <iostream>
namespace hachi
{
inline bool runTransitionPairAndImportOptionsSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String log,error;
    const auto check=[&](const juce::String& key,bool pass){ok&=pass;log<<key<<"="<<(pass?"PASS":"FAIL")<<" | "<<error<<"\n";error.clear();std::cout<<key<<"="<<pass<<std::endl;};
    ProjectData seed;TrackData track;track.id="native";track.name="Native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;
    for(int i=0;i<3;++i)
    {
        ClipData clip;clip.id="owner-"+juce::String(i);clip.sourceFile=folder.getChildFile("source-"+juce::String(i)+".wav");clip.startSeconds=i==0?0:i==1?.9:1.3;clip.durationSeconds=clip.sourceDurationSeconds=i==1?.5:1.1;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        {juce::AudioBuffer<float> pcm(1,96000);for(int s=0;s<96000;++s)pcm.setSample(0,s,float(.08*std::sin(juce::MathConstants<double>::twoPi*(220+i*30)*s/48000.)));auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;}
        NoteData note;note.id=juce::String::charToString(juce::juce_wchar('a'+i));note.durationSeconds=clip.durationSeconds;note.midiNote=note.sourceMidiCenter=i==0?60.f:i==1?36.f:67.f;note.sourcePitchMeasured=true;
        for(int f=0;f<=int(std::ceil(note.durationSeconds/.005));++f)note.contour.push_back({f*.005,0,0,true});
        if(i==1){note.pronunciationKind=PronunciationKind::transition;note.nativeTransition=NativeTransitionShape{1,.25,"manual-test-boundary"};}
        clip.notes={note};track.clips.push_back(clip);
    }seed.tracks={track};
    for(bool reverse:{false,true})
    {
        ProjectModel model;model.resetDocument(seed);const auto tag=juce::String(reverse?"bridge_to_c_first":"a_to_bridge_first");
        check(tag+"_pair_connects",model.setNativeVirtualConnectionForNotes(reverse?std::vector<juce::String>{"b","c"}:std::vector<juce::String>{"a","b"},true,false,error));
        const auto paired=model.snapshot();const auto midi=AudioEngine::diagnosticNativeTargetMidi(paired,0,1);const float held=reverse?67.f:60.f;
        check(tag+"_bridge_never_becomes_third_target",!midi.empty()&&std::all_of(midi.begin(),midi.end(),[&](float value){return value<=0||std::abs(value-held)<.001f;}));
        check(tag+"_other_pair_extends_same_route",model.setNativeVirtualConnectionForNotes(reverse?std::vector<juce::String>{"a","b"}:std::vector<juce::String>{"b","c"},true,false,error)&&model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"a","b","c"});
        const auto file=folder.getChildFile(tag+".hjpx");ProjectModel cold;check(tag+"_pair_route_cold_roundtrip",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"a","b","c"});
    }
    I18n strings;I18n::clearMissingTranslationKeys();
    for(int lang=0;lang<5;++lang)
    {
        strings.setLanguage(I18n::Language(lang));MelodyneImportOptimizationOptions initial;MelodyneTrackSelector selector(seed.tracks,strings,initial);const auto defaults=selector.getOptions();
        check("import_defaults_"+juce::String(lang),defaults.registerTransitions&&!defaults.virtualConnections&&!defaults.virtualPitch&&!defaults.mergeConsonants);
        auto choice=initial;choice.virtualPitch=true;selector.setOptions(choice);check("pitch_choice_preserved_but_disabled_without_connections_"+juce::String(lang),selector.getOptions().virtualPitch&&!selector.isVirtualPitchEnabled());
        choice.virtualConnections=true;choice.registerTransitions=false;choice.mergeConsonants=true;selector.setOptions(choice);const auto actual=selector.getOptions();check("same_real_controls_capture_all_choices_"+juce::String(lang),actual.virtualPitch&&actual.virtualConnections&&!actual.registerTransitions&&actual.mergeConsonants&&selector.isVirtualPitchEnabled());
        juce::PNGImageFormat png;auto image=selector.createComponentSnapshot(selector.getLocalBounds(),true,1);auto stream=folder.getChildFile("import-options-language-"+juce::String(lang)+".png").createOutputStream();check("real_import_options_picture_"+juce::String(lang),stream&&png.writeImageToStream(image,*stream));
    }
    check("all_import_options_in_file_catalog",I18n::missingTranslationKeys().isEmpty());
    strings.setLanguage(I18n::Language::zhCN);check("dangling_bridge_warning_is_file_translated",I18n::message("Select ordinary endpoints around a transition, not a dangling bridge")!=juce::String("Select ordinary endpoints around a transition, not a dangling bridge"));
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
