#pragma once
#include "../ProjectModel.h"
#include "UstImporter.h"
#include "UstText.h"
#include "LegacyTextCodec.h"
#include <set>
#include <cmath>
namespace hachi::backend::ustexchange
{
inline juce::String number(double v){if(std::abs(v)<.0000005)v=0;return juce::String(v,6).trimCharactersAtEnd("0").trimCharactersAtEnd(".");}
inline juce::String csv(std::initializer_list<double> values){juce::StringArray a;for(auto x:values)a.add(number(x));return a.joinIntoString(",");}
inline void applyMode1(NoteData& note,const UstNote& source,double bpm,const UstNote* previous=nullptr)
{
    if(source.mode1Cents.empty()&&(!previous||previous->mode1Cents.empty()))return;
    const double step=5.0*60.0/(480.0*bpm),start=source.mode1StartMs/1000.0,epsilon=1.e-7;
    std::vector<double> times{0};
    const auto addTimes=[&](const UstNote& n,double origin)
    {times.push_back(origin-epsilon);for(size_t i=0;i<n.mode1Cents.size();++i)times.push_back(origin+i*step);times.push_back(origin+n.mode1Cents.size()*step-epsilon);times.push_back(origin+n.mode1Cents.size()*step);};
    if(!source.mode1Cents.empty())addTimes(source,start);
    double prevOrigin=0;
    if(previous&&!previous->mode1Cents.empty())
    {
        prevOrigin=previous->mode1StartMs/1000.0-previous->lengthTicks*60.0/(480*bpm);
        for(size_t i=0;i<=previous->mode1Cents.size();++i){const auto t=prevOrigin+i*step;if(t<0&&(source.mode1Cents.empty()||t<start))times.push_back(t);}
        if(prevOrigin<0)times.push_back(prevOrigin-epsilon);
        const auto end=prevOrigin+previous->mode1Cents.size()*step;if(end<=0){times.push_back(end-epsilon);times.push_back(end);}
        times.push_back(-epsilon);
    }
    const auto valueAt=[&](double time)
    {
        if(!source.mode1Cents.empty()&&time>=start)
        {const auto pos=(time-start)/step;const auto i=(size_t)std::floor(pos+1.e-12);if(i>=source.mode1Cents.size())return note.midiNote;const auto j=std::min(i+1,source.mode1Cents.size()-1);return note.midiNote+(float)((source.mode1Cents[i]+(source.mode1Cents[j]-source.mode1Cents[i])*(pos-i))*.01);}
        if(previous&&time<0&&time>=prevOrigin&&!previous->mode1Cents.empty())
        {const auto pos=(time-prevOrigin)/step;const auto i=(size_t)std::floor(pos+1.e-12);if(i<previous->mode1Cents.size()){const auto j=std::min(i+1,previous->mode1Cents.size()-1);const auto base=previous->isRest()?note.midiNote:(float)previous->noteNum;return base+(float)((previous->mode1Cents[i]+(previous->mode1Cents[j]-previous->mode1Cents[i])*(pos-i))*.01);}}
        return note.midiNote;
    };
    std::sort(times.begin(),times.end());times.erase(std::unique(times.begin(),times.end()),times.end());note.pitchControlPoints.clear();
    for(auto t:times)note.pitchControlPoints.push_back({t,valueAt(t),PitchCurveShape::linear});
}
inline juce::var noteState(const NoteData& note,const ProjectData& project,double clipStart,bool mode2)
{
    auto* o=new juce::DynamicObject();juce::var result(o);
    const double start=clipStart+note.startSeconds;
    const auto tick=[&](double s){return (juce::int64)std::llround(project.quarterPositionForSeconds(s)*480.0);};
    const auto put=[&](const char* key,const juce::String& v){o->setProperty(key,v);};
    put("_start",juce::String(tick(start)));put("Length",juce::String(tick(start+note.durationSeconds)-tick(start)));
    put("Lyric",note.label);put("NoteNum",juce::String((int)std::lround(note.midiNote)));put("Flags",note.utauFlags);
    put("Velocity",note.utauConsonantVelocity==inheritedUtauConsonantVelocity?juce::String():juce::String(note.utauConsonantVelocity));
    put("Intensity",number(note.gain*100.0));put("Modulation",number(note.utauModulationPercent));put("STP",number(note.utauStpSeconds*1000));
    put("PreUtterance",note.utauPreutteranceOverrideEnabled?number(note.utauPreutteranceSeconds*1000):juce::String());
    put("VoiceOverlap",note.utauOverlapOverrideEnabled?number(note.utauOverlapSeconds*1000):juce::String());
    put("VBR",note.vibratoEnabled?csv({note.vibratoLengthPercent,note.vibratoCycleMs,note.vibratoDepthCents,note.vibratoFadeInPercent,note.vibratoFadeOutPercent,note.vibratoPhasePercent,note.vibratoOffsetPercent,0}):"0,0,0,0,0,0,0,0");
    const auto& points=note.pitchControlPoints;
    if(mode2)
    {
        // UST Mode 2 stores the shape on each outgoing interval.
        auto sampled=points;
        if(sampled.empty())sampled={{0,note.midiNote,PitchCurveShape::linear},{note.durationSeconds,note.midiNote,PitchCurveShape::linear}};
        if(sampled.size()==1)sampled.push_back({std::max(note.durationSeconds,sampled[0].timeSeconds+.001),sampled[0].targetMidi,PitchCurveShape::linear});
        // Custom Bezier/natural curves have no UST equivalent; sample those intervals.
        std::vector<PitchCurveEditPoint> out{sampled.front()};
        for(size_t i=1;i<sampled.size();++i)
        {
            const auto& end=sampled[i];const auto& begin=sampled[i-1];
            if(end.shape==PitchCurveShape::customBezier||end.shape==PitchCurveShape::natural)
            {const int count=juce::jlimit(1,10000,(int)std::ceil((end.timeSeconds-begin.timeSeconds)/.005));for(int k=1;k<=count;++k){const auto t=begin.timeSeconds+(end.timeSeconds-begin.timeSeconds)*k/count;out.push_back({t,evaluatePitchCurve(sampled,t),PitchCurveShape::linear});}}
            else out.push_back(end);
        }
        put("PBS",number(out[0].timeSeconds*1000)+";"+number((out[0].targetMidi-note.midiNote)*10));
        juce::StringArray widths,pitches,shapes;
        for(size_t i=1;i<out.size();++i)
        {widths.add(number((out[i].timeSeconds-out[i-1].timeSeconds)*1000));pitches.add(number((out[i].targetMidi-note.midiNote)*10));shapes.add(out[i].shape==PitchCurveShape::smooth?"":out[i].shape==PitchCurveShape::easeIn?"j":out[i].shape==PitchCurveShape::easeOut?"r":"s");}
        put("PBW",widths.joinIntoString(","));put("PBY",pitches.joinIntoString(","));put("PBM",shapes.joinIntoString(","));
    }
    else
    {
        const auto step=5.0*60.0/(480.0*project.tempoAtSeconds(start));
        const auto begin=points.empty()?0.0:std::min(0.0,points.front().timeSeconds);
        const auto end=points.empty()?note.durationSeconds:std::max(note.durationSeconds,points.back().timeSeconds);
        juce::StringArray values;const int count=juce::jlimit(1,1000000,(int)std::ceil((end-begin)/step)+1);
        for(int i=0;i<count;++i)values.add(number((points.empty()?0:(evaluatePitchCurve(points,begin+i*step)-note.midiNote)*100)));
        put("PBStart",number(begin*1000));put("PitchBend",values.joinIntoString(","));
    }
    // An unchanged source envelope is kept verbatim. A changed shape is checked below.
    juce::Array<juce::var> envelope;
    for(const auto& p:note.amplitudeEnvelope){juce::Array<juce::var> a;a.add(number(p.timeSeconds));a.add(number(p.gainDb));a.add(p.linearToNext);envelope.add(a);}
    put("_envelope",juce::JSON::toString(envelope,true));put("_envelopeBase",number(note.amplitudeEnvelopeBasePercent));
    return result;
}
inline juce::var trackState(const TrackData& track,const ProjectData& project)
{
    auto* o=new juce::DynamicObject();juce::var result(o);
    o->setProperty("ProjectName",track.name);o->setProperty("Flags",track.utauGlobalFlags);o->setProperty("VoiceDir",track.voicebankDirectory.getFullPathName());o->setProperty("Tempo",number(project.bpm));
    juce::Array<juce::var> changes,layout;
    for(const auto& t:project.tempoChanges){juce::Array<juce::var> a;a.add(number(t.quarterPosition));a.add(number(t.bpm));changes.add(a);}
    for(const auto& clip:track.clips)for(const auto& note:clip.notes)
    {juce::Array<juce::var> a;a.add(note.id);a.add(note.ustSourceSectionIndex);a.add((juce::int64)std::llround(project.quarterPositionForSeconds(clip.startSeconds+note.startSeconds)*480));a.add((juce::int64)std::llround(project.quarterPositionForSeconds(clip.startSeconds+note.startSeconds+note.durationSeconds)*480));layout.add(a);}
    o->setProperty("_tempos",juce::JSON::toString(changes,true));o->setProperty("_layout",juce::JSON::toString(layout,true));return result;
}
inline bool envelopeText(const NoteData& note,juce::String& value)
{
    const auto& p=note.amplitudeEnvelope;if(p.empty()){value="0,0,0,100,100,100,100";return note.amplitudeEnvelopeBasePercent==100;}
    if((p.size()!=6&&p.size()!=7)||p.front().gainDb>-59.9||p.back().gainDb>-59.9)return false;
    const auto v=[&](size_t i){return p[i].gainDb<=-60?0:std::pow(10.0,p[i].gainDb/20.0)*note.amplitudeEnvelopeBasePercent;};
    const size_t tail=p.size()-3;
    value=csv({(p[1].timeSeconds-p[0].timeSeconds)*1000,(p[2].timeSeconds-p[1].timeSeconds)*1000,(p.back().timeSeconds-p[tail+1].timeSeconds)*1000,v(1),v(2),v(tail),v(tail+1)})+",%,"+number((p[tail+1].timeSeconds-p[tail].timeSeconds)*1000);
    if(p.size()==7)value+=","+number((p[3].timeSeconds-p[2].timeSeconds)*1000)+","+number(v(3));
    return true;
}
inline bool patchNote(const NoteData& note,const juce::var& state,juce::String& text,juce::String& error,juce::StringArray& warnings)
{
    const auto baseline=juce::JSON::parse(note.ustBaseline);const bool fresh=note.ustSourceSection.isEmpty();
    text=fresh?juce::String("[#0000]\r\n"):note.ustSourceSection;
    for(const auto& field:state.getDynamicObject()->getProperties())
    {
        const auto key=field.name.toString();if(key.startsWithChar('_'))continue;
        const auto value=field.value.toString();
        if(value.containsAnyOf("\r\n")){error=juce::String::fromUTF8("UST 字段不能包含换行：")+key;return false;}
        if(fresh||baseline.getProperty(field.name,{}).toString()!=value)
        {
            // Pitches is an older spelling of PitchBend; keep both consistent when editing.
            if(key=="PitchBend"&&usttext::field(text,"Pitches").isNotEmpty())text=usttext::set(text,"Pitches",value);
            text=usttext::set(text,key,value);
        }
    }
    if(fresh||baseline.getProperty("_envelope",{}).toString()!=state.getProperty("_envelope",{}).toString()||baseline.getProperty("_envelopeBase",{}).toString()!=state.getProperty("_envelopeBase",{}).toString())
    {
        juce::String envelope;
        if(!envelopeText(note,envelope)){error=juce::String::fromUTF8("音符「")+note.label+juce::String::fromUTF8("」的自定义响度包络无法无损表示为 UST 包络。请保留 HJPX，或先恢复标准包络再导出。");return false;}
        if(!note.amplitudeEnvelope.empty())warnings.addIfNotAlreadyThere(juce::String::fromUTF8("UST 包络按线性振幅插值；编辑器的 dB 曲线形状可能不同。"));
        text=usttext::set(text,"Envelope",envelope);
    }
    if(note.utauAutoPitchTransition || note.modulation!=1 || note.drift!=1)warnings.addIfNotAlreadyThere(juce::String::fromUTF8("编辑器自动过渡及调制/漂移处理不属于标准 UST 参数；导出保留显式音高标点。"));
    if(note.utauFlagCurveEnabled||note.utauFlagSplit||note.utauTailFadeMode||!note.diffSingerPitchOffset.empty()||note.utauOto.enabled)
        warnings.addIfNotAlreadyThere(juce::String::fromUTF8("UST 不承载线性/分区 FLAG、DS 偏移、独立 OTO 与高级尾段淡出；这些数据仍保存在 HJPX 中。"));
    if(note.vibratoEnabled&&std::abs(note.vibratoEndPercent-100)>.001)warnings.addIfNotAlreadyThere(juce::String::fromUTF8("UST VBR 只能在音符尾部结束，提前结束的颤音位置不能完整表达。"));
    return true;
}
inline bool write(const ProjectData& project,const juce::String& trackId,const juce::File& file,juce::String& error,juce::StringArray& warnings,int encoding)
{
    const TrackData* track=nullptr;for(const auto& t:project.tracks)if(t.id==trackId)track=&t;
    if(!track||!track->compose){error=juce::String::fromUTF8("请选择一个音符轨道导出 UST。");return false;}
    auto source=usttext::sections(track->ustSourceDocument);juce::StringArray parseWarnings;
    const bool mode2=track->ustSourceDocument.isEmpty()||UstImporter::parse(track->ustSourceDocument,parseWarnings).mode2;
    const auto state=trackState(*track,project),baseline=juce::JSON::parse(track->ustBaseline);
    const bool sameLayout=state.getProperty("_layout",{}).toString()==baseline.getProperty("_layout",{}).toString();
    const bool sameTempo=state.getProperty("_tempos",{}).toString()==baseline.getProperty("_tempos",{}).toString()&&state.getProperty("Tempo",{}).toString()==baseline.getProperty("Tempo",{}).toString();
    struct Item {juce::int64 start,end;int source;juce::String text;};std::vector<Item> notes;
    for(const auto& clip:track->clips)for(const auto& note:clip.notes)
    {
        if(!std::isfinite(note.startSeconds)||!std::isfinite(note.durationSeconds)||!std::isfinite(note.midiNote)){error="Invalid note values";return false;}
        const auto s=noteState(note,project,clip.startSeconds,mode2);juce::String raw;if(!patchNote(note,s,raw,error,warnings))return false;
        const auto start=s.getProperty("_start",{}).toString().getLargeIntValue(),length=s.getProperty("Length",{}).toString().getLargeIntValue();
        if(start<0||length<=0){error=juce::String::fromUTF8("UST 需要非负位置及至少 1 tick 的音符时长。");return false;}
        notes.push_back({start,start+length,note.ustSourceSectionIndex,raw});
    }
    if(notes.empty()){error=juce::String::fromUTF8("该轨道没有可导出的音符。");return false;}
    std::stable_sort(notes.begin(),notes.end(),[](const auto& a,const auto& b){return a.start<b.start;});
    for(size_t i=1;i<notes.size();++i)if(notes[i].start<notes[i-1].end){error=juce::String::fromUTF8("同一 UST 不能表示重叠音符，请拆成多个轨道后分别导出。文件未修改。");return false;}
    int setting=-1;for(size_t i=0;i<source.size();++i)if(source[i].tag.equalsIgnoreCase("#SETTING"))setting=(int)i;
    juce::String settings=setting>=0?source[(size_t)setting].text:juce::String("[#SETTING]\r\nMode2=")+(mode2?"True":"False")+"\r\n";
    for(const auto* key:{"ProjectName","Flags","VoiceDir","Tempo"})
        if(setting<0||state.getProperty(key,{}).toString()!=baseline.getProperty(key,{}).toString())settings=usttext::set(settings,key,state.getProperty(key,{}).toString());
    juce::String output;
    if(sameLayout&&sameTempo&&!source.empty())
    {
        for(size_t i=0;i<source.size();++i)
        {if((int)i==setting){output+=settings;continue;}const auto found=std::find_if(notes.begin(),notes.end(),[i](const auto& n){return n.source==(int)i;});output+=found==notes.end()?source[i].text:found->text;}
    }
    else
    {
        // Preserve opaque sections; rebuild musical positions and rests when layout changes.
        for(const auto& part:source)if(!usttext::noteTag(part.tag)&&!part.tag.equalsIgnoreCase("#SETTING")&&!part.tag.equalsIgnoreCase("#TRACKEND"))output+=part.text;
        if(output.isEmpty())output="[#VERSION]\r\nUST Version1.2\r\n";
        if(!output.endsWithChar('\n')&&!output.endsWithChar('\r'))output+="\r\n";
        output+=settings;juce::int64 cursor=0;int index=0;
        const auto appendRest=[&](juce::int64 end)
        {
            // Tempo changes inside silence need their own rest boundary.
            std::vector<juce::int64> boundaries;for(const auto& t:project.tempoChanges){const auto tick=(juce::int64)std::llround(t.quarterPosition*480);if(tick>cursor&&tick<end)boundaries.push_back(tick);}boundaries.push_back(end);std::sort(boundaries.begin(),boundaries.end());
            for(auto stop:boundaries){if(stop<=cursor)continue;output+=usttext::retag("[#0000]\r\nLength="+juce::String(stop-cursor)+"\r\nLyric=R\r\nNoteNum=60\r\nTempo="+number(project.tempoAtQuarterPosition(cursor/480.0))+"\r\n",index++);cursor=stop;}
        };
        for(auto note:notes)
        {
            appendRest(note.start);
            for(const auto& change:project.tempoChanges){const auto tick=(juce::int64)std::llround(change.quarterPosition*480);if(tick>note.start&&tick<note.end){error=juce::String::fromUTF8("UST 不能在单个音符中间改变速度，请在变速点拆分该音符后导出。");return false;}}
            note.text=usttext::set(note.text,"Tempo",number(project.tempoAtQuarterPosition(note.start/480.0)));
            output+=usttext::retag(note.text,index++);if(!output.endsWithChar('\n')&&!output.endsWithChar('\r'))output+="\r\n";cursor=note.end;
        }
        auto end=cursor;for(const auto& clip:track->clips)end=std::max(end,(juce::int64)std::llround(project.quarterPositionForSeconds(clip.startSeconds+clip.durationSeconds)*480));appendRest(end);output+="[#TRACKEND]\r\n";
        if(!source.empty())warnings.addIfNotAlreadyThere(juce::String::fromUTF8("音符布局或速度已改变，休止符及编号已重建；原休止符附加字段仅保留在 HJPX 的导入原文中。"));
    }
    int cp=encoding?encoding:LegacyTextCodec::codePage(track->ustSourceEncoding);if(!cp)cp=65001;
    if(cp!=65001&&output.containsIgnoreCase("#Charset:UTF-8")){error=juce::String::fromUTF8("原 UST 声明为 UTF-8，请选择 UTF-8 或保持原编码。");return false;}
    LegacyTextDocument target;target.codePage=cp;target.bom=cp==65001&&(encoding?true:track->ustSourceDocument.isEmpty()||track->ustSourceBom);target.existed=file.existsAsFile();
    if(target.existed&&!file.loadFileAsData(target.bytes)){error="Could not read destination";return false;}
    if(output==track->ustSourceDocument&&cp==LegacyTextCodec::codePage(track->ustSourceEncoding)&&target.bom==track->ustSourceBom&&track->ustSourceBytes.isNotEmpty())
    {
        juce::MemoryBlock originalBytes;
        if(originalBytes.fromBase64Encoding(track->ustSourceBytes))return LegacyTextCodec::writeBytes(file,originalBytes,target,error);
    }
    return LegacyTextCodec::write(file,output,target,error);
}
}