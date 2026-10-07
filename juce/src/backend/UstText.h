#pragma once
#include <juce_core/juce_core.h>
#include <vector>
namespace hachi::backend::usttext
{
struct Line {juce::String body, ending;};
inline std::vector<Line> lines(const juce::String& text)
{
    // JUCE UTF strings have linear-time indexed access/length. Walk character
    // pointers once so preserving line endings in a large OTO/UST stays O(n).
    std::vector<Line> result;
    auto cursor=text.getCharPointer(),start=cursor;
    while(!cursor.isEmpty())
    {
        const auto ending=cursor;
        const auto ch=cursor.getAndAdvance();
        if(ch!='\n'&&ch!='\r')continue;
        if(ch=='\r'&&*cursor=='\n')++cursor;
        result.push_back({juce::String(start,ending),juce::String(ending,cursor)});
        start=cursor;
    }
    if(start.getAddress()!=cursor.getAddress())result.push_back({juce::String(start,cursor),{}});
    return result;
}
inline bool noteTag(const juce::String& tag)
{return tag.startsWithChar('#')&&tag.length()>1&&tag.substring(1).containsOnly("0123456789");}
struct Section {juce::String tag,text;};
inline std::vector<Section> sections(const juce::String& text)
{
    std::vector<Section> result;juce::String prefix;
    for(const auto& line:lines(text))
    {
        const auto s=line.body.trim();
        if(s.startsWithChar('[')&&s.endsWithChar(']'))
        {result.push_back({s.substring(1,s.length()-1),prefix});prefix.clear();}
        if(result.empty())prefix+=line.body+line.ending;
        else result.back().text+=line.body+line.ending;
    }
    return result;
}
inline juce::String eol(const juce::String& s){return s.contains("\r\n")?"\r\n":s.containsChar('\r')?"\r":"\n";}
inline juce::String field(const juce::String& text,const juce::String& key)
{
    juce::String result;
    for(const auto& line:lines(text)){const auto eq=line.body.indexOfChar('=');if(eq>=0&&line.body.substring(0,eq).trim().equalsIgnoreCase(key))result=line.body.substring(eq+1).trim();}
    return result;
}
inline juce::String set(const juce::String& text,const juce::String& key,const juce::String& value)
{
    juce::String result;bool found=false;
    for(const auto& line:lines(text))
    {
        const auto eq=line.body.indexOfChar('=');
        if(eq>=0&&line.body.substring(0,eq).trim().equalsIgnoreCase(key))
        {result+=line.body.substring(0,eq+1)+value+line.ending;found=true;}
        else result+=line.body+line.ending;
    }
    if(!found){const auto newline=eol(text);if(result.isNotEmpty()&&!result.endsWithChar('\n')&&!result.endsWithChar('\r'))result+=newline;result+=key+"="+value+newline;}
    return result;
}
inline juce::String retag(const juce::String& text,int index)
{
    juce::String result;bool done=false;
    for(const auto& line:lines(text))
    {const auto s=line.body.trim();if(!done&&s.startsWith("[#")&&s.endsWithChar(']')){result+="[#"+juce::String(index).paddedLeft('0',4)+"]"+line.ending;done=true;}else result+=line.body+line.ending;}
    return result;
}
}