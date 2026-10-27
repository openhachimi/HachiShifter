#pragma once
#include "../NativeVirtualRenderPlan.h"
#include <iostream>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#ifdef small
#undef small
#endif
#endif
namespace hachi
{
bool MainComponent::diagnosticRealMultiBridgeWindow(const juce::File& file,const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String log,error;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto finish=[&]{log+="manual_UI_and_listening_acceptance=NOT_PERFORMED; native_window_scale/DPI_are_measured_automated_evidence\n";output.getChildFile("verification.txt").replaceWithText(log);return ok;};
#if !JUCE_WINDOWS
    check("actual_Windows_window_required",false);return finish();
#else
    const auto isolated=juce::SystemStats::getEnvironmentVariable("HACHI_TEST_SETTINGS_DIR",{});
    check("isolated_preferences_and_message_thread",preferences&&isolated.isNotEmpty()&&preferences->getFile().getFullPathName().contains(isolated)&&juce::MessageManager::getInstance()->isThisTheMessageThread());if(!ok)return finish();
    audio.stop();audio.devices().closeAudioDevice();stopTimer();pianoRoll.setReadsVoicebankInBackground(false);
    preferences->setValue("ui.uiScale",1.25);applyUiScale();
    check("actual_ui_preference_scale_is_125_percent",std::abs(preferences->getDoubleValue("ui.uiScale",0)-1.25)<1.e-8);
    check("real_saved_two_bridge_project_loaded",project.load(file,error));if(!ok)return finish();
    auto data=project.snapshot();check("seven_physical_slices_and_two_source_proofs_loaded",data.tracks.size()==1&&data.tracks[0].clips.size()==7&&data.nativeVirtualGroups.size()==1&&data.nativeVirtualGroups[0].extraSplitTransitions.size()==1);if(!ok)return finish();
    const auto baseline=project.contentFingerprint();const auto originalOrder=data.nativeVirtualGroups[0].orderedNoteIds;
    selectedTrackId=data.tracks[0].id;selectedClipId=data.tracks[0].clips.front().id;
    pianoRoll.setFocusedTrack(selectedTrackId);pianoRoll.setFocusedClip({});
    setAssetManagerVisible(false);zoomSlider.setValue(1450,juce::sendNotificationSync);vZoomSlider.setValue(1.,juce::sendNotificationSync);
    struct Window:juce::DocumentWindow
    {Window():DocumentWindow("Real virtual bridges — isolated validation",Palette::panel,DocumentWindow::allButtons){setUsingNativeTitleBar(true);setResizable(true,false);}void closeButtonPressed()override{setVisible(false);}};
    Window host;host.setContentNonOwned(this,false);host.centreWithSize(1320,780);host.setVisible(true);host.toFront(true);resized();
    const auto pump=[&]
    {
        MSG message{};for(int i=0;i<128&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++i){if(message.message==WM_QUIT){PostQuitMessage(int(message.wParam));return false;}TranslateMessage(&message);DispatchMessageW(&message);}
        project.dispatchPendingMessages();pianoRoll.diagnosticRefresh();return host.isVisible();
    };
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+1200.;while(juce::Time::getMillisecondCounterHiRes()<deadline){if(!pump())break;juce::Thread::sleep(5);}
    pianoViewport.setViewPosition(0,std::max(0,int(pianoRoll.diagnosticYForMidi(data.tracks[0].clips[3].notes[0].midiNote)-pianoViewport.getViewHeight()*.5f)));
    diagnosticPressTool(PianoRollComponent::Tool::note);pump();
    const auto native=host.getPeer()?static_cast<HWND>(host.getPeer()->getNativeHandle()):nullptr;
    check("real_native_HWND_is_visible",native&&IsWindowVisible(native));
    unsigned dpi=0;using DpiFunction=UINT(WINAPI*)(HWND);const auto dpiFunction=reinterpret_cast<DpiFunction>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"));if(dpiFunction&&native)dpi=dpiFunction(native);
    check("native_window_really_reports_the_requested_120_DPI",dpi==120);
    RECT actual{};if(native)GetWindowRect(native,&actual);
    MONITORINFO monitor{};monitor.cbSize=sizeof(MONITORINFO);const bool hasMonitor=native&&GetMonitorInfoW(MonitorFromWindow(native,MONITOR_DEFAULTTONEAREST),&monitor);
    if(hasMonitor&&native)
    {
        const int left=std::max(int(monitor.rcWork.left)+8,std::min(int(actual.left),int(monitor.rcWork.right-(actual.right-actual.left))-8));
        const int top=std::max(int(monitor.rcWork.top)+8,std::min(int(actual.top),int(monitor.rcWork.bottom-(actual.bottom-actual.top))-8));
        SetWindowPos(native,nullptr,left,top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);pump();GetWindowRect(native,&actual);
    }
    check("native_test_window_fits_the_actual_monitor_work_area",hasMonitor&&actual.left>=monitor.rcWork.left&&actual.top>=monitor.rcWork.top&&actual.right<=monitor.rcWork.right&&actual.bottom<=monitor.rcWork.bottom);
    log+="capture=PrintWindow-owned-HWND-only; input=JUCE-event-dispatch-on-real-Main-not-physical-cursor;\n";
    log+="recorded_native_window_DPI="+juce::String(int(dpi))+"; global_ui_scale="+juce::String(juce::Desktop::getInstance().getGlobalScaleFactor(),4)+"; logical_Main="+juce::String(getWidth())+"x"+juce::String(getHeight())+"; native_window="+juce::String(actual.right-actual.left)+"x"+juce::String(actual.bottom-actual.top)+"; origin="+juce::String(actual.left)+","+juce::String(actual.top)+"\n";
    const auto capture=[&](const juce::String& name)
    {
        pump();if(!native)return false;host.repaint();if(host.getPeer())host.getPeer()->performAnyPendingRepaintsNow();
        RECT rect{};if(!GetWindowRect(native,&rect))return false;const int width=rect.right-rect.left,height=rect.bottom-rect.top;if(width<=0||height<=0)return false;
        HDC screen=GetDC(nullptr),memory=CreateCompatibleDC(screen);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void* pixels=nullptr;
        HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);HGDIOBJ previous=nullptr;bool captured=false;juce::Image image;
        if(bitmap&&pixels)
        {
            previous=SelectObject(memory,bitmap);captured=PrintWindow(native,memory,2/* PW_RENDERFULLCONTENT: owned HWND only, never other desktop windows */)!=0;
            if(captured){image=juce::Image(juce::Image::ARGB,width,height,true,juce::SoftwareImageType{});juce::Image::BitmapData dest(image,juce::Image::BitmapData::writeOnly);const auto* bytes=static_cast<const unsigned char*>(pixels);for(int y=0;y<height;++y)for(int x=0;x<width;++x){const auto at=std::size_t(y*width+x)*4;dest.setPixelColour(x,y,juce::Colour(bytes[at+2],bytes[at+1],bytes[at]));}}
        }
        if(previous)SelectObject(memory,previous);if(bitmap)DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(nullptr,screen);
        if(!captured)return false;auto stream=output.getChildFile(name+".png").createOutputStream();return stream&&juce::PNGImageFormat().writeImageToStream(image,*stream);
    };
    check("all_bridge_halves_have_logical_display_identity",pianoRoll.diagnosticLogicalTransitionMemberCount()==4&&pianoRoll.diagnosticVirtualGroupViewCount()==1);
    check("actual_native_window_screenshot_saved",capture("01-real-main-note-tool"));
    const auto marker=pianoRoll.diagnosticVirtualMarkerBounds(0);
    const auto mouse=[&](juce::Point<float> at,juce::Point<float> down){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&pianoRoll,&pianoRoll,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
    if(!marker.isEmpty()){const auto point=marker.getCentre();pianoRoll.mouseDown(mouse(point,point));pianoRoll.mouseUp(mouse(point,point));pump();}
    check("real_window_marker_selects_exactly_the_seven_group_members",!marker.isEmpty()&&pianoRoll.diagnosticSelectedCount()==7&&project.contentFingerprint()==baseline);
    check("actual_native_contribution_card_screenshot_saved",pianoRoll.diagnosticExpandedVirtualGroup()==data.nativeVirtualGroups[0].id&&capture("02-real-main-source-contributions"));
    // Hand-edit only one local logical bridge, then use the real point tool
    // inside the complete Main viewport. Source media and group topology stay.
    auto current=project.snapshot();current.nativeVirtualGroups[0].automaticPitch=true;project.resetDocument(current);pump();diagnosticPressTool(PianoRollComponent::Tool::points);pump();
    const auto curves=pianoRoll.diagnosticConnectionPitchCurves();check("real_main_point_tool_has_two_independent_logical_bridge_windows",curves.size()==2);
    check("point_tool_dismisses_the_card_that_would_occlude_curve_handles",pianoRoll.diagnosticExpandedVirtualGroup().isEmpty());
    if(curves.size()==2)
    {
        const auto& curve=curves.front();const auto t=(curve.fromSeconds+curve.toSeconds)*.5;const std::vector<PitchCurveEditPoint> points{curve.points.front(),{t,evaluatePitchCurve(curve.points,t),PitchCurveShape::customBezier},curve.points.back()};
        check("real_curve_compact_controls_seeded_without_modifying_other_bridge",project.setNativeVirtualPitchCurve(curve.connectionId,points,error));pump();
        const auto point=pianoRoll.diagnosticConnectionPitchPoint(curve.connectionId,1);const auto before=project.contentFingerprint();
        const juce::Rectangle<float> visible(float(pianoViewport.getViewPositionX()),float(pianoViewport.getViewPositionY()),float(pianoViewport.getViewWidth()),float(pianoViewport.getViewHeight()));
        check("edited_control_is_inside_the_real_visible_viewport",point&&visible.contains(*point));
        if(point){const auto moved=point->translated(0,-7);pianoRoll.mouseDown(mouse(*point,*point));pianoRoll.mouseDrag(mouse(moved,*point));check("real_main_drag_preview_does_not_write_early",project.contentFingerprint()==before);pianoRoll.mouseUp(mouse(moved,*point));pump();}
        check("real_main_curve_drag_commits_one_local_group_payload",point&&project.contentFingerprint()!=before&&project.snapshot().nativeVirtualGroups[0].hopPitchCurves.size()==1&&project.snapshot().nativeVirtualGroups[0].orderedNoteIds==originalOrder);
        check("actual_native_point_tool_screenshot_saved",capture("03-real-main-local-curve"));
        project.undo();check("real_main_local_curve_drag_has_exact_single_undo",project.contentFingerprint()==before);pump();
    }
    const auto result=finish();host.setContentNonOwned(nullptr,false);host.setVisible(false);return result;
#endif
}
}
