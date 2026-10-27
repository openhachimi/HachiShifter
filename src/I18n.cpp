#include "I18n.h"
#include <array>
#include <map>
#include <set>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <algorithm>
#include <vector>

namespace hachi
{
namespace
{
using Row=std::array<juce::String,5>;
struct Entry {Row cells;juce::String note;std::set<juce::String> aliases;};
struct MessagePattern {juce::String key,prefix,suffix,token;};
struct Catalog {std::map<juce::String,Entry> rows;std::map<juce::String,juce::String> aliases;std::vector<MessagePattern> patterns;};
std::shared_mutex catalogLock;
Catalog catalog;
std::atomic_bool attempted{false};
std::mutex initializeLock;
std::once_flag localeInitialized;
std::mutex missingLock;
std::set<juce::String> missingKeys;
std::atomic<I18n::Language> uiLanguage{I18n::Language::zhCN};

juce::String unescapeCell(const juce::String& text)
{
    juce::String out;
    for(int i=0;i<text.length();++i)
    {
        const auto c=text[i];
        if(c=='\\'&&i+1<text.length())
        {const auto n=text[++i];if(n=='n')out<<"\n";else if(n=='r')out<<"\r";else if(n=='t')out<<"\t";else if(n=='\\')out<<"\\";else out<<"\\"<<juce::String::charToString(n);}
        else out<<juce::String::charToString(c);
    }
    return out;
}
juce::String escapeCell(const juce::String& text)
{return text.replace("\\","\\\\").replace("\t","\\t").replace("\r","\\r").replace("\n","\\n");}
std::multiset<juce::String> placeholders(const juce::String& text)
{
    std::multiset<juce::String> result;
    for(int i=0;i<text.length();++i)if(text[i]=='{')
    {if(i+1<text.length()&&text[i+1]=='{'){++i;continue;}const auto end=text.indexOfChar(i+1,'}');if(end<0)break;const auto token=text.substring(i+1,end);if(token.isNotEmpty()&&token.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"))result.insert(token);i=end;}
    return result;
}
Catalog parseCatalog(const juce::File& file,I18n::LoadReport& report)
{
    Catalog next;
    if(!file.existsAsFile()){report.problems.add("Translation file missing: "+file.getFullPathName());return next;}
    juce::StringArray lines;
    try{lines=juce::StringArray::fromLines(file.loadFileAsString());}catch(...){report.problems.add("Cannot read translation file: "+file.getFullPathName());return next;}
    if(lines.isEmpty()){report.problems.add("Empty translation file: "+file.getFullPathName());return next;}
    const auto header=juce::StringArray::fromTokens(lines[0],"\t","");
    if(header.size()<6||header[0].trim().toLowerCase()!="key"||header[1]!="zh-CN"||header[2]!="zh-TW"||header[3]!="ja-JP"||header[4]!="ko-KR"||header[5]!="en-US")
    {report.problems.add("Translation header must be key<TAB>zh-CN<TAB>zh-TW<TAB>ja-JP<TAB>ko-KR<TAB>en-US<TAB>note<TAB>aliases");return next;}
    int aliasesColumn=-1;for(int i=6;i<header.size();++i)if(header[i]=="aliases")aliasesColumn=i;
    for(int index=1;index<lines.size();++index)
    {
        const auto& line=lines[index];if(line.trim().isEmpty()||line.startsWithChar('#'))continue;
        const auto cells=juce::StringArray::fromTokens(line,"\t","");
        if(cells.size()<6){++report.rejected;report.problems.add("Line "+juce::String(index+1)+": expected six language columns");continue;}
        const auto key=unescapeCell(cells[0]);if(key.isEmpty())continue;
        if(next.rows.count(key)){++report.rejected;report.problems.add("Line "+juce::String(index+1)+": duplicate key "+key+" (first row wins)");continue;}
        Entry row;for(int i=0;i<5;++i)row.cells[std::size_t(i)]=unescapeCell(cells[i+1]);if(cells.size()>6)row.note=unescapeCell(cells[6]);
        // Placeholders are validated against the FILE's English cell (not any
        // hidden embedded language table), or its key if English is missing.
        const auto expected=placeholders(row.cells[4].isNotEmpty()?row.cells[4]:key);
        for(int i=0;i<5;++i)
        {
            if(row.cells[std::size_t(i)].isEmpty())continue;
            if(placeholders(row.cells[std::size_t(i)])!=expected)
            {++report.rejected;report.problems.add("Line "+juce::String(index+1)+" ["+header[i+1]+"]: placeholders differ");row.cells[std::size_t(i)].clear();}
            else ++report.overrides;
        }
        row.aliases.insert(key);
        if(aliasesColumn>=0&&cells.size()>aliasesColumn)
        {
            const auto value=unescapeCell(cells[aliasesColumn]);
            if(value.isNotEmpty())
            {
                const auto parsed=juce::JSON::parse(value);
                if(const auto* list=parsed.getArray())
                {for(const auto& alias:*list)if(alias.isString()&&alias.toString().isNotEmpty())row.aliases.insert(alias.toString());}
                else {++report.rejected;report.problems.add("Line "+juce::String(index+1)+": aliases must be a JSON string array");}
            }
        }
        // Legacy files without alias metadata still support their literal
        // cells; new exported files keep aliases stable across language edits.
        if(aliasesColumn<0){if(row.cells[0].isNotEmpty())row.aliases.insert(row.cells[0]);if(row.cells[4].isNotEmpty())row.aliases.insert(row.cells[4]);}
        next.rows.emplace(key,std::move(row));++report.rows;
    }
    if(next.rows.empty()){report.problems.add("No valid translation rows: "+file.getFullPathName());return next;}
    for(const auto& [key,row]:next.rows)next.aliases.emplace(key,key); // explicit keys win
    for(const auto& [key,row]:next.rows)for(const auto& alias:row.aliases)next.aliases.emplace(alias,key);
    for(const auto& [alias,key]:next.aliases)
    {
        const auto open=alias.indexOfChar('{'),close=alias.indexOfChar('}');
        if(open<0||close<=open||alias.indexOfChar(close+1,'{')>=0||placeholders(alias).size()!=1)continue;
        const auto prefix=alias.substring(0,open),suffix=alias.substring(close+1);
        if(prefix.length()+suffix.length()<8)continue;
        next.patterns.push_back({key,prefix,suffix,alias.substring(open,close+1)});
    }
    std::stable_sort(next.patterns.begin(),next.patterns.end(),[](const auto& a,const auto& b){return a.prefix.length()+a.suffix.length()>b.prefix.length()+b.suffix.length();});
    report.loaded=true;return next;
}
void ensureLoaded()
{
    if(attempted.load(std::memory_order_acquire))return;
    const std::lock_guard guard(initializeLock);
    if(!attempted.load(std::memory_order_relaxed))I18n::loadTranslations();
}
juce::String resolveText(const juce::String& requested,I18n::Language language)
{
    if(requested.isEmpty())return {};
    ensureLoaded();const std::shared_lock guard(catalogLock);
    const auto alias=catalog.aliases.find(requested);const auto key=alias==catalog.aliases.end()?requested:alias->second;
    if(const auto found=catalog.rows.find(key);found!=catalog.rows.end())
    {
        const auto& row=found->second.cells;const auto index=std::size_t(language);
        if(index<row.size()&&row[index].isNotEmpty())return row[index];
        if(row[4].isNotEmpty())return row[4]; // fallback ALSO comes from this file
    }
    {const std::lock_guard missingGuard(missingLock);missingKeys.insert(key);}
    return "["+key+"]"; // visible missing-key diagnostic, never embedded translation
}
}

juce::File I18n::translationFile()
{return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("lang").getChildFile("translations.tsv");}
I18n::LoadReport I18n::loadTranslations(const juce::File& file)
{
    LoadReport report;auto next=parseCatalog(file,report);
    if(report.loaded){const std::unique_lock guard(catalogLock);catalog=std::move(next);}
    // Invalid/missing files preserve the last good FILE catalog; a fresh
    // process has only marked keys, not concealed compiled-in language values.
    attempted.store(true,std::memory_order_release);return report;
}
juce::StringArray I18n::missingTranslationKeys()
{const std::lock_guard guard(missingLock);juce::StringArray keys;for(const auto& key:missingKeys)keys.add(key);return keys;}
void I18n::clearMissingTranslationKeys()
{const std::lock_guard guard(missingLock);missingKeys.clear();}
int I18n::builtInRowCount()
{ensureLoaded();const std::shared_lock guard(catalogLock);return int(catalog.rows.size());} // legacy API name, no built-in catalog
bool I18n::exportTranslations(const juce::File& file,juce::String& error,const juce::StringArray& extra)
{
    error.clear();ensureLoaded();Catalog output;
    {const std::shared_lock guard(catalogLock);output=catalog;}
    if(file.existsAsFile())
    {
        LoadReport report;auto existing=parseCatalog(file,report);
        if(!report.loaded||report.rejected){error="Existing translation file was left unchanged: "+report.problems.joinIntoString(" | ");return false;}
        for(auto& [key,row]:existing.rows)
        {
            auto& target=output.rows[key];for(int i=0;i<5;++i)if(row.cells[std::size_t(i)].isNotEmpty())target.cells[std::size_t(i)]=row.cells[std::size_t(i)];
            if(row.note.isNotEmpty())target.note=row.note;target.aliases.insert(row.aliases.begin(),row.aliases.end());
        }
    }
    if(output.rows.empty()){error="No translation file is loaded; cannot export a compiled language table";return false;}
    for(const auto& key:extra)if(key.isNotEmpty()&&!output.aliases.count(key)&&!output.rows.count(key))
    {Entry row;row.note="needs translation";row.aliases.insert(key);output.rows.emplace(key,std::move(row));}
    juce::String text="key\tzh-CN\tzh-TW\tja-JP\tko-KR\ten-US\tnote\taliases\n";
    for(const auto& [key,row]:output.rows)
    {
        text<<escapeCell(key);for(const auto& value:row.cells)text<<"\t"<<escapeCell(value);text<<"\t"<<escapeCell(row.note)<<"\t";
        juce::Array<juce::var> aliases;for(const auto& alias:row.aliases)aliases.add(alias);text<<escapeCell(juce::JSON::toString(juce::var(aliases),true))<<"\n";
    }
    if(file.getParentDirectory().createDirectory().failed()){error="Cannot create "+file.getParentDirectory().getFullPathName();return false;}
    juce::TemporaryFile temporary(file);
    {juce::FileOutputStream stream(temporary.getFile());if(!stream.openedOk()||!stream.writeText(text,false,false,"\n")){error="Cannot write "+file.getFullPathName();return false;}stream.flush();if(stream.getStatus().failed()){error=stream.getStatus().getErrorMessage();return false;}}
    if(!temporary.overwriteTargetFileWithTemporary()){error="Cannot replace "+file.getFullPathName();return false;}return true;
}
I18n::I18n()
{
    std::call_once(localeInitialized,[]
    {
        const auto locale=juce::SystemStats::getUserLanguage().toLowerCase();auto initial=Language::zhCN;
        if(locale.startsWith("ja"))initial=Language::jaJP;else if(locale.startsWith("ko"))initial=Language::koKR;
        else if(locale.contains("tw")||locale.contains("hk")||locale.contains("hant"))initial=Language::zhTW;else if(!locale.startsWith("zh"))initial=Language::enUS;
        uiLanguage.store(initial,std::memory_order_relaxed);
    });
    language=uiLanguage.load(std::memory_order_relaxed);ensureLoaded();
}
void I18n::setLanguage(Language value){language=value;uiLanguage.store(value,std::memory_order_relaxed);}
juce::String I18n::uiText(const char* source){return resolveText(juce::String::fromUTF8(source),uiLanguage.load(std::memory_order_relaxed));}
juce::String I18n::text(const juce::String& key) const{return resolveText(key,language);}
juce::String I18n::pairText(const char* zh,const char* en,Language language)
{juce::ignoreUnused(zh);return resolveText(juce::String::fromUTF8(en),language);}
juce::String I18n::message(const juce::String& diagnostic)
{
    if(diagnostic.isEmpty())return {};ensureLoaded();juce::String key,token,detail;
    {
        const std::shared_lock guard(catalogLock);
        if(const auto direct=catalog.aliases.find(diagnostic);direct!=catalog.aliases.end())key=direct->second;
        else for(const auto& pattern:catalog.patterns)
            if(diagnostic.startsWith(pattern.prefix)&&diagnostic.endsWith(pattern.suffix)&&diagnostic.length()>pattern.prefix.length()+pattern.suffix.length())
            {key=pattern.key;token=pattern.token;detail=diagnostic.substring(pattern.prefix.length(),diagnostic.length()-pattern.suffix.length());break;}
    }
    if(key.isEmpty())return diagnostic; // opaque backend/user detail, not a second translation table
    const auto translated=resolveText(key,uiLanguage.load(std::memory_order_relaxed));
    return token.isEmpty()?translated:translated.replace(token,detail);
}
}
