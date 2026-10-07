#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <optional>
#include <vector>
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #pragma push_macro("small")
 #pragma push_macro("near")
 #pragma push_macro("far")
 #include <windows.h>
 #pragma pop_macro("far")
 #pragma pop_macro("near")
 #pragma pop_macro("small")
#elif __has_include(<iconv.h>)
 #include <iconv.h>
 #define HACHI_LEGACY_ICONV 1
#endif
namespace hachi::backend
{
// The bytes, codec and edit baseline travel together. Detection never writes to a bank.
struct LegacyTextDocument
{
    juce::String text, encoding, fingerprint;
    juce::MemoryBlock bytes;
    int codePage=65001;
    bool bom=false, existed=false, guessed=false;
};
class LegacyTextCodec
{
public:
    static juce::String name(int cp)
    {switch(cp){case 65001:return "UTF-8";case 932:return "Shift-JIS";case 936:return "GBK";case 950:return "Big5";case 54936:return "GB18030";default:return "CP"+juce::String(cp);}}
    static int codePage(juce::String s)
    {s=s.trim().toLowerCase().removeCharacters("_ -");if(s=="utf8"||s=="65001")return 65001;if(s=="shiftjis"||s=="sjis"||s=="cp932"||s=="932")return 932;if(s=="gbk"||s=="cp936"||s=="936")return 936;if(s=="big5"||s=="cp950"||s=="950")return 950;if(s=="gb18030"||s=="54936")return 54936;return 0;}
    static juce::String digest(const juce::MemoryBlock& b){return juce::SHA256(b).toHexString();}
    static std::optional<juce::String> decodeBytes(const juce::MemoryBlock& bytes,int cp)
    {
        if(bytes.getSize()==0)return juce::String();const auto* data=static_cast<const char*>(bytes.getData());const auto size=bytes.getSize();
        // NUL is not legal in these text protocols (also catches unsupported UTF-16 input).
        for(size_t i=0;i<size;++i)if(data[i]==0)return std::nullopt;
#if JUCE_WINDOWS
        const auto count=MultiByteToWideChar((UINT)cp,MB_ERR_INVALID_CHARS,data,(int)size,nullptr,0);if(count<=0)return std::nullopt;
        std::vector<wchar_t> wide((size_t)count+1,0);if(MultiByteToWideChar((UINT)cp,MB_ERR_INVALID_CHARS,data,(int)size,wide.data(),count)!=count)return std::nullopt;
        return juce::String(wide.data(),(size_t)count);
#else
        if(cp==65001){if(!juce::CharPointer_UTF8::isValidString(data,(int)size))return std::nullopt;return juce::String::fromUTF8(data,(int)size);}
 #if HACHI_LEGACY_ICONV
        auto conv=iconv_open("UTF-8",cp==932?"CP932":name(cp).toRawUTF8());if(conv==(iconv_t)-1)return std::nullopt;
        std::vector<char> result(size*4+8);char* in=const_cast<char*>(data);char* out=result.data();size_t left=size,room=result.size();const auto status=iconv(conv,&in,&left,&out,&room);iconv_close(conv);
        if(status==(size_t)-1||left)return std::nullopt;return juce::String::fromUTF8(result.data(),(int)(result.size()-room));
 #else
        return std::nullopt;
 #endif
#endif
    }
    static bool encode(const juce::String& text,int cp,bool bom,juce::MemoryBlock& bytes,juce::String& error)
    {
        bytes.reset();if(cp==65001){if(bom){const unsigned char marker[]{0xef,0xbb,0xbf};bytes.append(marker,3);}bytes.append(text.toRawUTF8(),text.getNumBytesAsUTF8());return true;}
        if(text.isEmpty())return true;
#if JUCE_WINDOWS
        const std::wstring wide(text.toWideCharPointer());BOOL replaced=FALSE;const auto flags=cp==54936?WC_ERR_INVALID_CHARS:WC_NO_BEST_FIT_CHARS;auto* substitution=cp==54936?nullptr:&replaced;
        const auto count=WideCharToMultiByte((UINT)cp,flags,wide.data(),(int)wide.size(),nullptr,0,nullptr,substitution);
        if(count>0&&!replaced){bytes.setSize((size_t)count);replaced=FALSE;const auto n=WideCharToMultiByte((UINT)cp,flags,wide.data(),(int)wide.size(),(char*)bytes.getData(),count,nullptr,substitution);if(n==count&&!replaced){const auto back=decodeBytes(bytes,cp);if(back&&*back==text)return true;}}
#else
 #if HACHI_LEGACY_ICONV
        auto conv=iconv_open(cp==932?"CP932":name(cp).toRawUTF8(),"UTF-8");if(conv!=(iconv_t)-1){std::vector<char> result(text.getNumBytesAsUTF8()*4+8);char* in=const_cast<char*>(text.toRawUTF8());char* out=result.data();size_t left=text.getNumBytesAsUTF8(),room=result.size();const auto status=iconv(conv,&in,&left,&out,&room);iconv_close(conv);if(status!=(size_t)-1&&!left){bytes.append(result.data(),result.size()-room);return true;}}
 #endif
#endif
        bytes.reset();error=juce::String::fromUTF8("文字无法无损写入 ")+name(cp)+juce::String::fromUTF8("；请选择 UTF-8 导出或使用该编码可表示的字符。文件未修改。");return false;
    }
    static juce::File directoryConfig(const juce::File& dir){return dir.getChildFile("hachi-encoding.json");}
    static int directoryCodePage(const juce::File& dir)
    {const auto json=juce::JSON::parse(directoryConfig(dir));return codePage(json.getProperty("encoding",{}).toString());}
    static bool setDirectoryCodePage(const juce::File& dir,int cp,juce::String& error)
    {
        if(cp!=0&&cp!=65001&&cp!=932&&cp!=936&&cp!=950&&cp!=54936){error="Unsupported encoding";return false;}
        auto json=juce::JSON::parse(directoryConfig(dir));if(!json.isObject())json=new juce::DynamicObject();json.getDynamicObject()->setProperty("encoding",cp==0?"auto":name(cp));
        LegacyTextDocument original;const auto file=directoryConfig(dir);original.existed=file.existsAsFile();
        if(original.existed&&!file.loadFileAsData(original.bytes)){error="Could not read encoding preference";return false;}
        return write(file,juce::JSON::toString(json,true),original,error);
    }
    static int detect(const juce::MemoryBlock& bytes,const juce::File& directory={},int local=0)
    {
        if(decodeBytes(bytes,65001))return 65001;
#if JUCE_WINDOWS
        if(local==0)local=(int)GetACP();
#else
        if(local==0)local=932;
#endif
        std::vector<int> candidates{local,932,936,950,54936};int best=0;double bestScore=-1.e9;
        for(int cp:candidates)
        {
            const auto text=decodeBytes(bytes,cp);if(!text)continue;juce::MemoryBlock roundtrip;juce::String error;if(!encode(*text,cp,false,roundtrip,error)||roundtrip!=bytes)continue;
            double score=cp==local?2:0;
            for(auto ch:*text){if(ch>=0x3041&&ch<=0x3096)score+=3;else if(ch>=0x30a1&&ch<=0x30fa)score+=2;else if(ch>=0xff61&&ch<=0xff9f)score-=.8;}
            if(directory.isDirectory())for(const auto& line:juce::StringArray::fromLines(*text)){const auto eq=line.indexOfChar('=');if(eq>0){const auto path=line.substring(0,eq).trim();if(path.endsWithIgnoreCase(".wav")||path.endsWithIgnoreCase(".flac"))score+=directory.getChildFile(path.replaceCharacter('\\','/')).existsAsFile()?80:-30;}}
            if(score>bestScore){bestScore=score;best=cp;}
        }return best;
    }
    static std::optional<LegacyTextDocument> read(const juce::File& file,juce::String& error,int requested=0,bool bank=true)
    {
        LegacyTextDocument doc;doc.existed=file.existsAsFile();if(!doc.existed){error="File not found: "+file.getFullPathName();return std::nullopt;}if(!file.loadFileAsData(doc.bytes)){error="Could not read "+file.getFullPathName();return std::nullopt;}doc.fingerprint=digest(doc.bytes);
        auto content=doc.bytes;const auto* p=(const unsigned char*)content.getData();doc.bom=content.getSize()>=3&&p[0]==0xef&&p[1]==0xbb&&p[2]==0xbf;if(doc.bom)content.removeSection(0,3);
        int cp=requested;if(cp==0&&bank)cp=directoryCodePage(file.getParentDirectory());
        juce::String ascii; for(size_t i=0;i<std::min<size_t>(content.getSize(),160);++i){const auto ch=((const unsigned char*)content.getData())[i];ascii+=juce::String::charToString(ch<128?ch:32);}
        if(doc.bom||ascii.containsIgnoreCase("#Charset:UTF-8"))cp=65001;
        if(cp==0&&bank&&!file.getFileName().equalsIgnoreCase("oto.ini")&&!decodeBytes(content,65001))
        {juce::MemoryBlock oto;const auto sibling=file.getSiblingFile("oto.ini");if(sibling.loadFileAsData(oto))cp=detect(oto,file.getParentDirectory());}
        doc.guessed=cp==0;doc.codePage=cp!=0?cp:detect(content,bank?file.getParentDirectory():juce::File());
        const auto text=decodeBytes(content,doc.codePage);if(doc.codePage==0||!text){error=juce::String::fromUTF8("无法无损读取文件，请指定正确编码：")+file.getFullPathName();return std::nullopt;}
        doc.text=*text;doc.encoding=name(doc.codePage);return doc;
    }
    static bool write(const juce::File& file,const juce::String& text,const LegacyTextDocument& original,juce::String& error)
    {
        juce::MemoryBlock output;
        if(original.existed&&text==original.text)output=original.bytes;
        else if(!encode(text,original.codePage,original.bom,output,error))return false;
        return writeBytes(file,output,original,error);
    }
    static bool writeBytes(const juce::File& file,const juce::MemoryBlock& output,const LegacyTextDocument& original,juce::String& error)
    {
        juce::MemoryBlock now;const auto exists=file.existsAsFile();if(exists!=original.existed||(exists&&(!file.loadFileAsData(now)||now!=original.bytes))){error=juce::String::fromUTF8("文件在读取后被其他操作修改，请刷新后重试：")+file.getFullPathName();return false;}
        if(exists&&now==output)return true;
        // replaceWithData would create another TemporaryFile, extending long bank paths twice.
        const auto shortName="ht"+juce::String::toHexString((unsigned int)juce::Random::getSystemRandom().nextInt()&0xffffffu);
        juce::TemporaryFile temp(file,file.getParentDirectory().getNonexistentChildFile(shortName,"",false));
        auto stream=temp.getFile().createOutputStream();
        if(!stream){error="Could not create temporary file: "+temp.getFile().getFullPathName();return false;}
        const auto written=stream->setPosition(0)&&stream->truncate().wasOk()&&stream->write(output.getData(),output.getSize());
        stream->flush();const auto status=stream->getStatus();stream.reset();
        if(!written||status.failed()){error="Could not write temporary file: "+status.getErrorMessage();return false;}
        juce::MemoryBlock verified;
        if(!temp.getFile().loadFileAsData(verified)||verified!=output){error="Temporary file verification failed";return false;}
        now.reset();
        if(file.existsAsFile()!=exists||(exists&&(!file.loadFileAsData(now)||now!=original.bytes))){error="File changed while saving";return false;}
        if(!temp.overwriteTargetFileWithTemporary()){error="Could not replace "+file.getFullPathName();return false;}return true;
    }
};
}
