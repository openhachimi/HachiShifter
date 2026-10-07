#pragma once
#include "Hamood.h"
#include <set>
namespace hachi::hamoodstate
{
inline juce::var object(){return new juce::DynamicObject();}
inline void put(juce::var& v,const char* key,juce::var value){v.getDynamicObject()->setProperty(key,std::move(value));}
inline juce::var read(const ProjectData& data)
{
    auto state=juce::JSON::parse(data.hamoodState);
    if(!state.isObject())state=object();
    if(!state.hasProperty("schema"))put(state,"schema",1);
    if(!state["settings"].isObject())put(state,"settings",object());
    for(const auto* key:{"chords","sections","key_predictions"})if(!state[key].isArray())put(state,key,juce::Array<juce::var>{});
    return state;
}
inline juce::String encode(const juce::var& state){return juce::JSON::toString(state,true);}
inline bool validate(const juce::var& state,juce::String& error)
{
    auto fail=[&](const char* message){error=juce::String::fromUTF8(message);return false;};error.clear();
    if(!state.isObject()||(int)state["schema"]!=1||!state["settings"].isObject())return fail("HAMOOD 资料格式无效。");
    if(encode(state).getNumBytesAsUTF8()>4*1024*1024)return fail("HAMOOD 资料过大。");
    const auto numeric=[](const juce::var& v){return v.isInt()||v.isInt64()||v.isDouble();};
    const auto settings=state["settings"];
    if(settings.hasProperty("voices"))
    {
        if(!settings["voices"].isArray()||settings["voices"].size()>10)return fail("声部列表无效。");
        std::set<int> seen;for(const auto& v:*settings["voices"].getArray())if(!numeric(v)||!std::set<int>{-7,-5,-4,-3,-2,2,3,4,5,7}.contains((int)v)||!seen.insert((int)v).second)return fail("声部音程无效或重复。");
    }
    const auto threshold=(double)settings.getProperty("minimum_chord_score",.35);
    if(!std::isfinite(threshold)||threshold<0||threshold>1)return fail("和弦分数阈值无效。");
    const auto mode=settings.getProperty("key_mode","auto").toString();
    if(mode!="manual"&&mode!="auto"&&mode!="sections")return fail("调性模式无效。");
    if((int)settings.getProperty("tonic",0)<0||(int)settings.getProperty("tonic",0)>11)return fail("主音必须为 0–11。");
    const int bars=(int)settings.getProperty("section_bars",8);
    const double gain=(double)settings.getProperty("gain_db",-6);
    if(bars<1||bars>64||!std::isfinite(gain)||gain< -36||gain>6)return fail("分段长度或和声音量无效。");
    const auto manual=settings["manual_sections"];
    if(!manual.isVoid()&&!manual.isArray())return fail("手动调性分段必须为列表。");
    if(auto* rows=manual.getArray())
    {
        if(rows->size()>2048)return fail("手动调性分段过多。");
        std::vector<std::pair<int,int>> ranges;
        for(const auto& row:*rows)
        {
            int start=(int)row["start_bar"],end=(int)row["end_bar"],tonic=(int)row["tonic"];
            if(!numeric(row["start_bar"])||!numeric(row["end_bar"])||!numeric(row["tonic"])||start<0||end<start||end>1000000||tonic<0||tonic>11)return fail("手动调性的小节范围无效。");
            ranges.emplace_back(start,end);
        }
        std::sort(ranges.begin(),ranges.end());for(size_t i=1;i<ranges.size();++i)if(ranges[i].first<=ranges[i-1].second)return fail("手动调性分段不能重叠。");
    }
    for(const auto* key:{"chords","sections","key_predictions"})
    {
        const auto value=state[key];if(!value.isArray()||value.getArray()->size()>4096)return fail("时间线列表无效或过长。");
        for(const auto& row:*value.getArray())
        {
            const double a=(double)row["start_quarter"],b=(double)row["end_quarter"];
            if(!row.isObject()||!numeric(row["start_quarter"])||!numeric(row["end_quarter"])||!std::isfinite(a)||!std::isfinite(b)||b<=a||a< -1000000||b>1000000)return fail("时间线的起止位置无效。");
            if(row.hasProperty("confirmed")&&!row["confirmed"].isBool())return fail("确认状态必须为布尔值。");
            if(juce::String(key)=="chords")
            {
                if(row["label"].toString().trim().isEmpty()||!row["pitch_classes"].isArray()||row["pitch_classes"].getArray()->isEmpty())return fail("和弦名称或音级为空。");
                for(const auto& pitch:*row["pitch_classes"].getArray())if(!numeric(pitch)||(double)pitch!=(int)pitch||(int)pitch<0||(int)pitch>11)return fail("和弦音级必须为 0–11。");
                const auto score=(double)row.getProperty("score",1.0);if(!std::isfinite(score)||score<0||score>1)return fail("和弦分数须为 0–1。");
            }
        }
    }
    return true;
}
inline juce::var defaults(const ProjectData& data,const juce::var& args)
{
    auto result=juce::JSON::parse(encode(args));if(!result.isObject())result=object();const auto settings=read(data)["settings"];
    for(const auto* key:{"key_mode","tonic","minor","section_bars","voices","preserve_pitch","gain_db","minimum_chord_score"})
        if(!result.hasProperty(key)&&settings.hasProperty(key))put(result,key,settings[key]);
    if(result["key_mode"].toString()=="manual"&&!result.hasProperty("manual_sections")&&settings["manual_sections"].isArray())put(result,"manual_sections",settings["manual_sections"]);
    return result;
}
inline void attach(const ProjectData& data,const juce::var& state,hamood::Options& options)
{
    if(!(bool)state["settings"].getProperty("use_chords",false))return;
    for(const auto& row:*state["chords"].getArray())
    {
        hamood::Chord chord;chord.start=data.secondsForQuarterPosition((double)row["start_quarter"]);chord.end=data.secondsForQuarterPosition((double)row["end_quarter"]);
        chord.label=row["label"].toString();chord.score=(bool)row["confirmed"]?1.0:(double)row.getProperty("score",1.0);
        for(const auto& pc:*row["pitch_classes"].getArray())chord.pitches.push_back((int)pc);
        options.chords.push_back(chord);
    }
    if(!options.chords.empty())options.audioClipId=state["settings"].getProperty("audio_clip_id","project").toString();
}
inline void settings(juce::var& state,const hamood::Options& options,const std::vector<hamood::ManualSection>& manual)
{
    auto s=state["settings"];put(s,"key_mode",options.keyMode);put(s,"tonic",options.tonic);put(s,"minor",options.minor);put(s,"section_bars",options.sectionBars);
    put(s,"preserve_pitch",options.preservePitch);put(s,"gain_db",options.gainDb);put(s,"minimum_chord_score",options.minimumChordScore);
    juce::Array<juce::var> voices;for(auto voice:options.voices)voices.add(voice);put(s,"voices",voices);
    juce::Array<juce::var> rows;for(const auto& section:manual)
    {
        auto row=object();put(row,"start_bar",section.startBar);put(row,"end_bar",section.endBar);put(row,"tonic",section.tonic);put(row,"minor",section.minor);put(row,"confirmed",section.confirmed);rows.add(row);
    }
    put(s,"manual_sections",rows);put(state,"settings",s);
}
inline void predictions(const ProjectData& data,juce::var& state,const hamood::Plan& plan)
{
    if(plan.error.isNotEmpty())return;
    juce::Array<juce::var> keys;
    for(const auto& passage:plan.passages)
    {
        auto row=object();put(row,"start_quarter",data.quarterPositionForSeconds(passage.start));put(row,"end_quarter",data.quarterPositionForSeconds(passage.end));
        put(row,"tonic",passage.key%12);put(row,"minor",passage.key>=12);put(row,"score_margin",passage.margin);put(row,"confirmed",plan.keyMode=="manual");keys.add(row);
    }
    put(state,"key_predictions",keys);
}
inline juce::String remember(const ProjectData& data,const hamood::Options& options,const hamood::Plan& plan)
{
    auto state=read(data);auto manual=options.manualSections;
    if(options.keyMode!="manual")if(auto* rows=state["settings"]["manual_sections"].getArray())for(const auto& row:*rows)
        manual.push_back({(int)row["start_bar"],(int)row["end_bar"],(int)row["tonic"],(bool)row["minor"],(bool)row.getProperty("confirmed",true)});
    settings(state,options,manual);predictions(data,state,plan);return encode(state);
}
inline void importChords(const ProjectData& data,juce::var& state,const std::vector<hamood::Chord>& source,const juce::String& clipId)
{
    juce::Array<juce::var> rows;for(const auto& row:*state["chords"].getArray())if((bool)row["confirmed"])rows.add(row);
    const auto protectedRows=rows;
    for(const auto& chord:source)
    {
        if(chord.pitches.empty()||chord.end<=chord.start)continue;
        const auto start=data.quarterPositionForSeconds(chord.start),end=data.quarterPositionForSeconds(chord.end);
        bool protectedRange=false;for(const auto& row:protectedRows)if(start<(double)row["end_quarter"]&&end>(double)row["start_quarter"])protectedRange=true;
        if(protectedRange)continue;
        auto row=object();put(row,"id",juce::Uuid().toString());put(row,"start_quarter",start);put(row,"end_quarter",end);put(row,"label",chord.label);put(row,"score",juce::jlimit(0.0,1.0,chord.score));
        put(row,"confirmed",false);put(row,"source_clip_id",clipId);juce::Array<juce::var> pcs;for(auto pitch:chord.pitches)pcs.add(pitch);put(row,"pitch_classes",pcs);rows.add(row);
    }
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return (double)a["start_quarter"]<(double)b["start_quarter"];});
    put(state,"chords",rows);auto s=state["settings"];put(s,"audio_clip_id",clipId);put(s,"use_chords",true);put(state,"settings",s);
}
inline std::vector<int> chordPitches(juce::String text)
{
    text=text.trim().replace(juce::String::fromUTF8("♯"),"#").replace(juce::String::fromUTF8("♭"),"b");
    if(text.isEmpty())return {};const auto rootChar=juce::CharacterFunctions::toUpperCase(text[0]);
    const juce::String roots="C D EF G A B";int root=roots.indexOfChar(rootChar);if(root<0||rootChar==' ')return {};
    int at=1;if(text[at]=='#'){++root;++at;}else if(text[at]=='b'){--root;++at;}root=(root+12)%12;
    auto tail=text.substring(at).upToFirstOccurrenceOf("/",false,false).replace(":","").toLowerCase();
    std::vector<int> intervals;
    if(tail==""||tail=="maj")intervals={0,4,7};else if(tail=="m"||tail=="min")intervals={0,3,7};
    else if(tail=="7")intervals={0,4,7,10};else if(tail=="m7"||tail=="min7")intervals={0,3,7,10};
    else if(tail=="maj7"||tail=="ma7")intervals={0,4,7,11};else if(tail=="dim")intervals={0,3,6};
    else if(tail=="dim7")intervals={0,3,6,9};else if(tail=="m7b5"||tail=="min7b5")intervals={0,3,6,10};
    else if(tail=="aug"||tail=="+")intervals={0,4,8};else if(tail=="sus2")intervals={0,2,7};else if(tail=="sus4"||tail=="sus")intervals={0,5,7};
    else if(tail=="6")intervals={0,4,7,9};else if(tail=="m6")intervals={0,3,7,9};else if(tail=="add9")intervals={0,4,7,2};
    else if(tail=="9")intervals={0,4,7,10,2};else if(tail=="m9")intervals={0,3,7,10,2};else if(tail=="maj9")intervals={0,4,7,11,2};else return {};
    for(auto& pitch:intervals)pitch=(root+pitch)%12;return intervals;
}
}