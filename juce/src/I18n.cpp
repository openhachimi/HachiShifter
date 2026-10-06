#include "I18n.h"
#include <array>
#include <unordered_map>

namespace hachi
{
namespace
{
using Row = std::array<const char*, 5>;
const std::unordered_map<std::string, Row> strings {
    { "settings.softwareRendering", { "回退到软件绘制（关闭 GPU 界面加速）", "退回軟體繪製（關閉 GPU 介面加速）", "ソフトウェア描画に切替（GPU描画を無効化）", "소프트웨어 렌더링 사용 (GPU UI 가속 끄기)", "Use software rendering (disable GPU UI acceleration)" } },
    { "app.title",       { "HachiShifter Next", "HachiShifter Next", "HachiShifter Next", "HachiShifter Next", "HachiShifter Next" } },
    { "file.open",       { "打开工程", "開啟工程", "プロジェクトを開く", "프로젝트 열기", "Open Project" } },
    { "file.save",       { "保存工程", "儲存工程", "プロジェクトを保存", "프로젝트 저장", "Save Project" } },
    { "file.saveAs",     { "工程另存为…", "工程另存新檔…", "プロジェクトを別名で保存…", "프로젝트 다른 이름으로 저장…", "Save Project As…" } },
    { "native.range", { "范围", "範圍", "範囲", "범위", "Range" } },
    { "native.envelope", { "包络", "包絡", "エンベロープ", "엔벨로프", "Envelope" } },
    { "native.renderedWave", { "合成波形", "合成波形", "合成波形", "합성 파형", "Rendered waveform" } },
    { "native.pitchLine", { "音高线", "音高線", "ピッチカーブ", "피치 곡선", "Pitch curve" } },
    { "native.display", { "显示", "顯示", "表示", "표시", "Display" } },
    { "native.velocityHelp", { "辅音速度：100 为基准，增大会缩短起音；保持音符总时长和源选区。", "子音速度：100 為基準，增大会縮短起音；保留音符總時長和來源範圍。", "子音速度：100が基準。値を上げるとアタックが短くなります。ノート全長と素材範囲は維持されます。", "자음 속도: 100이 기준입니다. 높이면 어택이 짧아지며 음표 길이와 원본 범위는 유지됩니다.", "Consonant velocity: 100 is neutral; higher values shorten the onset while retaining note duration and source range." } },
    { "file.recent",     { "最近工程", "最近工程", "最近使ったプロジェクト", "최근 프로젝트", "Recent Projects" } },
    { "file.recentEmpty", { "没有最近工程", "沒有最近工程", "最近のプロジェクトはありません", "최근 프로젝트 없음", "No Recent Projects" } },
    { "file.export",     { "导出 WAV", "匯出 WAV", "WAVを書き出す", "WAV 내보내기", "Export WAV" } },
    { "file.exportLastRender", { "导出上次渲染音频", "匯出上次算繪音訊", "前回レンダリングした音声を書き出す", "마지막 렌더링 오디오 내보내기", "Export Last Render" } },
    { "export.chooseDestination", { "下一步：保存位置…", "下一步：儲存位置…", "次へ：保存先…", "다음: 저장 위치…", "Next: save location…" } },
    { "export.advanced", { "高级", "進階", "詳細設定", "고급", "Advanced" } },
    { "export.breathOnly", { "仅导出气声", "僅匯出氣聲", "息成分のみ", "기식 성분만 내보내기", "Export breath only" } },
    { "export.nonBreathOnly", { "仅导出非气声", "僅匯出非氣聲", "非息成分のみ", "비기식 성분만 내보내기", "Export non-breath only" } },
    { "export.componentHelp", { "两项互斥，均不选则导出完整人声。气声为引擎的噪声分量，可能包含清辅音；仅对本次导出生效。", "兩項互斥，均不選則匯出完整人聲。氣聲為引擎的噪聲分量，可能包含清子音；僅對本次匯出生效。", "未選択で全音声を書き出します。息成分には無声子音も含まれます。今回の書き出しのみに適用。", "둘 다 해제하면 전체 음성을 내보냅니다. 기식에는 무성 자음도 포함됩니다. 이번 내보내기에만 적용됩니다.", "Choose one or leave both off for the complete voice. Breath is the noise component and may include unvoiced consonants. Applies to this export only." } },
    { "export.wavSettings", { "WAV 导出设置", "WAV 匯出設定", "WAV 書き出し設定", "WAV 내보내기 설정", "WAV export settings" } },
    { "export.wavHelp", { "单声道会混合左右声道；32 bit 使用浮点格式。设置会记住并用于下次导出。", "單聲道會混合左右聲道；32 bit 使用浮點格式。設定會保留供下次匯出。", "モノラルは左右をミックスします。32 bit は浮動小数点です。設定は次回も保持されます。", "모노는 좌우 채널을 합칩니다. 32 bit는 부동소수점 형식입니다. 설정은 다음에도 유지됩니다.", "Mono mixes left and right channels. 32 bit uses floating point. Settings are remembered for the next export." } },
    { "export.channels", { "声道", "聲道", "チャンネル", "채널", "Channels" } },
    { "export.mono", { "单声道", "單聲道", "モノラル", "모노", "Mono" } },
    { "export.stereo", { "双声道（立体声）", "雙聲道（立體聲）", "ステレオ", "스테레오", "Stereo" } },
    { "export.bitDepth", { "位深", "位元深度", "ビット深度", "비트 깊이", "Bit depth" } },
    { "export.float32", { "32 bit 浮点", "32 bit 浮點", "32 bit 浮動小数点", "32 bit 부동소수점", "32 bit float" } },
    { "export.sampleRate", { "采样率", "取樣率", "サンプルレート", "샘플 레이트", "Sample rate" } },
    { "export.followDevice", { "跟随播放设备（默认）", "跟隨播放裝置（預設）", "再生デバイスに合わせる（既定）", "재생 장치에 맞춤 (기본값)", "Follow playback device (default)" } },
    { "export.lastRenderSuffix", { "上次渲染", "上次算繪", "前回のレンダリング", "마지막 렌더링", "last render" } },
    { "error.exportNoRender", { "还没有播放过框选的音符，没有可导出的渲染", "尚未播放過框選的音符，沒有可匯出的算繪", "選択したノートをまだ再生していないため、書き出せるレンダリングがありません", "선택한 음표를 재생한 적이 없어 내보낼 렌더링이 없습니다", "No marquee has been played yet, so there is no render to export" } },
    { "export.allTracks", { "全部轨道（每轨一个文件）", "全部音軌（每軌一個檔案）", "全トラック（トラックごとに1ファイル）", "모든 트랙 (트랙당 파일 1개)", "All tracks (one file each)" } },
    { "export.oneTrack", { "单个轨道", "單一音軌", "単一トラック", "단일 트랙", "A single track" } },
    { "export.untitledTrack", { "未命名轨道", "未命名音軌", "名称未設定トラック", "이름 없는 트랙", "Untitled track" } },
    { "status.exportRendering", { "正在渲染全曲音符…", "正在算繪全曲音符…", "曲全体のノートをレンダリング中…", "곡 전체 노트 렌더링 중…", "Rendering every note…" } },
    { "status.exportDone", { "已导出", "已匯出", "書き出しました", "내보냈습니다", "Exported" } },
    { "status.exportFiles", { "个文件", "個檔案", "ファイル", "개 파일", "file(s)" } },
    { "error.exportSilent", { "该轨道被静音或未独奏，导出会是静音", "該音軌被靜音或未獨奏，匯出會是靜音", "このトラックはミュート/非ソロのため無音になります", "이 트랙은 음소거 상태여서 무음으로 내보내집니다", "That track is muted or not soloed, so it would export silence" } },
    { "error.exportEmpty", { "没有可导出的轨道", "沒有可匯出的音軌", "書き出せるトラックがありません", "내보낼 트랙이 없습니다", "There is no track to export" } },
    { "file.audio",      { "导入音频", "匯入音訊", "オーディオを読み込む", "오디오 가져오기", "Import Audio" } },
    { "file.melodyne",   { "导入 Melodyne", "匯入 Melodyne", "Melodyneを読み込む", "Melodyne 가져오기", "Import Melodyne" } },
    { "file.new",        { "新建工程", "新增工程", "新規プロジェクト", "새 프로젝트", "New Project" } },
    { "file.midi",       { "导入 MIDI", "匯入 MIDI", "MIDIを読み込む", "MIDI 가져오기", "Import MIDI" } },
    { "file.ust",        { "导入 UST", "匯入 UST", "USTを読み込む", "UST 가져오기", "Import UST" } },
    { "file.exportMidi", { "导出 MIDI", "匯出 MIDI", "MIDIを書き出す", "MIDI 내보내기", "Export MIDI" } },
    { "error.exportMidi", { "无法导出 MIDI", "無法匯出 MIDI", "MIDIを書き出せません", "MIDI를 내보낼 수 없습니다", "Could not export the MIDI" } },
    { "status.midiExported", { "MIDI 已导出", "MIDI 已匯出", "MIDIを書き出しました", "MIDI를 내보냈습니다", "MIDI exported" } },
    { "error.ust",       { "无法读取 UST 工程", "無法讀取 UST 專案", "USTを読み込めません", "UST를 읽을 수 없습니다", "Could not read the UST" } },
    { "status.ustLoaded", { "UST 已导入为 UTAU 轨道", "UST 已匯入為 UTAU 軌道", "USTをUTAUトラックとして読み込みました", "UST를 UTAU 트랙으로 가져왔습니다", "UST imported as a UTAU track" } },
    { "file.exit",       { "退出", "結束", "終了", "종료", "Exit" } },
    { "file.settings",   { "设置…", "設定…", "設定…", "설정…", "Settings…" } },
    { "file.assets",     { "素材管理器…", "素材管理器…", "素材マネージャー…", "소재 관리자…", "Asset Manager…" } },
    { "menu.file",       { "文件", "檔案", "ファイル", "파일", "File" } },
    { "menu.edit",       { "编辑", "編輯", "編集", "편집", "Edit" } },
    { "menu.track",      { "轨道", "軌道", "トラック", "트랙", "Track" } },
    { "menu.view",       { "视图", "檢視", "表示", "보기", "View" } },
    { "menu.help",       { "帮助", "說明", "ヘルプ", "도움말", "Help" } },
    { "edit.undo",       { "撤销", "復原", "元に戻す", "실행 취소", "Undo" } },
    { "edit.redo",       { "重做", "重做", "やり直す", "다시 실행", "Redo" } },
    { "edit.selectAll",  { "全选音符", "全選音符", "全ノートを選択", "모든 노트 선택", "Select All Notes" } },
    { "edit.deselect", { "取消选择", "取消選取", "選択解除", "선택 해제", "Deselect" } },
    { "edit.copyNotes", { "复制所选音符", "複製所選音符", "選択ノートをコピー", "선택 음표 복사", "Copy Selected Notes" } },
    { "edit.cutNotes", { "剪切所选音符", "剪下所選音符", "選択ノートを切り取り", "선택 음표 잘라내기", "Cut Selected Notes" } },
    { "edit.pasteNotes", { "在播放位置粘贴音符", "在播放位置貼上音符", "再生位置にノートを貼り付け", "재생 위치에 음표 붙여넣기", "Paste Notes at Playhead" } },
    { "edit.pasteNotesAtOrigin", { "粘贴到原时间位置", "貼上到原時間位置", "元の時間位置に貼り付け", "원래 시간 위치에 붙여넣기", "Paste Notes at Original Time" } },
    { "edit.transposeCents", { "按音分移调…", "按音分移調…", "セントで移調…", "센트 단위 조옮김…", "Transpose by Cents…" } },
    { "edit.setPitch", { "设置音高…", "設定音高…", "ピッチを設定…", "피치 설정…", "Set Pitch…" } },
    { "edit.averagePitch", { "平均所选音高", "平均所選音高", "選択ピッチを平均化", "선택 피치 평균", "Average Selected Pitch" } },
    { "edit.quantizePitch", { "量化到半音", "量化到半音", "半音にクオンタイズ", "반음으로 퀀타이즈", "Quantize to Semitone" } },
    { "edit.hanziToPinyin", { "汉字转拼音", "漢字轉拼音", "漢字をピンインに変換", "한자를 병음으로 변환", "Convert Chinese Lyrics to Pinyin" } },
    { "edit.cents", { "音分", "音分", "セント", "센트", "Cents" } },
    { "edit.midiNote", { "MIDI 音高", "MIDI 音高", "MIDI ノート", "MIDI 음높이", "MIDI Note" } },
    { "edit.copyClip",   { "复制所选采样", "複製所選取樣", "選択クリップをコピー", "선택 클립 복사", "Copy Selected Clip" } },
    { "edit.pasteClip",  { "在播放位置粘贴采样", "在播放位置貼上取樣", "再生位置にクリップを貼り付け", "재생 위치에 클립 붙여넣기", "Paste Clip at Playhead" } },
    { "edit.duplicateClip", { "紧接复制所选采样", "緊接複製所選取樣", "選択クリップを直後に複製", "선택 클립 바로 뒤에 복제", "Duplicate Selected Clip" } },
    { "view.zoomIn",     { "放大", "放大", "拡大", "확대", "Zoom In" } },
    { "view.zoomOut",    { "缩小", "縮小", "縮小", "축소", "Zoom Out" } },
    { "view.zoomFit",    { "适合工程", "符合工程", "プロジェクト全体", "프로젝트 맞춤", "Fit Project" } },
    { "view.showWaveforms", { "显示波形", "顯示波形", "波形を表示", "파형 표시", "Show Waveforms" } },
    { "view.vZoomIn", { "纵向放大", "縱向放大", "縦方向に拡大", "세로 확대", "Zoom In Vertically" } },
    { "view.vZoomOut", { "纵向缩小", "縱向縮小", "縦方向に縮小", "세로 축소", "Zoom Out Vertically" } },
    { "help.about",      { "关于 HachiShifter", "關於 HachiShifter", "HachiShifterについて", "HachiShifter 정보", "About HachiShifter" } },
    { "help.aboutText",  { "HachiShifter Next · JUCE/C++ 原生重构版", "HachiShifter Next · JUCE/C++ 原生重構版", "HachiShifter Next · JUCE/C++ ネイティブ版", "HachiShifter Next · JUCE/C++ 네이티브 버전", "HachiShifter Next · Native JUCE/C++ edition" } },
    { "transport.play",  { "播放", "播放", "再生", "재생", "Play" } },
    { "transport.pause", { "暂停", "暫停", "一時停止", "일시 정지", "Pause" } },
    { "transport.stop",  { "停止", "停止", "停止", "정지", "Stop" } },
    { "tool.main",       { "音符编辑", "音符編輯", "ノート編集", "노트 편집", "Note Edit" } },
    { "tool.wrench",     { "采样精修", "取樣精修", "サンプル編集", "샘플 정밀 편집", "Sample Edit" } },
    { "tool.draw",       { "自由绘制", "自由繪製", "フリーハンド", "자유 그리기", "Free Draw" } },
    { "tool.line",       { "直线工具", "直線工具", "直線ツール", "직선 도구", "Line Tool" } },
    { "tool.points",     { "标点音高工具", "標點音高工具", "ピッチポイント", "피치 포인트", "Pitch Points" } },
    { "stretch.unit", { "最小拉伸", "最小拉伸", "最小ストレッチ", "최소 늘이기", "Min Stretch" } },
    { "stretch.unitHelp", { "拉伸和移动音符时的最小单位，为一拍的 1/N。默认 1/64。", "拉伸和移動音符時的最小單位，為一拍的 1/N。預設 1/64。", "ノートの伸縮と移動の最小単位（1拍の 1/N）。既定は 1/64。", "노트 늘이기와 이동의 최소 단위(한 박의 1/N). 기본값 1/64.", "Smallest unit for stretching and moving notes, as 1/N of a beat. Default 1/64." } },
    { "pitchCurve.incomingSegment", { "上一点 → 当前点", "上一點 → 目前點", "前の点 → 現在の点", "이전 점 → 현재 점", "Previous Point → This Point" } },
    { "pitchCurve.noPrevious", { "音头点没有上一段", "音頭點沒有上一段", "先頭点には前の区間がありません", "첫 점에는 이전 구간이 없습니다", "The first point has no incoming segment" } },
    { "pitchCurve.natural", { "自然连续（默认）", "自然連續（預設）", "自然連続（既定）", "자연스러운 연속 (기본값)", "Natural Continuous (Default)" } },
    { "pitchCurve.linear", { "直线", "直線", "直線", "직선", "Linear" } },
    { "pitchCurve.smooth", { "S 型平滑", "S 型平滑", "S字スムーズ", "S자 부드럽게", "Smooth S" } },
    { "pitchCurve.easeIn", { "J形曲线（前慢后快）", "J形曲線（前慢後快）", "J字曲線（前半ゆっくり・後半速く）", "J자 곡선 (앞은 느리게, 뒤는 빠르게)", "J Curve (Slow Then Fast)" } },
    { "pitchCurve.easeOut", { "R形曲线（前快后慢）", "R形曲線（前快後慢）", "R字曲線（前半速く・後半ゆっくり）", "R자 곡선 (앞은 빠르게, 뒤는 느리게)", "R Curve (Fast Then Slow)" } },
    { "pitchCurve.customBezier", { "自定义贝塞尔…", "自訂貝茲曲線…", "カスタムベジェ…", "사용자 베지어…", "Custom Bezier…" } },
    { "pitchCurve.snapToNote", { "调整到标准音高", "調整到標準音高", "標準ピッチに合わせる", "표준 음높이로 맞추기", "Snap to Note Pitch" } },
    { "pitchCurve.setFrequency", { "输入音高…", "輸入音高…", "ピッチを入力…",
                                   "음높이 입력…", "Set Frequency…" } },
    { "pitchCurve.frequencyTitle", { "输入音高", "輸入音高", "ピッチを入力",
                                     "음높이 입력", "Set Point Frequency" } },
    { "pitchCurve.frequencyHelp", {
        "直接输入这个标点的基频 F0，单位赫兹（Hz）。440 即标准音 A4。\n只改这一个标点，相邻标点和音符本身都不动。",
        "直接輸入這個標點的基頻 F0，單位赫茲（Hz）。440 即標準音 A4。\n只改這一個標點，相鄰標點和音符本身都不動。",
        "このポイントの基本周波数 F0 をヘルツ（Hz）で直接入力します。440 が基準音 A4 です。\nこのポイントだけが動き、隣のポイントもノート自体も変わりません。",
        "이 점의 기본 주파수 F0을 헤르츠(Hz)로 직접 입력합니다. 440이 표준음 A4입니다.\n이 점만 바뀌고 이웃한 점과 노트 자체는 그대로입니다.",
        "Type this point's fundamental frequency in hertz; 440 is concert A4.\nOnly this point moves -- its neighbours and the note itself stay put." } },
    { "pitchCurve.frequencyField", { "F0（Hz）", "F0（Hz）", "F0（Hz）",
                                     "F0 (Hz)", "F0 (Hz)" } },
    { "pitchCurve.deletePoint", { "删除此标点", "刪除此標點", "このポイントを削除", "이 점 삭제", "Delete This Point" } },
    { "pitchCurve.bezierTitle", { "自定义贝塞尔曲线", "自訂貝茲曲線", "カスタムベジェ曲線", "사용자 베지어 곡선", "Custom Bezier Curve" } },
    { "pitchCurve.bezierHelp", { "设置归一化控制点。X 控制变化发生的时间，Y 控制音高推进量；Y 超出 0–1 可产生转音过冲。", "設定正規化控制點。X 控制變化時間，Y 控制音高推進量；Y 超出 0–1 可產生轉音過衝。", "正規化した制御点を設定します。X は変化のタイミング、Y はピッチの進行量です。0～1 外の Y でオーバーシュートを作れます。", "정규화된 제어점을 설정합니다. X는 변화 시점, Y는 피치 진행량이며 0–1 밖의 Y로 오버슈트를 만들 수 있습니다.", "Set normalised control points. X controls timing and Y controls pitch progress; Y outside 0–1 creates overshoot." } },
    { "pitchCurve.bezierX1", { "控制点 1 X", "控制點 1 X", "制御点 1 X", "제어점 1 X", "Control 1 X" } },
    { "pitchCurve.bezierY1", { "控制点 1 Y", "控制點 1 Y", "制御点 1 Y", "제어점 1 Y", "Control 1 Y" } },
    { "pitchCurve.bezierX2", { "控制点 2 X", "控制點 2 X", "制御点 2 X", "제어점 2 X", "Control 2 X" } },
    { "pitchCurve.bezierY2", { "控制点 2 Y", "控制點 2 Y", "制御点 2 Y", "제어점 2 Y", "Control 2 Y" } },
    { "tool.connect",    { "连接/分离音符", "連接/分離音符", "ノート接続/分離", "노트 연결/분리", "Connect/Separate Notes" } },
    { "editor.parameters", { "参数编辑器", "參數編輯器", "パラメータ", "매개변수 편집기", "Parameters" } },
    { "editor.smooth",   { "平滑", "平滑", "平滑化", "평활", "Smooth" } },
    { "editor.drift",    { "漂移修正", "漂移修正", "ドリフト補正", "드리프트 보정", "Drift Correction" } },
    { "editor.attackSpeed", { "辅音速度", "子音速度", "アタックスピード", "어택 속도", "Attack Speed" } },
    { "param.pitch",     { "音高", "音高", "ピッチ", "피치", "Pitch" } },
    { "param.drift",     { "漂移", "漂移", "ドリフト", "드리프트", "Drift" } },
    { "param.attack",    { "起音", "起音", "アタック", "어택", "Attack" } },
    { "param.breath",    { "呼吸", "呼吸", "ブレス", "브레스", "Breath" } },
    { "param.tension",   { "张力", "張力", "テンション", "텐션", "Tension" } },
    { "param.formant",   { "共振峰", "共振峰", "フォルマント", "포먼트", "Formant" } },
    { "param.volume",    { "音量", "音量", "音量", "음량", "Volume" } },
    { "param.robustPitchCurveShort", { "稳健线", "穩健線", "ロバスト", "강건선", "Robust" } },
    { "param.robustPitchCurve", { "稳健音高线（仅当前音符）", "穩健音高線（僅目前音符）", "ロバストピッチカーブ（現在のノートのみ）", "강건한 피치 곡선 (현재 음표만)", "Robust Pitch Curve (current note only)" } },
    { "beats.bar",       { "每小节", "每小節", "拍子", "마디 박자", "Beats" } },
    { "grid",            { "网格", "網格", "グリッド", "그리드", "Grid" } },
    { "base.scale",      { "基准调", "基準調", "基準キー", "기준 키", "Key" } },
    { "algo.pitch",      { "变调算法", "變調演算法", "ピッチアルゴリズム", "피치 알고리즘", "Pitch Algorithm" } },
    { "algo.stretch",    { "拉伸算法", "拉伸演算法", "タイムアルゴリズム", "타임 알고리즘", "Stretch Algorithm" } },
    { "algo.stretch.melodyneHybrid", { "算法原生拉伸", "演算法原生拉伸", "エンジン標準伸縮", "엔진 기본 타임 스트레치", "Engine-native stretch" } },
    { "algo.stretch.nsfVariableMel", { "NSF 可变 Hop Mel 先拼接后合成", "NSF 可變 Hop Mel 先拼接後合成", "NSF 可変 Hop Mel 結合後合成", "NSF 가변 Hop Mel 연결 후 합성", "NSF Variable-Hop Mel: Splice then Synthesize" } },
    { "algo.stretch.nsfShiftThenSplice", { "NSF 先变调后拼接", "NSF 先變調後拼接", "NSF ピッチ後結合", "NSF 피치 먼저 연결", "NSF Shift then Splice" } },
    { "algo.stretch.loop", { "循环拉伸", "循環拉伸", "ループストレッチ", "루프 스트레치", "Loop Stretch" } },
    { "algo.stretch.soundTouch", { "SoundTouch 拉伸", "SoundTouch 拉伸", "SoundTouch ストレッチ", "SoundTouch 스트레치", "SoundTouch Stretch" } },
    { "algo.order",      { "处理顺序", "處理順序", "処理順", "처리 순서", "Render Order" } },
    { "algo.order.processThenSplice", { "先合成后拼接", "先合成後拼接", "合成後結合", "합성 후 연결", "Process then Splice" } },
    { "algo.order.stretchSpliceThenPitch", { "先拼接后合成", "先拼接後合成", "結合後合成", "연결 후 합성", "Splice then Pitch" } },
    { "track.compose",   { "旋律", "旋律", "メロディック", "멜로디", "Compose" } },
    { "track.toggleCompose", { "切换旋律/普通音轨", "切換旋律/一般音軌", "メロディック/通常を切替", "멜로디/일반 전환", "Toggle Compose/Audio" } },
    { "track.addCompose", { "添加旋律轨道", "新增旋律軌道", "メロディックトラックを追加", "멜로디 트랙 추가", "Add Melodic Track" } },
    { "track.addAudio", { "添加普通音轨", "新增一般音軌", "通常トラックを追加", "일반 오디오 트랙 추가", "Add Audio Track" } },
    { "track.accompaniment", { "伴奏轨道", "伴奏軌道", "伴奏トラック", "반주 트랙", "Accompaniment" } },
    { "track.newAccompaniment", { "新建伴奏轨道", "新建伴奏軌道", "伴奏トラックを新規作成", "새 반주 트랙", "New Accompaniment Track" } },
    { "track.originalAudio", { "原音播放", "原音播放", "原音再生", "원음 재생", "Original audio" } },
    { "track.noProcessing", { "不启用", "不啟用", "無効", "사용 안 함", "Disabled" } },
    { "track.accompanimentHelp", { "伴奏轨道直接播放原音，不进行音高分析、变调或人声合成；参与正常播放与导出。", "伴奏軌道直接播放原音，不進行音高分析、變調或人聲合成；參與正常播放與匯出。", "伴奏は原音を再生します。ピッチ解析・変換・歌声合成を使用せず、通常のミックスに含まれます。", "반주는 음높이 분석이나 보컬 합성 없이 원음을 재생하며 믹스에 포함됩니다.", "Plays the original audio without pitch analysis, tuning or vocal synthesis. Included in playback and export." } },
    { "track.newHere", { "新建轨道", "新建軌道", "トラックを新規作成", "새 트랙", "New Track" } },
    { "track.newReference", { "新建素材轨道", "新建素材軌道", "素材トラックを新規作成",
                              "소재 트랙 새로 만들기", "New Reference Track" } },
    { "track.importMidi", { "导入 MIDI 轨道", "匯入 MIDI 軌道", "MIDIトラックを読み込む",
                            "MIDI 트랙 가져오기", "Import MIDI Track" } },
    { "status.midiTrackImported", { "已导入 MIDI 轨道：", "已匯入 MIDI 軌道：", "MIDIトラックを読み込みました：",
                                    "MIDI 트랙을 가져왔습니다: ", "MIDI track imported: " } },
    { "track.rename", { "重命名所选轨道…", "重新命名所選軌道…", "選択トラック名を変更…", "선택 트랙 이름 바꾸기…", "Rename Selected Track…" } },
    { "track.name", { "轨道名称", "軌道名稱", "トラック名", "트랙 이름", "Track Name" } },
    { "track.delete",    { "删除所选轨道", "刪除所選軌道", "選択トラックを削除", "선택 트랙 삭제", "Delete Selected Track" } },
    { "clip.delete",     { "删除所选采样", "刪除所選取樣", "選択クリップを削除", "선택 클립 삭제", "Delete Selected Clip" } },
    { "clip.split",      { "拆分", "拆分", "分割", "분할", "Split" } },
    { "clip.addEmptyTuning", { "添加空调音片段（10 秒）", "新增空白調音片段（10 秒）", "空の調声クリップを追加（10 秒）", "빈 튜닝 클립 추가 (10초)", "Add Empty Tuning Clip (10 s)" } },
    { "clip.startNormalDisplay", { "开始正常显示", "開始正常顯示", "通常表示を開始", "일반 표시 시작", "Show Normally" } },
    { "clip.stopNormalDisplay", { "关闭正常显示", "關閉正常顯示", "通常表示を終了", "일반 표시 끄기", "Stop Showing Normally" } },
    { "clip.startNoteHints", { "开始提示显示", "開始提示顯示", "ノートガイドを表示", "음표 가이드 표시", "Show Note Hints" } },
    { "clip.stopNoteHints", { "关闭提示显示", "關閉提示顯示", "ノートガイドを非表示", "음표 가이드 숨기기", "Hide Note Hints" } },
    { "clip.merge",      { "合并", "合併", "結合", "병합", "Merge" } },
    { "clip.mute",       { "静音所选采样", "靜音所選取樣", "選択クリップをミュート", "선택 클립 음소거", "Mute Selected Clip" } },
    { "clip.unmute",     { "取消采样静音", "取消取樣靜音", "クリップのミュート解除", "클립 음소거 해제", "Unmute Clip" } },
    { "clip.gain",       { "设置采样增益…", "設定取樣增益…", "クリップゲインを設定…", "클립 게인 설정…", "Set Clip Gain…" } },
    { "clip.gainDb",     { "增益（dB）", "增益（dB）", "ゲイン（dB）", "게인 (dB)", "Gain (dB)" } },
    { "clip.gainKnobHint", { "片段响度：上下拖动，Shift 精细调节，双击恢复 0 dB。",
                            "片段響度：上下拖動，Shift 精細調節，雙擊恢復 0 dB。",
                            "クリップ音量：上下にドラッグ、Shift で微調整、ダブルクリックで 0 dB。",
                            "클립 음량: 위아래로 드래그, Shift로 미세 조정, 더블 클릭으로 0 dB 복원.",
                            "Clip gain: drag up/down, Shift for fine adjustment, double-click to reset to 0 dB." } },
    { "track.envelopeHint", { "区域响度包络：双击线添加点；拖动点调整时间和响度，Shift 精调；右键点可删除或复位。",
                              "區域響度包絡：雙擊線新增點；拖動點調整時間和響度，Shift 微調；右鍵點可刪除或復位。",
                              "リージョン音量：線をダブルクリックして点を追加。点をドラッグして調整、Shift で微調整。右クリックで削除・リセット。",
                              "영역 음량: 선을 더블 클릭하여 점 추가, 점을 드래그하여 조정, Shift로 미세 조정, 우클릭으로 삭제 또는 초기화.",
                              "Region gain envelope: double-click the line to add a point; drag points to adjust time and gain, Shift for fine adjustment; right-click to delete or reset." } },
    { "track.envelopeDelete", { "删除响度包络点", "刪除響度包絡點", "音量ポイントを削除", "음량 포인트 삭제", "Delete gain envelope point" } },
    { "track.envelopeResetPoint", { "包络点恢复 0 dB", "包絡點恢復 0 dB", "ポイントを 0 dB に戻す", "포인트를 0 dB로 초기화", "Reset point to 0 dB" } },
    { "track.envelopeReset", { "区域包络恢复 0 dB", "區域包絡恢復 0 dB", "リージョン音量を 0 dB に戻す", "영역 음량을 0 dB로 초기화", "Reset region envelope to 0 dB" } },
    { "track.audio",     { "普通音轨", "一般音軌", "通常トラック", "일반 트랙", "Audio Track" } },
    { "track.mute",      { "静音", "靜音", "ミュート", "음소거", "Mute" } },
    { "track.tip.compose", { "旋律轨道：开启后音符才会按音高渲染，钢琴窗里也才看得到这条轨道；关闭则作为普通音轨，素材原样播放",
                             "旋律軌道：開啟後音符才會依音高算繪，鋼琴窗中也才看得到這條軌道；關閉則作為一般音軌，素材原樣播放",
                             "メロディックトラック：オンのときだけノートが音高どおりにレンダリングされ、ピアノロールにも表示されます。オフなら通常トラックとして素材をそのまま再生します",
                             "멜로디 트랙: 켜면 노트가 음높이대로 렌더링되고 피아노 롤에도 표시됩니다. 끄면 일반 트랙으로 소재를 그대로 재생합니다",
                             "Compose track: only then are its notes rendered at their pitch and shown in the piano roll. Off, it is an audio track and the material plays as it is" } },
    { "track.tip.mute",  { "静音：这条轨道不发声，其余轨道照常",
                           "靜音：這條軌道不發聲，其餘軌道照常",
                           "ミュート：このトラックだけ音を出しません",
                           "음소거: 이 트랙만 소리가 나지 않습니다",
                           "Mute: this track is silent, the others play as usual" } },
    { "track.tip.solo",  { "独奏：只播放带独奏标记的轨道，其余全部静音",
                           "獨奏：只播放帶獨奏標記的軌道，其餘全部靜音",
                           "ソロ：ソロが付いたトラックだけを再生し、ほかはすべて無音になります",
                           "솔로: 솔로가 켜진 트랙만 재생하고 나머지는 모두 음소거됩니다",
                           "Solo: only soloed tracks play, everything else is silenced" } },
    { "track.tip.smooth", { "重叠淡化：相邻素材重叠处自动交叉淡化并把叠加音量归一，紧挨的接缝再补一段极短淡入，避免爆音",
                            "重疊淡化：相鄰素材重疊處自動交叉淡化並把疊加音量歸一，緊鄰的接縫再補一段極短淡入，避免爆音",
                            "重なりのフェード：素材が重なる部分を自動でクロスフェードし、合計音量をそろえます。隙間なく続く継ぎ目にはごく短いフェードを足してノイズを防ぎます",
                            "겹침 페이드: 소재가 겹치는 구간을 자동으로 크로스페이드하고 합쳐진 음량을 고르게 맞춥니다. 딱 붙은 이음매에는 아주 짧은 페이드를 넣어 잡음을 막습니다",
                            "Fade overlaps: overlapping material is crossfaded and its summed level evened out, and a very short fade is added at butt joins so they do not click" } },
    { "track.tip.normalize", { "音量归一：渲染后把音量匹配回原素材（NSF-HiFiGAN 按参考 Mel，其余后端按有声段 RMS）",
                               "音量歸一：算繪後把音量匹配回原素材（NSF-HiFiGAN 依參考 Mel，其餘後端依有聲段 RMS）",
                               "音量をそろえる：レンダリング後の音量を元の素材に合わせます（NSF-HiFiGANは参照メル、ほかのバックエンドは有声区間のRMS）",
                               "음량 정규화: 렌더링 후 음량을 원본 소재에 맞춥니다 (NSF-HiFiGAN은 참조 멜, 나머지 백엔드는 유성 구간 RMS)",
                               "Match level: after rendering, the level is matched back to the source (NSF-HiFiGAN against a reference mel, other backends by voiced RMS)" } },
    { "track.volume",    { "音量", "音量", "音量", "음량", "Vol" } },
    { "status.ready",    { "就绪", "就緒", "準備完了", "준비됨", "Ready" } },
    { "status.loading",  { "正在加载…", "正在載入…", "読み込み中…", "불러오는 중…", "Loading…" } },
    { "status.rendering", { "正在预渲染…", "正在預先算繪…", "プリレンダリング中…", "사전 렌더링 중…", "Pre-rendering…" } },
    { "status.analyzing", { "正在分析原始音高…", "正在分析原始音高…", "元ピッチを解析中…", "원본 피치 분석 중…", "Analysing source pitch…" } },
    { "status.analysisComplete", { "音高与音符分析完成", "音高與音符分析完成", "ピッチとノートの解析が完了しました", "피치 및 노트 분석 완료", "Pitch and note analysis complete" } },
    { "status.analysisSkipped", { "已保留现有音符数据", "已保留現有音符資料", "既存のノートデータを保持しました", "기존 노트 데이터를 유지했습니다", "Existing note data preserved" } },
    { "status.exporting", { "正在导出 WAV…", "正在匯出 WAV…", "WAVを書き出しています…", "WAV 내보내는 중…", "Exporting WAV…" } },
    { "status.clipCopied", { "已复制采样", "已複製取樣", "クリップをコピーしました", "클립을 복사했습니다", "Clip copied" } },
    { "status.clipPasted", { "已粘贴采样", "已貼上取樣", "クリップを貼り付けました", "클립을 붙여넣었습니다", "Clip pasted" } },
    { "status.notesCopied", { "已复制音符", "已複製音符", "ノートをコピーしました", "음표를 복사했습니다", "Notes copied" } },
    { "status.notesCut", { "已剪切音符", "已剪下音符", "ノートを切り取りました", "음표를 잘라냈습니다", "Notes cut" } },
    { "status.notesPasted", { "已粘贴音符", "已貼上音符", "ノートを貼り付けました", "음표를 붙여넣었습니다", "Notes pasted" } },
    { "status.projectOpened", { "已打开工程", "已開啟工程", "プロジェクトを開きました", "프로젝트를 열었습니다", "Project opened" } },
    { "status.projectSaved", { "已保存工程", "已儲存工程", "プロジェクトを保存しました", "프로젝트를 저장했습니다", "Project saved" } },
    { "status.midiPending", { "MIDI 导入器将在下一阶段接入", "MIDI 匯入器將於下一階段接入", "MIDIインポーターは次段階で接続します", "MIDI 가져오기는 다음 단계에서 연결됩니다", "MIDI importer will be connected in the next stage" } },
    { "status.noTracks", { "导入音频或工程以开始", "匯入音訊或工程以開始", "音声またはプロジェクトを読み込んでください", "오디오 또는 프로젝트를 가져오세요", "Import audio or a project to begin" } },
    { "edit.source",     { "原始采样编辑：此模式不允许拉伸", "原始取樣編輯：此模式不允許拉伸", "元サンプル編集：このモードではストレッチできません", "원본 샘플 편집: 이 모드에서는 늘이기를 사용할 수 없습니다", "Original sample edit: stretching is disabled" } },
    { "error.audio",     { "音频文件读取失败", "音訊檔案讀取失敗", "オーディオを読み込めません", "오디오 파일을 읽지 못했습니다", "Could not read audio file" } },
    { "error.midi",      { "MIDI 导入失败", "MIDI 匯入失敗", "MIDIの読み込みに失敗しました", "MIDI 가져오기에 실패했습니다", "MIDI import failed" } },
    { "error.mpd",       { "Melodyne 工程读取失败", "Melodyne 工程讀取失敗", "Melodyneプロジェクトを読み込めません", "Melodyne 프로젝트를 읽지 못했습니다", "Could not read Melodyne project" } },
    { "error.export",    { "音频导出失败", "音訊匯出失敗", "オーディオの書き出しに失敗しました", "오디오 내보내기 실패", "Audio export failed" } },
    { "warning.missingMedia", { "以下素材未找到", "找不到以下素材", "次の素材が見つかりません", "다음 미디어를 찾지 못했습니다", "The following media files were not found" } },
    { "mpd.stage.open", { "打开工程", "開啟工程", "プロジェクトを開く", "프로젝트 열기", "Opening project" } },
    { "mpd.stage.scan_container", { "扫描工程容器", "掃描工程容器", "コンテナを走査", "프로젝트 컨테이너 검사", "Scanning container" } },
    { "mpd.stage.decompress_graph", { "解压工程数据", "解壓工程資料", "データを展開", "프로젝트 데이터 압축 해제", "Decompressing graph" } },
    { "mpd.stage.read_tracks", { "读取轨道和 BPM", "讀取軌道與 BPM", "トラックとBPMを読込", "트랙 및 BPM 읽기", "Reading tracks and BPM" } },
    { "mpd.stage.create_tracks", { "恢复音符和编辑", "還原音符與編輯", "ノートと編集を復元", "노트 및 편집 복원", "Restoring notes and edits" } },
    { "mpd.stage.reanalyse_pitch", { "重新分析原始 F0", "重新分析原始 F0", "元のF0を再解析", "원본 F0 재분석", "Reanalysing source F0" } },
    { "mpd.stage.complete", { "完成", "完成", "完了", "완료", "Complete" } },
    { "mpd.compose.title", { "选择 Compose 轨道", "選擇 Compose 軌道", "Composeトラックを選択", "Compose 트랙 선택", "Choose Compose Tracks" } },
    { "mpd.compose.description", { "勾选需要恢复 Melodyne 音符和修音的旋律轨道；其余轨道按普通音频播放。", "勾選需要還原 Melodyne 音符與修音的旋律軌道；其餘軌道作為一般音訊播放。", "Melodyneのノート編集を復元する旋律トラックを選択します。その他は通常の音声トラックとして扱います。", "Melodyne 노트 편집을 복원할 멜로디 트랙을 선택하세요. 나머지는 일반 오디오 트랙으로 처리됩니다.", "Select melodic tracks whose Melodyne note edits should be restored. Other tracks remain regular audio tracks." } },
    { "dialog.import", { "导入", "匯入", "読み込む", "가져오기", "Import" } },
    { "dialog.cancel", { "取消", "取消", "キャンセル", "취소", "Cancel" } }
    ,{ "dialog.delete", { "删除", "刪除", "削除", "삭제", "Delete" } }
    ,{ "dialog.destructiveMessage", { "此操作会删除所选内容，是否继续？", "此操作會刪除所選內容，是否繼續？", "選択した内容を削除します。続行しますか？", "선택한 내용을 삭제합니다. 계속할까요?", "The selected content will be deleted. Continue?" } }
    ,{ "dialog.apply", { "应用", "套用", "適用", "적용", "Apply" } }
    ,{ "dialog.save", { "保存", "儲存", "保存", "저장", "Save" } }
    ,{ "dialog.discard", { "放弃更改", "放棄變更", "変更を破棄", "변경 내용 버리기", "Discard Changes" } }
    ,{ "dialog.ustImportTitle", { "导入 UST", "匯入 UST", "USTの読み込み", "UST 가져오기", "Import UST" } }
    ,{ "dialog.ustImportMessage", { "当前工程已有内容。要用这个 UST 新建工程，还是把它添加为一条新音轨？", "目前工程已有內容。要用這個 UST 新建工程，還是把它加為一條新軌道？", "プロジェクトには既に内容があります。このUSTで新しく開き直しますか、それとも新しいトラックとして追加しますか？", "프로젝트에 이미 내용이 있습니다. 이 UST로 새로 열까요, 아니면 새 트랙으로 추가할까요?", "This project already has content. Open this UST as the project, or add it as a new track?" } }
    ,{ "dialog.ustReplace", { "作为新工程打开", "作為新工程開啟", "新しいプロジェクトとして開く", "새 프로젝트로 열기", "Open as the Project" } }
    ,{ "dialog.ustAddTrack", { "添加为新音轨", "加為新軌道", "新しいトラックとして追加", "새 트랙으로 추가", "Add as a New Track" } }
    ,{ "dialog.midiTrackTitle", { "导入 MIDI 轨道", "匯入 MIDI 軌道", "MIDIトラックの読み込み", "MIDI 트랙 가져오기", "Import MIDI Track" } }
    ,{ "dialog.midiTrackMessage", { "这个 MIDI 文件有多个带音符的轨道，选择要导入哪一个。", "這個 MIDI 檔案有多個帶音符的軌道，選擇要匯入哪一個。", "このMIDIファイルには音符のあるトラックが複数あります。読み込むトラックを選んでください。", "이 MIDI 파일에는 음표가 있는 트랙이 여러 개 있습니다. 가져올 트랙을 선택하세요.", "This MIDI file has more than one track with notes. Choose the one to import." } }
    ,{ "dialog.midiTrackLabel", { "轨道", "軌道", "トラック", "트랙", "Track" } }
    ,{ "dialog.midiTrackNotes", { "个音符", "個音符", "音符", "개 음표", "notes" } }
    ,{ "dialog.unsavedTitle", { "工程尚未保存", "工程尚未儲存", "プロジェクトは未保存です", "프로젝트가 저장되지 않음", "Unsaved Project" } }
    ,{ "dialog.unsavedMessage", { "是否先保存当前工程的更改？", "是否先儲存目前工程的變更？", "現在のプロジェクトの変更を保存しますか？", "현재 프로젝트 변경 내용을 저장할까요?", "Save changes to the current project first?" } }
    ,{ "settings.title", { "设置", "設定", "設定", "설정", "Settings" } }
    ,{ "settings.interface", { "界面", "介面", "インターフェース", "인터페이스", "Interface" } }
    ,{ "settings.audio", { "音频", "音訊", "オーディオ", "오디오", "Audio" } }
    ,{ "settings.audioDevice", { "当前音频设备", "目前音訊裝置", "現在のオーディオデバイス", "현재 오디오 장치", "Current Audio Device" } }
    ,{ "settings.sampleRate", { "采样率", "取樣率", "サンプルレート", "샘플 레이트", "Sample Rate" } }
    ,{ "settings.bufferSize", { "缓冲区大小", "緩衝區大小", "バッファサイズ", "버퍼 크기", "Buffer Size" } }
    ,{ "settings.advancedAudio", { "选择输入、输出和驱动…", "選擇輸入、輸出與驅動…", "入出力とドライバーを選択…", "입출력 및 드라이버 선택…", "Choose Inputs, Outputs and Driver…" } }
    ,{ "settings.noAudioDevice", { "未选择音频设备", "尚未選擇音訊裝置", "オーディオデバイス未選択", "오디오 장치가 선택되지 않음", "No audio device selected" } }
    ,{ "settings.algorithm", { "算法", "演算法", "アルゴリズム", "알고리즘", "Algorithms" } }
    ,{ "settings.operation", { "操作", "操作", "操作", "조작", "Operations" } }
    ,{ "settings.import", { "文件导入", "檔案匯入", "ファイル読込", "파일 가져오기", "File Import" } }
    ,{ "settings.language", { "语言", "語言", "言語", "언어", "Language" } }
    ,{ "settings.theme", { "颜色主题", "色彩主題", "カラーテーマ", "색상 테마", "Colour Theme" } }
    ,{ "settings.themeDark", { "深色", "深色", "ダーク", "다크", "Dark" } }
    ,{ "settings.themeLight", { "浅色", "淺色", "ライト", "라이트", "Light" } }
    ,{ "settings.accent", { "主色（Hex）", "主色（Hex）", "アクセント（Hex）", "강조색 (Hex)", "Accent (Hex)" } }
    ,{ "settings.accentLight", { "浅主色（Hex）", "淺主色（Hex）", "明るい主色（Hex）", "밝은 강조색 (Hex)", "Light Accent (Hex)" } }
    ,{ "settings.noteColour", { "音符色（Hex）", "音符色（Hex）", "ノート色（Hex）", "노트 색상 (Hex)", "Note Colour (Hex)" } }
    ,{ "settings.showNoteLabels", { "显示已标注的发音/别名", "顯示已標註的發音/別名", "注釈済みの発音・別名を表示", "표시된 발음/별칭 표시", "Show annotated pronunciation/alias" } }
    ,{ "settings.uiScale", { "界面缩放", "介面縮放", "UIスケール", "UI 배율", "UI Scale" } }
    ,{ "settings.gamePath", { "GAME 模型目录", "GAME 模型目錄", "GAMEモデルフォルダー", "GAME 모델 폴더", "GAME Model Directory" } }
    ,{ "settings.gameModel", { "GAME 默认模型", "GAME 預設模型", "GAME既定モデル", "GAME 기본 모델", "Default GAME Model" } }
    ,{ "settings.fcpePath", { "FCPE 模型或目录", "FCPE 模型或目錄", "FCPEモデルまたはフォルダー", "FCPE 모델 또는 폴더", "FCPE Model or Directory" } }
    ,{ "settings.hifiganPath", { "HiFi-GAN 模型目录", "HiFi-GAN 模型目錄", "HiFi-GANモデルフォルダー", "HiFi-GAN 모델 폴더", "HiFi-GAN Model Directory" } }
    ,{ "settings.inference", { "推理方式", "推理方式", "推論バックエンド", "추론 백엔드", "Inference Backend" } }
    ,{ "settings.device", { "推理设备", "推理裝置", "推論デバイス", "추론 장치", "Inference Device" } }
    ,{ "settings.auto", { "自动", "自動", "自動", "자동", "Auto" } }
    ,{ "settings.utauResampler", { "UTAU 重采样器（可选，留空则内置变调）", "UTAU 重取樣器（可選，留空則內建變調）", "UTAUリサンプラー（任意、空欄は内蔵処理）", "UTAU 리샘플러 (선택, 비우면 내장 처리)", "UTAU Resampler (optional; built-in fallback)" } }
    ,{ "settings.shortcuts", { "快捷键方案", "快速鍵配置", "ショートカット方式", "단축키 방식", "Shortcut Scheme" } }
    ,{ "settings.wheel", { "鼠标滚轮", "滑鼠滾輪", "マウスホイール", "마우스 휠", "Mouse Wheel" } }
    ,{ "settings.wheelZoom", { "缩放", "縮放", "ズーム", "확대/축소", "Zoom" } }
    ,{ "settings.wheelScroll", { "滚动", "捲動", "スクロール", "스크롤", "Scroll" } }
    ,{ "settings.spacePlayback", { "空格键播放/暂停", "空白鍵播放/暫停", "スペースで再生/一時停止", "스페이스바 재생/일시정지", "Space toggles playback" } }
    ,{ "settings.confirmDestructive", { "删除前确认", "刪除前確認", "削除前に確認", "삭제 전 확인", "Confirm before delete" } }
    ,{ "settings.melodyneCompose", { "Melodyne Compose 默认方式", "Melodyne Compose 預設方式", "Melodyne Composeの既定値", "Melodyne Compose 기본값", "Default Melodyne Compose" } }
    ,{ "settings.composeAsk", { "每次询问", "每次詢問", "毎回確認", "매번 확인", "Ask Every Time" } }
    ,{ "settings.composeMelodic", { "旋律轨道", "旋律軌道", "メロディックトラック", "멜로디 트랙", "Melodic Tracks" } }
    ,{ "settings.composeAll", { "全部轨道", "全部軌道", "すべてのトラック", "모든 트랙", "All Tracks" } }
    ,{ "settings.composeAudio", { "仅普通音轨", "僅一般音軌", "通常音声のみ", "일반 오디오만", "Audio Only" } }
    ,{ "settings.melodynePitch", { "Melodyne 原音高来源", "Melodyne 原音高來源", "Melodyne元ピッチの取得元", "Melodyne 원본 피치 소스", "Melodyne Source Pitch" } }
    ,{ "settings.pitchProject", { "工程记录", "工程記錄", "プロジェクトデータ", "프로젝트 데이터", "Project Data" } }
    ,{ "settings.pitchReanalyse", { "GAME + FCPE（无模型时使用原生分析）", "GAME + FCPE（無模型時使用原生分析）", "GAME + FCPE（モデルなしはネイティブ解析）", "GAME + FCPE (모델 미포함 시 네이티브 분석)", "GAME + FCPE (native fallback without models)" } }
    ,{ "settings.importAlgorithm", { "导入工程默认变调算法", "匯入工程預設變調演算法", "読込時の既定ピッチアルゴリズム", "가져오기 기본 피치 알고리즘", "Default Import Pitch Algorithm" } }
    ,{ "settings.importStretchAlgorithm", { "导入工程默认拉伸算法", "匯入工程預設拉伸演算法", "読込時の既定タイムストレッチ", "가져오기 기본 타임 스트레치", "Default Import Stretch Algorithm" } }
    ,{ "settings.preserveEdits", { "保留工程中的修音、Attack、音量和音色编辑", "保留工程中的修音、Attack、音量與音色編輯", "ピッチ補正・Attack・音量・音色の編集を保持", "피치 보정·Attack·음량·음색 편집 유지", "Preserve tuning, Attack, level and timbre edits" } }
    ,{ "settings.recursiveMedia", { "递归查找缺失素材", "遞迴尋找遺失素材", "不足素材を再帰検索", "누락 미디어 재귀 검색", "Search recursively for missing media" } }
    ,{ "sample.alias", { "别名", "別名", "エイリアス", "별칭", "Alias" } }
    ,{ "sample.start", { "起点", "起點", "開始", "시작", "Start" } }
    ,{ "sample.end", { "终点", "終點", "終了", "끝", "End" } }
    ,{ "sample.alignment", { "对齐", "對齊", "整列", "정렬", "Align" } }
    ,{ "sample.fixed", { "辅音", "子音", "子音", "자음", "Fixed" } }
    ,{ "sample.save", { "保存设定", "儲存設定", "設定を保存", "설정 저장", "Save Settings" } }
    ,{ "sample.saved", { "音频设定已保存", "音訊設定已儲存", "音声設定を保存しました", "오디오 설정 저장됨", "Audio settings saved" } }
    ,{ "sample.importOto", { "读取 oto", "讀取 oto", "oto読込", "oto 읽기", "Import oto" } }
    ,{ "sample.exportOto", { "导出 oto", "匯出 oto", "oto書出", "oto 내보내기", "Export oto" } }
    ,{ "asset.title", { "素材管理器", "素材管理器", "素材マネージャー", "소재 관리자", "Asset Manager" } }
    ,{ "asset.register", { "注册音频素材", "註冊音訊素材", "音声素材を登録", "오디오 소재 등록", "Register Audio Assets" } }
    ,{ "asset.utau", { "导入 UTAU 音源库", "匯入 UTAU 音源庫", "UTAU 音源を読み込む", "UTAU 음원 가져오기", "Import UTAU Voicebank" } }
    ,{ "asset.utauDone", { "已注册 {files} 个音频，生成 {sidecars} 个 HJM 文件、{regions} 个分段。", "已註冊 {files} 個音訊，產生 {sidecars} 個 HJM 檔案、{regions} 個分段。", "{files} 件の音声を登録し、{sidecars} 件の HJM と {regions} 件の区間を作成しました。", "오디오 {files}개를 등록하고 HJM {sidecars}개와 구간 {regions}개를 생성했습니다.", "Registered {files} audio files and generated {sidecars} HJM files with {regions} regions." } }
    ,{ "asset.remove", { "移除", "移除", "削除", "제거", "Remove" } }
    ,{ "asset.empty", { "把素材文件夹或音频拖入此处；音源库和音频文件夹都会成为可复用的素材文件夹。", "將素材資料夾或音訊拖入此處；音源庫和音訊資料夾都會成為可重複使用的素材資料夾。", "素材フォルダーまたは音声をここへドロップします。音源も音声フォルダーも再利用できる素材フォルダーになります。", "소재 폴더나 오디오를 여기에 놓으세요. 음원과 오디오 폴더 모두 재사용 가능한 소재 폴더가 됩니다.", "Drop a material folder or audio here; voicebanks and audio folders both become reusable material folders." } }
    ,{ "asset.newFolder", { "新建素材夹", "新建素材夾", "素材フォルダー作成", "소재 폴더 생성", "New Folder" } }
    ,{ "asset.importFolder", { "导入文件夹", "匯入資料夾", "フォルダー読込", "폴더 가져오기", "Import Folder" } }
    ,{ "asset.addAudio", { "添加音频", "新增音訊", "音声を追加", "오디오 추가", "Add Audio" } }
    ,{ "asset.assemble", { "活字印刷", "活字印刷", "活字印刷", "활자 인쇄", "Movable Type" } }
    ,{ "asset.folderEmpty", { "这个素材文件夹还没有音频。用「添加音频」或拖入音频，系统会自动粗略识别参数。", "這個素材資料夾還沒有音訊。用「新增音訊」或拖入音訊，系統會自動粗略辨識參數。", "この素材フォルダーにはまだ音声がありません。「音声を追加」またはドロップすると自動で概略パラメーターを検出します。", "이 소재 폴더에는 아직 오디오가 없습니다. '오디오 추가' 또는 드롭하면 자동으로 파라미터를 대략 감지합니다.", "This folder has no audio yet. Add or drop audio and parameters are roughly detected automatically." } }
    ,{ "asset.paramsReady", { "参数已识别，可编辑并复用", "參數已辨識，可編輯並重複使用", "パラメーター検出済み・編集/再利用可", "파라미터 감지됨 · 편집/재사용 가능", "Parameters detected — editable and reusable" } }
    ,{ "asset.paramsNone", { "尚无参数", "尚無參數", "パラメーター未検出", "파라미터 없음", "No parameters yet" } }
    ,{ "asset.pickFolderFirst", { "请先在左侧选择一个素材文件夹。", "請先在左側選擇一個素材資料夾。", "先に左側で素材フォルダーを選択してください。", "먼저 왼쪽에서 소재 폴더를 선택하세요.", "Select a material folder on the left first." } }
    ,{ "asset.assembleHint", { "输入歌词，按行/字转拼音并从所选素材库匹配，生成有序素材序列。", "輸入歌詞，按行/字轉拼音並從所選素材庫比對，產生有序素材序列。", "歌詞を入力すると各文字を拼音に変換し、選択した素材から順序付き素材列を生成します。", "가사를 입력하면 각 글자를 병음으로 변환해 선택한 소재에서 순서 있는 소재열을 생성합니다.", "Enter lyrics; each character is converted to pinyin and matched from the chosen material into an ordered sequence." } }
    ,{ "asset.assembleName", { "新素材夹名称", "新素材夾名稱", "新しい素材フォルダー名", "새 소재 폴더 이름", "New folder name" } }
    ,{ "asset.assembleLyrics", { "歌词（每字一个素材）", "歌詞（每字一個素材）", "歌詞（1文字ごとに1素材）", "가사 (글자마다 소재 하나)", "Lyrics (one material per character)" } }
    ,{ "asset.assembleDone", { "已生成 {matched} 个素材，缺失 {missing} 个。", "已產生 {matched} 個素材，缺失 {missing} 個。", "{matched} 件を生成し、{missing} 件が不足しました。", "{matched}개를 생성했고 {missing}개가 누락되었습니다.", "Assembled {matched} materials; {missing} missing." } }
    ,{ "asset.docked", { "素材管理器已在下方打开；再次点击菜单项可收起。", "素材管理器已在下方開啟；再次點選選單項可收起。", "素材マネージャーを下部に表示しました。メニュー項目を再度選ぶと閉じます。", "소재 관리자를 아래에 열었습니다. 메뉴 항목을 다시 선택하면 닫힙니다.", "Material manager docked below; choose the menu item again to hide it." } }
    ,{ "asset.editTitle", { "素材参数编辑器", "素材參數編輯器", "素材パラメーター編集", "소재 파라미터 편집", "Material Parameter Editor" } }
    ,{ "asset.editNoOto", { "该素材在 oto.ini 中没有对应条目，无法编辑。", "該素材在 oto.ini 中沒有對應條目，無法編輯。", "この素材は oto.ini に対応する行がないため編集できません。", "이 소재는 oto.ini에 해당 항목이 없어 편집할 수 없습니다.", "This material has no oto.ini entry to edit." } }
    ,{ "asset.exportOto", { "另存为 OTO", "另存為 OTO", "OTO として書き出し", "OTO로 내보내기", "Export as OTO" } }
    ,{ "asset.exportOtoDone", { "已导出 {rows} 行到 oto.ini（原生标注仍保留）。", "已匯出 {rows} 行到 oto.ini（原生標註仍保留）。", "{rows} 行を oto.ini に書き出しました（ネイティブ注釈は保持）。", "{rows}행을 oto.ini로 내보냈습니다(네이티브 주석 유지).", "Exported {rows} rows to oto.ini (native annotations kept)." } }
    ,{ "asset.melodyneFolderTitle", { "创建素材文件夹", "建立素材資料夾", "素材フォルダーを作成", "소재 폴더 생성", "Create Material Folder" } }
    ,{ "asset.melodyneFolderPrompt", { "此 Melodyne 工程引用了 {count} 个源音频文件夹。是否登记为可复用的素材文件夹？", "此 Melodyne 工程引用了 {count} 個來源音訊資料夾。是否登記為可重複使用的素材資料夾？", "この Melodyne プロジェクトは {count} 個の音源フォルダーを参照しています。再利用可能な素材フォルダーとして登録しますか？", "이 Melodyne 프로젝트는 소스 오디오 폴더 {count}개를 참조합니다. 재사용 가능한 소재 폴더로 등록할까요?", "This Melodyne project references {count} source audio folder(s). Register them as reusable material folders?" } }
    ,{ "asset.melodyneFolderCreate", { "登记素材夹", "登記素材夾", "登録する", "등록", "Register" } }
    ,{ "asset.melodyneRegisterInPlace", { "登记（默认）", "登記（預設）", "登録（既定）", "등록(기본)", "Register (default)" } }
    ,{ "asset.melodyneRegisterCopy", { "登记并复制到文件夹…", "登記並複製到資料夾…", "登録してフォルダーへコピー…", "등록 후 폴더로 복사…", "Register and copy to folder…" } }
    ,{ "asset.melodyneNoRegister", { "不登记", "不登記", "登録しない", "등록 안 함", "Don't register" } }
    ,{ "asset.melodyneFolderDone", { "已登记 {count} 个素材文件夹，可在素材管理器中复用。", "已登記 {count} 個素材資料夾，可在素材管理器中重複使用。", "{count} 個の素材フォルダーを登録しました。素材マネージャーで再利用できます。", "소재 폴더 {count}개를 등록했습니다. 소재 관리자에서 재사용할 수 있습니다.", "Registered {count} material folder(s), reusable in the Asset Manager." } }
    ,{ "settings.browse", { "浏览…", "瀏覽…", "参照…", "찾아보기…", "Browse…" } }
    ,{ "settings.utauVoicebank", { "UTAU 默认音源文件夹", "UTAU 預設音源資料夾", "UTAU 既定音源フォルダー", "UTAU 기본 음원 폴더", "Default UTAU Voicebank" } }
    ,{ "settings.utauWavtool", { "UTAU 合成器 / wavtool（可选）", "UTAU 合成器 / wavtool（可選）", "UTAU 合成ツール / wavtool（任意）", "UTAU 합성기 / wavtool (선택)", "UTAU Synthesis Tool / wavtool (optional)" } }
    ,{ "settings.chooseUtauVoicebank", { "选择 UTAU 音源文件夹", "選擇 UTAU 音源資料夾", "UTAU 音源フォルダーを選択", "UTAU 음원 폴더 선택", "Choose UTAU Voicebank Folder" } }
    ,{ "settings.chooseUtauWavtool", { "选择 UTAU 合成器 / wavtool", "選擇 UTAU 合成器 / wavtool", "UTAU 合成ツール / wavtool を選択", "UTAU 합성기 / wavtool 선택", "Choose UTAU Synthesis Tool / wavtool" } }
    ,{ "settings.chooseUtauResampler", { "选择 UTAU 重采样器", "選擇 UTAU 重取樣器", "UTAU リサンプラーを選択", "UTAU 리샘플러 선택", "Choose UTAU Resampler" } }
};
}

I18n::I18n()
{
    const auto locale = juce::SystemStats::getUserLanguage().toLowerCase();
    if (locale.startsWith("ja")) language = Language::jaJP;
    else if (locale.startsWith("ko")) language = Language::koKR;
    else if (locale.contains("tw") || locale.contains("hk") || locale.contains("hant")) language = Language::zhTW;
    else if (!locale.startsWith("zh")) language = Language::enUS;
}

juce::String I18n::text(const juce::String& key) const
{
    const auto found = strings.find(key.toStdString());
    if (found == strings.end()) return key;
    return juce::String::fromUTF8(found->second[static_cast<std::size_t>(language)]);
}
}
