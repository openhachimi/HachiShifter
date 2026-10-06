#include "Hamood.h"
#include "backend/UtauRenderer.h"
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <limits>
#include <numeric>

namespace hachi::hamood
{
namespace
{
constexpr std::array<int,7> major {0,2,4,5,7,9,11}, minor {0,2,3,5,7,8,10};
int pc(int n) { return (n%12+12)%12; }
const auto& scale(int key) { return key>=12?minor:major; }
juce::String uid(const char* prefix) { return juce::String(prefix)+"_"+juce::Uuid().toString(); }
bool rest(const NoteData& n, bool diffSinger)
{
    const auto label=n.label.trim();
    return backend::isRestLyric(label)
        || (diffSinger && (label.equalsIgnoreCase("R") || label.equalsIgnoreCase("SP") || label.equalsIgnoreCase("AP")))
        || n.melodyneConsonantCandidate || !std::isfinite(n.midiNote) || n.durationSeconds<=0;
}
bool selected(const Options& o,const juce::String& id)
{ return o.wholeTrack || std::find(o.noteIds.begin(),o.noteIds.end(),id)!=o.noteIds.end(); }
struct Evidence
{
    std::array<double,24> scores{};
    int notes=0, pitchClasses=0;
    double beats=0;
};
// Published Krumhansl-Kessler and Temperley-Kostka-Payne empirical profiles.
// Reference: music21.analysis.discrete documentation (not implementation code).
constexpr std::array<std::array<double,12>,4> profiles {{
    {6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88},
    {6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17},
    {.748,.060,.488,.082,.670,.460,.096,.715,.104,.366,.057,.400},
    {.712,.084,.474,.618,.049,.460,.105,.747,.404,.067,.133,.330}
}};
double correlation(const std::array<double,12>& histogram,const std::array<double,12>& profile,int tonic)
{
    const auto hm=std::accumulate(histogram.begin(),histogram.end(),0.0)/12.0;
    const auto pm=std::accumulate(profile.begin(),profile.end(),0.0)/12.0;
    double product=0,hvariance=0,pvariance=0;
    for(int p=0;p<12;++p)
    {
        const double h=histogram[p]-hm,v=profile[pc(p-tonic)]-pm;
        product+=h*v;hvariance+=h*h;pvariance+=v*v;
    }
    return product/std::max(1.e-12,std::sqrt(hvariance*pvariance));
}
Evidence evidence(const ProjectData& data,const std::vector<Plan::Note>& notes,double from,double to)
{
    Evidence result;std::array<double,12> histogram{};
    for(const auto& n:notes)
    {
        const double first=std::max(from,n.start),last=std::min(to,n.start+n.duration);
        if(last<=first)continue;
        // Quarter-note duration is invariant under tempo edits. Partial notes
        // contribute only their overlap; sparse/empty bars cannot invent a key.
        const auto weight=std::max(0.0,data.quarterPositionForSeconds(last)-data.quarterPositionForSeconds(first));
        histogram[pc((int)std::lround(n.pitch))]+=weight;result.beats+=weight;++result.notes;
    }
    for(auto weight:histogram)if(weight>1.e-6)++result.pitchClasses;
    for(int key=0;key<24;++key)
        result.scores[key]=.8*correlation(histogram,profiles[key/12],key%12)
                          +.2*correlation(histogram,profiles[2+key/12],key%12);
    return result;
}
std::vector<std::pair<int,double>> candidates(const std::array<double,24>& scores)
{
    std::vector<std::pair<int,double>> ranked;
    for(int key=0;key<24;++key)ranked.emplace_back(key,scores[key]);
    std::stable_sort(ranked.begin(),ranked.end(),[](auto a,auto b){return a.second>b.second;});
    ranked.resize(3);return ranked;
}
double chosenMargin(const std::array<double,24>& scores,int key)
{
    double other=-2;for(int k=0;k<24;++k)if(k!=key)other=std::max(other,scores[k]);
    return scores[key]-other;
}
double quartersPerBar(const ProjectData& data)
{return std::max(1.e-6,data.numerator*4.0/std::max(1,data.denominator));}
int barAt(const ProjectData& data,double seconds)
{return (int)std::floor(data.quarterPositionForSeconds(seconds)/quartersPerBar(data)+1.e-8)+1;}
std::vector<Passage> automaticPassages(const ProjectData& data,const std::vector<Plan::Note>& notes,
                                     const Evidence& global,int bars)
{
    const double from=notes.front().start;
    double to=from;for(const auto& n:notes)to=std::max(to,n.start+n.duration);
    const auto bar=quartersPerBar(data),step=bar*bars;
    const auto first=std::floor(data.quarterPositionForSeconds(from)/step)*step;
    const auto last=data.quarterPositionForSeconds(to);
    if((last-first)/step>2048)return {};
    std::vector<Passage> passages;
    std::vector<Evidence> raw;
    std::vector<std::array<double,24>> emission;
    for(double q=first;q<last-1.e-8;q+=step)
    {
        Passage p;const double left=data.secondsForQuarterPosition(q),right=data.secondsForQuarterPosition(q+step);
        p.start=std::max(from,left);p.end=std::min(to,right);p.startBar=(int)std::floor(q/bar+1.e-8)+1;p.endBar=p.startBar+bars-1;
        auto local=evidence(data,notes,p.start,p.end);
        const auto context=evidence(data,notes,data.secondsForQuarterPosition(q-bar),data.secondsForQuarterPosition(q+step+bar));
        const auto support=std::min({1.0,local.notes/8.0,local.beats/8.0,local.pitchClasses/4.0});
        std::array<double,24> scores{};
        for(int k=0;k<24;++k)scores[k]=support*(.75*local.scores[k]+.15*context.scores[k])+(1-.9*support)*global.scores[k];
        const auto ranked=candidates(local.scores);
        // Smoothing may resolve an ambiguous phrase, but must not flatten a
        // well-supported change (including parallel major/minor). Keep the
        // plausible local candidates even if the top two are close together.
        if(support>=.99)
            for(int k=0;k<24;++k)if(ranked[0].second-local.scores[k]>.15)scores[k]=-1.e6;
        p.evidenceNotes=local.notes;p.evidenceBeats=local.beats;
        p.candidates=ranked;passages.push_back(p);raw.push_back(local);emission.push_back(scores);
    }
    if(passages.empty())return passages;
    auto cost=emission;std::vector<std::array<int,24>> previous(cost.size());
    for(size_t i=1;i<cost.size();++i)for(int k=0;k<24;++k)
    {
        double best=-1.e20;int prev=k;
        for(int j=0;j<24;++j)
        {
            // A relative major/minor flip needs at least as much support as
            // another change; a few chord tones are not a modulation.
            const bool relative=(j<12&&k==12+pc(j+9))||(k<12&&j==12+pc(k+9));
            const auto value=cost[i-1][j]-(j==k?0.0:relative?.32:.26);
            if(value>best){best=value;prev=j;}
        }
        cost[i][k]=emission[i][k]+best;previous[i][k]=prev;
    }
    int key=(int)std::distance(cost.back().begin(),std::max_element(cost.back().begin(),cost.back().end()));
    for(int i=(int)passages.size()-1;i>=0;--i)
    {
        auto& p=passages[(size_t)i];p.key=key;p.margin=chosenMargin(raw[(size_t)i].scores,key);
        if(p.evidenceNotes<8||raw[(size_t)i].pitchClasses<3)p.margin=0;
        if(i)key=previous[(size_t)i][key];
    }
    return passages;
}

}
juce::String keyName(int key)
{
    static const char* names[]{"C","C# / Db","D","D# / Eb","E","F","F# / Gb","G","G# / Ab","A","A# / Bb","B"};
    return juce::String(names[pc(key)])+juce::String::fromUTF8(key>=12?" 自然小调":" 大调");
}
juce::String voiceName(int steps)
{
    const char* interval=std::abs(steps)==2?"三度":std::abs(steps)==3?"四度":std::abs(steps)==4?"五度":std::abs(steps)==5?"六度":"八度";
    return juce::String::fromUTF8(steps<0?"下方":"上方")+juce::String::fromUTF8(interval);
}
float harmonyPitch(float pitch,int key,int steps)
{
    const int rounded=(int)std::lround(pitch),root=key%12;
    const auto& degrees=scale(key);
    int nearest=0,bestDistance=999,index=0;
    // Retain chromatic alteration relative to the nearest scale degree. Ties
    // use the lower scale note, so C# in C major is a raised C, not a flat D.
    for(int oct=-2;oct<=12;++oct) for(int d=0;d<7;++d)
    {
        const int candidate=root+12*oct+degrees[d],distance=std::abs(candidate-rounded);
        if(distance<bestDistance) {nearest=candidate;bestDistance=distance;index=oct*7+d;}
    }
    const int targetIndex=index+steps;
    const int oct=(int)std::floor(targetIndex/7.0),degree=targetIndex-oct*7;
    return pitch+float(root+oct*12+degrees[degree]-nearest);
}
Plan analyse(const ProjectData& data,const Options& o)
{
    Plan plan;
    auto fail=[&](const char* error){plan.error=juce::String::fromUTF8(error);return plan;};
    const auto track=std::find_if(data.tracks.begin(),data.tracks.end(),[&](const auto& t){return t.id==o.trackId;});
    if(track==data.tracks.end())return fail("请先选择一条旋律轨道。");
    if(track->accompaniment || (!track->compose && track->pitchAlgorithm!=PitchAlgorithm::utau))return fail("HAMOOD 用于旋律、UTAU 和 DS 轨道；音频请先转换为旋律轨道。");
    if(o.keyMode!="manual"&&o.keyMode!="auto"&&o.keyMode!="sections")return fail("调性模式必须为 auto、sections 或 manual。");
    if(o.tonic<0||o.tonic>11||o.sectionBars<1||o.sectionBars>64||!std::isfinite(o.gainDb)||o.gainDb< -36||o.gainDb>6)return fail("调性、分段长度或音量超出范围。");
    if(o.voices.empty()||o.voices.size()>10)return fail("请至少选择一个和声声部（最多 10 个）。");
    std::set<int> unique;
    for(const auto steps:o.voices)
        if(!std::set<int>{-7,-5,-4,-3,-2,2,3,4,5,7}.contains(steps)||!unique.insert(steps).second)
            return fail("和声音程无效或重复。");
    plan.keyMode=o.keyMode;
    std::vector<Plan::Note> contextNotes;
    for(const auto& clip:track->clips)for(const auto& note:clip.notes)
        if(!rest(note,trackIsDiffSinger(*track)))
        {
            Plan::Note entry{note.id,note.label,clip.startSeconds+note.startSeconds,note.durationSeconds,note.midiNote};
            contextNotes.push_back(entry);if(selected(o,note.id))plan.notes.push_back(entry);
        }
    if(plan.notes.empty())return fail("当前范围没有可生成和声的音符。请选中音符，或明确选择整轨。");
    auto chronological=[](const auto& a,const auto& b){return a.start<b.start;};
    std::stable_sort(plan.notes.begin(),plan.notes.end(),chronological);
    std::stable_sort(contextNotes.begin(),contextNotes.end(),chronological);
    plan.analysisNotes=(int)contextNotes.size();
    const double start=plan.notes.front().start;
    double end=start,contextEnd=start;
    for(const auto& n:plan.notes)end=std::max(end,n.start+n.duration);
    for(const auto& n:contextNotes)contextEnd=std::max(contextEnd,n.start+n.duration);
    if(o.keyMode=="manual")
    {
        if(o.manualSections.empty())
            plan.passages.push_back({start,end,o.tonic+(o.minor?12:0),1.0,barAt(data,start),barAt(data,end-1.e-8)});
        else
        {
            if(o.manualSections.size()>2048)return fail("手动分段不能超过 2048 段。");
            auto sections=o.manualSections;
            std::stable_sort(sections.begin(),sections.end(),[](const auto& a,const auto& b){return a.startBar<b.startBar;});
            int previousEnd=std::numeric_limits<int>::min();
            for(const auto& section:sections)
            {
                if(section.startBar<0||section.endBar<section.startBar||section.endBar>1000000||section.tonic<0||section.tonic>11)
                    return fail("手动分段的小节范围或调性无效；小节从 1 起，0 可用于弱起小节。");
                if(section.startBar<=previousEnd)return fail("手动分段不能重叠，请修改起止小节。");
                previousEnd=section.endBar;
                const auto first=data.secondsForQuarterPosition((section.startBar-1)*quartersPerBar(data));
                const auto last=data.secondsForQuarterPosition(section.endBar*quartersPerBar(data));
                plan.passages.push_back({first,last,section.tonic+(section.minor?12:0),1.0,section.startBar,section.endBar});
            }
            for(const auto& note:plan.notes)
                if(std::none_of(plan.passages.begin(),plan.passages.end(),[&](const auto& p){return note.start>=p.start-1.e-8&&note.start<p.end-1.e-8;}))
                {
                    plan.error=juce::String::fromUTF8("手动分段未覆盖第 ")+juce::String(barAt(data,note.start))+juce::String::fromUTF8(" 小节的音符，请补齐该段。");return plan;
                }
        }
    }
    else
    {
        if(!o.manualSections.empty())return fail("manual_sections 只能与 manual 调性方式一起使用。");
        const auto global=evidence(data,contextNotes,contextNotes.front().start,contextEnd);
        plan.globalCandidates=candidates(global.scores);
        const auto segments=automaticPassages(data,contextNotes,global,o.sectionBars);
        if(segments.empty())return fail("自动分析分段数量过多，请增大小节数或缩小源轨范围。");
        std::map<int,double> supported;
        for(const auto& p:segments)if(p.margin>.05&&p.evidenceNotes>=8)supported[p.key]+=p.evidenceBeats;
        plan.mixedTonality=std::count_if(supported.begin(),supported.end(),[&](const auto& item){return item.second>=std::max(8.0,global.beats*.1);})>=2;
        if(o.keyMode=="auto")
        {
            const auto key=plan.globalCandidates.front().first;
            Passage p{start,end,key,chosenMargin(global.scores,key),barAt(data,start),barAt(data,end-1.e-8),global.notes,global.beats};
            p.candidates=plan.globalCandidates;if(global.notes<8||global.pitchClasses<3)p.margin=0;
            plan.passages.push_back(p);
        }
        else for(auto p:segments)if(p.end>start&&p.start<end)
        {p.start=std::max(start,p.start);p.end=std::min(end,p.end);plan.passages.push_back(std::move(p));}
    }
    if(plan.passages.empty())return fail("音符时间范围无效。");
    for(auto& note:plan.notes)
    {
        const auto section=std::find_if(plan.passages.begin(),plan.passages.end(),[&](const auto& s){return note.start>=s.start-1.e-8&&note.start<s.end-1.e-8;});
        note.key=(section==plan.passages.end()?plan.passages.back():*section).key;
        const auto& steps=scale(note.key);
        if(std::find(steps.begin(),steps.end(),pc((int)std::lround(note.pitch)-note.key%12))==steps.end())++plan.chromaticNotes;
        for(const int voice:o.voices)
        {
            const float target=harmonyPitch(note.pitch,note.key,voice);
            if(target<0||target>127)return fail("部分和声音高超出 MIDI 0–127；请改用其他音程或先调整原旋律。");
            note.targets.push_back(target);
        }
    }
    if(!o.audioClipId.isEmpty())
    {
        plan.audioClipId=o.audioClipId;
        // Prefer supported chord tones near the requested interval, with a
        // phrase-local voice-leading cost. Uncertain/uncovered notes retain
        // the ordinary scale-based target instead of fabricating a chord.
        std::vector<std::array<double,12>> support(plan.notes.size());
        std::vector<bool> covered(plan.notes.size(),false);
        for(size_t i=0;i<plan.notes.size();++i)
        {
            const auto& n=plan.notes[i];double weight=0;
            for(const auto& c:o.chords)
            {
                const auto overlap=std::max(0.0,std::min(n.start+n.duration,c.end)-std::max(n.start,c.start));
                if(overlap<=0||c.score<o.minimumChordScore||c.pitches.empty())continue;
                weight+=overlap*c.score;
                for(auto p:c.pitches)support[i][pc(p)]+=overlap*c.score;
            }
            if(weight>=n.duration*o.minimumChordScore*.5)
            {covered[i]=true;++plan.audioCoveredNotes;for(auto& v:support[i])v/=weight;}
        }
        for(size_t voice=0;voice<o.voices.size();++voice)
        {
            std::vector<std::vector<float>> choices(plan.notes.size());
            std::vector<std::vector<double>> costs(plan.notes.size());
            std::vector<std::vector<int>> previous(plan.notes.size());
            for(size_t i=0;i<plan.notes.size();++i)
            {
                const auto& n=plan.notes[i];const float wanted=n.targets[voice];
                const float fraction=n.pitch-std::round(n.pitch);
                if(covered[i])for(int pitch=std::max(0,(int)std::ceil(wanted-4));pitch<=std::min(127,(int)std::floor(wanted+4));++pitch)
                    if(support[i][pc(pitch)]>=.35 && (o.voices[voice]>0?pitch+fraction>=n.pitch+2:pitch+fraction<=n.pitch-2))choices[i].push_back(pitch+fraction);
                if(choices[i].empty())choices[i].push_back(wanted);
                costs[i].resize(choices[i].size());previous[i].resize(choices[i].size(),-1);
                const bool connected=i>0&&n.start-(plan.notes[i-1].start+plan.notes[i-1].duration)<.8;
                for(size_t j=0;j<choices[i].size();++j)
                {
                    const auto target=choices[i][j];
                    const auto interval=pc((int)std::lround(target-n.pitch));
                    const auto local=.7*std::abs(target-wanted)+(interval==1||interval==11||interval==6?1.6:0)
                        +(covered[i]?2.0*(1.0-support[i][pc((int)std::lround(target))]):0);
                    costs[i][j]=local;
                    if(connected)
                    {
                        double best=1.e20;int back=-1;
                        for(size_t k=0;k<choices[i-1].size();++k)
                        {
                            const auto leap=std::abs(target-choices[i-1][k]);
                            const auto candidate=costs[i-1][k]+.18*leap+.35*std::max(0.f,leap-5.f);
                            if(candidate<best){best=candidate;back=(int)k;}
                        }
                        costs[i][j]+=best;previous[i][j]=back;
                    }
                }
            }
            int index=-1;
            for(int i=(int)plan.notes.size()-1;i>=0;--i)
            {
                if(index<0)index=(int)std::distance(costs[(size_t)i].begin(),std::min_element(costs[(size_t)i].begin(),costs[(size_t)i].end()));
                auto& target=plan.notes[(size_t)i].targets[voice];
                if(std::abs(target-choices[(size_t)i][(size_t)index])>.001)++plan.audioAdjustedNotes;
                target=choices[(size_t)i][(size_t)index];index=previous[(size_t)i][(size_t)index];
            }
        }
    }
    return plan;
}
juce::String Plan::description() const
{
    if(error.isNotEmpty())return error;
    juce::String text=juce::String::fromUTF8("源音符：")+juce::String((int)notes.size())+juce::String::fromUTF8("  ·  新声部：")+juce::String(notes.empty()?0:(int)notes[0].targets.size())+"\n";
    if(keyMode!="manual")text+=juce::String::fromUTF8("调性分析上下文：整条源轨 ")+juce::String(analysisNotes)+juce::String::fromUTF8(" 个音符（候选分数不是概率）\n");
    if(mixedTonality)text+=juce::String::fromUTF8("检测到多个持续调性区域；单一调性只代表整体匹配，建议分段或手动确认。\n");
    for(const auto& section:passages)
    {
        text+=juce::String::fromUTF8("第 ")+juce::String(section.startBar)+"–"+juce::String(section.endBar)+juce::String::fromUTF8(" 小节 | ")
            +juce::String(section.start,2)+"–"+juce::String(section.end,2)+" s  "+keyName(section.key)
            +(section.margin<.08?juce::String::fromUTF8("（证据不足或存在歧义，结合上下文选择）"):juce::String())+"\n";
        if(!section.candidates.empty())
        {
            text+=juce::String::fromUTF8("  候选：");
            for(size_t i=0;i<section.candidates.size();++i)text+=(i?" / ":"")+keyName(section.candidates[i].first)+" "+juce::String(section.candidates[i].second,2);
            text+="\n";
        }
    }
    if(chromaticNotes>0)text+=juce::String::fromUTF8("包含调外音：")+juce::String(chromaticNotes)+juce::String::fromUTF8("；按最近音级保留升降变化。\n");
    text+=juce::String::fromUTF8("\n时间 / 歌词 / 原音 → 和声\n");
    int count=0;
    for(const auto& note:notes)
    {
        if(count++==120){text+=juce::String::fromUTF8("…其余音符将一并生成。\n");break;}
        text+=juce::String(note.start,2)+" s  "+note.lyric+"  "+juce::MidiMessage::getMidiNoteName((int)std::lround(note.pitch),true,true,4)+" → ";
        for(size_t i=0;i<note.targets.size();++i)
            text+=(i?" / ":"")+juce::MidiMessage::getMidiNoteName((int)std::lround(note.targets[i]),true,true,4);
        text+="\n";
    }
    return text;
}
juce::var Plan::json() const
{
    auto* root=new juce::DynamicObject();root->setProperty("error",error);root->setProperty("summary",description());
    root->setProperty("analysis_note_count",analysisNotes);root->setProperty("mixed_tonality",mixedTonality);root->setProperty("key_mode",keyMode);
    root->setProperty("audio_clip_id",audioClipId);root->setProperty("audio_covered_notes",audioCoveredNotes);root->setProperty("audio_adjusted_targets",audioAdjustedNotes);
    root->setProperty("note_count",(int)notes.size());root->setProperty("chromatic_notes",chromaticNotes);
    juce::Array<juce::var> sections,rows;
    for(const auto& s:passages){auto* v=new juce::DynamicObject();v->setProperty("from_seconds",s.start);v->setProperty("to_seconds",s.end);v->setProperty("key",keyName(s.key));v->setProperty("tonic",s.key%12);v->setProperty("minor",s.key>=12);v->setProperty("score_margin",s.margin);v->setProperty("start_bar",s.startBar);v->setProperty("end_bar",s.endBar);v->setProperty("evidence_notes",s.evidenceNotes);v->setProperty("evidence_beats",s.evidenceBeats);
        juce::Array<juce::var> ranked;for(const auto& candidate:s.candidates){auto* c=new juce::DynamicObject();c->setProperty("tonic",candidate.first%12);c->setProperty("minor",candidate.first>=12);c->setProperty("score",candidate.second);ranked.add(c);}v->setProperty("candidates",ranked);sections.add(v);}
    for(const auto& n:notes){auto* v=new juce::DynamicObject();v->setProperty("note_id",n.id);v->setProperty("lyric",n.lyric);v->setProperty("start_seconds",n.start);v->setProperty("midi",n.pitch);juce::Array<juce::var> pitches;for(auto p:n.targets)pitches.add(p);v->setProperty("harmony_midi",pitches);rows.add(v);}
    root->setProperty("passages",sections);root->setProperty("notes",rows);return root;
}
std::vector<juce::String> generate(ProjectData& data,const Options& o,const Plan& plan)
{
    if(plan.error.isNotEmpty()||plan.notes.empty())return {};
    const auto at=std::find_if(data.tracks.begin(),data.tracks.end(),[&](const auto& t){return t.id==o.trackId;});
    if(at==data.tracks.end())return {};
    const auto source=*at;
    const auto insertion=std::distance(data.tracks.begin(),at)+1;
    std::map<juce::String,const Plan::Note*> planned;
    for(const auto& n:plan.notes)planned[n.id]=&n;
    std::vector<TrackData> generated;std::vector<juce::String> trackIds;
    for(size_t voice=0;voice<o.voices.size();++voice)
    {
        auto track=source;track.id=uid("track");track.name=source.name+" · HAMOOD "+voiceName(o.voices[voice])+(o.audioClipId.isEmpty()?juce::String():juce::String::fromUTF8("附近 · 伴奏和弦"));
        track.muted=track.solo=track.referenceOnly=false;
        track.volume=juce::jlimit(0.0f,2.0f,source.volume*std::pow(10.0f,o.gainDb/20.0f));
        track.clips.clear();
        std::map<juce::String,juce::String> noteIds;
        for(const auto& original:source.clips)
        {
            auto clip=original;clip.id=uid("clip");clip.notes.clear();
            clip.glideConnectedToNext=clip.glideConnectedFromPrevious=false;
            for(const auto& originalNote:original.notes)
            {
                const auto found=planned.find(originalNote.id);if(found==planned.end())continue;
                auto note=originalNote;note.id=uid("note");noteIds[originalNote.id]=note.id;
                const auto delta=found->second->targets[voice]-note.midiNote;note.midiNote+=delta;
                note.connectedToPrevious=note.connectedToNext=false;
                for(auto& segment:note.nativeSegments)segment.id=uid("segment");
                // These depend on source phrase identity/pitch. Manual actual parameters
                // and additive offsets remain, automatic predictions are recomputed.
                if(trackIsDiffSinger(source))
                {
                    note.diffSingerTiming.clear();
                    std::erase_if(note.utauFlagCurves,[](const auto& c){return c.flag.startsWith("DS:AUTO:")||c.flag.startsWith("DS:REF:");});
                }
                if(o.preservePitch)
                {
                    for(auto& p:note.pitchControlPoints)p.targetMidi+=delta;
                    for(auto& p:note.diffSingerPitchReference)p.targetMidi+=delta;
                    // contour/manual targets are relative cents, offset is additive.
                }
                else
                {
                    note.contour={{0.0,0.0f},{note.durationSeconds,0.0f}};
                    note.pitchControlPoints.clear();note.diffSingerPitchReference.clear();note.diffSingerPitchOffset.clear();
                    note.diffSingerPitchReferenceFromSavedPitch=false;note.vibratoEnabled=false;
                }
                clip.notes.push_back(std::move(note));
            }
            if(!clip.notes.empty())track.clips.push_back(std::move(clip));
        }
        for(auto& c:track.clips)for(auto& n:c.notes)
        {
            const auto id=noteIds.find(n.melodyneVowelNoteId);
            n.melodyneVowelNoteId=id==noteIds.end()?juce::String():id->second;
        }
        trackIds.push_back(track.id);generated.push_back(std::move(track));
    }
    data.tracks.insert(data.tracks.begin()+insertion,generated.begin(),generated.end());
    return trackIds;
}
bool parseOptions(const juce::var& a,const juce::String& focused,const std::vector<juce::String>& selection,Options& o,juce::String& error)
{
    o.trackId=a.getProperty("track_id",focused).toString();o.noteIds=selection;
    o.wholeTrack=(bool)a.getProperty("whole_track",false);o.keyMode=a.getProperty("key_mode","auto").toString();
    o.tonic=(int)a.getProperty("tonic",0);o.minor=(bool)a.getProperty("minor",false);
    o.sectionBars=(int)a.getProperty("section_bars",8);o.preservePitch=(bool)a.getProperty("preserve_pitch",true);
    o.gainDb=(float)a.getProperty("gain_db",-6.0);
    o.minimumChordScore=(double)a.getProperty("minimum_chord_score",.35);
    if(!std::isfinite(o.minimumChordScore)||o.minimumChordScore<0||o.minimumChordScore>1){error="minimum_chord_score must be 0..1";return false;}
    if(a.hasProperty("manual_sections"))
    {
        if(!a["manual_sections"].isArray()){error="manual_sections must be an array";return false;}
        for(const auto& row:*a["manual_sections"].getArray())
        {
            if(row.getDynamicObject()==nullptr||!row.hasProperty("start_bar")||!row.hasProperty("end_bar")||!row.hasProperty("tonic"))
            {error="Each manual section requires start_bar, end_bar and tonic";return false;}
            o.manualSections.push_back({(int)row["start_bar"],(int)row["end_bar"],(int)row["tonic"],(bool)row.getProperty("minor",false)});
        }
    }
    if(a.hasProperty("voices"))
    {
        o.voices.clear();if(!a["voices"].isArray()){error="voices must be an array of signed diatonic steps";return false;}
        for(const auto& v:*a["voices"].getArray())o.voices.push_back((int)v);
    }
    return true;
}
}
