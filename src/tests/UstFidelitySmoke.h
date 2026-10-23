#pragma once
#include "../backend/UstExchange.h"
#include "../SampleSettings.h"
#include "../VoicebankSettingsComponent.h"
#include "../AudioEngine.h"
#include <iostream>
namespace hachi
{
inline bool ustFidelitySmoke(const juce::File& folder,const juce::File& probe={})
{
    using backend::LegacyTextCodec;folder.createDirectory();bool ok=true;int checks=0;juce::Array<juce::var> failures;
    const auto check=[&](const juce::String& name,bool pass){++checks;ok&=pass;if(!pass)failures.add(name);std::cout<<name<<"="<<pass<<std::endl;};
    const auto bytes=[](const juce::File& f){juce::MemoryBlock b;f.loadFileAsData(b);return b;};
    juce::String error;juce::StringArray warnings;
    // Real sample paths disambiguate legacy Chinese/Japanese bank encodings.
    for(int cp:{932,936,950,54936,65001})
    {
        const auto tag=juce::String(cp);const auto bank=folder.getChildFile("bank-"+tag);bank.createDirectory();
        const auto name=juce::String::fromUTF8(cp==932?"あいう":cp==950?"國語聲音":"中文声音");
        const auto wav=bank.getChildFile(name+".wav");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(wav.createOutputStream().release(),44100,1,16,{},0));juce::AudioBuffer<float> audio(1,4410);audio.clear();if(writer)writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples());writer.reset();
        const auto original=juce::String("; opaque comment\r\n")+name+".wav="+name+",000.000,100.00,-200.0,080.0,20.0,extra=keep\r\n\r\n";
        juce::MemoryBlock encoded;check(tag+"_fixture_encoded",LegacyTextCodec::encode(original,cp,cp==65001,encoded,error));const auto oto=bank.getChildFile("oto.ini");oto.replaceWithData(encoded.getData(),encoded.getSize());
        auto entries=SampleSettings::loadVoicebankOto(bank,warnings);check(tag+"_auto_read_filename",entries.size()==1&&entries[0].sourceName==name+".wav");if(entries.empty())continue;
        check(tag+"_read_encoding",entries[0].sourceEncoding==LegacyTextCodec::name(cp)||cp==54936&&entries[0].sourceEncoding=="GBK");
        auto same=entries[0];check(tag+"_unchanged_write",SampleSettings::updateVoicebankOtoEntry(entries[0],same,error)&&bytes(oto)==encoded);
        auto changed=entries[0];changed.offsetMs=12.5;check(tag+"_edit_saved",SampleSettings::updateVoicebankOtoEntry(entries[0],changed,error));
        auto doc=LegacyTextCodec::read(oto,error);check(tag+"_unknown_numeric_crlf_preserved",doc&&doc->text==original.replace("000.000","12.5"));
        entries=SampleSettings::loadVoicebankOto(bank,warnings);const auto before=bytes(oto);changed=entries[0];changed.alias=juce::String::fromUTF8("歌😀");
        if(cp!=65001&&cp!=54936)check(tag+"_unrepresentable_rejected",!SampleSettings::updateVoicebankOtoEntry(entries[0],changed,error)&&bytes(oto)==before);
        auto stale=entries[0];oto.appendText("; concurrent\r\n",false,false,nullptr);const auto concurrent=bytes(oto);changed=stale;changed.offsetMs=55;
        check(tag+"_external_edit_rejected",!SampleSettings::updateVoicebankOtoEntry(stale,changed,error)&&bytes(oto)==concurrent);
        check(tag+"_explicit_directory_preference",LegacyTextCodec::setDirectoryCodePage(bank,cp,error)&&LegacyTextCodec::directoryCodePage(bank)==cp);
        entries=SampleSettings::loadVoicebankOto(bank,warnings);if(!entries.empty())
        {changed=entries[0];changed.consonantMs=101;check(tag+"_jie_independent_save",SampleSettings::updateJieVoicebankOtoEntry(entries[0],changed,error)&&bytes(oto)==concurrent);auto jie=LegacyTextCodec::read(bank.getChildFile("oto.jie.ini"),error);check(tag+"_jie_preserves_encoding",jie&&jie->codePage==cp);}
        // prefix.map and the renderer use the same directory decoding choice.
        juce::MemoryBlock prefix;LegacyTextCodec::encode("C4\t\t"+name+"\r\n",cp,cp==65001,prefix,error);bank.getChildFile("prefix.map").replaceWithData(prefix.getData(),prefix.getSize());
        const auto timing=backend::UtauRenderer::sampleTiming(bank,name,60);check(tag+"_renderer_resolves_same_alias",timing.has_value());
        if(cp==936){VoicebankSettingsComponent panel(bank,false,false);panel.setSize(1040,600);juce::PNGImageFormat png;if(auto out=folder.getChildFile("voicebank-encoding.png").createOutputStream())png.writeImageToStream(panel.createComponentSnapshot(panel.getLocalBounds()),*out);}
    }
    const auto original=juce::String::fromUTF8("; preserved prefix\r\n[#VERSION]\r\nUST Version1.2\r\n[#SETTING]\r\nTempo=120.000\r\nProjectName=保真あ\r\nVoiceDir=%VOICE%voice\r\nFlags=g0\r\nMode2=False\r\nTool1=do-not-execute.exe\r\nVendor=foo=bar\r\n[#VENDOR]\r\nBlob=x=y\r\n[#0000]\r\nLength=120\r\nLyric=R\r\nNoteNum=60\r\nRestData=keep\r\n[#0001]\r\nLength=480\r\nLyric=あ\r\nNoteNum=60\r\nPreUtterance=\r\nVoiceOverlap=0\r\nVelocity=000\r\nIntensity=100.00\r\nModulation=37\r\nSTP=12.5\r\nPBStart=-10\r\nPitchBend=0,100,-100,50\r\nPBS=-80;20\r\nPBW=100\r\nPBY=0\r\nVBR=50,180,40,20,20,0,0,77\r\nEnvelope=0,5,35,0,100,100,0,%,5\r\nMystery=foo=bar\r\nEmpty=\r\n[#0002]\r\nLength=480\r\nLyric=i\r\nNoteNum=64\r\nPreUtterance=0\r\nVoiceOverlap=\r\n[#0003]\r\nLength=240\r\nLyric=R\r\nNoteNum=60\r\n[#TRACKEND]\r\nPost=keep\r\n");
    for(int cp:{932,936,65001})
    {
        const auto tag=juce::String(cp);const auto file=folder.getChildFile("roundtrip-"+tag+".ust"),out=folder.getChildFile("export-"+tag+".ust");juce::MemoryBlock encoded;check("ust_"+tag+"_encode",LegacyTextCodec::encode(original,cp,cp==65001,encoded,error));file.replaceWithData(encoded.getData(),encoded.getSize());
        ProjectModel model;check("ust_"+tag+"_import",model.addUstFile(file,error,warnings,ProjectModel::UstImportMode::replaceProject,nullptr,cp));auto data=model.snapshot();if(data.tracks.empty())continue;
        const auto& clip=data.tracks[0].clips[0];const auto& n=clip.notes[0];check("ust_"+tag+"_unknown_section_not_note",clip.notes.size()==2);
        check("ust_"+tag+"_rest_positions",std::abs(n.startSeconds-.125)<1.e-8&&std::abs(clip.durationSeconds-1.375)<1.e-8);
        check("ust_"+tag+"_absent_empty_zero",!n.utauPreutteranceOverrideEnabled&&n.utauOverlapOverrideEnabled&&n.utauOverlapSeconds==0&&clip.notes[1].utauPreutteranceOverrideEnabled);
        check("ust_"+tag+"_envelope_linear_amplitude",n.amplitudeEnvelope.size()==6&&n.amplitudeEnvelope[1].linearToNext);
        check("ust_"+tag+"_modulation_stp",n.utauModulationPercent==37&&n.utauStpSeconds==.0125);
        check("ust_"+tag+"_mode1_units",std::abs(evaluatePitchCurve(n.pitchControlPoints,-.01+5*60.0/(480*120))-61)<1.e-4&&!n.vibratoEnabled);
        check("ust_"+tag+"_mode1_returns_to_note",std::abs(evaluatePitchCurve(n.pitchControlPoints,.08)-60)<1.e-4);
        check("ust_"+tag+"_byte_identical_export",model.exportUst(out,data.tracks[0].id,error,warnings)&&bytes(out)==encoded);
        const auto hjpx=file.withFileExtension("hjpx");ProjectModel loaded;check("ust_"+tag+"_hjpx_saved_loaded",model.save(hjpx,error)&&loaded.load(hjpx,error));
        check("ust_"+tag+"_reopened_byte_identical",loaded.exportUst(out,data.tracks[0].id,error,warnings)&&bytes(out)==encoded);
        data=loaded.snapshot();data.tracks[0].clips[0].notes[0].label="u";model.resetDocument(data);check("ust_"+tag+"_lyric_edit_export",model.exportUst(out,data.tracks[0].id,error,warnings));
        auto doc=LegacyTextCodec::read(out,error,cp,false);check("ust_"+tag+"_only_lyric_changed",doc&&doc->text==original.replace(juce::String::fromUTF8("Lyric=あ"),"Lyric=u"));
        data.tracks[0].clips[0].notes[0].utauPreutteranceOverrideEnabled=true;data.tracks[0].clips[0].notes[0].utauPreutteranceSeconds=0;model.resetDocument(data);model.exportUst(out,data.tracks[0].id,error,warnings);doc=LegacyTextCodec::read(out,error,cp,false);check("ust_"+tag+"_explicit_zero_after_edit",doc&&doc->text.contains("PreUtterance=0\r\n"));
        data.tracks[0].clips[0].notes[1].startSeconds=.3;model.resetDocument(data);const auto before=bytes(out);check("ust_"+tag+"_overlap_atomic_failure",!model.exportUst(out,data.tracks[0].id,error,warnings)&&bytes(out)==before);
    }
    juce::StringArray parseWarnings;const auto parsed=backend::UstImporter::parse(original.replace("Mode2=False","Mode2=True"),parseWarnings);check("mode2_keeps_inactive_mode1_values",parsed.mode2&&parsed.notes[1].mode1Cents.size()==4&&parsed.notes[1].hasPitchBend);
    auto f=folder.getChildFile("mode2.ust");f.replaceWithText(original.replace("Mode2=False","Mode2=True"),false,false,nullptr);ProjectModel model;model.addUstFile(f,error,warnings);auto data=model.snapshot();
    if(!data.tracks.empty())
    {
        auto& n=data.tracks[0].clips[0].notes[0];check("mode2_active_vibrato_pitch",n.vibratoEnabled&&std::abs(evaluatePitchCurve(n.pitchControlPoints,-.08)-62)<.001);auto before=n;n.utauModulationPercent+=1;check("modulation_invalidates_raw_audio",AudioEngine::utauNoteAudioHash(n)!=AudioEngine::utauNoteAudioHash(before));
        n.label="edited";n.pitchControlPoints={{-.02,61,PitchCurveShape::linear},{.1,62,PitchCurveShape::smooth}};model.resetDocument(data);const auto out=folder.getChildFile("mode2-edited.ust");check("mode2_edited_export",model.exportUst(out,data.tracks[0].id,error,warnings));auto round=backend::UstImporter::read(out,error,warnings);check("mode2_edited_reimport",round&&round->notes[1].pitchStartMs==-20&&round->notes[1].pitchStartTenths==10&&round->notes[1].widthsMs[0]==120&&round->notes[1].mode1Cents.size()==4);
        data.tracks[0].clips[0].notes[1].startSeconds+=.5;data.tracks[0].clips[0].durationSeconds+=.5;model.resetDocument(data);check("layout_change_rebuilds_rests",model.exportUst(out,data.tracks[0].id,error,warnings));ProjectModel moved;check("layout_change_reimport",moved.addUstFile(out,error,warnings));if(!moved.snapshot().tracks.empty())check("layout_positions_retained",std::abs(moved.snapshot().tracks[0].clips[0].notes[1].startSeconds-1.125)<1.e-6);
    }
    {
        backend::UstNote previous,current;previous.noteNum=57;previous.lyric="a";previous.lengthTicks=480;previous.mode1Cents.assign(98,100);current.noteNum=60;current.mode1StartMs=-5;current.mode1Cents={200,0};NoteData note;note.midiNote=60;
        backend::ustexchange::applyMode1(note,current,120,&previous);
        check("mode1_inherits_previous_before_own_start",std::abs(evaluatePitchCurve(note.pitchControlPoints,-.02)-58)<.001);
        check("mode1_own_start_takes_priority",std::abs(evaluatePitchCurve(note.pitchControlPoints,-.005)-62)<.001);
        auto tempoData=model.snapshot();tempoData.bpm=150;ProjectModel added;added.resetDocument(tempoData);const auto sourceFile=folder.getChildFile("roundtrip-65001.ust");check("add_track_with_existing_tempo",added.addUstFile(sourceFile,error,warnings));const auto id=added.snapshot().tracks.back().id;const auto out=folder.getChildFile("adopted-tempo.ust");
        check("export_uses_adopted_project_tempo",added.exportUst(out,id,error,warnings)&&backend::UstImporter::read(out,error,warnings)->tempo==150);
    }
    {
        const juce::String prefix="[#SETTING]\r\nTempo=120\r\nVendor=";juce::MemoryBlock raw(prefix.toRawUTF8(),prefix.getNumBytesAsUTF8());const unsigned char duplicateBytes[]{0x87,0x90};raw.append(duplicateBytes,2);
        const juce::String tail="\r\n[#0000]\r\nLength=480\r\nLyric=a\r\nNoteNum=60\r\n[#TRACKEND]\r\n";raw.append(tail.toRawUTF8(),tail.getNumBytesAsUTF8());
        const auto source=folder.getChildFile("sjis-duplicate.ust"),out=folder.getChildFile("sjis-duplicate-out.ust"),save=folder.getChildFile("sjis-duplicate.hjpx");source.replaceWithData(raw.getData(),raw.getSize());ProjectModel duplicate,reopened;
        check("cp932_duplicate_byte_spelling_import",duplicate.addUstFile(source,error,warnings,ProjectModel::UstImportMode::replaceProject,nullptr,932));
        const auto d=duplicate.snapshot();if(!d.tracks.empty()){check("cp932_duplicate_byte_spelling_export",duplicate.exportUst(out,d.tracks[0].id,error,warnings)&&bytes(out)==raw);check("cp932_duplicate_bytes_survive_hjpx",duplicate.save(save,error)&&reopened.load(save,error)&&reopened.exportUst(out,d.tracks[0].id,error,warnings)&&bytes(out)==raw);}
        juce::MemoryBlock extended;check("gb18030_four_byte_unicode",LegacyTextCodec::encode(juce::String::fromUTF8("中文😀"),54936,false,extended,error)&&LegacyTextCodec::decodeBytes(extended,54936).value_or(juce::String())==juce::String::fromUTF8("中文😀"));
    }
    if(probe.existsAsFile())
    {
        backend::UtauRenderRequest request;request.voicebankDirectory=folder.getChildFile("bank-936");request.resamplerExecutable=probe;request.targetDurationSeconds=.2;
        backend::UtauNoteRenderSpec note;note.alias=juce::String::fromUTF8("中文声音");note.durationSeconds=.1;note.modulationPercent=37;request.notes.push_back(note);
        const auto result=backend::UtauRenderer::render(request);const auto args=juce::StringArray::fromLines(juce::File(probe.getFullPathName()+".args.txt").loadFileAsString());
        check("modulation_reaches_external_resampler_argument",result.buffer.getNumSamples()>0&&args.size()>=11&&std::abs(args[10].getDoubleValue()-37)<.0001);
    }
    auto* report=new juce::DynamicObject();report->setProperty("passed",ok);report->setProperty("checks",checks);report->setProperty("failures",failures);report->setProperty("last_error",error);folder.getChildFile("ust-encoding-validation.json").replaceWithText(juce::JSON::toString(juce::var(report),false));return ok;
}
}