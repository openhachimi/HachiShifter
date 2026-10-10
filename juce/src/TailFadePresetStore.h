#pragma once
#include <juce_core/juce_core.h>
#include "backend/MixedEnvelopeIO.h"
#include <vector>
#include <algorithm>
namespace hachi
{
struct TailFadePreset
{
    juce::String id,name;
    int shape=1;
    bool enabled=true;
    backend::TailFadeSettings settings;
};
// A separate user library: never save presets in an individual project or overwrite
// a library that could not be read. Atomic replacement protects interrupted writes.
class TailFadePresetStore
{
public:
    explicit TailFadePresetStore(juce::File path):file(std::move(path)){}
    bool available() const {return file!=juce::File{};}
    bool load(std::vector<TailFadePreset>& out,juce::String& error) const
    {
        out.clear();if(!available()||!file.exists())return true;
        juce::var root;auto result=juce::JSON::parse(file.loadFileAsString(),root);
        const auto rows=root.getProperty("presets",juce::var{});
        if(result.failed()||((int)root.getProperty("version",0)!=1&&(int)root.getProperty("version",0)!=2&&(int)root.getProperty("version",0)!=3&&(int)root.getProperty("version",0)!=4)||!rows.isArray())return fail(error,"预设文件无法读取，未覆盖原文件。");
        for(const auto& row:*rows.getArray())
        {
            TailFadePreset p;p.id=row.getProperty("id","").toString();p.name=row.getProperty("name","").toString();
            p.shape=(int)row.getProperty("shape",0);p.enabled=(bool)row.getProperty("enabled",true);
            auto& v=p.settings;
            v.allowRise=(bool)row.getProperty("allowRise",false);
            const char* keys[]{"startFraction","endFraction","startGain","endGain","curvePower","control1Time","control1Progress","control2Time","control2Progress"};
            double* values[]{&v.startFraction,&v.endFraction,&v.startGain,&v.endGain,&v.curvePower,&v.control1Time,&v.control1Progress,&v.control2Time,&v.control2Progress};
            for(size_t i=0;i<9;++i){auto n=row.getProperty(keys[i],juce::var{});if(!n.isDouble()&&!n.isInt()&&!n.isInt64())return fail(error,"预设参数无效，未覆盖原文件。");*values[i]=(double)n;}
            v.customCurve=p.shape==3;
            if((int)root.getProperty("version",0)>=2)
            {
                const auto head=row.getProperty("head",juce::var{});if(!head.isObject())return fail(error,"音头预设参数无效，未覆盖原文件。");
                v.head.mode=(int)head.getProperty("mode",0);v.head.customCurve=(bool)head.getProperty("customCurve",false);
                double* headValues[]{&v.head.startFraction,&v.head.endFraction,&v.head.startGain,&v.head.endGain,&v.head.curvePower,&v.head.control1Time,&v.head.control1Progress,&v.head.control2Time,&v.head.control2Progress};
                for(size_t i=0;i<9;++i){const auto n=head.getProperty(keys[i],juce::var{});if(!n.isDouble()&&!n.isInt()&&!n.isInt64())return fail(error,"音头预设参数无效，未覆盖原文件。");*headValues[i]=(double)n;}
            }
            if(!backend::mixedEnvelopeFromVar(row.getProperty("mixed",{}),v)
                ||!backend::mixedEnvelopeFromVar(row.getProperty("head",{}).getProperty("mixed",{}),v.head))return fail(error,"混合包络参数无效，未覆盖原文件。");
            if(p.id.isEmpty()||p.name.trim().isEmpty()||p.name.length()>64||p.shape<1||p.shape>4||!v.valid())return fail(error,"预设参数无效，未覆盖原文件。");
            for(const auto& old:out)if(old.id==p.id||old.name.equalsIgnoreCase(p.name))return fail(error,"预设文件存在重复条目，未覆盖原文件。");
            out.push_back(p);
        }
        return true;
    }
    bool add(TailFadePreset& p,juce::String& error) const
    {
        p.name=p.name.trim();if(p.name.isEmpty()||p.name.length()>64)return fail(error,"名称不能为空，且最多 64 个字符。");
        if(!p.settings.valid()||p.shape<1||p.shape>4)return fail(error,"当前参数无效，无法保存。");
        juce::InterProcessLock lock("HachiTailPresets-"+juce::String::toHexString(file.getFullPathName().hashCode64()));
        if(!lock.enter(500))return fail(error,"预设正在被其他窗口修改，请稍后重试。");
        const Unlock unlock{lock};
        std::vector<TailFadePreset> rows;if(!load(rows,error))return false;
        for(const auto& old:rows)if(old.name.equalsIgnoreCase(p.name))return fail(error,"已有同名预设，请使用其他名称。");
        p.id=juce::Uuid().toString();rows.push_back(p);return write(rows,error);
    }
    bool remove(const juce::String& id,juce::String& error) const
    {
        juce::InterProcessLock lock("HachiTailPresets-"+juce::String::toHexString(file.getFullPathName().hashCode64()));
        if(!lock.enter(500))return fail(error,"预设正在被其他窗口修改，请稍后重试。");
        const Unlock unlock{lock};
        std::vector<TailFadePreset> rows;if(!load(rows,error))return false;
        rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& p){return p.id==id;}),rows.end());return write(rows,error);
    }
private:
    struct Unlock {juce::InterProcessLock& lock;~Unlock(){lock.exit();}};
    static bool fail(juce::String& error,const char* message){error=juce::String::fromUTF8(message);return false;}
    bool write(const std::vector<TailFadePreset>& rows,juce::String& error) const
    {
        if(!available()||file.getParentDirectory().createDirectory().failed())return fail(error,"无法创建预设目录。");
        juce::Array<juce::var> entries;
        for(const auto& p:rows)
        {
            auto* o=new juce::DynamicObject();juce::var row(o);o->setProperty("id",p.id);o->setProperty("name",p.name);o->setProperty("shape",p.shape);o->setProperty("enabled",p.enabled);
            const auto& v=p.settings;
            o->setProperty("allowRise",v.allowRise);
            const char* keys[]{"startFraction","endFraction","startGain","endGain","curvePower","control1Time","control1Progress","control2Time","control2Progress"};
            const double values[]{v.startFraction,v.endFraction,v.startGain,v.endGain,v.curvePower,v.control1Time,v.control1Progress,v.control2Time,v.control2Progress};
            for(size_t i=0;i<9;++i)o->setProperty(keys[i],values[i]);
            auto* h=new juce::DynamicObject();juce::var head(h);h->setProperty("mode",v.head.mode);h->setProperty("customCurve",v.head.customCurve);
            const double headValues[]{v.head.startFraction,v.head.endFraction,v.head.startGain,v.head.endGain,v.head.curvePower,v.head.control1Time,v.head.control1Progress,v.head.control2Time,v.head.control2Progress};
            for(size_t i=0;i<9;++i)h->setProperty(keys[i],headValues[i]);h->setProperty("mixed",backend::mixedEnvelopeToVar(v.head));
            o->setProperty("mixed",backend::mixedEnvelopeToVar(v));o->setProperty("head",head);entries.add(row);
        }
        auto* o=new juce::DynamicObject();juce::var root(o);o->setProperty("version",4);o->setProperty("presets",entries);
        juce::TemporaryFile temp(file);
        if(!temp.getFile().replaceWithText(juce::JSON::toString(root,false,17))||!temp.overwriteTargetFileWithTemporary())return fail(error,"预设保存失败，请检查目录写入权限。");
        return true;
    }
    juce::File file;
};
}
