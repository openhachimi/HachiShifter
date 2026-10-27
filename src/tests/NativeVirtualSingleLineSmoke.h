#pragma once
#include "../PianoRollComponent.h"
#include "../AudioEngine.h"
#include "../NativeVirtualRenderPlan.h"
#include <iostream>
namespace hachi
{
// User rule: overlapping members of a virtual group expose exactly ONE pitch
// line (display == request), passing straight over the middle source.
inline bool runNativeVirtualSingleLineSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;
    struct Spec{const char* id;double start,length;float midi;};
    const std::array<Spec,3> specs{{{"a",0.0,1.1,60.f},{"b",0.9,0.5,63.f},{"c",1.3,1.0,67.f}}};
    for(const auto& s:specs)
    {
        ClipData clip;clip.id=juce::String("owner-")+s.id;clip.sourceFile=folder.getChildFile(juce::String(s.id)+".wav");clip.startSeconds=s.start;clip.durationSeconds=clip.sourceDurationSeconds=s.length;clip.sourceTimeMap={{0,0},{s.length,s.length}};
        {juce::AudioBuffer<float> pcm(1,int(48000*s.length)+10);for(int i=0;i<pcm.getNumSamples();++i)pcm.setSample(0,i,float(.1*std::sin(i*juce::MathConstants<double>::twoPi*220/48000.)));auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;}
        NoteData note;note.id=s.id;note.durationSeconds=s.length;note.midiNote=note.sourceMidiCenter=s.midi;note.sourcePitchMeasured=true;
        for(int f=0;f<=int(std::round(s.length/.005));++f)note.contour.push_back({f*.005,0,0,true});
        clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};ProjectModel model;model.resetDocument(seed);
    const auto plainTargets=[&](const ProjectData& data,int clip){return AudioEngine::diagnosticNativeTargetMidi(data,0,clip);};
    const auto aPlain=plainTargets(model.snapshot(),0),bPlain=plainTargets(model.snapshot(),1),cPlain=plainTargets(model.snapshot(),2);
    const auto id=model.createNativeVirtualConnection({"a","b","c"},false,error);check("virtual_route_without_auto_glide",id.isNotEmpty());if(id.isEmpty())return false;
    const auto data=model.snapshot();
    // Display authority.
    const auto lines=overlayNativeVirtualSingleLine(data.tracks[0],sharedPitchLines(data.tracks[0],data.nativeConnections),data.nativeVirtualGroups);
    const auto at=[&](const char* note,double t)->float{const auto* m=lines.memberFor(note);return m&&m->line?m->line->midiAt(t):-1.f;};
    bool single=true;double worst=0;
    for(double t=0.9;t<=1.4;t+=0.005)
    {
        std::vector<float> values;
        if(t<=1.1)values.push_back(at("a",t));values.push_back(at("b",t));if(t>=1.3)values.push_back(at("c",t));
        for(const auto v:values){single&=v>0;worst=std::max(worst,double(std::abs(v-values.front())));}
    }
    log+="display_worst_overlap_difference="+juce::String(worst,6)+"\n";
    check("overlapping_members_draw_one_identical_line",single&&worst<1.e-4);
    check("line_runs_from_a_through_b_to_c",std::abs(at("a",0.5)-60)<.01&&std::abs(at("b",1.2)-63)<.01&&std::abs(at("c",2.0)-67)<.01);
    // Request authority: absolute-time F0 equal across every overlapping member.
    const auto aT=plainTargets(data,0),bT=plainTargets(data,1),cT=plainTargets(data,2);
    const auto frame=[](const std::vector<float>& v,double local){const auto i=std::size_t(std::llround(local/.005));return i<v.size()?v[i]:-1.f;};
    bool request=true;double requestWorst=0;
    for(double t=0.905;t<=1.395;t+=0.005)
    {
        std::vector<float> values;
        if(t<1.1)values.push_back(frame(aT,t));values.push_back(frame(bT,t-0.9));if(t>1.3)values.push_back(frame(cT,t-1.3));
        // At the single step of the line (no auto glide) two frame grids may
        // sample opposite sides of the same instant; that is still one line.
        for(const auto v:values){request&=v>0;requestWorst=std::max(requestWorst,double(std::abs(v-values.front())));}
        bool mismatch=false;for(const auto v:values)mismatch|=std::abs(v-values.front())>.1f;
        if(mismatch||std::abs(t-0.95)<.003||std::abs(t-1.05)<.003||std::abs(t-1.2)<.003||std::abs(t-1.35)<.003)
        {log+="t="+juce::String(t,3);for(const auto v:values)log+=" "+juce::String(v,3);log+=" line="+juce::String(at("b",t),3)+"\n";}
    }
    log+="request_worst_overlap_difference="+juce::String(requestWorst,6)+"\n";
    check("render_requests_carry_one_F0_in_overlaps",request&&requestWorst<.02);
    check("non_overlapping_frames_keep_each_members_own_target",std::abs(frame(aT,0.4)-frame(aPlain,0.4))<1.e-4&&std::abs(frame(cT,0.8)-frame(cPlain,0.8))<1.e-4);
    check("request_equals_display_line",std::abs(frame(bT,0.3)-at("b",1.2))<.02);
    {
        // With automatic glide the line still never splits around its windows.
        ProjectModel glide;glide.resetDocument(data);glide.setNativeVirtualConnectionForNotes({"a","b","c"},true,true,error,.1);
        const auto g=glide.snapshot();const auto gl=overlayNativeVirtualSingleLine(g.tracks[0],sharedPitchLines(g.tracks[0],g.nativeConnections),g.nativeVirtualGroups);
        const auto gat=[&](const char* note,double t)->float{const auto* m=gl.memberFor(note);return m&&m->line?m->line->midiAt(t):-1.f;};
        double gw=0;for(double t=0.9;t<=1.4;t+=0.005){const auto b=gat("b",t);if(t<=1.1)gw=std::max(gw,double(std::abs(gat("a",t)-b)));if(t>=1.3)gw=std::max(gw,double(std::abs(gat("c",t)-b)));}
        log+="auto_glide_display_worst="+juce::String(gw,6)+"\n";
        check("auto_glide_route_also_draws_one_line",g.nativeVirtualGroups.size()==1&&g.nativeVirtualGroups[0].automaticPitch&&gw<1.e-3);
    }
    {
        auto varied=data;auto& a=varied.tracks[0].clips[0].notes[0];auto& b=varied.tracks[0].clips[1].notes[0];auto& c=varied.tracks[0].clips[2].notes[0];
        a.vibratoEnabled=b.vibratoEnabled=c.vibratoEnabled=true;a.vibratoDepthCents=45;b.vibratoDepthCents=80;c.vibratoDepthCents=30;
        a.vibratoLengthPercent=b.vibratoLengthPercent=c.vibratoLengthPercent=100;a.vibratoCycleMs=170;b.vibratoCycleMs=130;c.vibratoCycleMs=210;
        const auto v0=plainTargets(varied,0),v1=plainTargets(varied,1),v2=plainTargets(varied,2);double vibratoError=0;
        for(int frameIndex=181;frameIndex<220;++frameIndex)vibratoError=std::max(vibratoError,double(std::abs(v0[std::size_t(frameIndex)]-v1[std::size_t(frameIndex-180)])));
        for(int frameIndex=261;frameIndex<280;++frameIndex)vibratoError=std::max(vibratoError,double(std::abs(v1[std::size_t(frameIndex-180)]-v2[std::size_t(frameIndex-260)])));
        check("different_member_vibratos_do_not_create_competing_F0",vibratoError<.002);
        auto typed=data;typed.tracks[0].clips[1].notes[0].pronunciationKind=PronunciationKind::transition;
        typed.tracks[0].clips[1].notes[0].nativeTransition=NativeTransitionShape{1,.25,"manual-explicit-middle"};typed.tracks[0].clips[1].notes[0].midiNote=36;
        const auto bridge=plainTargets(typed,1);
        check("typed_bridge_is_crossed_by_a_to_c_reference_not_its_source_pitch",std::abs(frame(bridge,.3)-63.5f)<.05);
        typed.tracks[0].clips[1].notes[0].nativeUnpitched=true;for(auto& p:typed.tracks[0].clips[1].notes[0].contour)p.voiced=false;
        const auto uv=plainTargets(typed,1);check("unvoiced_bridge_remains_unvoiced",!uv.empty()&&std::all_of(uv.begin(),uv.end(),[](float midi){return midi==0;}));
        const auto common=overlayNativeVirtualSingleLine(data.tracks[0],sharedPitchLines(data.tracks[0],data.nativeConnections),data.nativeVirtualGroups);
        const auto* left=common.memberFor("a");const auto* middle=common.memberFor("b");const auto* right=common.memberFor("c");
        check("group_uses_one_shared_object_and_nonoverlapping_draw_ranges",left&&middle&&right&&left->line==middle->line&&middle->line==right->line&&left->drawTo<=middle->drawFrom+1.e-9&&middle->drawTo<=right->drawFrom+1.e-9);
        check("last_request_frame_retains_the_common_final_pitch",std::abs(cT.back()-67.f)<.002);
    }
    model.setNativeVirtualConnectionEnabled(id,false,error);
    const auto off=model.snapshot();
    check("disabled_group_restores_original_independent_requests",plainTargets(off,0)==aPlain&&plainTargets(off,1)==bPlain&&plainTargets(off,2)==cPlain);
    check("project_sources_ids_and_contours_unchanged",off.tracks[0].clips.size()==3&&off.tracks[0].clips[1].notes[0].contour.size()==seed.tracks[0].clips[1].notes[0].contour.size()&&off.tracks[0].clips[1].notes[0].midiNote==63.f);
    ProjectModel visual;visual.resetDocument(data);const bool typed=visual.setNativeNoteTransition("b",.25,error);
    check("typed_overlap_demo_saved_for_actual_UI_review",typed&&visual.save(folder.getChildFile("single-target-bridge.hjpx"),error));
    {
        AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(visual.snapshot());
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+15000.;while(juce::Time::getMillisecondCounterHiRes()<deadline){const auto progress=engine.renderProgress();if(!progress||*progress>=.999)break;juce::Thread::sleep(5);}
        const auto backends=engine.activeRenderBackends();log+="actual_typed_bridge_backend="+backends+"\n";
        check("actual_render_does_not_bypass_changed_bridge_target",engine.activeRenderWarnings().isEmpty()&&backends.containsIgnoreCase("world"));
        check("actual_full_single_target_export_succeeds",engine.exportWav(folder.getChildFile("single-target-bridge.wav"),error));
    }
    I18n strings;PianoRollComponent roll(visual,strings);roll.setFocusedTrack("native");roll.setPixelsPerSecond(400);roll.setRowHeight(22);roll.setBounds(0,0,1200,1800);roll.diagnosticRefresh();
    const auto picture=roll.createComponentSnapshot({0,int(roll.diagnosticYForMidi(67))-50,1200,330});
    {auto output=folder.getChildFile("one-line-overlapping-bridge.png").createOutputStream();juce::PNGImageFormat png;check("actual_roll_overlap_screenshot_written",output&&png.writeImageToStream(picture,*output));}
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
