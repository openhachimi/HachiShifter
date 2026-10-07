#pragma once
#include "ProjectModel.h"
#include "Theme.h"
#include "OtoRegionGuides.h"
#include "TailFadePresetStore.h"
#include "backend/AdvancedEnvelope.h"
#include "backend/UtauRenderer.h"
namespace hachi
{
// A draft editor: moving controls does not alter the document until Apply.
class AdvancedEnvelopePanel final : public juce::Component
{
public:
    struct PreviewNote
    {
        juce::String id, label;
        backend::UtauTailFadeSpan tail;
        std::vector<AmplitudeEnvelopePoint> base;
        std::vector<OtoRegionGuide> regions;
        std::optional<backend::UtauTailFadeSpan> head;
    };
    AdvancedEnvelopePanel(std::vector<PreviewNote> notes,int selectedCount,int initialMode,
                          backend::TailFadeSettings initial,bool mixed,juce::File presetFile = {})
        : presetStore(std::move(presetFile)),previews(std::move(notes)),graph(*this)
    {
        addAndMakeVisible(title);title.setText(text("OTO 音头与音尾包络"),juce::dontSendNotification);title.setFont(juce::FontOptions(21.0f));
        addAndMakeVisible(summary);summary.setText(text("选中 ")+juce::String(selectedCount)+text(" 个音符，可用 ")+juce::String((int)previews.size())+text(" 个")
            +(mixed?text(" · 设置不同，应用时统一为当前参数"):juce::String()),juce::dontSendNotification);
        enabled.setButtonText(text("启用尾段淡出"));enabled.setToggleState(initialMode!=0,juce::dontSendNotification);addAndMakeVisible(enabled);
        shape.addItem(text("线性"),1);shape.addItem(text("曲线"),2);shape.setSelectedId(initialMode==2?2:1,juce::dontSendNotification);addAndMakeVisible(shape);
        addAndMakeVisible(shapeLabel);shapeLabel.setText(text("曲线形状"),juce::dontSendNotification);
        for(size_t i=0;i<previews.size();++i)preview.addItem(previews[i].label+" · "+juce::String((int)i+1),(int)i+1);
        preview.setSelectedId(1,juce::dontSendNotification);addAndMakeVisible(preview);addAndMakeVisible(previewLabel);previewLabel.setText(text("预览音符"),juce::dontSendNotification);
        section.addItem(text("音尾 · 最后一区"),1);section.addItem(text("音头 · 第一区"),2);
        section.setSelectedId(1,juce::dontSendNotification);addAndMakeVisible(section);
        section.onChange=[this]{const auto v=values();const auto m=mode();const auto sh=tailShape();activeSection=section.getSelectedId()-1;setValues(m,v);draftTailShape=sh;if(!editingHead())shape.setSelectedId(sh,juce::dontSendNotification);refresh();};
        addAndMakeVisible(graph);
        const char* captions[]{"淡出起点 · 尾段 %","淡出终点 · 尾段 %","淡出前响度 · %","结束响度 · %","曲线强度"};
        for(size_t i=0;i<sliders.size();++i)
        {
            addAndMakeVisible(labels[i]);labels[i].setText(text(captions[i]),juce::dontSendNotification);
            addAndMakeVisible(sliders[i]);sliders[i].setSliderStyle(juce::Slider::LinearHorizontal);sliders[i].setTextBoxStyle(juce::Slider::TextBoxRight,false,94,27);
            sliders[i].setRange(i==4?.25:0.0,i<2?100.0:i<4?200.0:4.0,i==4?.05:.1);
            sliders[i].setTextValueSuffix(i==4?" x":" %");
            sliders[i].onValueChange=[this,i]{adjustLimits(i);refresh();};
        }
        sliders[4].setSkewFactorFromMidPoint(1.0);
        const char* curveCaptions[]{"控制点 1 · 时间 %","控制点 1 · 过渡 %","控制点 2 · 时间 %","控制点 2 · 过渡 %"};
        for(size_t i=0;i<curveControls.size();++i)
        {
            addAndMakeVisible(curveLabels[i]);curveLabels[i].setText(text(curveCaptions[i]),juce::dontSendNotification);
            addAndMakeVisible(curveControls[i]);auto& c=curveControls[i];c.setSliderStyle(juce::Slider::LinearHorizontal);
            c.setTextBoxStyle(juce::Slider::TextBoxRight,false,62,26);c.setRange(0,100);c.setNumDecimalPlacesToDisplay(1);c.setTextValueSuffix(" %");
            c.onValueChange=[this,i]{
                const auto other=i<2?i+2:i-2;
                const auto limit=curveCoordinates[other]*100;
                curveControls[i].setValue(i<2?std::min(limit,curveControls[i].getValue()):std::max(limit,curveControls[i].getValue()),juce::dontSendNotification);
                beginControlEditing();curveCoordinates[i]=curveControls[i].getValue()/100;refresh();
            };
        }
        addAndMakeVisible(preset);preset.onChange=[this]{applyPreset(preset.getSelectedId());};
        savePresetButton.setButtonText(text("保存为预设…"));deletePresetButton.setButtonText(text("删除预设"));
        addAndMakeVisible(savePresetButton);addAndMakeVisible(deletePresetButton);
        savePresetButton.onClick=[this]{promptSavePreset();};deletePresetButton.onClick=[this]{deleteSelectedPreset();};


        addAndMakeVisible(detail);detail.setFont(juce::FontOptions(13.0f));
        addAndMakeVisible(help);help.setFont(juce::FontOptions(13.0f));help.setJustificationType(juce::Justification::topLeft);
        help.setText(text("位置按 OTO 尾段比例跟随音符；可拖动预览中的两端圆点。淡出从起点响度连续降至结束响度；原包络收尾由所选直线或曲线替代。曲线强度 < 1 先快后慢，> 1 先慢后快。"),juce::dontSendNotification);
        addAndMakeVisible(status);status.setFont(juce::FontOptions(13.0f));
        reset.setButtonText(text("恢复默认"));apply.setButtonText(text("应用"));listen.setButtonText(text("应用并试听"));close.setButtonText(text("关闭"));
        for(auto* button:{&reset,&apply,&listen,&close})addAndMakeVisible(button);
        reset.onClick=[this]{backend::TailFadeSettings v;if(editingHead())backend::copyEnvelopeShape(backend::HeadEnvelopeSettings{},v);setControls(1,v);status.setText(text("已恢复默认草稿，点击应用生效。"),juce::dontSendNotification);};
        apply.onClick=[this]{commit(false);};listen.onClick=[this]{commit(true);};
        close.onClick=[this]{if(auto* window=findParentComponentOfClass<juce::DialogWindow>())window->exitModalState(0);};
        enabled.onClick=[this]{refresh();};shape.onChange=[this]{if(shape.getSelectedId()==2)beginControlEditing();refresh();};preview.onChange=[this]{refresh();};
        reloadPresets();setValues(initialMode,initial);setSize(820,680);
    }
    std::function<juce::String(int,const backend::TailFadeSettings&,bool)> onApply;
    bool editingHead() const {return activeSection==1;}
    int editingMode() const {return enabled.getToggleState()?std::min(2,shape.getSelectedId()):0;}
    int mode() const {return editingHead()?draftTailMode:editingMode();}
    int tailShape() const {return editingHead()?draftTailShape:shape.getSelectedId();}
    backend::UtauTailFadeSpan editingSpan(const PreviewNote& n) const {return editingHead()?n.head.value_or(backend::UtauTailFadeSpan{}):n.tail;}
    backend::TailFadeSettings values() const
    {
        const auto controls=editingValues();auto result=editingHead()?draft:controls;
        if(editingHead()){backend::copyEnvelopeShape(controls,result.head);result.head.mode=editingMode();}
        else result.head=draft.head;
        return result;
    }
    void setValues(int mode,const backend::TailFadeSettings& v)
    {
        draft=v;draftTailMode=mode;draftTailShape=(v.customCurve||mode==2)?2:1;
        if(editingHead()){backend::TailFadeSettings h;backend::copyEnvelopeShape(v.head,h);setControls(v.head.mode,h);}
        else setControls(mode,v);
    }
    backend::TailFadeSettings editingValues() const
    {
        backend::TailFadeSettings v{sliders[0].getValue()/100,sliders[1].getValue()/100,sliders[2].getValue()/100,sliders[3].getValue()/100,sliders[4].getValue()};
        v.customCurve=shape.getSelectedId()==2&&bezierCurve;v.control1Time=curveCoordinates[0];v.control1Progress=curveCoordinates[1];
        v.control2Time=curveCoordinates[2];v.control2Progress=curveCoordinates[3];return v;
    }
    void setControls(int mode,const backend::TailFadeSettings& v)
    {
        bezierCurve=v.customCurve;enabled.setToggleState(mode!=0,juce::dontSendNotification);shape.setSelectedId(mode==1?1:(v.customCurve||mode==2)?2:1,juce::dontSendNotification);
        const double x[]{v.startFraction*100,v.endFraction*100,v.startGain*100,v.endGain*100,v.curvePower};
        for(size_t i=0;i<sliders.size();++i)sliders[i].setValue(x[i],juce::dontSendNotification);
        curveCoordinates={v.control1Time,v.control1Progress,v.control2Time,v.control2Progress};
        const auto& controls=curveCoordinates;
        for(size_t i=0;i<curveControls.size();++i)curveControls[i].setValue(controls[i]*100,juce::dontSendNotification);refresh();
    }
    void diagnosticSection(bool head) {section.setSelectedId(head?2:1,juce::sendNotificationSync);}
    int diagnosticShapeCount() const {return shape.getNumItems();}
    void diagnosticSelectShape(int id) {shape.setSelectedId(id,juce::sendNotificationSync);}
    void diagnosticSetControl(size_t i,double value) {sliders.at(i).setValue(value,juce::sendNotificationSync);}
    bool diagnosticSavePreset(const juce::String& name) {return saveCurrentPreset(name);}
    void diagnosticDeletePreset() {deleteSelectedPreset();}
    size_t diagnosticPresetCount() const {return userPresets.size();}
    void diagnosticPreset(int id) {applyPreset(id);}
    void diagnosticCurveControl(size_t i,double value) {curveControls.at(i).setValue(value,juce::sendNotificationSync);}
    void diagnosticDragCurveHandle(int index,float dx,float dy) {graph.dragControl(index,graph.controlPoint(index)+juce::Point<float>{dx,dy});}
    void diagnosticApply(bool audition=false) {commit(audition);}
    void diagnosticSelectPreview(int index) {preview.setSelectedId(index+1,juce::dontSendNotification);refresh();}
    std::vector<OtoRegionGuide> diagnosticPreviewRegions() const {return current()?current()->regions:std::vector<OtoRegionGuide>{};}
    bool diagnosticControlsFit() const
    {
        for(const auto& slider:sliders)if(slider.isVisible()&&(!getLocalBounds().contains(slider.getBounds())||slider.getWidth()<250))return false;
        for(const auto& c:curveControls)if(c.isVisible()&&(!getLocalBounds().contains(c.getBounds())||c.getWidth()<150))return false;
        return graph.getHeight()>=140&&help.getHeight()>=36&&close.getHeight()>=30&&getLocalBounds().contains(close.getBounds());
    }
    void paint(juce::Graphics& g) override {g.fillAll(Palette::panel);}
    void resized() override
    {
        auto area=getLocalBounds().reduced(20);
        auto heading=area.removeFromTop(30);section.setBounds(heading.removeFromRight(190));title.setBounds(heading);summary.setBounds(area.removeFromTop(26));area.removeFromTop(8);
        auto row=area.removeFromTop(30);enabled.setBounds(row.removeFromLeft(180));shapeLabel.setBounds(row.removeFromLeft(90));shape.setBounds(row.removeFromLeft(150));row.removeFromLeft(20);previewLabel.setBounds(row.removeFromLeft(82));preview.setBounds(row);
        area.removeFromTop(6);auto presetRow=area.removeFromTop(28);deletePresetButton.setBounds(presetRow.removeFromRight(90));presetRow.removeFromRight(8);savePresetButton.setBounds(presetRow.removeFromRight(116));presetRow.removeFromRight(8);preset.setBounds(presetRow);
        area.removeFromTop(4);graph.setBounds(area.removeFromTop(std::max(140,area.getHeight()-(shape.getSelectedId()==2?(bezierCurve?344:366):310))));area.removeFromTop(6);
        detail.setBounds(area.removeFromTop(24));
        for(size_t i=0;i<(shape.getSelectedId()==2&&bezierCurve?4:5);++i){auto control=area.removeFromTop(shape.getSelectedId()==2&&!bezierCurve?30:34);labels[i].setBounds(control.removeFromLeft(175));sliders[i].setBounds(control);}
        if(shape.getSelectedId()==2)for(size_t i=0;i<4;i+=2){auto row=area.removeFromTop(bezierCurve?34:28);auto left=row.removeFromLeft(row.getWidth()/2);row.removeFromLeft(10);
            curveLabels[i].setBounds(left.removeFromLeft(130));curveControls[i].setBounds(left);
            curveLabels[i+1].setBounds(row.removeFromLeft(130));curveControls[i+1].setBounds(row);}
        area.removeFromTop(4);help.setBounds(area.removeFromTop(40));status.setBounds(area.removeFromTop(26));area.removeFromTop(6);
        auto buttons=area.removeFromTop(34);reset.setBounds(buttons.removeFromLeft(116));close.setBounds(buttons.removeFromRight(94));buttons.removeFromRight(8);apply.setBounds(buttons.removeFromRight(94));buttons.removeFromRight(8);listen.setBounds(buttons.removeFromRight(144));
    }
private:
    static juce::String text(const char* s){return juce::String::fromUTF8(s);}
    const PreviewNote* current() const {const auto i=preview.getSelectedId()-1;return i>=0&&i<(int)previews.size()?&previews[(size_t)i]:nullptr;}
    void adjustLimits(size_t changed)
    {
        if(changed==0&&sliders[0].getValue()>sliders[1].getValue()-.2)sliders[0].setValue(sliders[1].getValue()-.2,juce::dontSendNotification);
        if(changed==1&&sliders[1].getValue()<sliders[0].getValue()+.2)sliders[1].setValue(sliders[0].getValue()+.2,juce::dontSendNotification);
        if(!editingHead()&&changed==2&&sliders[3].getValue()>sliders[2].getValue())sliders[3].setValue(sliders[2].getValue(),juce::dontSendNotification);
        if(!editingHead()&&changed==3&&sliders[3].getValue()>sliders[2].getValue())sliders[3].setValue(sliders[2].getValue(),juce::dontSendNotification);
    }
    void beginControlEditing()
    {
        // Keep legacy powered S curves intact until the user edits their handles.
        // New curves and handle edits share the Bezier editor, without a second mode.
        if(!bezierCurve){bezierCurve=true;sliders[4].setValue(1,juce::dontSendNotification);}
    }
    void applyPreset(int id)
    {
        if(id>=100)
        {
            const auto index=(size_t)(id-100);if(index>=userPresets.size())return;const auto p=userPresets[index];
            setValues(p.enabled?(p.shape==1?1:2):0,p.settings);draftTailShape=std::min(2,p.shape);if(!editingHead())shape.setSelectedId(draftTailShape,juce::dontSendNotification);
            preset.setSelectedId(id,juce::dontSendNotification);refresh();
            status.setText(text("已载入预设：")+p.name+text("。点击应用生效。"),juce::dontSendNotification);return;
        }
        if(id<2)return;auto v=editingValues();v.customCurve=true;v.curvePower=1;
        v.control1Time=1.0/3;v.control2Time=2.0/3;
        v.control1Progress=id==3?.75:id==5?1.0/3:0;
        v.control2Progress=id==4?.25:id==5?2.0/3:1;
        setControls(2,v);preset.setSelectedId(1,juce::dontSendNotification);refresh();
    }
    void reloadPresets(const juce::String& selected = {})
    {
        juce::String error;std::vector<TailFadePreset> loaded;
        if(!presetStore.load(loaded,error))status.setText(error,juce::dontSendNotification);
        userPresets=std::move(loaded);preset.clear(juce::dontSendNotification);
        preset.addItem(text("选择预设…"),1);preset.addSectionHeading(text("内置曲线"));
        preset.addItem(text("平滑 S 曲线"),2);preset.addItem(text("先快后慢"),3);preset.addItem(text("先慢后快"),4);preset.addItem(text("均匀淡出"),5);
        int selectedId=1;if(!userPresets.empty())preset.addSectionHeading(text("自定义预设"));
        for(size_t i=0;i<userPresets.size();++i){preset.addItem(userPresets[i].name,(int)i+100);if(userPresets[i].id==selected)selectedId=(int)i+100;}
        preset.setSelectedId(selectedId,juce::dontSendNotification);
    }
    bool saveCurrentPreset(const juce::String& name)
    {
        TailFadePreset p;p.name=name;p.shape=values().customCurve?3:tailShape();p.enabled=mode()!=0;p.settings=values();juce::String error;
        if(!presetStore.add(p,error)){status.setText(error,juce::dontSendNotification);return false;}
        reloadPresets(p.id);refresh();status.setText(text("已保存预设：")+p.name,juce::dontSendNotification);return true;
    }
    void promptSavePreset()
    {
        auto* dialog=new juce::AlertWindow(text("保存自定义预设"),text("同时保存音头和音尾的曲线、范围、起止响度和启用状态。"),juce::MessageBoxIconType::NoIcon);
        dialog->addTextEditor("name","",text("预设名称"));dialog->getTextEditor("name")->setInputRestrictions(64);
        dialog->addButton(text("保存"),1,juce::KeyPress(juce::KeyPress::returnKey));dialog->addButton(text("取消"),0,juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<AdvancedEnvelopePanel> safe(this);
        dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,dialog](int result){if(safe&&result==1)safe->saveCurrentPreset(dialog->getTextEditorContents("name"));delete dialog;}),false);
    }
    void deleteSelectedPreset()
    {
        const auto index=preset.getSelectedId()-100;if(index<0||index>=(int)userPresets.size())return;
        const auto p=userPresets[(size_t)index];juce::String error;
        if(!presetStore.remove(p.id,error)){status.setText(error,juce::dontSendNotification);return;}
        reloadPresets();refresh();status.setText(text("已删除预设：")+p.name+text("。当前草稿保留。"),juce::dontSendNotification);
    }
    void refresh()
    {
        const auto active=editingMode()!=0;for(auto& slider:sliders)slider.setEnabled(active);shape.setEnabled(active);sliders[4].setEnabled(active&&editingMode()==2&&!editingValues().customCurve);
        const bool custom=editingValues().customCurve;const bool curved=shape.getSelectedId()==2;sliders[4].setVisible(!custom);labels[4].setVisible(!custom);
        for(size_t i=0;i<4;++i){curveControls[i].setVisible(curved);curveLabels[i].setVisible(curved);curveControls[i].setEnabled(active);}
        preset.setEnabled(true);savePresetButton.setEnabled(presetStore.available());deletePresetButton.setEnabled(preset.getSelectedId()>=100);
        help.setText(text(custom?"拖动方形控制柄改变曲线；时间决定横向位置，过渡决定响度变化进度。按住 Shift 精细拖动，双击控制柄恢复默认。圆点调整起止位置。控制点保持顺序，避免响度反弹。":"圆点调整起止位置。曲线模式可拖动控制柄或输入参数；已有 S 曲线保留原强度，调整控制点后改用控制点定义曲线。"),juce::dontSendNotification);
        enabled.setButtonText(text(editingHead()?"启用音头包络":"启用尾段淡出"));
        const char* headLabels[]{"包络起点 · 首区 %","包络终点 · 首区 %","起点响度 · %","结束响度 · %"};
        const char* tailLabels[]{"淡出起点 · 尾段 %","淡出终点 · 尾段 %","淡出前响度 · %","结束响度 · %"};
        for(size_t i=0;i<4;++i)labels[i].setText(text(editingHead()?headLabels[i]:tailLabels[i]),juce::dontSendNotification);
        if(editingHead())help.setText(text("音头按 OTO 第一区定位，替代原包络起音；结束响度作为后续原包络的倍率。可拖动圆点和曲线控制柄。两侧独立启用，预设同时保存两侧设置。"),juce::dontSendNotification);
        resized();
        if(const auto* n=current())
        {
            const auto v=editingValues();const auto span=editingSpan(*n);const auto length=span.endSeconds-span.startSeconds;
            detail.setText(length<=1.e-9?text("当前音符没有有效的此区域，渲染时跳过此侧包络。"):text(editingHead()?"OTO 音头：":"OTO 尾段：")+juce::String(span.startSeconds*1000,1)+" – "+juce::String(span.endSeconds*1000,1)+text(" ms  ·  包络：")
                +juce::String((span.startSeconds+length*v.startFraction)*1000,1)+" – "+juce::String((span.startSeconds+length*v.endFraction)*1000,1)+text(" ms（相对音符起点）"),juce::dontSendNotification);
        }
        graph.repaint();
    }
    void commit(bool audition)
    {
        if(!values().valid()){status.setText(text("请检查范围和响度：终点应晚于起点，结束响度不大于淡出前响度。"),juce::dontSendNotification);return;}
        if(onApply)status.setText(onApply(mode(),values(),audition),juce::dontSendNotification);
    }
    struct Graph final : juce::Component
    {
        explicit Graph(AdvancedEnvelopePanel& p):owner(p){setMouseCursor(juce::MouseCursor::PointingHandCursor);}
        juce::Rectangle<float> plot() const {return getLocalBounds().toFloat().withTrimmedTop(28).withTrimmedBottom(25).withTrimmedLeft(47).withTrimmedRight(15);}
        double first(const PreviewNote& n) const {return std::min(n.base.empty()?0.0:n.base.front().timeSeconds,n.tail.startSeconds);}
        double last(const PreviewNote& n) const {return std::max(n.base.empty()?n.tail.endSeconds:n.base.back().timeSeconds,n.tail.endSeconds);}
        float x(double time,const PreviewNote& n) const {return plot().getX()+(float)((time-first(n))/std::max(1.e-9,last(n)-first(n)))*plot().getWidth();}
        float gainAt(const PreviewNote& n,double t) const
        {
            if(n.base.empty())return 1;const auto after=std::upper_bound(n.base.begin(),n.base.end(),t,[](double at,const auto& p){return at<p.timeSeconds;});
            if(after==n.base.begin())return backend::envelopeGainFromDb(n.base.front().gainDb);
            if(after==n.base.end())return backend::envelopeGainFromDb(n.base.back().gainDb);
            const auto& a=*std::prev(after);const auto& b=*after;
            return backend::envelopeGainFromDb(backend::envelopeDbBetween(a.gainDb,b.gainDb,(float)((t-a.timeSeconds)/std::max(1.e-9,b.timeSeconds-a.timeSeconds)),a.linearToNext));
        }
        float resultAt(double t,const PreviewNote& n) const
        {
            return backend::advancedEnvelopeGain(owner.mode(),t,n.tail.startSeconds,n.tail.endSeconds,
                n.head?n.head->startSeconds:0,n.head?n.head->endSeconds:0,owner.values(),[&](double at){return gainAt(n,at);});
        }
        float anchor(const PreviewNote& n,double start,double end) const
        {
            const auto v=owner.values();
            if(owner.editingHead())return gainAt(n,end)*(owner.mode()!=0?(float)v.startGain:1.0f);
            return backend::headEnvelopeGain(start,n.head?n.head->startSeconds:0,n.head?n.head->endSeconds:0,v.head,[&](double at){return gainAt(n,at);});
        }
        float ceiling() const
        {
            float c=1.2f;if(const auto* n=owner.current())
            {
                for(const auto& p:n->base)c=std::max(c,backend::envelopeGainFromDb(p.gainDb)*1.1f);
                for(int i=0;i<=64;++i)c=std::max(c,resultAt(first(*n)+(last(*n)-first(*n))*i/64.0,*n)*1.1f);
            }return c;
        }
        juce::Point<float> controlPoint(int index) const
        {
            const auto* n=owner.current();if(!n)return {};const auto v=owner.editingValues();const auto len=owner.editingSpan(*n).endSeconds-owner.editingSpan(*n).startSeconds;
            const auto start=owner.editingSpan(*n).startSeconds+len*v.startFraction,end=owner.editingSpan(*n).startSeconds+len*v.endFraction;
            const double times[]{0,v.control1Time,v.control2Time,1},progress[]{0,v.control1Progress,v.control2Progress,1};
            return {x(start+(end-start)*times[index],*n),plot().getBottom()-plot().getHeight()*anchor(*n,start,end)*(float)(v.startGain+(v.endGain-v.startGain)*progress[index])/ceiling()};
        }
        void dragControl(int index,juce::Point<float> at)
        {
            if(index<1||index>2||owner.editingMode()!=2)return;
            const auto a=controlPoint(0),b=controlPoint(3);const auto offset=(size_t)(index-1)*2;
            owner.curveControls[offset].setValue(100*(at.x-a.x)/std::max(1.e-6f,b.x-a.x),juce::sendNotificationSync);
            if(std::abs(b.y-a.y)>1.e-6f)owner.curveControls[offset+1].setValue(100*(at.y-a.y)/(b.y-a.y),juce::sendNotificationSync);
        }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(Palette::graphBackground);g.setFont(juce::FontOptions(13.0f));g.setColour(Palette::textMuted);g.drawText(text("实线：原包络    绿色虚线：实际响度    彩色分区：OTO"),12,3,getWidth()-24,22,juce::Justification::centredLeft);
            const auto* n=owner.current();if(!n)return;const auto bounds=plot();const auto v=owner.editingValues();const auto length=owner.editingSpan(*n).endSeconds-owner.editingSpan(*n).startSeconds;
            const auto start=owner.editingSpan(*n).startSeconds+length*v.startFraction,end=owner.editingSpan(*n).startSeconds+length*v.endFraction;
            g.setColour(Palette::textMuted.withAlpha(.08f));g.fillRect(juce::Rectangle<float>(x(owner.editingSpan(*n).startSeconds,*n),bounds.getY(),x(owner.editingSpan(*n).endSeconds,*n)-x(owner.editingSpan(*n).startSeconds,*n),bounds.getHeight()));
            if(owner.editingMode()!=0){g.setColour(juce::Colour(0xff72d6aa).withAlpha(.1f));g.fillRect(juce::Rectangle<float>(x(start,*n),bounds.getY(),x(end,*n)-x(start,*n),bounds.getHeight()));}
            paintOtoRegionGuides(g,bounds,n->regions,[&](double time){return x(time,*n);});
            const auto ceiling=this->ceiling();
            for(int i=0;i<=4;++i){const auto y=bounds.getBottom()-bounds.getHeight()*i/4;g.setColour(Palette::grid);g.drawHorizontalLine((int)y,bounds.getX(),bounds.getRight());g.setColour(Palette::textMuted);g.drawText(juce::String((int)(ceiling*100*i/4))+"%",0,(int)y-8,43,16,juce::Justification::centredRight);}
            juce::Path original,result;
            for(int i=0;i<=512;++i)
            {
                const auto t=first(*n)+(last(*n)-first(*n))*i/512.0;const auto base=gainAt(*n,t);
                const juce::Point<float> a{x(t,*n),bounds.getBottom()-bounds.getHeight()*base/ceiling};
                const juce::Point<float> b{x(t,*n),bounds.getBottom()-bounds.getHeight()*resultAt(t,*n)/ceiling};
                if(i==0){original.startNewSubPath(a);result.startNewSubPath(b);}else{original.lineTo(a);result.lineTo(b);}
            }
            g.setColour(Palette::textMuted);g.strokePath(original,juce::PathStrokeType(1.6f));
            juce::Path dashed;const float dash[]{6,4};juce::PathStrokeType(2).createDashedStroke(dashed,result,dash,2);g.setColour(juce::Colour(0xff72d6aa));g.fillPath(dashed);
            if(owner.editingMode()!=0)for(auto t:{start,end}){const auto at=x(t,*n);g.drawVerticalLine((int)at,bounds.getY(),bounds.getBottom());g.fillEllipse(at-5,bounds.getY()-5,10,10);}
            if(owner.editingMode()==2)
            {
                g.setColour(juce::Colour(0xffffc86a));
                g.drawLine(juce::Line<float>(controlPoint(0),controlPoint(1)),1.2f);g.drawLine(juce::Line<float>(controlPoint(3),controlPoint(2)),1.2f);
                for(int i=1;i<=2;++i){const auto p=controlPoint(i);g.fillRect(juce::Rectangle<float>(p.x-5,p.y-5,10,10));
                    g.drawText(juce::String(i),(int)p.x+7,(int)p.y-20,16,18,juce::Justification::centred);}
            }
            g.setColour(Palette::textMuted);g.drawText(juce::String(first(*n)*1000,0)+" ms",(int)bounds.getX(),(int)bounds.getBottom()+3,90,20,juce::Justification::centredLeft);g.drawText(juce::String(last(*n)*1000,0)+" ms",(int)bounds.getRight()-90,(int)bounds.getBottom()+3,90,20,juce::Justification::centredRight);
        }
        void mouseDown(const juce::MouseEvent& e) override
        {
            dragged=-1;draggedControl=-1;if(owner.editingMode()==0)return;
            if(owner.editingMode()==2)for(int i=1;i<=2;++i)if(e.position.getDistanceFrom(controlPoint(i))<=12){draggedControl=i;dragOrigin=e.position;controlOrigin=controlPoint(i);return;}if(const auto* n=owner.current())
            {const auto v=owner.editingValues();const auto len=owner.editingSpan(*n).endSeconds-owner.editingSpan(*n).startSeconds;const auto a=x(owner.editingSpan(*n).startSeconds+len*v.startFraction,*n),b=x(owner.editingSpan(*n).startSeconds+len*v.endFraction,*n);if(std::min(std::abs(e.position.x-a),std::abs(e.position.x-b))<=12)dragged=std::abs(e.position.x-a)<std::abs(e.position.x-b)?0:1;}
        }
        void mouseDrag(const juce::MouseEvent& e) override
        {
            if(draggedControl>0){dragControl(draggedControl,controlOrigin+(e.position-dragOrigin)*(e.mods.isShiftDown()?.15f:1.0f));return;}
            if(dragged<0)return;if(const auto* n=owner.current();n&&owner.editingSpan(*n).endSeconds-owner.editingSpan(*n).startSeconds>1.e-9)
            {const auto t=first(*n)+(e.position.x-plot().getX())/plot().getWidth()*(last(*n)-first(*n));owner.sliders[(size_t)dragged].setValue(100*(t-owner.editingSpan(*n).startSeconds)/(owner.editingSpan(*n).endSeconds-owner.editingSpan(*n).startSeconds),juce::sendNotificationSync);}
        }
        void mouseDoubleClick(const juce::MouseEvent& e) override
        {
            if(owner.editingMode()!=2)return;
            for(int i=1;i<=2;++i)if(e.position.getDistanceFrom(controlPoint(i))<=12)
            {owner.curveControls[(size_t)(i-1)*2].setValue(i*100.0/3,juce::sendNotificationSync);owner.curveControls[(size_t)(i-1)*2+1].setValue(i==1?0:100,juce::sendNotificationSync);break;}
        }
        void mouseUp(const juce::MouseEvent&) override {dragged=-1;draggedControl=-1;}
        AdvancedEnvelopePanel& owner;int dragged=-1,draggedControl=-1;juce::Point<float> dragOrigin,controlOrigin;
    };
    backend::TailFadeSettings draft;int draftTailMode=0,draftTailShape=1,activeSection=0;
    bool bezierCurve=false;
    TailFadePresetStore presetStore;std::vector<TailFadePreset> userPresets;
    std::vector<PreviewNote> previews;
    juce::Label title,summary,shapeLabel,previewLabel,detail,help,status;
    juce::ToggleButton enabled;juce::ComboBox shape,preview,preset,section;
    Graph graph;std::array<juce::Label,5> labels;std::array<juce::Slider,5> sliders;
    std::array<double,4> curveCoordinates{1.0/3.0,0,2.0/3.0,1};
    std::array<juce::Label,4> curveLabels;std::array<juce::Slider,4> curveControls;
    juce::TextButton reset,apply,listen,close,savePresetButton,deletePresetButton;
};
}
