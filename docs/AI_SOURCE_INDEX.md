# HachiShifter 逐文件索引与命令入口

## 当前204：dev滚动版本与GitHub扁平发布

公开版本文字改为`dev`，内部合法数值版本0.0.0用于CMake／Windows，WCSNDM0.0803不改。新增Windows `resources/windows/HachiVersion.rc.in` 通过JUCE已有自定义VERSIONINFO入口提供dev文字和prerelease/special-build标记；程序／MCP共同使用HACHI_DISPLAY_VERSION。`--version`输出当前版本，容许命令行末尾空token。

GitHub dev仓库继续根CMake、src、engines、resources、third_party布局与既有Windows云编译、app-only／with-models分包；外部lang必入云包并校验新增导入词条。云脚本、固定资源URL／哈希与独立构建补丁保留，不将旧嵌套juce/源码目录搬入仓库，不提供维护者独立维护的README。

## 当前203：真实Melodyne先行锚、G盘翻译部署及默认处理顺序

先行不是齿音或UV：Importer此前漏读`MUElement.anchorPoint`，SampleSettings错误以stable F0起点猜P。现以真实element anchor经原source time function投影为source-P，显式零／缺锚不制造P；保留原时钟，C=P反投影源／目标单位正确。实际test1 228音符=87正锚／141零锚，83个起点有声音符仍有先行；普通先行和齿音字段独立。新oracle及两样式像素／边界／UV／完整音频验证通过。旧HJPX作者手调P不盲改。

用户实际运行`G:\\HachiShifter-0.2.4-Windows-x64`只拷EXE，G盘语言目录仍1491条而D盘1509，已原位复现10个`mpd.options.*`缺键。现在D盘bin与G盘EXE+lang成套备份更新，G盘冷测缺键0，不改模型／引擎／用户偏好。以后移动程序必须同时携带lang目录。

统一默认权威：`defaultPitchAlgorithm`=NSF、`defaultRenderOrder`=stretchSpliceThenPitch、`defaultImportedPitchAlgorithm`保留显式偏好，`pitchAlgorithmPickerItem`/`renderOrderPickerItem`统一toolbar映射。仅引擎与处理顺序默认改变，`stretchAlgorithm`默认及已存选择不动。新增`--smoke-focused-defaults OUT`；既有`--smoke-native-melodyne-yellow OUT MPD`已改为真实anchor证据oracle而非stable-F0自证。详细取证与安装见本机 validation/ordinary-lead203。

## 当前202：旧HJPX连接恢复、导入选项与先行颜色

用户确认新版／新导入的移动正确，问题是旧HJPX缺少源usage字段，不应重写新版编辑器。新增 [LegacyNativeSourceUsage.h](../src/LegacyNativeSourceUsage.h)：仅加载时对缺字段clip，凭已有双向note/glide标记、同源及唯一精确source/target接缝恢复稳定usage身份；不改音频、clock、ID或明确解除。真实旧Downloads/teio.hjpx恢复38接缝，整工程76方向移动／撤销及实际鼠标／冷读通过，原文件SHA不变。

导入UI提取为 [MelodyneTrackSelector.h](../src/MelodyneTrackSelector.h)，轨道选择框常显本次导入的衔接音识别／虚拟连接／自动滑音／辅音合并开关，默认值沿用原偏好但不回写偏好；捕获同次选项传入既有prepare→convert→finish权威。普通音符与已标注衔接音可先连接一侧再延长另一侧，半路由只保持普通参考，不以桥源音高为第三目标。相关错误已接外部TSV五语，共1509条。

先行填充／波形已有物理P，修复外框、mld底线、包络轮廓全紫覆盖窄黄区。只分色不改P来源／clock／render；真实test1的6个有效稍后P，两样式像素／边界／UV／transition通过。专项：`--smoke-legacy-native-usage OUT old.hjpx`、`--smoke-transition-pair-import-options OUT`、`--smoke-native-melodyne-yellow OUT real.mpd`。完整音频4份SHA均与201一致，详见本机 validation/legacy-project202。

## 当前201：非完全切割永久接缝与虚拟组一次组源渲染（已本地安装，旧文件兼容见202）

用户明确要求：所有非完全切割（含虚拟连接）上下左右移动／伸缩不得断开；未连接素材允许重叠。已修复共同编辑轴、真实鼠标预览提交、极限夹持与旧自由移动入口。虚拟组现在在内存中按源时钟／支持区／作者增益／贡献窗组装，NSF幅度Mel或WORLD power/AP一次合成一个组输出，再切片到成员，不再逐源变调后混音。未连接声部仍各自保留。见 `validation/permanent-seam201/FINAL-REPORT.md`。

专项：`--smoke-native-permanent-seam`（4种源结构×移动／edge／上下鼠标／撤销冷读）、`--smoke-native-source-assembly`、`--smoke-world-source-assembly`；最终实际 AudioEngine、teio完整导出及旧单F0专项均通过。启用虚拟连接的teio两版因真正组源而为新声音，需重新试听；关闭版与WORLD默认基线不变。当前明确阻断WCS/HiFiShifter、robust组控件、分离导出和UTAU voicebank组装。

## 当前200：外部翻译文件为唯一语言来源（已验证并本地安装）

用户要求补全漏译，所有运行时翻译来自文件。`I18n.cpp` 已移除内置五语言表，统一 `text/uiText/pairText` 从 EXE 同目录 `lang/translations.tsv` 加载；空格子仅可回退该文件的英文，缺键显示 `[key]`，坏／缺文件保留上次有效文件目录，不偷偷回读内置译文。源码规范目录为 [translations.tsv](../resources/lang/translations.tsv)，CMake复制至构建程序目录，不编译进EXE。原七列后增加 `aliases` JSON元数据以保留旧源码字面量查词，用户修改译文不会破坏键。

新增 `--smoke-translation-no-file` 与文件唯一来源哨兵测试；`HACHI_TRANSLATION_AUDIT_FILE` 可选导出实际缺键。静态校验工具 `tools/validate-translations.py` 检查空格／占位符／别名／模型稳定FLAG显示标签；字段ID／用户歌词／别名／路径不做语言变换。当前1491条五语／7455格全部非空，主窗／OTTO／高级包络／和声／DS发音／素材选择等实际场景缺键为0；五语设置与面板截图、14专项及安装后3专项通过，完整NSF三版和WORLD基线音频哈希不变。`--smoke-file-only-translation-ui` 为五语真实组件覆盖；`I18n::message` 在显示边界匹配同一文件中的固定或单参数消息别名，保留技术详情，不更改后端协议。内置预设显示延迟取词以免冻结首次语言。维护说明见 [翻译文件](translation-file.md)，进度／备份见本机 validation/translation-file-only/STATE.md。

## 最新199反馈：字母按钮与虚拟组唯一目标线

轨道头恢复C/M/S/A/N，摘要去掉齿轮。新 [NativeVirtualSinglePitch.h](../src/NativeVirtualSinglePitch.h) 只派生完整组目标，复用现有SharedPitchLine／resolved overlay，无新DSP／持久副本。正常素材贡献决定基线，typed衔接音／逻辑两半桥不叠第三个音高，组内颤音只计一次；源UV掩码不变。所有成员使用同一目标对象，绘图区间不重叠，关闭原素材／独立颤音／虚拟图形的重复描线；标点仍可编辑。

`--smoke-native-virtual-single-line` 检查重叠F0、不同颤音、桥源移低、UV和绘制分区；`--smoke-native-virtual-multi-split` 新增逻辑半桥F0检查（84个重叠点最大差0）；共享节点实际鼠标预览／提交、同高Bezier等复验通过。最终真实渲染已确认消费新F0：完整NSF三版均成功，全关闭版保持原SHA，开启虚拟连接的两版按用户新规则改变（需重听）；WORLD默认关全工程基线仍53879EF9…C3E22。已观察真实主窗与桥接示例；细节见本机 validation/gui-review-198/STATE.md 的最新补充。

## GUI198 界面／操作整改（已本地安装，后续反馈整改中）

用户 2026-10-26 在虚拟连接验收后恢复 GUI198。进度权威：本机 validation/gui-review-198/STATE.md。摘要见 [更新记录198](updates.md)。

- **Melodyne 先行 P**：`SampleSettings::convertMelodyneProject` 取第一个稳定有声帧（连续两帧），上限 min(300 ms, 音长 40%, attackDuration)；起点有声／无证据 P=起点；C=P 经冻结原时钟。`AudioEngine::renderCapabilityWarnings` 把 `nativeProvenance=="melodyne"` 的源快照 P 视为原生几何，不报 UTAU-only 覆盖。门：`--render-teio-full-virtual … baseline` WAV SHA 与 197 一致；`--smoke-melodyne-timing OUT real.mpd`（参数顺序：输出在前），现在使用 `publishAnnotations=false`。
- **单行编辑栏**：`MainComponent::resized` 按 Group.priority/required/menuable/optional 规划；溢出组件进入 `showMoreActionsMenu`（TextEditor 以弹窗编辑，回调同原控件）。紧凑传输栏隐藏说明标签。主窗口最小 760×460 逻辑像素。门：`--smoke-gui198-layout OUT`（中／日×深／浅×侧栏×四尺寸，单行、无重叠、状态栏/位置/传输可见、分隔条双击、自适应 20–50%）。实窗截图脚本 validation/gui-review-198/capture.ps1。
- **重叠标记**：`PianoRollComponent::nativeOverlapMarkers`（绘制于重叠绘制循环）、`nativeOverlapMarkerMouseDown`、`applyNativeOverlapOrganize(index, 1保留|2裁前尾|3裁后头|4连接)` 复用 `setNativeAudioOverlap`／`trimNativeNoteEdge`／`linkNativeAudio`。
- **翻译**：`I18n::loadTranslations` 重复键首行生效；`exportTranslations` 遇未知表头拒绝覆盖；新增 `engine.*` 渲染能力提示键。
- **OTTO**：`OtoWaveformEditorComponent::formatNumber` 与 `MaterialAnnotationEditor::msText` 统一 0.1 ms；未改动文本不回写。两个波形视图都画淡入／淡出斜坡并可拖动内侧端点（写同一 edge 字段）。
- **联动**：`materialSourcesChanged` 末尾对打开的素材选择框重建索引；`showStatusText(text, holdMs)` 保持提示。
- **已知 197 同样失败的专项**（非本轮回归）：timeline-selection(6)、material-binding(3)、oto-spectrum（环境）、native-vibrato/tempo-change 截图与最小宽度断言、accompaniment 1 项。
- **测试素材库隔离**：validation/otto.ini（空清单）阻止测试写入上级 D:\hjm\otto.ini；后者已被历史测试写入约 680 个 validation 段，待用户同意后清理。

## 已完成：虚拟连接收尾（NSF声音已获用户验收）

**最新用户验收与最终停点：** 用户确认 NSF／先拼接后合成三版自动滑音提升效果。四项收尾现已完成，最终12专项全过，重新完整渲染的三份NSF WAV与用户已验收文件SHA各自完全一致；**在此停止，未开始GUI198**。此前“未验收／仍待”阶段说明已被此条替代。操作与测试摘要见本机 validation/virtual-connections/CLOSURE-READY.md。

**邻接联动：** `NativeCoupledSelectionMove` 复用现有编辑时间轴warp后还原原物理owner；默认coupledMovement=true，菜单可选独立移动，已分离／大交叉不夹回也不造源。多音符／嵌套owner有无关音符时明确保护，改独立移动或先分离owner，不擅自伸缩da。`--smoke-native-virtual-coupled-move`20项含实际接缝、冷读PCM通过。

**贡献窗双柄：** [NativeVirtualHandoverWindow.h](../src/NativeVirtualHandoverWindow.h)＋Model单事务setter/reset，稳定物理端点ID和相对偏移，限制真实支持／typed-middle。音符工具选组显示左右小柄，拖动预览／抬起提交，Esc取消、双击单窗复位。`--smoke-native-virtual-handover-window`27项、`--smoke-native-virtual-handover-gesture`10项含真实PCM最大差1.56e-8通过；失支持保留资料并报警而不静音。

**同高直接起拱：** `nativeVirtualCurveFromAbsoluteGrips` 复用原V2精确cubic/de Casteljau细分；两点同高不再拒绝控制柄，原标点预览／提交权威不变。`--smoke-native-virtual-flat-bezier`10项真实拖柄／实际目标／撤销冷读通过，无折线近似或新DSP。

**新增音符右键身份切换：** `ProjectModel::setNativeNoteTransition` 复用真实源覆盖／`validateNativeTransitionShape`，明确中间源分界、单步撤销／冷读，不写源库、不改 P/C／时钟／F0；nullopt恢复普通音。原生音符菜单提供切换／编辑中间分界／恢复普通，真实对话框按鼠标源时钟位置预填、用户确认，取消／过期不写；已有逻辑半桥、失效组端点／手调交接整体保护。`--smoke-native-note-transition-menu out`22项通过，9相关专项通过。

**用户指定保存工程的 NSF 三版渲染：** `--render-saved-nsf-three saved.hjpx models/nsf_hifigan OWN-output` 只加载指定HJPX，不替换为旧MPD、不重新转换源标注。实际已确认 Downloads/teio.hjpx（原写hjps未找到，经用户确认），BPM192/81片段81音符/约9.812秒；三版均实际 `nsf-hifigan-onnx[cpu]+variable-mel-hop-splice-first`、先拼接后合成、48kHz/24bit立体声。16虚拟组／18有效滑音窗口，歧义7对保守跳过；逐帧核对及31源WAV/13模型文件完整性通过。结果在 validation/virtual-connections/teio-hjpx-nsf-three/render46；不代表此前开发总目标或用户听觉已通过。

**完整 teio 最终技术核对（非用户验收）：** `--render-teio-full-virtual real.mpd OWN-output baseline|disabled|off|on` 四模式均完整228片段／228音符、221.201333秒，原MPD＋98源WAV字节不变。新全关基线／禁用组／安装197只读打开同一完整基线HJPX的WORLD导出，全部10,617,664帧以及WAV SHA逐位一致；安装EXE不变。完整滑音OFF／ON有真实差异但未试听判定。文件在validation/virtual-connections/fullteio45，原时间轴前约197秒空白必须提示，不裁成短片冒充整首。目标仍ACTIVE，未安装／未恢复GUI198。

**最高验收前提／全 teio 已渲染：** 用户要求完整工程可渲染后才评价自动滑音。`--render-teio-full-virtual real.mpd OWN-output off|on`保留全部228片段／228音符及原时间轴，明确WORLD，开／关均约5秒导出221.201333秒完整文件（实际音频区约197秒后）。全帧读回和10,617,664帧A/B比较完成，但试听／有效性未判定。修复实测219.35秒0.975ms自动窗口冲突：只自动／自动重寻真实锚点，不改手调或绕警告。`--profile-teio-request-fingerprint real.mpd`不复制素材，定点实测旧函数完整请求约1022ms，历史长等待本次未复现，不能声称原因已修复。9专项全过，安装未动。

**实际主窗缩放：** `--smoke-main-real-multi-bridge-window OWN-project.hjpx OWN-output`19项通过，实际Main HWND/UI1.25/GetDpiForWindow120DPI，原生2080×1265在工作区内；只捕获自有窗口。7成员选择／源贡献卡／两逻辑桥曲线／可见控制点预览提交撤销验证。截图发现卡片遮挡，现切入点工具收起卡片，不改模型；7相关专项全过。输入是向实际组件派发JUCE事件，不是用户物理鼠标／试听验收，安装197未动。

**真实多桥局部音频：** `--smoke-native-real-multi-split-audio real.mpd OWN-output` 提取实际共享端点的两条两半桥，只渲染7片段短窗（WORLD明确诊断），产出无组／仅路由／加滑音三份A/B。12项实际通过：作者事实指纹、原MPD/5WAV字节、A/B关逐位恢复、冷读逐位一致、播放导出差1.19e-7、两个滑音窗口实际活跃。**首交接20ms能量相对原叠加降低约5dB，已明确记录，未盲补增益，仍须试听**；这不是NSF或整句质量验收。

**长链逐交接手调：** 组内 `hopPitchCurves` 以左右端点 ID 定位；`nativeVirtualPitchHops` 共用逻辑桥投影，`prepareNativeVirtualPitchCurve` 共用整组／局部 V2 权威。原点工具现可编辑局部交接，前插成员不重命名旧曲线；共享底线之后叠加手调，菜单可只恢复当前段自动。`--smoke-native-virtual-hop-curve out` 验证鼠标／冷读／撤销／延伸／共享覆盖，`--smoke-native-virtual-multi-split` 增加局部两半桥手调＋UV／证明保护。后续窗口变更冲突仅暂停相交音高段、保留源绑定并明确诊断，远端交接不受影响。下文“逐 hop 仍未接”的旧阶段说明已由此替代；实际桌面与试听仍未验收。

**波形一致性：** [NativeVirtualWaveformContext.h](../src/NativeVirtualWaveformContext.h) 给作者波形键加入组内 peer 实际时钟／音高／UV／源文件戳依赖，不波及无关音符。引擎组 owner 发布原作者键，不拿清除 connected 标志的投影当 UI 身份。峰值控制线程复用 mixer 同一虚拟贡献对实际 PCM 逐桶测量；明确只含逐源路由权重，不是最终混音。弱 entry＋作者 hash 缓存跨重建复用，无关编辑不重测。`--smoke-native-virtual-peer-waveform out`14项通过，11专项全过；后续真实演唱／桌面试听仍待验收。

**虚拟曲线实际鼠标工具：** 原 ConnectionPitch 点工具现接受虚拟组 DTO，Model 草稿预览／一次提交携带组；顶点、端点时间、Bézier 两柄、曲率菜单、加点／删点／Esc／整轨笔刷共用既有路径。精确笔刷提取为 `composeResolvedConnectionPitchStroke`（旧／虚拟共用同一 splice），不是平行实现。`--smoke-native-virtual-curve-tool out`15项真实鼠标通过，原完整连接UI45项也通过。旧声明“专用鼠标交互全部未接”已由此替代；多 hop 逐段独立手调和实际桌面／试听仍待完成。

**非线性曲线新权威：** [NativeVirtualCurveCodec.h](../src/NativeVirtualCurveCodec.h) V2 保存原形状／控制柄，实时锚点通过整曲线仿射变换适配；同高端点退化时精确 de Casteljau 细分，不采样成折线。Model setter 接受五种既有非线性、真实有声端点时间编辑和手柄 no-op 检测，内部极值／不支持端点整体拒绝；旧V1解释不变。卷帘手调组曲线覆盖源共享线。`--smoke-native-virtual-nonlinear-curve out`19项通过，12专项全过；**专用组曲线／端点手柄鼠标交互仍待接入**，此阶段不是完整UI验收。

**最新直接用户纠正——同文件非连续也允许：** a、b、c 中 a→c、重复／反向使用源区间都可走虚拟组。`NativeVirtualMember` 派生实际源范围，`nativeVirtualMembersSourceContinuous` 同时供规划与 Model 入口分流；不再仅按文件名判“同源不准虚拟”。真正连续相接的新源切片仍保留 partial-cut 权威，既有组不自动改类／断链。`--smoke-native-virtual-same-file-skip out`12项通过含实际PCM、音频链接分流、非零源时钟、撤销冷读与连续源反例，7相关专项全过。此前任何“同一录音一概拒绝”的阶段说明均由此纠正。

**统一 GUI／CLI／MCP：** [MelodyneImportOptimization.h](../src/MelodyneImportOptimization.h) 两阶段复用现有检测与 Model。MCP `project_open`(MPD)／`import_melodyne` 支持3个默认false布尔 `auto_merge_consonants`／`auto_virtual_connections`／`auto_virtual_pitch`；恢复工具 `restore_merged_consonant` 同一存档权威、显式布尔确认。CLI `--import-mpd in.mpd NEW-out.hjpx` 配套3个 `--auto-*` 开关，不覆盖目标。各入口源转换显式 `publishAnnotations=false`，仅工程内标注，不隐式写原素材库；明确注册仍走原发布服务。`--smoke-melodyne-optimization-entry out [real.mpd]` 11项含实际MCP调用通过；实际CLI默认／仅滑音均0组、全开27组／35连接提交，源哈希不变。接口静态导航已重新生成，非音频验收证明。

**多两半桥长链：** [NativeVirtualGroupPlan.h](../src/NativeVirtualGroupPlan.h) 将旧单证明＋附加证明解析为原物理成员／逻辑角色／索引范围；标量实时贡献按逻辑角色再分给唯一半段。扩展、自动导入、逐逻辑桥滑音、所有半段联动和视图身份共用这些证明。`--smoke-native-virtual-multi-split out` 14项通过含真实7源PCM、冷重开、UV及显示；真实 test1 只读模型预览已提交全部3个两半桥（证明计数1＋2），无原文件写入、无NN。下文“多两半桥仍未完成”的阶段说明已被此项取代；真实演唱／整窗验收尚未完成。

**已有链延伸新入口：** [NativeVirtualChainExtension.h](../src/NativeVirtualChainExtension.h) 保留旧有向边及组 ID，在同一 Model 事务中延伸／合并普通链，拒绝分叉环路、真额外声部及会丢失手工曲线的合并。自动新候选可共享端点，不重写输入已有组；长链自动 pitch 按2音／完整桥3音分成局部窗口，渲染消费各窗口而非覆盖全链。`--smoke-native-virtual-chain-extension out` 20项通过含生产请求、撤销／冷读及两条完整 typed bridge 共享端点。**多个两半桥的长链仍未完成**，现有四片段证据不会被丢弃来强行延伸。

**共享节点／故障隔离新专项：** `--smoke-native-virtual-shared-pitch-gesture out` 11项通过，实际卷帘抓取共同节点、两侧预览、提交／撤销／冷读和渲染目标一致（原 owner 不合并）。[NativeVirtualRenderPlan.h](../src/NativeVirtualRenderPlan.h) 新增可选不可用组诊断；作者校验仍严格，生产播放逐组隔离缺失成员／WAV，禁止坏组使整首静音；显示警告且导出仍阻止静默忽略。`--smoke-native-virtual-unavailable-group out` 6项通过含真实回调逐位对照与明确导出拒绝。

**最新入口（任意链／共享模式）：** 用户已确认音频链接也只能提交虚拟组，并为组增加 `sharedPitch`。`ProjectModel::linkNativeAudio` 跨源→同一 `setNativeVirtualConnectionForNotes(...sharedPitch=true)`，不绕回旧复合 writer。普通链不再限2／3／32；AudioEngine／贡献卡统一用无分配标量 `nativeVirtualMemberContribution`。新 [NativeVirtualSharedPitch.h](../src/NativeVirtualSharedPitch.h) 临时投影复用 `sharedPitchLines`，原 owner／文件／ID 不变，UI 与请求均消费同一线。`--smoke-native-virtual-chain-shared out` 12项通过（含40成员、7源逐样本PCM、冷重开和真实卷帘共享线）；最终12专项全部 exit0。已有组延伸共享端点、多个两半桥共处长链、完整共享节点编辑及 peer 变更后的波形身份仍待完成。下文旧阶段说明是历史，不代表当前所有能力仍未接入。

[设计及实施边界](native-virtual-connections-design.md)。[NativeVirtualConnectionPlan.h](../src/NativeVirtualConnectionPlan.h) 只派生明确端点的双素材／带识别衔接音三素材窗口，不提交工程、不新增DSP。桥接身份来自 kind 与 source middle，不使用 `e_g` 等名称猜测。专项入口 `--smoke-native-virtual-plan out`；[NativeVirtualGroupIO.h](../src/NativeVirtualGroupIO.h) 与 ProjectModel 已接持久组/撤销/解除，`--smoke-native-virtual-model out` 已验证基础往返。跨源旧操作转发 `setNativeVirtualConnectionForNotes`，`--smoke-native-virtual-entrypoints out` 检查没有第二套 legacy 写入。[NativeVirtualRenderPlan.h](../src/NativeVirtualRenderPlan.h) 是 render-snapshot 路径投影，暂时抑制原内部直接边而保留原工程数据；AudioEngine LoadedClip 复用统一 fadeEnvelope 消费组权重，`--smoke-native-virtual-mixer out` 为控制 PCM 校验。后两项新专项实施中，以实际运行报告为准；能量谷补偿、独立源拆分、自动滑音、辅音合并和导入选项尚未完成，不能把规划／模型测试冒充用户试听验收。

[NativeVirtualPitchPlan.h](../src/NativeVirtualPitchPlan.h) 的 `--smoke-native-virtual-pitch-plan out` 验证稳定有声锚点、目标走势、手工保护和跨无声桥参考规划（16 项通过）。已复用 `overlayResolvedConnectionPitchCurves` 接入原 request 路径，并加入仅作用于相关片段的组曲线缓存摘要；`--smoke-native-virtual-pitch-request out` 验证目标改变/UV保护/关闭恢复，不代替演唱试听。AudioEngine 的 `virtualBindingAt` 将组增益严格限制在成员实际支持内，支持 da_re 的 re 接组而 da 保持原 PCM；mixer 专项11项通过。`nativeVirtualOwnersIndependentlyRenderable` 现仅拒绝同一已混合 owner 内的实际重叠、无法分别加权的音符，而非所有多音符片段。

移动绑定专项 `--smoke-native-virtual-move out` 覆盖单音／组原生移动、移调、交叉、嵌套、撤销／重做、冷重开及实际音频回调（20 项通过）。`NativeVirtualResolveMode` 将新建候选验证与已有连接解析分开：后者保留成员 ID 和方向，纯几何移动不拆链。当前真实空隙仍静音而关系保留，尚未完成同源式邻接伸缩，不能把这项测试当作全部操作／声音适配完成。

手工组曲线基础：`setNativeVirtualPitchCurve`／`resetNativeVirtualPitchCurve`，旧 `setConnectionPitchCurve/Point` 遇组 ID 转发同一 writer，跨源旧图只读，不能旁路。`NativeVirtualGroupIO` 持久 normalized-time／live-chord residual，支持移动、移调、冷重开与撤销。`--smoke-native-virtual-pitch-edit out` 17 项通过。当前提交仅接受固定首尾锚点及线性段，完整曲率/端点拖动与界面未完成，明确拒绝非线性手柄而非静默近似。

[NativeVirtualAutoConnect.h](../src/NativeVirtualAutoConnect.h) 为导入候选发现，提交仍只调用 Model 公共虚拟组动作；typed 桥优先，不按名称，已有组不覆盖。导入页 `import.autoVirtualConnections` / `import.autoVirtualPitch` 均默认关，Main 两条用户确认分支在最终轨道配置后调用同一优化入口。`--smoke-native-virtual-auto out` 16 项通过（含真实设置控件默认值）。辅音合并、CLI/MCP 显式优化参数、真实工程识别质量仍待完成。

两半衔接音新基础：[NativeSplitTransitionPlan.h](../src/NativeSplitTransitionPlan.h) 区分“独立识别到的一个逻辑桥”和原始 join/source 证据，验证四物理片段→三逻辑角色。内部实际源切点直接为桥中间分界，不按能量中点，不把左右正常音重叠当第四声部。`--smoke-native-split-transition-plan out` 16 项通过（纯规划，不是实际四源渲染/自动识别完成）。[MelodyneImporter.cpp](../src/backend/MelodyneImporter.cpp) 的只读 `inspectJoinGraph` / `--inspect-mpd-joins input.mpd output.json` 输出原始 graph 字段，不解析/复制媒体；真实 test1 只读运行通过并核对原 SHA 不变。当前 detector 仍会跳过连接片段，后续需要接逻辑桥识别/登记及四物理组混音，不能以这项基础宣称图示已自动支持。

四片段实际组路径：`NativeVirtualGroupPlan.h::resolveNativeVirtualGroup` 是模型/混音/显示共用分发器，桥源证据由 `NativeVirtualGroup.splitTransition` 经同一创建接口持久保存，`NativeVirtualGroupIO` 严格验证14字段。render 缓存四物理成员/三逻辑贡献，无内部额外虚拟边；`--smoke-native-split-transition-mixer out` 已验证实际回调、关闭 PCM 逐位恢复、撤销/重做、冷重开、同一入口及不按交叉 start 排错桥前后。自动两半识别和OTTO单逻辑行登记尚未接入；四片段自身各半单独移动的连续伸缩还需实现，不把当前纯整体移动等同完整操作验收。

最新两半检测：[NativeSplitTransitionDetection.h](../src/NativeSplitTransitionDetection.h) 消费导入器在图销毁前保存的 `MelodyneImportResult.continuousSourceJoins`（明确 source/element/join/非默认时钟证据），组合短连续片段、稳定有声前半、真实切点能量谷与唯一左右上下文，名称无关。`--smoke-native-split-transition-detection out` 15 项通过；开启自动虚拟连接时优先经同一 Model 提交四物理组。默认关闭无音频扫描。只读 `--inspect-mpd-split-transitions input.mpd output.json` 不递归找媒体、不转标注、不复制/渲染；真实 test1 保留17对源事实、输出3候选，无缺失、原SHA不变，不是人工正确率验收。OTTO 单逻辑桥登记仍未接入；默认身份登记不能因此宣称已经支持两半桥。完整相邻多桥链共享端点也仍需扩展，现有单组成员互斥会跳过后续共享端点候选。

独立身份／明确注册：`ProjectData.logicalTransitions` 保存两半属于一个发音的完整源范围与真实切点，`NativeLogicalTransitionIO.h` 与组共用源证据 codec；身份不触发音频优化、不改半片段 C/P/voicing，注册音乐指纹排除它。`ProjectMaterialRegistrationFacts::derive` 在用户确认注册时合并两个 source facts 为一条 transition 行/两个绑定成员，由既有 prepare/stage/publish 发布；`--smoke-native-logical-transition-registration out` 13 项通过含实际 OTTO 冷读一行。导入旧 sidecar 转换尚未做相同合并，不混淆两条路径。

`--smoke-native-virtual-connection-ui out` 验证真实 PianoRoll 缓存、两半绿色视图身份（原 NoteData 不变）、两个外连接标记、菜单整组选中/贡献卡/开关对照/滑音请求/解除/撤销；软件固定1100×700预览已检查，非真实整窗 DPI 验收。原 `--smoke-registration-facts` 的“两个独立 variant ID”断言在当前与已安装197均失败，其他项通过，已记录而未伪称历史专项全通过。

导入 sidecar 一致性：[NativeLogicalTransitionSource.h](../src/NativeLogicalTransitionSource.h) 共用完整逻辑 transition 行构造及精确源半行匹配；Main 在转换前识别逻辑身份，SampleSettings 只整理证明充分的 fresh half rows，再走原 save。显式注册 Facts 使用同一构造权威，其他行不改、冲突警告、重复幂等。`--smoke-native-logical-transition-registration out` 现17项通过，包含实际 sidecar 冷读一行与两半原时钟/请求精确一致。此前“旧 sidecar 仍是半行”的阶段说明仅属历史状态。

辅音合并基础：[NativeConsonantMergePlan.h](../src/NativeConsonantMergePlan.h) 为纯同源规划，不走跨源虚拟连接。明确齿音决定＋原始相向 join/source 事实＋精确源切点，合并保留元音 ID/中心/走势/颤音和所有 warp 节点；P=切点，C=元音原P，否则C=P，逻辑桥任一半排除。`--smoke-native-consonant-merge-plan out` 18项通过。原片段目前仅完整复制到规划输出，持久 Model 合并／冷重开拆回／自动检测／默认关闭开关尚待接入；不同表达增益/包络明确拒绝而非丢失。不能把纯规划通过冒充辅音合并功能已交付。

辅音实际提交／持久拆回：Model `mergeMelodyneConsonant`／`restoreMelodyneConsonant` 通过原 `toValueTree/fromValueTree` 二进制 codec 保留两片段，不另做通用分割；`NativeConsonantMergeIO.h` 严格版本/身份/SHA/有界存档。当前源路径+原SHA支持同字节注册搬移，恢复编辑后状态需要显式确认。`NoteData.consonantMergeId` 经存档核验保护转换中的独立 P/C，普通导入不变。`--smoke-native-consonant-merge-model out`16项通过（含冷重开完整原指纹拆回、源搬移、OTTO P/C、转换请求一致），自动识别/默认关闭开关/恢复UI仍待接入，不能提前称已交付。

默认关闭辅音优化：[NativeConsonantMergeDetection.h](../src/NativeConsonantMergeDetection.h) 验证真实源 join/warp + explicit-UV + 稳定元音，再用短 PCM 高频/过零/非周期证据；marker 仅支持，纯高频周期音/呼吸/逻辑桥/竞争候选不吞并。`autoMergeMelodyneConsonants` 只委托既有 Model 持久合并。导入页 `import.autoMergeConsonants=false`，Main 在旧P被转换之前处理；新右键19820走存档恢复并明确确认后续编辑丢弃。`--smoke-native-consonant-merge-detection out`15项通过含真实设置及菜单。只读检查实际 test1 为17源事实/3桥候选/0辅音候选，17前段都未通过明确全UV门槛，未强行合并；不是实际齿音演唱验收。CLI/MCP优化参数仍待接入。

导入非完全切割的移动联动：[NativeCoupledSelectionMove.h](../src/NativeCoupledSelectionMove.h) 按真实导入证据（同 usage group／同录音／相向连接标志／源与目标接缝吻合）联动，复用既有 warp 投影，`moveNativeNotes` 先走此路径并保持单次撤销；未触及该关系时原独立移动不变。只有同源的 `splitTransition` 两半联动，普通跨源虚拟成员仍可自由分离／交叠／交叉。[AudioEngine.cpp](../src/AudioEngine.cpp) 的 `virtualBindingAt`／`fadeEnvelope` 改用与混音一致的采样时钟（修复移动重叠时 0.0605 的 PCM 偏差）。`--smoke-native-partial-cut-move out` 15 项通过，四片段混音入口另加 5 个移动渲染用例；真实工程拖动试听仍待完成。

## 更新197：衔接音、自然源音高与 MIDI

`PronunciationTiming.h/.cpp` 的 `NativeTransitionShape` 只持有严格内部源分界；`OtoWaveformEditorComponent` 在过渡发音模式下复用已有边界／fade 编辑，不显示 C/P/V 控件。`NativeTransitionDetection.h` 是 Melodyne 导入的保守短完全切割能量谷分类器，默认开关由 `SettingsComponent` 的 `import.autoRegisterTransitions` 管理；`ProjectMaterialRegistrationModel.inc` 的音乐源指纹排除纯身份注释，避免类型登记制造音频变更。

`NativeScoreMaterialReplacement.inc` 与 `ProjectModel.cpp` 的 `nativeSourceTrendTranspose` 让新原生绑定沿既有 owner clock 保留真实源走势并移到目标中心，显式用户音高编辑优先。`ProjectModel.cpp` MIDI 全文件／单轨共用精确 Lyric onset，缺省 `-`。聚焦入口：`--smoke-native-transition-detection`、`--smoke-native-transition-shape`、`--smoke-native-source-trend`、`--smoke-native-midi-material`；teio 专用 `--smoke-teio-transition-registration`。仅 teio 做真实 CPU PCM gate，东京工程未运行。

## 更新196：单标签与设置主题同步

PianoRoll 普通原生发音统一在音符级标签路径输出，不能按 C/V 各画一次；`nativeWaveformLabelBounds` 使用每音符的实际样式高度和先行支持起点。[SettingsComponent.cpp](../src/SettingsComponent.cpp) 的 themeEdited 区分明确主题改动和其他设置；`lookAndFeelChanged` 同步页／标签页／现有文本字形色，Main 为设置的独立窗口共享自身 HachiLookAndFeel。`--smoke-native-material-appearance out` 增加重复别名分段截图与真实主题／样式控件回归；final25 证据不冒充音频验收。

## 更新195：登记源切点与音符外观

[NativeMaterialContinuity.h](../src/NativeMaterialContinuity.h) 只派生真实后继／同源时钟切点、先行显示支持和相位颜色，不新增持久时钟或音频写者。PianoRoll 展开子源前缓存切点，源／渲染波形按同一所有权分配；每个所有者的时钟在绘制／命中循环共享。NativeNoteTiming 的连续组移动保留完整 score-aligned 源支持，UST/MID 协议乐谱的起点即目标 P；PronunciationTiming 统一登记衔接音无先行的 getter/setter。

[SettingsComponent.cpp](../src/SettingsComponent.cpp) 的界面项 `ui.noteStyle=mld/rep` 由 Main 应用到 PianoRoll，非法值回 mld。 [SourceWaveformPreview.h](../src/SourceWaveformPreview.h) 使用相位同色包络，不再用原火焰色带。新命令 `--smoke-native-material-appearance out`，验证切点、两样式／两主题亮度、命中／移动和真实设置保存；[测试源码](../src/tests/NativeMaterialAppearanceSmoke.h) 明确使用控制事实，不冒充 NN 验收。[完整说明](native-material-appearance-195.md)。

## 更新194：鼠标操作分工（仅界面）

[PianoRollComponent.cpp](../src/PianoRollComponent.cpp) 在已有音符命中／移动权威上记录再次短单击意图，释放后通过系统双击间隔延迟，最后仍调用原菜单33素材动作；快速双击取消待执行意图、保留 `alignNotePreutteranceToGrid`。长按、拖动、键盘、失焦、修订／选中变化会使延迟请求失效。[操作专项](../src/tests/NativeNoteClickInteractionSmoke.h) 入口 `--smoke-native-note-clicks out`，没有 AudioEngine、NN 或音频对照。不再将双击写作选择素材入口；193的历史完整音频证据保留，不重复运行。

## 更新193：乐谱原生替换／C=P／物理交叠强制连接（待用户验收）

[行为及实际证据](native-score-replacement-193.md)。[NativeSourceReplacementPlan.h](../src/NativeSourceReplacementPlan.h) 为纯时钟规划；[NativeScoreMaterialReplacement.inc](../src/NativeScoreMaterialReplacement.inc) 仅由 ProjectModel 引入，批量提交和单音乐谱替换不得另建写者。[MainMaterialSourcePicker.inc](../src/MainMaterialSourcePicker.inc) 持有 Main 的同一选择会话，复用 Catalog 精确连续行和最终重解析；候选试听复用 Main 原音传输。

[NativeOverlapConnection.h](../src/NativeOverlapConnection.h) 是现场物理交叠证明。[PianoRollComponent.cpp](../src/PianoRollComponent.cpp) 展开显示子片之前缓存有效边界／滑音投影，绘制不额外复制整工程；AudioEngine 新式交叠的滑音／缓存键使用原物理轨，无该结构的旧路径不变。SampleSettings 转换 Melodyne 时默认源 C=P，保存旧时钟后再同步目标锚点。

新命令：`--smoke-native-score-material-replacement out`、`--smoke-native-score-replacement-ui out NSFmodel`、`--smoke-native-force-connection out`、`--smoke-native-force-connection-ui out`、`--smoke-melodyne-timing out actual.mpd [legacy.hjpx]`。实际 UI 测试仅由 MainComponent TU 引入，不在 Main TU 重复定义。teio 仍使用 `--smoke-registered-project-audio project out NSFmodel`，必须比较冻结版本、注册前／收纳后／冷重开三阶段实际 PCM，不以接口清单代替验证。

## 用户实际保存／三模式／重叠放入修复（2026-10-23 更新192）

`JapaneseAutoAnnotation::infer` 接收显式 `InferenceMode`，不得依手工栏是否为空猜执行阶段；`JapaneseAnnotationBatch` 分开只读 `recognitions` 和可发布的对齐 `results`。Dialog 只适配三模式、回填及确认，OTTO 仍由 Batch 的同一原子发布权威写入。实际提交后释放门闩，再由 Main 刷新同一干净人工窗口的新确切 Session／指纹；源发布和普通重开统一经 Main 前检／完成回调，将稳定 Session audioVariantId 传给 `ProjectModel::applySourceSettingsWithReport` 的可选确切 scope。不能以一套变体的行覆盖其他变体或给 bare 音符猜登记。

`NativeMaterialSelectionPlan.h` 为真实 `addMaterialPronunciations` 服务：保留支持快照，按用户实际选择规划物理并集与不重叠的所有权，使用明确源锚点而非中点猜测。裁剪实际关系供既有 `NativeSourceEdgePlan` 验证；源别名保存不得清除项目实际使用后继。

新增命令：`--smoke-japanese-inference-modes out realAudio models`、`--smoke-japanese-inference-dialog out realAudio models`、`--smoke-native-material-overlap out`、`--smoke-native-source-reopen-scope out`；真实 Main 入口沿用 `--smoke-main-japanese-annotation out models realAudio`。实际结果、旧绑定专项既存失败和包内推理证据见 verification/annotation-save-modes，不以接口或测试文件存在代替运行。

## 自动标注人工修正闭环（2026-10-23 后续）

`PronunciationTiming` 的 affine／clip／merge／default／edit NativePronunciationEdges 函数是边界编辑共同权威，供 Session、现有概览／精确编辑器及自动标注生产者复用。未明确改边界字段时，源范围修改按既有语义仿射映射；源标注拆分裁剪物理窗、合并仅取真正最外侧。`NativeSourceEdgePlan` 使用组合时钟分配已激活外沿跨切片的增益贡献，拓扑外沿身份与窗口覆盖分开；AE 对带 profile 的采样时间使用同一实际 sample-grid，不改变无 profile 算式。`NativeNoteTiming` 对 profile 与已知物理端点、当前时钟仅相差数值容差的投影保留原精确锚点，不放宽 codec。

新增：`--smoke-native-pronunciation-edge-edit <输出目录>`、`--smoke-native-edge-profile-gui <输出目录>`、`--smoke-native-source-slice-edges <输出目录>`。分别验证真实 Session 保存／撤销、现有 GUI 源及局部目的地、实际 ProjectModel 拆分与跨子素材 PCM。结果以 verification/annotation-editing 为准。

## 显示菜单能力修复（2026-10-23）

旧配布 r 编译成功后定点移植：`MainComponent::viewMenuItemEnabled(int, TrackType)` 为显示菜单与调用守卫的共同策略，`selectedEditorTrackType` 从模型读取选中轨道类型，不读算法下拉框。Hachi 灰禁拼字信息，UTAU／DiffSinger 使用实际音素显示；原生实际波形与辅音虚线仅 Hachi 可用。`Theme.cpp` 的弹出菜单禁用文字保持 opaque muted 灰色。`tests/EngineViewCapabilitiesSmoke.h` 由 MainComponent.cpp 单次包含，命令 `--smoke-engine-view-capabilities <输出目录>` 记录三轨菜单、守卫、偏好及深浅主题 PNG；测试结果以实际报告为准。保留自动注音、源试听与 CSV 门闩入口，未覆盖 Main 整文件或改动 DSP。

## 开发包修复入口（2026-10-22）

当前修改基线为 `HachiShifter-0.2.6-dev/source`。别名发布由 `ProjectModel::applySourceSettingsWithReport` 同步显示元数据（局部调音快照仍保留）；`PianoRollComponent` 不用继承的“-”区段文字遮挡已命名发音。`AssetManagerComponent::refreshMaterialAnnotations` 供快速／精确编辑保存复用，`OtoWaveformEditorComponent::materialSourceSaved` 刷新共用发音外层。此修复按用户要求仅编译、不执行测试或渲染。Melodyne 导入将真实同录音连接组保存为 `ClipData::materialSourceUsageGroupId`，由 `ProjectMaterialRegistrationFacts::derive` 生成实际多行变体，`prepare` 合并相同完整标注集；旧工程缺失端点时不猜分组。

## 当前整合导航（2026-10-22，开发中，尚未整体验收）

公开版本仍为 **0.25**，WCSNDM 仍为 **0.0803**。阶段一已获用户验收；2026-10-22 后已授权接通社区 CSV 与日语 ASR／TIFA 并要求实际专项测试。当前导航优先于以下历史暂缓记录。

第二阶段权威入口：`MainCommunityDatasetImport.cpp` 复用 `CommunityDatasetIO`、导入向导和 OTTO 原子写入；`JapaneseAutoAnnotation` 调用原生 `JapanesePhonemeAsr`／`TifaJapaneseAligner` 生成音节提案，`JapaneseAnnotationBatch`／`MainJapaneseAutoAnnotation.cpp` 负责预览、明确发布及生命周期。`NativeSourceEdgePlan` 解析当前明确同源链接的实际外沿，`AudioEngine::fadeEnvelope(LoadedClip,...)` 复用混音权威，无 profile 旧路径不启用。`MainComponent::startSourcePreview/stopSourcePreview` 与 OTO `PlaybackHost` 共用 raw audition，文件全段、发音精确区域，不改音乐。流程、包内模型、许可和限制见 [日语注音说明](japanese-annotation.md)。

新增专项以 Main.cpp 参数为准：`--smoke-main-community-import`、`--smoke-japanese-auto-annotation`、`--smoke-main-japanese-annotation`、`--smoke-japanese-annotation-safety`、`--smoke-native-source-edge-plan`、`--smoke-native-source-edge-mixer`、`--smoke-source-preview-ui`、`--smoke-engine-view-capabilities`。实际结果见随包 verification；模型 ready、测试文件存在或历史记录均不替代本次运行证据。

| 功能 | 当前权威入口与边界 |
| --- | --- |
| 三种轨道 | [ProjectModel.h](../src/ProjectModel.h) 的 `TrackType`、`effectiveTrackType`、`addTypedTrack`；Hachi／UTAU／DiffSinger 与渲染器、伴奏标记分离。`addUstFile` 默认目的类型为 Hachi。[MainComponent.cpp](../src/MainComponent.cpp) 的 `refreshTrackEditingInterface` 负责现有界面的类型分流，不另建工程模型。 |
| USTX 格式边界 | [UstxImporter.h](../src/backend/UstxImporter.h) 的严格 YAML 读取进入既有 `UstProject` 公共映射。0.8 的 480 ticks、声部绝对位置、多轨道及不支持字段警告不等于完整 OpenUTAU 行为兼容。 |
| 音源家族保护 | [UtauRenderer.h](../src/backend/UtauRenderer.h) 的 `RequestedVoicebankFamily`／`resolveRequestedVoicebankFamily`；请求明确声明 UTAU 或 DiffSinger，错误家族拒绝而不静默换引擎。真实 DiffSinger 神经模型正向验证仍缺资源。 |
| 同录音多套注音 | [OttoMaterialIO.h](../src/OttoMaterialIO.h) 的 `listAudioVariants` 及带 `audioId` 的加载／保存／替换；同一路径可对应多个稳定音频 ID，物理文件不复制成多份。未指定 ID 且路径歧义时拒绝。 |
| 社区数据集 | [CommunityDatasetIO.h](../src/CommunityDatasetIO.h)：openvpi `transcriptions.csv` 的读取、显式字典／分组、规划与原子发布；不是自动对齐器。[CommunityDatasetImportDialog.h](../src/CommunityDatasetImportDialog.h) 是调用同一服务的导入选择向导；当前已进入宿主模拟 GUI 测试但尚未报告通过；生产 Main 整合未验收。 |
| 工程素材登记 | [ProjectMaterialRegistration.h](../src/ProjectMaterialRegistration.h) 的 `prepare`／`makeCandidate`／`stage` 与租约发布；[ProjectMaterialRegistrationFacts.h](../src/ProjectMaterialRegistrationFacts.h) 默认拒绝未确认事实，仅接受用户明确确认的现有摆放解释。模型由 `adoptRegisteredMaterialBindings` 一次撤销地收纳引用，不重分析、不重排原时钟。 |
| 当前音符局部参数 | [MaterialAnnotationSession.h](../src/MaterialAnnotationSession.h) 的 `EditScope`／`NoteScopeBinding` 与 [ProjectModel.h](../src/ProjectModel.h) 的 `getNoteMaterialLocalBinding`／`applyNoteMaterialLocalRow`／`restoreNoteMaterialLocalRow`。源发布和项目局部应用分离；完整两个 GUI 入口仍待验收。 |
| 重叠线与选择器 | [PianoRollComponent.h](../src/PianoRollComponent.h) 的 `setShowOverlap` 默认关闭；现有 View 菜单 613／`ui.showOverlap` 只控制音符内黄色交叠线，不改工程。素材选择器仍只能右键显式打开一个，与此显示开关独立。 |

新增命令以 [Main.cpp](../src/Main.cpp) 实际分支为准：

- 各取一个输出目录：`--smoke-track-types`、`--smoke-track-editing-interfaces-ui`、`--smoke-voicebank-family`、`--smoke-ustx-importer`、`--smoke-material-variants`、`--smoke-main-material-variants`、`--smoke-project-material-registration`、`--smoke-project-material-registration-model`、`--smoke-registration-facts`、`--smoke-note-material-local-override`、`--smoke-local-material-scope`、`--smoke-connection-pitch-ui`、`--smoke-overlap-selector`。
- `--smoke-community-dataset <输出目录> [NSF模型目录]`；模型缺省时不能据此声称神经音频通过。
- `--smoke-community-import-gui <输出目录> <NSF模型目录>` 是新向导测试分支，登记了命令不等于已经编译或通过。
- `--audit-project-material-registration <工程> <输出目录>` 只审计携带事实，不把每个音符／分析帧认作词或音节。
- `--smoke-registered-project-audio <输出目录> <工程> <NSF模型目录>` 用于登记前／收纳后／冷重开实际音频门槛；28b 真实 teio 的 23 项检查通过，31 个物理录音／81 套变体，三阶段实际 CPU ONNX 无警告／回退且解码 PCM 完全一致，见 [实测输出](<G:/hjs-tc/tmp/teio-reg28/stdout.txt>)；这不是 Main 登记 GUI 的验收。

当前结果及限制见 [实现状态](native-material-implementation.md)。接口清单／指纹待可变头文件稳定后统一再生成，本次未运行接口生成器。

## 原生素材基础导航（早期整合记录）

- `src/OttoMaterialIO.{h,cpp}`: compact folder-level codec, stable IDs, strict validation and atomic conflict-protected writes.
- `src/OttoMaterialMigration.{h,cpp}`: explicit all-or-nothing legacy migration and lossy path-aware standard OTO export.
- `src/MaterialAnnotationSession.{h,cpp}`: shared non-destructive multi-pronunciation draft/history/publication service.
- `OtoWaveformEditorComponent::forMaterial`: the single precise editor extends the existing UTAU component; native overview/default and double-click detail share one draft. `MaterialAnnotationEditor` is only its internal overview child; there is no parallel compatibility editor action.
- [NativeExpression.h](../src/backend/NativeExpression.h): one shared native gain/tension/breath DSP used by native-file and real NSF voicebank rendering. Native semitone formants are applied to final Mel before ONNX inference, not guessed FLAG units.
- [PronunciationTiming](../src/PronunciationTiming.h): one P/C interpretation for all annotation views, default one C/one elastic V; `vowelSplitsExplicit` allows only explicitly named manual subdivisions. Source analysis epochs never imply authoring regions. Simplified consonant mode stores only its boolean intent; C-P is derived.
- `MaterialAnnotationSession::splitVowel/editVowel/mergeVowels`: atomic named authoring planners shared with the detailed editor. Draft normalization does not publish or rewrite the recording.
- `ProjectModel::alignNotePreutteranceToGrid`: default note-tool double-click translates the note onto the nearest feasible tempo grid without flattening pitch or changing selected duration/automation. The existing timing planner preserves C/pre-P and allocates stretch only to elastic V.
- `ProjectModel::setNotesPortamento`: explicit pitch-only graph edges; no source join, amplitude splice or note-control duplication. Graph-aware `sharedPitchLines(track, connections)` and `connectionPitchDigest` are shared editor/render contracts; protocol-note display also uses the canonical `AudioEngine::projectTrackNotePitch`. Compiled graph-handle, mixed-batch, strict global-seam and real forced-pitch audio fixtures have passed; this does not replace all-backend or final human acceptance.
- `PianoRollComponent::PitchScope`: focused versus whole-current-track editing. View item 612/preferences choose scope without musical undo or audio changes. Track strokes use the common `composeNativePitchStroke` and atomic `setPointCurves` pipeline.
- New acceptance commands: `--smoke-pronunciation-timing`, `--smoke-named-vowel`, `--smoke-adjacent-pronunciation`, `--smoke-vowel-stretch-alignment`, `--smoke-material-source-replacement`, `--smoke-forced-portamento`, `--smoke-track-pitch-scope` each take output; `--smoke-vowel-timing-audio <output> <nsfModel>` requires actual ONNX inference. Source/test presence alone is not a pass.
- `ProjectModel::applySourceSettingsWithReport(..., apply=false)`: publication preflight; default true applies one identity-based project transaction.
- Pure/model checks: `--smoke-otto-material`, `--smoke-material-annotation`, `--smoke-material-binding`, `--smoke-material-insertion`, `--smoke-native-connection-graph`, `--smoke-material-topology`, `--smoke-native-nsf-compatibility`, `--smoke-midi-export-scope` (each takes an output directory).
- Compiled UI captures: `--smoke-unified-material-editor`, `--smoke-material-asset-tree`, `--smoke-unified-draw-action`, `--smoke-single-entry-ui`. Capture existence is not visual acceptance: open and inspect the images.
- Real offline PCM fixture: `--smoke-material-topology-audio <output>` (two placements, source splits, save/reopen and exact stereo PCM). `AudioEngine(false)` does not open a device; normal GUI construction remains unchanged.
- Combined workflow: `--smoke-native-workflow-e2e <output> <nsfModelDirectory>`; requires isolated `HACHI_TEST_SETTINGS_DIR`, real GAME+FCPE resources and NSF model, and fails on analysis/fallback/automation/audio mismatch. This generated fixture does not replace the previous real-project references.
- Native controls: `--smoke-nsf-native-expression <output> [modelDirectory] [recording]`; without a model only DSP/mapping is tested, not real ONNX.
- `--render-project <project> <wav> [voicebank] [resampler] [nsfModel] [soloIndex] [splice|process|process-merge]` uses the same engine offline. Optional diagnostic ablations are explicitly labelled in logs and are not acceptance at unchanged settings. The old-project noise-protection default and decoder UV carrier are covered by `--smoke-native-nsf-compatibility`; new projects keep the explicit new feature defaults.
- Explicit CLI actions: `--migrate-otto-folder <folder> [classic|jie|mou] [--include-bare]`; `--export-otto-folder <folder> <output.ini> [--replace]`.
- Read [implementation status](native-material-implementation.md) for verified checkpoints and unfinished requirements. New annotation writes no longer use HJM CSV; historical entries below are retained for provenance, not current write policy.

更新 185：素材管理器增加声音文件名搜索框、清空按钮和无结果提示。搜索跨全部已注册及工程音源文件夹，包含未展开的子目录；文件名包含匹配忽略英文大小写，支持中文。结果平铺显示“文件名（音源／素材文件夹名字）”，同名录音保留各自来源；清空搜索恢复原展开状态。`AssetManagerComponent::ensureSearchFiles` 在单工作线程枚举尚未加载的文件列表，后续输入只筛选缓存，不解码音频或建立 OTO 索引。结果回主线程按仍存在的文件夹归并，使用最新搜索词，项目移除的音源不会重新出现。点击、Enter、右键、双击和拖拽继续使用原文件与所属音源 OTO 设置。新增 `--smoke-asset-search`，回归树视图、OTO 裁剪拆分、普通音频加载和大型音源注册；结果与界面截图见 `test-output/update185/`。

更新 184：工程文件记录素材管理器打开／关闭状态。`ProjectData::assetManagerVisible` 随工程保存及恢复快照写入，旧文件缺省为关闭。`MainComponent::setAssetManagerVisible` 统一应用界面状态，加载工程、新建与替换工程同步恢复；用户切换通过 `ProjectModel::setAssetManagerVisible` 标记工程待保存，不新增音乐撤销项或触发音频重同步。音乐撤销／重做与普通 replace 保持当前面板状态。新增保存开／关、不同工程切换、全新模型读取、旧工程缺省、恢复快照与撤销重做验证，构建及安装版工程安全专项通过。报告 `test-output/update184/verification.json`。

更新 183：OTO 别名选择菜单在录音拖放位置附近弹出。`itemDropped` 将落点由主界面坐标转为屏幕坐标，传给 `importAssetAudio`；菜单先指定主窗口以保持 DPI 缩放，再使用 withTargetScreenArea 指定该落点，不再锚定素材管理器整块面板的下沿。位置在读取音频与 OTO 前捕获，鼠标随后移动不会改变落点。右键／Enter 加载按调用时鼠标位置显示。导入时间、目标轨道、别名内容与取消行为不变。构建与安装版 OTO 专项共 66 项检查通过；专项验证导入流程，未模拟实体鼠标菜单操作。报告 `test-output/update183/verification.json`。

更新 182：修复素材拖拽时整块素材管理器跟随鼠标显示。`AssetManagerComponent::mouseDrag` 原先未指定拖拽图，JUCE 因而截图整个源组件；现显式提供 220×26 逻辑像素的录音文件名标签，使用 2 倍像素图保持高 DPI 清晰。拖拽源、文件路径、所属音源与 OTO 类型数据及导入流程保持不变。构建与安装版 `--smoke-oto-audio` 共 66 项检查通过，覆盖三种 OTO 拖入、裁剪拆分及导出音频；该专项验证导入逻辑，不模拟实体鼠标拖拽。报告 `test-output/update182/verification.json`。

更新 181：OTO 类型选择移入每个素材音源文件夹行的右下角，使用 100×18 小下拉菜单，保留 40 像素文件夹行高，移除顶部全局选择行。每个音源独立选择普通 OTO、界 OTO、谋 OTO 或不读取；按音源绝对路径记忆，重新排序、切换工程与重启均保留。已有安装首次沿用原全局选择，之后各音源互不影响。

主入口 `AssetManagerComponent::otoImportKind(fileOrFolder)` / `setOtoImportKind(folder, kind)`；路径归一后以 SHA256 作为 PropertiesFile 键。内部拖拽、右键加载与 Enter 均按源录音所属音源读取类型，沿用更新 180 的统一解析、裁剪和拆分入口。树视图专项新增多音源独立设置、非当前音源拖拽、重排与磁盘重开验证，并检查 320/220 像素面板截图；回归 OTO 实际音频输出、普通素材加载与真实大型音源导入。报告 `test-output/update181/`。

更新 180：素材管理器新增统一且持久记忆的“OTO 类型”下拉框：普通 OTO（二区）、界 OTO（四区）、谋 OTO（二至四区）以及不读取（完整录音），首次默认普通 OTO。素材内部拖入音轨、右键加载和 Enter 共用 `MainComponent::importAssetAudio`。按选定格式读取录音所属的最近 OTO，普通用 oto.ini，界优先独立 oto.jie.ini 并合并 oto4.ini，谋用 oto.ini 与 otomou.ini；不跨格式静默替代。一个录音有多个条目时列出别名供选择，取消不改工程。

`SampleSettings::loadAudioOto` 只读该录音的祖先文件夹中相关 OTO，不扫描整库；`nativeAudioFromOto` 将 offset／正负 cutoff 转成真实源范围，普通固定区按 consonant 拆分，界按三边界拆成四区，谋按 C/V/S 与有效边界数拆分；零长度区不生成音符。`ProjectModel::addAudioFile` 接收可选 OTO，直接生成原生 ClipData 与独立 NoteData，复用既有插入、一次撤销、异步音高分析和保存，不转换或覆盖 HJM。分析只补充源音高，保留作者边界。不读取模式保留更新 179 的普通音频/HJM 行为。内部拖拽携带文件、根目录与所选类型；外部拖拽沿用原文件/副本策略。同时修复谋 OTO 二区简写 CV,offset,b1 被四字段门槛误跳过的问题。

专项 `--smoke-oto-audio <输出目录>` 验证三种格式、二／三／四区、正负截尾、无效范围、单步撤销／重做、真实拖入处理、选择持久化、音高分析后边界、工程重开及导出 PCM。回归普通素材加载、文件夹树和 3219 文件音源注册，报告 `test-output/update180/`。公开版本保持 0.25。

更新 179：素材管理器在非 U 模式也可导入、展开 UTAU 文件夹。右键录音选择“加载为音频素材”，或选中后按 Enter，按播放头位置导入并聚焦素材。选中原生调音轨道时复用该轨道和算法；选中 UTAU、DiffSinger、伴奏等轨道时新建普通音频轨道。文件夹导入后自动展开。浏览只枚举文件，加载复用普通音频分析，不应用 OTO 别名／拆音或修改录音与 OTO。分析后沿用普通音频流程生成 HJM 注释，已有 HJM 侧文件沿用原导入规则，双击参数编辑与拖动保持。

主入口：`AssetManagerComponent::loadAudio` 经 `onLoadAudio` 委托 `MainComponent::addAnalysedAudioFile` → `ProjectModel::addAudioFile`／`scheduleAnalysis`；右键与 Enter 共用，异步菜单捕获原文件并用 SafePointer 检查组件生命周期。新增 `--smoke-asset-audio <输出目录> <真实录音>`，验证原生轨道、UTAU 选区分流、异步音高分析、保存重开和源文件不变；复用音源导入性能与树视图回归。

更新 178：HN-SEP 完整谐波波形磁盘缓存由内置 HiFiSampler、非 U 噪声保护与 WCSNDM HN1 共用。实际输入 PCM、模型和配置内容 SHA256 加分离算法版本／声学网格决定身份；实际 PCM 相同才互用，保留两边原有预处理。内容校验、跨进程互斥、原子发布、512 MiB 磁盘预算及模型字节快照会话详见 [HN 模式说明](wcsndm-hn.md)。入口为 `backend/HnsepDiskCache.inc` 与外置 `hf_backend/hn_cache.py`，既有 `hifiSeparate`／`hn_mel.separate` 接入。测试入口 `--smoke-hn-cache`、外置 `src/test-hn-cache.py`／`test-hn-cache-render.py`；报告 `test-output/update178/`。

更新 177：音高、响度与线性 FLAG 标点支持 Ctrl 加选／取消，可跨音符及 CVVC 音素。拖动任一已选点，以相同秒数和显示值位移整组平移；共享边界约束防止选中点越过未选中邻点，首尾保护沿用既有规则。Ctrl+R／多选右键菜单初始化所选点，保留时间：音高回音符音高、响度到 100%、FLAG 回参数默认值，DiffSinger 实参回模型参考值。Delete／Backspace 批量删除，连按不误删整音符；松开鼠标保留选择，普通点击新点替换整组。原生共享包络去重并同步预览。高级包络混合模式节点支持相同多选、平移、默认值与删除操作，仍为草稿，应用后保存。

主入口：`PianoRollPointGroup.h` 复用现有命中检测与参数读取，`PointGroupEdit.h` 计算整组位移边界；`ProjectModel::setPointCurves` 使用已有各参数提交函数并合并为一次撤销，不复制新的工程格式。高级包络使用 `AdvancedEnvelopePanel::editKnots`。回归：`--smoke-point-group`、`--smoke-envelope-point-selection`、`--smoke-pitch-point-selection`、`--smoke-advanced-envelope-panel`；报告 `test-output/update177/{build,installed}/verification.json`。

更新 176：高级包络混合模式的节点在点击／拖动并松开后保持选中高亮，Delete／Backspace 调用既有 deleteKnot 删除该节点；音头、音尾、原生音频及 CVVC 音素共用。选择与拖动状态分离，点击其他位置／面板控件、失焦、切换形状／预览、替换草稿时清除选择。连按 Delete 不继续删除其他节点，首尾边界点和贝塞尔控制柄沿用原有规则。删除只改草稿，应用时走原有提交入口，关闭不应用则不改工程。 入口 `AdvancedEnvelopePanel::Graph`，回归 `--smoke-advanced-envelope-panel <目录>`；报告 `test-output/update176/{build,installed}/verification.json`。

更新 175：响度包络和线性 FLAG 包络沿用音高标点的持久选择；点击或拖动后松开仍高亮，Delete／Backspace 删除所选标点，继续按键不会转而删除整音符。统一 PointSelection 记录类型、音符、点序号、CVVC 分片及 FLAG 键；点击其他位置、切换工具／FLAG／选区、失焦时清除。右键删除与快捷键共用 deleteEnvelopePoint，响度沿用已有选区／粘连组映射，FLAG 只改当前参数，CVVC 只改对应音素的对应包络；保留原来的响度／CVVC 两端与 FLAG 最后一个点。普通响度及 FLAG 纯点击不再提交工程编辑。 回归入口：`--smoke-envelope-point-selection <输出目录>`，复测 `--smoke-pitch-point-selection`；报告 `test-output/update175/{build,installed}/verification.json`。

更新 174：音高标点的选中状态独立于拖动状态；点击并松开或拖动后松开，标点继续高亮，Delete／Backspace 优先删除该点。点击其他位置、切换工具／轨道／片段、失去键盘焦点或重新选择音符时清除。删除使用与标点右键菜单相同的 deletePitchAnchor 入口，沿用端点保留和 DiffSinger 隐藏采样整理规则；通过既有 setNotePitchCurve 提交，支持撤销、重做及工程保存。删除完成或版本过期后保留按键消费状态直到下一次选择，防止连续 Delete 误删整音符。选择本身不改工程、不触发合成。 回归入口：`--smoke-pitch-point-selection <输出目录>`；测试文件 `src/tests/PitchPointSelectionSmoke.h`，报告 `test-output/update174/{build,installed}/verification.json`。

更新 173：“自动交叉淡化”按钮增加右键直线／曲线菜单，默认直线，偏好 ui.autoCrossfadeCurve 记住选择。菜单只改后续操作方式，左键仍对同轨两个音符的已有发声重叠执行，不移动音符。PianoRollComponent::crossfadeSelectedNotes 将方式传给唯一提交入口 ProjectModel::crossfadeNotes；曲线复用 CVVC UtauMixRamp 的互补 smoothstep，以 32 段幅度直线保存到既有包络点格式，普通音符及 CVVC 分片共用，继续支持保存、撤销与重做。切回直线清除重叠区内的曲线中间点；无选区时仍可右键设置。

更新 172 补充核验：HN1 八类谐波参数在普通 UTAU 线性 FLAG 及带标注的二／三／四段分区中通过 145 次真实模型合成、660 项检查；曲线／分区继承、优先级、零长度段和短音已验证。普通 UTAU 的经典二区不暴露独立分区 FLAG。详见 [HN 模式说明](wcsndm-hn.md)。

更新 172：HN1 接入 g、Mo、QX/ME、Mr、Mq、Mt、Rd、NA 八类谐波参数及逐帧 FLAG 曲线。C 端复用现有声道编辑和 LF 模型生成相对中性谱的 dB 变化，Python 仅将功率增益作用于 HN-SEP 谐波；不重新合成 LLSM 谐波。源时间映射携带同帧增益，内部服务协议升级 HN2，公开开关仍只有 HN1。零参数保持上一版谐波功率完全一致；Mq 与普通路径共用实现。见 [wcsndm-hn.md](wcsndm-hn.md)。

更新 171：仅 HN1 选择 HN-SEP 谐波＋LLSM 非谐波神经内核，与 HF2、V1、V2、K/L/M 并列互斥；继续使用第一个有效内核优先。裸 HN、HN0、HN2 等不启用、不占内核位置，也不拦截后面的 HN1。编辑器分离导出资格识别同步采用相同规则；合成算法未改。见 [wcsndm-hn.md](wcsndm-hn.md)。

更新 170：WCSNDM 新增可选 HN/HN1 实验内核，HN-SEP 谐波波形的 STFT 功率按现有源时间映射拉伸，与 LLSM 非谐波功率相加后统一生成 log-Mel，目标 F0 交给 PC-NSF。HN0／不填保持默认 HF2；不混入 HN-SEP 噪声残差。独立版本化服务防止旧服务误处理；编辑器同步识别 HN 内核并禁止不支持的分离导出。详细范围、谐波 FLAG 限制与验证见 [wcsndm-hn.md](wcsndm-hn.md)。

更新 169：高级包络图表沿用主编辑器滚轮快捷键：滚轮纵向滚动，Shift＋滚轮横向滚动，Ctrl＋滚轮纵向缩放，Ctrl＋Shift＋滚轮横向缩放。共用 ZoomScrollBar.h 的修饰键映射；缩放锚定鼠标所在时间／响度，边缘按可见范围限位。支持横向触控板滚动和反向缩放事件，拖动控制点时忽略滚轮；视图操作不改包络草稿。图表悬停提示显示快捷键。

更新 168：高级包络图表增加横轴／纵轴独立缩放、倍率显示、滚动条与“适应全图”。时间轴最高 32 倍，响度轴最高 16 倍；图形、坐标刻度、控制柄与鼠标编辑共用视图变换。缩放和滚动仅属于面板视图，不写工程或预设；普通音符、音头／音尾、原生音频与 CVVC 单音素共用。

更新 167：包络预设下拉菜单的六个选项恢复绿色形状缩略图，左图右文，复用原预设数据和缩略图比例。菜单保留键盘选择和无障碍名称，应用仍走既有 applyEnvelopePreset；不修改包络或合成规则。

更新 166：工具栏六个包络预设按钮合并为“包络预设”下拉菜单，按原顺序提供标准、柔起、缓起、渐弱、句尾、短收。选择后仍调用 PianoRollComponent::applyEnvelopePreset，保留整音符和 CVVC 单音素选区语义；操作后清空菜单选中值，允许再次选择同一预设。移除旧的六按钮和重复标题，沿用原 UTAU 显示范围。

更新 165：中文 CVVC 的音符右键菜单将“辅音强制重置”改为“强制重置”。现有菜单动作按轨道发音方式分流，ProjectModel::resetCvvcBoundaries 批量清除所选音符的手动拆音位置和派生缓存，恢复发音器自动位置；一次撤销，多次点击无额外修改。菜单不依赖后台拼字布局是否已就绪。响度／FLAG 包络、辅音速度及父音符起止保留，普通 UTAU 与四区模式继续使用原辅音重置。

更新 164：CVVC 音符内部的 VC/CV、CV/尾音分界可左右拖动，固定相邻两片的外端点和父音符时长。NoteData::cvvcBoundaries 按右侧角色保存相对音符时长的位置；ProjectModel::setCvvcBoundary 是唯一提交入口，松手提交一次、支持保存及撤销，Esc 放弃草稿。CvvcTiming.h 的 applyCvvcBoundaries 统一发音规划和辅音速度预览的限位；上下包络复用实际分片布局，渲染键和派生拼字缓存随位置失效。分界手势不调用音源读取或合成，后台布局返回前保留已提交的位置。

2026-10-13 增补：[功能接口与架构审计](AI_INTERFACE_ARCHITECTURE.md) 回答每项功能应调用谁、数据在哪里多次复制、如何最小化改造；[实际接口声明清单](AI_API_REFERENCE.md) 为公开签名和协议命令目录，由 `tools/generate-interface-reference.py` 生成。扫描范围、位置及源码指纹在 [interface-audit-inventory.json](interface-audit-inventory.json)。本页继续作为逐文件导航，不重复维护另一份接口表。

更新日期：2026-10-15。实现基线：0.25 内部更新 166（包络预设下拉菜单）。先读 [代码功能总览](AI_CODE_GUIDE.md)。

更新 163：单独选中 CVVC 音素后，“高级包络”不再被提前返回拦截。PianoRollComponent::selectedCvvcEnvelope 提供当前音素的范围与曲线；MainComponent 复用 AdvancedEnvelopePanel 的整段编辑界面，应用仍统一调用 setCvvcEnvelope，仅提交该角色。包络点可保存可选贝塞尔控制柄，cvvcEnvelopeValue 与两个 UTAU 渲染器共用同一插值，曲线加入混音缓存键。打开／未改动应用不写工程；草稿应用、试听、修订保护、保存和撤销保留。恢复默认使用该音素标准梯形。

`tests/CvvcAdvancedEnvelopeSmoke.inc` 在中文 CVVC 专项中执行三个音素标签 → 高级包络按钮 → 应用／撤销／保存的实际交互，并检查两条渲染路径；沿用 `--smoke-chinese-cvvc`。

更新 162：修复旧 CVVC 工程保存的整音符默认分贝包络继续叠乘，造成句首弯曲起音和收尾弯曲淡出。ProjectModel::fromValueTree 仅对缺少 cvvcEnvelopeVersion、无高级包络及 UST 原始段的中文 CVVC 音符，识别四点 0 dB 平台、5/15 ms 起音、35 ms 收尾的历史默认形状，恢复为各音素独立标准梯形。新工程保存 cvvcEnvelopeVersion=1；现代显式曲线、非标准时长／音量、线性手绘和非 CVVC 曲线保持。音频与拼字混音缓存版本同步更新。真实“CVVC测试”工程专项覆盖修复前失败、保存重开、首尾像素位置、两种交叉方式逐采样增益及 HiFiSampler／WCSNDM 合成。

新增 `src/tests/CvvcLegacyEnvelopeSmoke.h`，由 `HifisamplerEnvelopeSmoke.h` 的真实工程分支调用，命令仍为 `--smoke-hifisampler-envelope`。包含旧文件模板迁移、保留自定义曲线、工程副本往返与句首／句尾实际像素检查。

更新 161：CVVC 默认梯形与“标准”预设统一使用 standardCvvcEnvelope 的 5 ms／35 ms 线性幅度规则。自动混音 useStandardUtauEnvelope 在重叠边缘接管标准淡入／淡出，避免手动默认梯形与自动层重复相乘产生转折；保留非标准手动曲线。发音器菜单新增“默认交叉方式→直线／曲线”，TrackData::cvvcCurveCrossfade 随工程保存、撤销，并参与声音与混音元数据缓存键。曲线为互补 smoothstep，两端斜率为零；传统 UTAU 与 HiFiSampler 共享增益描述。replacesStandardEnvelope 标记让上下包络显示使用同一实际增益。整音符 CVVC 预设按音素批量提交 setCvvcEnvelopes，原单片接口委托此入口，一次撤销。

更新 160：CVVC 音符上方增加 VC／CV／尾音可点击标签，复用父音符 ID 加 envelopePart 的选区，不生成独立假音符。选中音素时响度预设只提交 setCvvcEnvelope，响度／FLAG 面板仅保留当前音素的编辑曲线和控制点，其他音素淡显实际曲线；点击主体或显式整音符选择恢复整音符操作。上方包络改为逐音素绘制实际自动混音增益，移除父音符包络的重复外形。音素选择时高级包络入口聚焦分片响度编辑，整音符 OTO 设置需点击主体。选择不写工程；曲线复用既有保存、撤销和渲染入口。

更新 159：`mixNsfUtauNotes` 的自动淡出完成后保持零增益，消除前一 VC/CV 在交叉区后恢复音量的错误；负 OTO 重叠按间隔提前结束前音并使用 3 ms 去爆音，长正重叠按相邻音符时序为前音补足尾部。`UtauMixEnvelope` 的显示元数据与实际混音使用同一持久淡出规则；手动包络仍先应用再乘自动层。HiFiSampler 音频缓存与 CVVC 混音元数据版本更新。专项 `--smoke-hifisampler-envelope <输出目录> [工程] [模型] [重采样器]` 覆盖多音重叠、短后音、零／负重叠及用户工程只读合成；现有 CVVC 专项增加真实模型淡出保持及跨节拍长重叠。

更新 158：CVVC 音头手柄按已解析的 VC／CV／收尾 OTO 与相邻音符时长构造纯时序求值器，通过二分反求整数辅音速度；拖动过程不读文件、不跑发音器或声码器。`cvvcTransitionDuration` 统一 VC 提前量规则，`scaledUtauPreutterance` 统一源时长封顶和固定辅音区缩放，移除 CVVC 独有的 0–200 限制；松手等待后台布局时保留已提交的预览音头位置。旧拼字与音频派生缓存按时序规则版本失效。`tests/CvvcConsonantDragSmoke.h` 随中文 CVVC 专项验证负值、快速辅音、饱和上限、实际渲染、撤销及松手前后位置。

更新 157：新增 `backend/CvvcPhonemeCache.h`（派生拼字缓存序列化）及 `tests/CvvcLayoutCacheSmoke.h`（真实消息循环下验证无选择、多片段后台补全和保存重开）。主入口 `ProjectModel::rememberCvvcPhonemes`；卷帘内 `requestCvvcLayouts` 调用现有发音器，`setUtauNoteWaveforms` 合并有效元数据。“显示”菜单第 9 项为拼字信息，显示偏好 `ui.showPhonemeInfo`；不改变声码器和合成算法。

更新 156：`MainComponent.cpp` 新增全模式“自动交叉淡化”按钮；`PianoRollCvvcEnvelope.h` 内的 `crossfadeSelectedNotes` 复用所有模式响度布局数据；`ProjectModel::crossfadeNotes` 是唯一生成／原子提交入口。测试 `tests/AutoCrossfadeSmoke.h`（`--smoke-auto-crossfade <目录>`），CVVC 实际渲染验证仍归 `tests/CvvcEnvelopeSmoke.h`。不新设混音器或工程字段。

更新 155：`backend::standardCvvcEnvelope` 统一生成各 CV／VC／收尾的默认四点直线梯形（5 ms 起音、35 ms 释放，短片段用三等分避免点序倒置）。`planChineseCvvc` 与 `PianoRollCvvcEnvelope.h` 复用该函数；仅在无手动父包络、无分片响度覆盖时使用。已有手动曲线及导入曲线保留，默认值不写回工程。`cvvcDefaultEnvelopeGain` 传递默认包络的基准音量，防止调节音量后被误判为手工平直曲线；CVVC 渲染键升级至 v3。

更新 154：`backend/UtauMixEnvelope.h` 保存渲染器实际使用的采样级自动淡化窗口，传统 UTAU 与 HiFisampler 混音、CVVC 响度可视化共享增益计算。`noteMixEnvelope` 经发音器折回 `UtauPhonemeSpan::mixEnvelope`，只读数据不写工程；实线显示手动包络乘自动淡化，虚线／圆点保留编辑值，VC／CV／收尾用不同颜色，交叉区着色。HiFisampler CVVC 波形回调移到已确定自动混音增益之后，避免预览缺少淡化。`utauPhonemeMixContextHash` 防止邻音时序或引擎变化后继续显示过期交叉曲线；包络／FLAG 编辑保留布局和有效淡化。

更新 153：`backend/CvvcEnvelope.h` 是分片曲线类型／存储格式；`PianoRollCvvcEnvelope.h` 为原卷帘的响度／FLAG 分片绘制和鼠标交互；`ProjectModel::setCvvcEnvelope` 是唯一提交入口，`ChineseCvvcPhonemizer.h` 是唯一合成映射入口。`tests/CvvcEnvelopeSmoke.h` 随 `--smoke-chinese-cvvc` 验证，未增加第二套发音器或独立音符模型。

更新 152：`MainComponent::phonemizerMenu/showPhonemizerMenu` 复用轨道发音器菜单；按钮位于拉伸算法下方。`ChineseCvvcRules::aliases` 统一配置别名展开；`tests/ChineseCvvcSmoke.h` 扩展分隔符、候选／代表元音、旧式配置优先级、手写覆盖和工具栏检查。沿用 `--smoke-chinese-cvvc`，不添加平行发音器接口。

更新 151：`AssetManagerComponent::importVoicebankDialog` 复用轻量 `registerFolder`；[tests/VoicebankImportSmoke.h](../src/tests/VoicebankImportSmoke.h) 提供 `--smoke-voicebank-import 输出目录 音源目录`。`backend/ChineseCvvcPhonemizer.h` 加入受音源规则及 OTO 约束的等价拼写选择；`tests/ChineseCvvcSmoke.h` 增加元音继承、精确别名／替换优先及衔接回归。配置研究见 [chinese-cvvc.md](chinese-cvvc.md)。

更新 150：`MainComponent::selectVoicebank` 统一选择和默认音源的轻量绑定；[tests/VoicebankSelectionSmoke.h](../src/tests/VoicebankSelectionSmoke.h) 提供 `--smoke-voicebank-selection 输出目录 音源目录`，验证真实音源选择及界面刷新。本页列出自有源码、测试和脚本；第三方供应代码按目录归类。

更新 148：素材管理器音频项改为 24 像素单行，只显示一次相对文件名；音源文件夹保持 40 像素名称／路径双行。`TreeRow` 缓存每行位置和高度，绘制、命中与滚动范围统一使用这些坐标，保持展开／收起、拖拽及参数编辑行为。

更新 147：素材管理器改为全宽可折叠文件列表；音源左侧箭头展开／收起，双击文件夹名称也可切换。展开后列出包含子目录在内的音频文件，显示文件名和相对路径；单击选中、拖拽使用素材、双击编辑参数。多个文件夹可同时展开，滚轮与纵向滚动条浏览长音源库。`AssetManagerComponent` 缓存已展开成员，绘制只遍历可见行；工程更新保留现存文件夹展开状态，换工程清除失效引用。扩展 `tests/ProjectVoicebankAssetsSmoke.h` 验证窄面板点击、展开／收起、子目录、滚动末尾和工程切换。

更新 146：`AssetManagerComponent.*` 新增工程音源文件夹同步、去重、工程标记和手动登记分离；`MainComponent.cpp` 的 `showAssetManager`／`changeListenerCallback` 调用同步。`tests/ProjectVoicebankAssetsSmoke.h` 由 `Main.cpp` 接入，命令 `--smoke-project-voicebank-assets 输出目录`，检查工程切换、保存重开、共用音源、合成类型过滤、全局登记隔离及缺失引用；输出实际面板截图。既有 `--smoke-asset-manager` 继续覆盖素材导入、参数及 OTO 导出。

更新 145：`AccompanimentTempo.*` / `AccompanimentTempoPanel.h` 和 `tools/hamood_audio/tempo_worker.py` 提供伴奏音频右键“识别曲速”。只运行已有 Beat This! small0，后台读取裁剪／合并片段，显示 BPM，不写工程；取消及析构回收进程和临时文件。新增 `tests/AccompanimentTempoSmoke.h`，命令 `--smoke-accompaniment-tempo 输出目录 [真实伴奏]`。

更新 144：新增 [backend/WcsNonLoopFeatures.cpp](../src/backend/WcsNonLoopFeatures.cpp)／[WcsNonLoopFeatures.h](../src/backend/WcsNonLoopFeatures.h)，非 U NSF 的 WCSNDM 非循环参数拉伸、物理 Mel、源特征缓存及分块取消。`RenderService` 传递源 MIDI，`NsfHifiganRenderer` 在既有噪声保护及可选 F0 平滑之后生成新特征。新增 [backend/WcsNonLoopSmoke.inc](../src/backend/WcsNonLoopSmoke.inc)，由 NSF 渲染源包含；命令 `--smoke-wcs-nonloop 输出目录 NSF模型目录 [真实录音]`。`NsfPickerSmoke.h` 增加选择、撤销重做、工程保存、单段／整句请求、缓存失效和实际界面截图验证。值 6／菜单 ID 7／键 `wcsndm-nonloop` 追加，不改旧工程枚举含义。无需新模型，不启动外置引擎；没有增加发音时长保护。报告在 `test-output/update144/`。

更新 143：`NativePitchVoicingDisplay.*` 增加人为目标范围与真实圆点绘制，无 F0 处的人工曲线显示为圆点线，未编辑源曲线仍断开。`PianoRollComponent` 支持共享／独立曲线、边界外端点及手绘／直线预览；`ProjectModel::setNotePitchCurve` 保留无声帧目标形状而不改变清音掩码。扩展 `NativePitchVisibilitySmoke.h` 验证像素、圆点／短划线区别、滚动相位、手绘、撤销及无新增渲染 F0。报告在 `test-output/update143/`。

更新 142：`NativePitchVoicingDisplay.*` 增加真实有声区间与绘制裁切，`PianoRollComponent` 对普通／共享目标曲线、原始参考和颤音隐藏无 F0 区域，取消缺失音高的虚线参考。新增 `tests/NativePitchVisibilitySmoke.h`，命令 `--smoke-native-pitch-visibility 输出目录` 检查实际像素、特殊片段、普通音符内部清音、批量拉直、颤音、拉伸、占位和渲染不变；更新气声显示专项以验证其开关不会填补缺失 F0。报告在 `test-output/update142/`，无需新增模型。

更新 141：新增 `NativeUnpitchedRegions.h`，非 U 自动／右键补齐无音高片段，灰色斜纹区分；删除会切掉真实播放源范围，沿用裁剪／响度／剪贴板与撤销。`nativeUnpitched` 通过工程、HJM 与 MCP 保存，渲染不生成 F0。新增 `tests/NativeUnpitchedRegionsSmoke.h` 和 `--smoke-native-unpitched-regions 输出目录 [真实录音] [分析模型根目录]`，验证实际 PCM、源映射、保存、裁剪、响度与复制。无需新增模型。

更新 140：显示项更名“气声虚线”。新增 `backend/SourceBreathiness.h` 独立显示证据，使用周期抵消残差与多频段噪声分析，覆盖带音高气声；`NativePitchVoicingDisplay.*` 继续后台缓存和源时间映射，音频渲染 `SourceVoicing` 仍保守保护清音。新增 `tests/SourceBreathinessSmoke.h`，`--smoke-source-breathiness 输出目录 [真实录音]` 检查弱气声／低频气声、干净元音、滑音、颤音、边界、取消及实际录音；现有显示专项新增气声元音虚线验证。报告在 `test-output/update140/`，无需新增模型。

更新 139：`MainComponent::nativeVibratoButton` 提供非 U 颤音入口，`PianoRollComponent::showSelectedNativeVibratoDialog` 复用批量参数对话框并可选实际音高线显示；`ProjectModel::setNotesVibrato` 可将显示选择纳入同一步撤销。新增 `src/tests/NativeVibratoSmoke.h`：`--smoke-native-vibrato 输出目录` 通过实际按钮、对话框和拖动验证界面及工程；`--smoke-native-vibrato-render 输出目录 NSF模型目录` 检查单段／整句／粘连目标、清音、缓存及真实音频 F0。报告在 `test-output/update139/`。

本页覆盖 `juce/src` 下的自有非测试 C++／头文件／包含单元，测试在后面单列。头文件也可能包含完整实现，`.inc` 文件通过其他源文件编译。

## 编辑器与工程模块

| 文件 | 功能 |
| --- | --- |
| [AccompanimentTempo.h](../src/AccompanimentTempo.h) / [AccompanimentTempo.cpp](../src/AccompanimentTempo.cpp) | 伴奏可见音频范围的只读曲速分析、后台解码、Beat This! 进程及取消回收 |
| [AccompanimentTempoPanel.h](../src/AccompanimentTempoPanel.h) | 右键“识别曲速”的非模态进度／BPM 结果窗口，不写工程 |
| [AdvancedEnvelopePanel.h](../src/AdvancedEnvelopePanel.h) | 原生音频、OTO 音头和音尾的高级包络草稿、曲线与预设界面 |
| [AssetManagerComponent.cpp](../src/AssetManagerComponent.cpp) / [AssetManagerComponent.h](../src/AssetManagerComponent.h) | 素材资源管理界面 |
| [AudioEngine.cpp](../src/AudioEngine.cpp) / [AudioEngine.h](../src/AudioEngine.h) | 音频设备、工程同步、合成缓存、波形、播放混音和导出 |
| [ClipParts.h](../src/ClipParts.h) | 组合区域展开、源切片、坐标转换和父层增益继承 |
| [DiffSingerParameterCurves.h](../src/DiffSingerParameterCurves.h) | DS 预测实参和偏移曲线 |
| [DiffSingerPhonemeDisplay.h](../src/DiffSingerPhonemeDisplay.h) | DS 辅音元音和实际音素范围显示 |
| [DiffSingerPhonemeEditor.h](../src/DiffSingerPhonemeEditor.h) | DS 音素时长和边界编辑窗口 |
| [DiffSingerPitchHandles.h](../src/DiffSingerPitchHandles.h) | DS 音高控制点和拖动把手 |
| [DiffSingerPitchRestore.h](../src/DiffSingerPitchRestore.h) | DS 原始预测音高恢复 |
| [DiffSingerPronunciationEditor.h](../src/DiffSingerPronunciationEditor.h) | DS 读音覆盖编辑窗口 |
| [DiffSingerRequest.h](../src/DiffSingerRequest.h) | 从工程音符组织 DS 请求 |
| [FlagCurveDrawing.h](../src/FlagCurveDrawing.h) | FLAG 曲线绘制辅助 |
| [Hamood.cpp](../src/Hamood.cpp) / [Hamood.h](../src/Hamood.h) | 调性分析、声部方案和和声生成 |
| [HamoodAudio.h](../src/HamoodAudio.h) | 伴奏分析进程、缓存查询和工程时间转换 |
| [HamoodProject.h](../src/HamoodProject.h) | 工程内和声信息和持久化辅助 |
| [HamoodTimelinePanel.h](../src/HamoodTimelinePanel.h) | 和声调性时间轴面板 |
| [I18n.cpp](../src/I18n.cpp) / [I18n.h](../src/I18n.h) | 多语言界面文本 |
| [Main.cpp](../src/Main.cpp) | 程序启动、窗口生命周期、CLI 分支和内联诊断 |
| [MainComponent.cpp](../src/MainComponent.cpp) / [MainComponent.h](../src/MainComponent.h) | 主窗口布局、菜单工具栏、选择同步、设置与工程操作 |
| [MainComponentDiffSinger.cpp](../src/MainComponentDiffSinger.cpp) | 主界面 DS 推理任务、发音和参数操作 |
| [MainComponentHamood.cpp](../src/MainComponentHamood.cpp) | 主界面和声配置、分析与生成 |
| [MainComponentMcp.cpp](../src/MainComponentMcp.cpp) | 窗口实时 MCP、选区、修订校验、事务和后台任务 |
| [ModelessWindows.h](../src/ModelessWindows.h) | 非模态窗口共用封装 |
| [NativeAudioClipboard.h](../src/NativeAudioClipboard.h) | 原生分段复制粘贴的素材和参数组织 |
| [NativeAudioDisconnect.h](../src/NativeAudioDisconnect.h) | 关联音频分离成独立区域 |
| [NativeUnpitchedRegions.h](../src/NativeUnpitchedRegions.h) | 生成无音高片段与实际播放范围删除 |
| [NativeAudioFocus.h](../src/NativeAudioFocus.h) | 重叠音频显示聚焦、亮度和绘制次序 |
| [NativeAudioLink.h](../src/NativeAudioLink.h) | 不同素材按原时钟组装为粘连区域 |
| [NativeAudioOverlap.h](../src/NativeAudioOverlap.h) | 碰撞、重叠许可与关闭时的裁切处理 |
| [NativeAudioTrim.h](../src/NativeAudioTrim.h) | 自由端点裁剪计划、源映射和音高包络保留 |
| [NativeTrimSource.h](../src/NativeTrimSource.h) | 隐藏源时间映射、完整源音高参考、扩展音高恢复及持久化 |
| [NativeNoteJoin.h](../src/NativeNoteJoin.h) | 相邻同源粘连段的非破坏合并、统一拉伸与曲线保留 |
| [NativeNoteTiming.h](../src/NativeNoteTiming.h) | 移动、拉伸、分段联动和源范围绑定 |
| [NativePitchIdentity.h](../src/NativePitchIdentity.h) | 真实源 F0 已知和未编辑音高判断 |
| [NativeSharedEnvelope.h](../src/NativeSharedEnvelope.h) | 原生粘连音频的公共响度包络、插值、分段存储与断开／复制时物化 |
| [NativeSourceTimeMap.h](../src/NativeSourceTimeMap.h) | 源目标时间互换和有效音频窗口转换 |
| [NoteDanceAnimation.h](../src/NoteDanceAnimation.h) | 音符舞动显示动画 |
| [OtoRegionGuides.h](../src/OtoRegionGuides.h) | OTO 分区参考位置与绘制辅助 |
| [OtoWaveformEditorComponent.cpp](../src/OtoWaveformEditorComponent.cpp) / [OtoWaveformEditorComponent.h](../src/OtoWaveformEditorComponent.h) | OTO 波形、频谱和分段边界编辑 |
| [PianoRollComponent.cpp](../src/PianoRollComponent.cpp) / [PianoRollComponent.h](../src/PianoRollComponent.h) | 钢琴卷帘、音符音高包络 FLAG 绘制、命中、拖动和菜单 |
| [PianoRollDiffSingerOffset.h](../src/PianoRollDiffSingerOffset.h) | 卷帘 DS 音高偏移辅助 |
| [Pinyin.cpp](../src/Pinyin.cpp) / [Pinyin.h](../src/Pinyin.h) | 中文拼音转换 |
| [PinyinTable.inc](../src/PinyinTable.inc) | 生成的拼音映射数据包含单元 |
| [ProjectFileIO.h](../src/ProjectFileIO.h) | 工程二进制完整性、临时写入、备份和安全替换 |
| [ProjectModel.cpp](../src/ProjectModel.cpp) / [ProjectModel.h](../src/ProjectModel.h) | 数据结构、正式编辑、快照、撤销重做、保存和加载 |
| [ProjectRecovery.h](../src/ProjectRecovery.h) | 异步自动恢复快照和租约 |
| [RenderedWaveformPeaks.h](../src/RenderedWaveformPeaks.h) | 已渲染音频峰值数据 |
| [SampleSettings.cpp](../src/SampleSettings.cpp) / [SampleSettings.h](../src/SampleSettings.h) | 素材设置与注释数据 |
| [SettingsComponent.cpp](../src/SettingsComponent.cpp) / [SettingsComponent.h](../src/SettingsComponent.h) | 用户设置、算法、模型路径和推理选项 |
| [SourceWaveformPreview.h](../src/SourceWaveformPreview.h) | 按时间映射绘制拉伸后的原音近似波形 |
| [NativePitchVoicingDisplay.cpp](../src/NativePitchVoicingDisplay.cpp) / [NativePitchVoicingDisplay.h](../src/NativePitchVoicingDisplay.h) | 真实有声区间的绘制遮罩、人为曲线的圆点引导；气声虚线后台证据缓存、源／目标时钟映射和分区绘制；不改变合成 |
| [StartupLog.h](../src/StartupLog.h) | 启动诊断日志 |
| [TailFadePresetStore.h](../src/TailFadePresetStore.h) | 自定义高级包络预设保存、读取和删除 |
| [Theme.cpp](../src/Theme.cpp) / [Theme.h](../src/Theme.h) | 配色、控件外观和工具图标 |
| [TimelineComponent.cpp](../src/TimelineComponent.cpp) / [TimelineComponent.h](../src/TimelineComponent.h) | 上方轨道区域、框选、跨轨移动和波形预览 |
| [TimelineGainEnvelope.h](../src/TimelineGainEnvelope.h) | 区域增益包络辅助 |
| [TrackGainEnvelope.h](../src/TrackGainEnvelope.h) | 轨道增益包络和求值 |
| [TrackListComponent.cpp](../src/TrackListComponent.cpp) / [TrackListComponent.h](../src/TrackListComponent.h) | 轨道名称、引擎标识和混音控件 |
| [UtauOutputEnginePanel.h](../src/UtauOutputEnginePanel.h) | 每轨重采样器与 wavtool 配置 |
| [VoicebankSettingsComponent.cpp](../src/VoicebankSettingsComponent.cpp) / [VoicebankSettingsComponent.h](../src/VoicebankSettingsComponent.h) | 音源库与 OTO 设置 |
| [WavExportOptions.h](../src/WavExportOptions.h) | WAV 格式、范围相关选项与分量类型 |
| [ZoomScrollBar.h](../src/ZoomScrollBar.h) | 带端点缩放的滚动条 |

## 分析 合成与自动化后端

| 文件 | 功能 |
| --- | --- |
| [AdvancedEnvelope.h](../src/backend/AdvancedEnvelope.h) | 高级包络曲线和效果求值 |
| [AmplitudeEnvelopeCurve.h](../src/backend/AmplitudeEnvelopeCurve.h) | 响度包络求值和时长适配 |
| [AnalysisService.cpp](../src/backend/AnalysisService.cpp) / [AnalysisService.h](../src/backend/AnalysisService.h) | GAME、FCPE、native-hq 统一分析和源 F0 附着 |
| [AudioFileReader.cpp](../src/backend/AudioFileReader.cpp) / [AudioFileReader.h](../src/backend/AudioFileReader.h) | 音频读取与转码缓存 |
| [DiffSingerOptions.h](../src/backend/DiffSingerOptions.h) | DS 设备和预览导出质量配置 |
| [DiffSingerProjectCache.h](../src/backend/DiffSingerProjectCache.h) | DS 工程缓存 |
| [DiffSingerRenderer.cpp](../src/backend/DiffSingerRenderer.cpp) / [DiffSingerRenderer.h](../src/backend/DiffSingerRenderer.h) | DS C++ 请求、后台进程和合成桥 |
| [DiffSingerTiming.h](../src/backend/DiffSingerTiming.h) | DS 时长上下文与边界覆盖 |
| [FcpeAnalyzer.cpp](../src/backend/FcpeAnalyzer.cpp) / [FcpeAnalyzer.h](../src/backend/FcpeAnalyzer.h) | FCPE 连续 F0 ONNX 分析 |
| [GameAnalyzer.cpp](../src/backend/GameAnalyzer.cpp) / [GameAnalyzer.h](../src/backend/GameAnalyzer.h) | GAME 音符 ONNX 分析 |
| [HifisamplerDsp.inc](../src/backend/HifisamplerDsp.inc) | 内置 HiFisampler DSP 包含实现 |
| [HifisamplerFlags.h](../src/backend/HifisamplerFlags.h) | 文本 FLAG、参数范围、曲线和优先级 |
| [HifisamplerSmoke.inc](../src/backend/HifisamplerSmoke.inc) | HiFisampler 专项验证包含实现 |
| [LegacyTextCodec.h](../src/backend/LegacyTextCodec.h) | 旧文本编码兼容 |
| [LiveMcpBridge.cpp](../src/backend/LiveMcpBridge.cpp) / [LiveMcpBridge.h](../src/backend/LiveMcpBridge.h) | 实时窗口发现、会话绑定和进程通信 |
| [Llsm2Renderer.cpp](../src/backend/Llsm2Renderer.cpp) / [Llsm2Renderer.h](../src/backend/Llsm2Renderer.h) | LLSM2 合成适配 |
| [McpServer.cpp](../src/backend/McpServer.cpp) / [McpServer.h](../src/backend/McpServer.h) | 独立 MCP 工具 schema 和执行 |
| [MelodyneImporter.cpp](../src/backend/MelodyneImporter.cpp) / [MelodyneImporter.h](../src/backend/MelodyneImporter.h) | 独立 MPD 解析和工程转换 |
| [MelodyneProvider.cpp](../src/backend/MelodyneProvider.cpp) / [MelodyneProvider.h](../src/backend/MelodyneProvider.h) | 安装发现、VST3 探测和能力开关 |
| [Mld3Renderer.cpp](../src/backend/Mld3Renderer.cpp) / [Mld3Renderer.h](../src/backend/Mld3Renderer.h) | MLD3 兼容和自有渲染实现 |
| [Mld5Renderer.cpp](../src/backend/Mld5Renderer.cpp) / [Mld5Renderer.h](../src/backend/Mld5Renderer.h) | MLD5 兼容和自有渲染实现 |
| [NativeAnalyzer.cpp](../src/backend/NativeAnalyzer.cpp) / [NativeAnalyzer.h](../src/backend/NativeAnalyzer.h) | 无神经模型的原生音频分析 |
| [NsfHifiganRenderer.cpp](../src/backend/NsfHifiganRenderer.cpp) / [NsfHifiganRenderer.h](../src/backend/NsfHifiganRenderer.h) | NSF 模型、声学特征、帧对齐和 ONNX 推理 |
| [NsfRenderChunkCache.h](../src/backend/NsfRenderChunkCache.h) | 非 U NSF 会话内核心音频复用；完整输入指纹、LRU 内存限制及不可变共享读者 |
| [OrtExecution.cpp](../src/backend/OrtExecution.cpp) / [OrtExecution.h](../src/backend/OrtExecution.h) | 原生 ONNX 环境、执行后端和设备支持 |
| [OtoAudioAnalysis.cpp](../src/backend/OtoAudioAnalysis.cpp) / [OtoAudioAnalysis.h](../src/backend/OtoAudioAnalysis.h) | OTO 音频与频谱分析 |
| [PlaybackRenderPriority.h](../src/backend/PlaybackRenderPriority.h) | 按播放位置计算合成优先级 |
| [PlaybackRenderQueue.h](../src/backend/PlaybackRenderQueue.h) | 任务排队、并发隔离、取消和过期清理 |
| [RenderService.cpp](../src/backend/RenderService.cpp) / [RenderService.h](../src/backend/RenderService.h) | 原音保真判断、调度和各合成后端调用 |
| [NativeEnvelope.h](../src/backend/NativeEnvelope.h) | 原生独立包络参数、序列化、求值、显示和贝塞尔裁剪 |
| [MixedEnvelopeIO.h](../src/backend/MixedEnvelopeIO.h) | 混合包络分段点与各段形状的工程／预设 JSON 编解码和合法性检查 |
| [SourceBreathiness.h](../src/backend/SourceBreathiness.h) | 显示用气声／噪声证据，周期抵消残差、多频段平坦度和短时确认；带音高气声不抹去 F0 |
| [SourceVoicing.h](../src/backend/SourceVoicing.h) | 渲染／分析用的保守清音保护，独立于气声显示，不把有声气声元音整体归为清音 |
| [TailFadeSettings.h](../src/backend/TailFadeSettings.h) | 高级音头音尾包络参数与形状 |
| [UstExchange.h](../src/backend/UstExchange.h) | UST 导出、兼容字段和警告 |
| [UstImporter.cpp](../src/backend/UstImporter.cpp) / [UstImporter.h](../src/backend/UstImporter.h) | UST 导入解析 |
| [UstText.h](../src/backend/UstText.h) | UST 文本结构和辅助 |
| [UtauOtoOverride.h](../src/backend/UtauOtoOverride.h) | 单音 OTO 覆盖 |
| [ChineseCvvcPhonemizer.h](../src/backend/ChineseCvvcPhonemizer.h) | 中文 CVVC presamp 规则、严格别名编排、父音符时钟和波形聚合；标准 UTAU 两种后端共用 |
| [UtauRenderer.cpp](../src/backend/UtauRenderer.cpp) / [UtauRenderer.h](../src/backend/UtauRenderer.h) | 音源与 OTO、传统重采样器、内置 NSF 句段、缓存和 HF 进程管理 |
| [UtauMixEnvelope.h](../src/backend/UtauMixEnvelope.h) | 混音与 CVVC 包络显示共用的采样级自动淡化窗口和增益；只读渲染元数据，不保存到工程 |
| [WorldRenderer.cpp](../src/backend/WorldRenderer.cpp) / [WorldRenderer.h](../src/backend/WorldRenderer.h) | WORLD 合成适配 |

## DiffSinger Python 模块

位于 `engines/diffsinger/`，由构建复制到运行资源目录。

| 文件 | 功能 |
| --- | --- |
| [bridge.py](../engines/diffsinger/bridge.py) | 音源和模型装载、分组、时长、pitch、variance、声学和声码器操作 |
| [compatibility.py](../engines/diffsinger/compatibility.py) | 采样输入兼容和音源预检 |
| [english_g2p.py](../engines/diffsinger/english_g2p.py) | 英文读音预测 |
| [expressions.py](../engines/diffsinger/expressions.py) | 能力和参数采样、variance、音色混合 |
| [inference.py](../engines/diffsinger/inference.py) | 推理配置、执行后端、Context 和 Session |
| [pronunciation.py](../engines/diffsinger/pronunciation.py) | 多语言前缀、词典、Resolver 和发音覆盖 |
| [runtime.py](../engines/diffsinger/runtime.py) | 后台请求服务、缓存指纹、父进程存活和退出 |
| [timing.py](../engines/diffsinger/timing.py) | 时长上下文与边界覆盖应用 |
| [variance_retake.py](../engines/diffsinger/variance_retake.py) | 局部重生成 mask、结构键和预测 |

## 原生测试文件

检查项摘录仅用于定位，不表示已执行通过。完整运行参数在 Main.cpp 中确认。

| 文件 | 检查项摘录 |
| --- | --- |
| [SourceBreathinessSmoke.h](../src/tests/SourceBreathinessSmoke.h) | 弱／低频气声、滑音和颤音隔离、静音、边界、取消及真实录音显示证据 |
| [AccompanimentSmoke.h](../src/tests/AccompanimentSmoke.h) | `source_written`；`source_sidecar_written`；`independent_track_type` |
| [AccompanimentTempoSmoke.h](../src/tests/AccompanimentTempoSmoke.h) | 实际右键入口与非模态窗口、已有节拍模型、裁剪／合并、任务取消、缺资源及工程快照不变 |
| [AdvancedEnvelopePanelSmoke.h](../src/tests/AdvancedEnvelopePanelSmoke.h) | `settings_validate`；`reversed_range_rejected`；`nonfinite_level_rejected` |
| [AdvancedEnvelopeSmoke.h](../src/tests/AdvancedEnvelopeSmoke.h) | `voice_fixture_written`；`oto_fixture_loaded`；`oto_regions_written` |
| [ClassicFlagUiSmoke.h](../src/tests/ClassicFlagUiSmoke.h) | 查看文件的 run／diagnostic 实现和 Main.cpp 调用 |
| [ClipEditorScopeSmoke.h](../src/tests/ClipEditorScopeSmoke.h) | `split_created_two_regions`；`left_click_focuses_only_left_notes`；`select_all_is_region_local` |
| [ClipGainKnobSmoke.h](../src/tests/ClipGainKnobSmoke.h) | `source_written`；`export_succeeded`；`export_length_unchanged` |
| [ClipMergeSmoke.h](../src/tests/ClipMergeSmoke.h) | `source_written`；`export_succeeded`；`export_length_unchanged` |
| [ClipSplitSmoke.h](../src/tests/ClipSplitSmoke.h) | `source_written`；`export_succeeded`；`right_click_mute_badge_opens_menu_without_muting` |
| [ClipTrimSmoke.h](../src/tests/ClipTrimSmoke.h) | `source_written`；`export_succeeded`；`export_keeps_outer_clip_length` |
| [ComponentExportSmoke.h](../src/tests/ComponentExportSmoke.h) | `component_fixture`；`world_component_capability`；`llsm_component_capability` |
| [ContinuousFlagSmoke.h](../src/tests/ContinuousFlagSmoke.h) | `existing_point_mode_toolbar`；`mode_preference_roundtrip`；`right_click_menu_checked` |
| [CrossRegionEditingSmoke.h](../src/tests/CrossRegionEditingSmoke.h) | `mouse_down_retains_cross_region_selection`；`ui_moves_both_regions_by_same_time_and_pitch`；`ui_keeps_selection_and_hidden_region_unchanged` |
| [DiffSingerCacheSmoke.h](../src/tests/DiffSingerCacheSmoke.h) | `project_saved`；`real_render_ok`；`expected_disk_hit` |
| [DiffSingerConsonantSmoke.h](../src/tests/DiffSingerConsonantSmoke.h) | `user_project_render`；`user_project_export`；`real_zhang_wang_render` |
| [DiffSingerExpressionsSmoke.h](../src/tests/DiffSingerExpressionsSmoke.h) | `ordinary_U_DF_eligible`；`ordinary_UTAU_WCSNDM_eligible`；`DF_namespace_rejected_in_WCSNDM` |
| [DiffSingerInferenceSmoke.h](../src/tests/DiffSingerInferenceSmoke.h) | 查看文件的 run／diagnostic 实现和 Main.cpp 调用 |
| [DiffSingerParameterRetakeSmoke.h](../src/tests/DiffSingerParameterRetakeSmoke.h) | `retake_completed`；`DS_button_named_FLAG`；`FLAG_click_opens_actual_without_enabling_offsets` |
| [DiffSingerParametersSmoke.h](../src/tests/DiffSingerParametersSmoke.h) | `accept_prediction`；`stale_prediction_rejected`；`only_selected_note` |
| [DiffSingerPitchHandlesSmoke.h](../src/tests/DiffSingerPitchHandlesSmoke.h) | `generation_applied`；`full_prediction_and_reference_preserved`；`generation_has_2_to_16_key_handles` |
| [DiffSingerPitchOffsetSmoke.h](../src/tests/DiffSingerPitchOffsetSmoke.h) | `DS_offset_mode_available`；`DS_offset_mode_enabled`；`default_line_click_does_not_edit` |
| [DiffSingerPitchPersistenceSmoke.h](../src/tests/DiffSingerPitchPersistenceSmoke.h) | `fresh_process_keeps_loaded_reference`；`fresh_process_keeps_reference_provenance`；`fresh_process_keeps_manual_curve_separate` |
| [DiffSingerPitchReferenceSmoke.h](../src/tests/DiffSingerPitchReferenceSmoke.h) | `generation_applied`；`one_undo_for_generation_and_reference`；`reference_matches_prediction` |
| [DiffSingerPitchRestoreSmoke.h](../src/tests/DiffSingerPitchRestoreSmoke.h) | `DS_reference_enables_mode`；`restore_mode_enabled`；`anchor_context_menu_also_offers_restore` |
| [DiffSingerSmoke.h](../src/tests/DiffSingerSmoke.h) | 查看文件的 run／diagnostic 实现和 Main.cpp 调用 |
| [DiffSingerTimingSmoke.h](../src/tests/DiffSingerTimingSmoke.h) | `legacy_DS_migrated_to_mou`；`DS_stays_in_mou`；`real_duration_prediction` |
| [DiffSingerUiSmoke.h](../src/tests/DiffSingerUiSmoke.h) | 查看文件的 run／diagnostic 实现和 Main.cpp 调用 |
| [EmptyTuningClipSmoke.h](../src/tests/EmptyTuningClipSmoke.h) | `non_backing_menu_enabled`；`backing_menu_disabled`；`outside_lanes_has_no_entry` |
| [GameDefaultsSmoke.h](../src/tests/GameDefaultsSmoke.h) | `default_medium`；`portable_game_and_fcpe_ready`；`portable_runtime_ready` |
| [HamoodPersistenceSmoke.h](../src/tests/HamoodPersistenceSmoke.h) | `opening_hamood_does_not_dirty_project`；`manual_key_is_stored_without_generating_harmony`；`closing_and_reopening_retains_manual_keys` |
| [HamoodUtauSmoke.h](../src/tests/HamoodUtauSmoke.h) | `real source sample copied into isolated bank` |
| [IndependentZoomSmoke.h](../src/tests/IndependentZoomSmoke.h) | `upper_wheel_leaves_lower_scale_and_position_unchanged`；`upper_wheel_changes_upper_scale`；`lower_wheel_leaves_upper_scale_and_position_unchanged` |
| [IntegratedSmoke.h](../src/tests/IntegratedSmoke.h) | 查看文件的 run／diagnostic 实现和 Main.cpp 调用 |
| [ModelessOtoSmoke.h](../src/tests/ModelessOtoSmoke.h) | `modeless_audio_fixture`；`bank_window_is_modeless`；`main_transport_and_roll_are_not_blocked` |
| [MouToJieSmoke.h](../src/tests/MouToJieSmoke.h) | `fixture_cp932_written`；`conversion_succeeds`；`all_four_region_rows_converted_once` |
| [NativeUnpitchedRegionsSmoke.h](../src/tests/NativeUnpitchedRegionsSmoke.h) | 自动／手动补齐、实际 PCM 删除、撤销、非线性时钟、保存、响度、裁剪与剪贴板 |
| [NativePitchVisibilitySmoke.h](../src/tests/NativePitchVisibilitySmoke.h) | 源曲线断线／人为曲线圆点的实际像素、线型区别、滚动相位、局部手绘、边界外端点、批量拉直、撤销与零 F0 |
| [NativeAudioDisconnectSmoke.h](../src/tests/NativeAudioDisconnectSmoke.h) | `source_written`；`single_selection_cannot_disconnect`；`right_marquee_selects_three` |
| [NativeAudioLinkSmoke.h](../src/tests/NativeAudioLinkSmoke.h) | `single_selection_cannot_link`；`multi_source_selection_offers_link`；`three_sources_linked_without_unselected_neighbor` |
| [NativeAudioOverlapFocusSmoke.h](../src/tests/NativeAudioOverlapFocusSmoke.h) | `chosen_source_stays_bright`；`other_overlapping_source_dims`；`nonoverlapping_source_keeps_brightness` |
| [NativeAudioOverlapSmoke.h](../src/tests/NativeAudioOverlapSmoke.h) | `two_sources_written`；`default_prohibits_overlap`；`note_move_stops_at_neighbor` |
| [ChineseCvvcSmoke.h](../src/tests/ChineseCvvcSmoke.h) | 发音规则、边界／编码／多音高、上下文试听、两种实际合成、菜单／工程保存和缓存 |
| [NativeEnvelopeSmoke.h](../src/tests/NativeEnvelopeSmoke.h) | `--smoke-native-envelope 输出目录`；原生响度通道鼠标加点／拖动／删除／多选及高级包络、持久化、裁剪、实际导出验证 |
| [NativeAudioTrimSmoke.h](../src/tests/NativeAudioTrimSmoke.h) | `source_written`；`subpixel_left_trim_ignores_coarse_grid_and_drag_threshold`；`undo_restores_full_audio` |
| [NativeNoteCopyPasteSmoke.h](../src/tests/NativeNoteCopyPasteSmoke.h) | `source_written`；`three_native_segments_created`；`head_moved_and_middle_stretched` |
| [NativeNoteMoveSmoke.h](../src/tests/NativeNoteMoveSmoke.h) | `source_written`；`split_creates_three_contiguous_notes`；`utau_drag_fixture_saved` |
| [NativeRenderedWaveformSmoke.h](../src/tests/NativeRenderedWaveformSmoke.h) | `real_waveform_is_an_optional_view`；`real_waveform_toggle_does_not_change_source_toggle`；`real_waveform_option_persists` |
| [NativeShutdownSmoke.h](../src/tests/NativeShutdownSmoke.h) | `cpu_model_available`；`fixture_written`；`oto_written` |
| [NativeIncrementalRenderSmoke.h](../src/tests/NativeIncrementalRenderSmoke.h) | 真实 NSF 局部音高／Mel、重叠边界、缓存清理、取消恢复和过期任务回写验证；记录冷／热耗时与推理块数 |
| [NativeNoiseOptionSmoke.h](../src/tests/NativeNoiseOptionSmoke.h) | 谐波／噪声开关的真实模型与 RenderService、清音 PCM、非线性时间映射、关闭后的核心复用和原音直通 |
| [NativeSourcePitchRestoreSmoke.h](../src/tests/NativeSourcePitchRestoreSmoke.h) | `native_context_offers_restore`；`right_marquee_selects_two_fragments`；`restore_keeps_timeline_and_nonlinear_source_clock` |
| [NativeSourcePitchSmoke.h](../src/tests/NativeSourcePitchSmoke.h) | `fixture_written`；`fixture_written`；`real_analysis_models_available` |
| [NativeWaveformPreviewSmoke.h](../src/tests/NativeWaveformPreviewSmoke.h) | `fixture_written`；`fixture_written`；`background_source_peaks_loaded` |
| [NormalDisplaySmoke.h](../src/tests/NormalDisplaySmoke.h) | `normal_display_starts_disabled`；`empty_tuning_region_can_enable`；`backing_region_cannot_enable` |
| [NoteDancePreview.h](../src/tests/NoteDancePreview.h) | 查看文件的 run／diagnostic 实现和 Main.cpp 调用 |
| [NoteHintsSmoke.h](../src/tests/NoteHintsSmoke.h) | `start_hint_menu_on_note_region`；`backing_and_empty_regions_disabled`；`menu_enables_only_target_region` |
| [NsfPickerSmoke.h](../src/tests/NsfPickerSmoke.h) | `--smoke-nsf-picker`：空轨算法往返、响应时间、默认音源无整库转换、原生／旧版 NSF 区分、撤销重做与保存重开 |
| [NsfProjectNoteSmoke.h](../src/tests/NsfProjectNoteSmoke.h) | `actual_project_native_render_succeeds`；`saved_short_envelope_reproduces_reported_silence`；`fitted_envelope_preserves_actual_shang_latter_half` |
| [NsfRegionsSmoke.h](../src/tests/NsfRegionsSmoke.h) | `recursive_oto_and_independent_jie_timing`；`resolver_preserves_jie_regions_and_mou_classes`；`mou_oto_loads_two_and_three_region_entries` |
| [OtoContinuitySmoke.h](../src/tests/OtoContinuitySmoke.h) | `line_scanner_preserves_unicode_and_mixed_line_endings`；`continuity_audio_fixture`；`real_voicebank_dialog_opens` |
| [OtoOverlapSmoke.h](../src/tests/OtoOverlapSmoke.h) | `snapshot` |
| [OtoSpectrumSmoke.h](../src/tests/OtoSpectrumSmoke.h) | `source_fixture_written`；`background_analysis_finishes`；`spectrum_and_fcpe_loaded` |
| [OutputEngineSmoke.h](../src/tests/OutputEngineSmoke.h) | `duplicate_nsf_jie_mou_algorithms_removed`；`ordinary_mou_mode_visible`；`region_menu_has_three_output_engine_choices` |
| [PlaybackRenderSmoke.h](../src/tests/PlaybackRenderSmoke.h) | `sounding_before_future`；`future_before_past`；`nearest_past_first` |
| [ProjectSafetySmoke.h](../src/tests/ProjectSafetySmoke.h) | `ordinary_edit_remains_undoable`；`ordinary_edit_redo`；`new_document_clears_undo_and_redo` |
| [ScrollZoomSmoke.h](../src/tests/ScrollZoomSmoke.h) | `custom_scrollbar_installed`；`endpoint_changes_zoom_in_correct_direction`；`opposite_endpoint_stays_anchored` |
| [TempoChangeSmoke.h](../src/tests/TempoChangeSmoke.h) | 查看文件的 run／diagnostic 实现和 Main.cpp 调用 |
| [TimelineNotePreviewSmoke.h](../src/tests/TimelineNotePreviewSmoke.h) | `preview_written`；`notes_visible_without_source_or_render`；`note_start_and_duration_align_with_timeline` |
| [TimelinePitchSmoke.h](../src/tests/TimelinePitchSmoke.h) | 查看文件的 run／diagnostic 实现和 Main.cpp 调用 |
| [TimelineSelectionSmoke.h](../src/tests/TimelineSelectionSmoke.h) | `cross_track_marquee_selects_only_intersections`；`marquee_does_not_seek_or_change_project`；`marquee_preview_written` |
| [TrackGainEnvelopeSmoke.h](../src/tests/TrackGainEnvelopeSmoke.h) | `source_written`；`preview_written`；`empty_gap_does_not_create_points` |
| [UstFidelitySmoke.h](../src/tests/UstFidelitySmoke.h) | `ust_`；`ust_`；`ust_` |
| [WavExportSmoke.h](../src/tests/WavExportSmoke.h) | `modal_callback_completed`；`fixture_stereo_tones_written`；`write_stereo_` |

## Main.cpp 命令行入口

以下扫描实际 `arguments[0] == ...` 分支，参数数量含命令本身。行号可能随代码移动，可按命令字符串重新搜索。执行前阅读完整分支；有的需要模型、音源、WAV 或探针 EXE，有的会启动窗口或写出文件，不要批量执行全部命令。

| 命令 | 参数数量条件 | 分支位置 |
| --- | --- | --- |
| `--audit-hamood-keys` | `size >= 3` | [Main.cpp L108](../src/Main.cpp#L108) |
| `--smoke-hamood-utau` | `size >= 4` | [Main.cpp L127](../src/Main.cpp#L127) |
| `--smoke-hamood` | `size >= 2` | [Main.cpp L133](../src/Main.cpp#L133) |
| `--smoke-diffsinger-pitch-offset` | `size >= 2` | [Main.cpp L140](../src/Main.cpp#L140) |
| `--smoke-accompaniment` | `size >= 2` | [Main.cpp L147](../src/Main.cpp#L147) |
| `--smoke-wav-export-options` | `size >= 2` | [Main.cpp L154](../src/Main.cpp#L154) |
| `--smoke-diffsinger-pitch-persistence` | `size >= 3` | [Main.cpp L164](../src/Main.cpp#L164) |
| `--smoke-diffsinger-pitch-handles` | `size >= 2` | [Main.cpp L172](../src/Main.cpp#L172) |
| `--smoke-diffsinger-pitch-restore` | `size >= 2` | [Main.cpp L178](../src/Main.cpp#L178) |
| `--smoke-diffsinger-pitch-reference` | `size >= 2` | [Main.cpp L185](../src/Main.cpp#L185) |
| `--smoke-continuous-flag` | `size >= 2` | [Main.cpp L191](../src/Main.cpp#L191) |
| `--smoke-classic-flag-ui` | `查看分支` | [Main.cpp L198](../src/Main.cpp#L198) |
| `--smoke-diffsinger-inference` | `size >= 3` | [Main.cpp L207](../src/Main.cpp#L207) |
| `--smoke-diffsinger-parameter-retake` | `size >= 3` | [Main.cpp L213](../src/Main.cpp#L213) |
| `--smoke-diffsinger-cache` | `size >= 4` | [Main.cpp L220](../src/Main.cpp#L220) |
| `--smoke-diffsinger-parameters` | `size >= 2` | [Main.cpp L225](../src/Main.cpp#L225) |
| `--smoke-diffsinger-expressions` | `查看分支` | [Main.cpp L230](../src/Main.cpp#L230) |
| `--smoke-diffsinger-consonants` | `size >= 3` | [Main.cpp L237](../src/Main.cpp#L237) |
| `--smoke-diffsinger-timing` | `size >= 3` | [Main.cpp L243](../src/Main.cpp#L243) |
| `--smoke-diffsinger-ui` | `size >= 3` | [Main.cpp L250](../src/Main.cpp#L250) |
| `--smoke-diffsinger` | `size >= 3` | [Main.cpp L259](../src/Main.cpp#L259) |
| `--smoke-chinese-cvvc` | `size >= 2`，可选模型目录 | [Main.cpp L266](../src/Main.cpp#L266) |
| `--smoke-output-engine` | `size >= 4` | [Main.cpp L274](../src/Main.cpp#L274) |
| `--smoke-native-incremental` | 输出目录、NSF 模型目录 | [Main.cpp](../src/Main.cpp) |
| `--smoke-native-noise-option` | 输出目录、NSF 模型目录 | [Main.cpp](../src/Main.cpp) |
| `--smoke-native-shutdown` | `size >= 3` | [Main.cpp](../src/Main.cpp) |
| `--smoke-hifisampler` | `size >= 3` | [Main.cpp L291](../src/Main.cpp#L291) |
| `--smoke-nsf-regions` | `size >= 3` | [Main.cpp L298](../src/Main.cpp#L298) |
| `--smoke-nsf-picker` | `查看分支` | [Main.cpp L306](../src/Main.cpp#L306) |
| `--smoke-cross-region-editing` | `size >= 2` | [Main.cpp L315](../src/Main.cpp#L315) |
| `--smoke-hamood-persistence` | `size >= 2` | [Main.cpp L315](../src/Main.cpp#L315) |
| `--smoke-mou-to-jie` | `size >= 2` | [Main.cpp L322](../src/Main.cpp#L322) |
| `--smoke-oto-overlap` | `size >= 2` | [Main.cpp L328](../src/Main.cpp#L328) |
| `--smoke-modeless-oto` | `size >= 2` | [Main.cpp L334](../src/Main.cpp#L334) |
| `--smoke-oto-continuity` | `size >= 2` | [Main.cpp L339](../src/Main.cpp#L339) |
| `--smoke-advanced-envelope` | `size >= 2` | [Main.cpp L344](../src/Main.cpp#L344) |
| `--smoke-advanced-envelope-panel` | `size >= 2` | [Main.cpp L344](../src/Main.cpp#L344) |
| `--smoke-playback-priority` | `size >= 3` | [Main.cpp L349](../src/Main.cpp#L349) |
| `--smoke-ust-fidelity` | `size >= 2` | [Main.cpp L350](../src/Main.cpp#L350) |
| `--smoke-project-safety` | `size >= 2` | [Main.cpp L351](../src/Main.cpp#L351) |
| `--smoke-normal-display` | `size >= 2` | [Main.cpp L359](../src/Main.cpp#L359) |
| `--smoke-note-hints` | `size >= 2` | [Main.cpp L367](../src/Main.cpp#L367) |
| `--smoke-clip-editor-scope` | `size >= 2` | [Main.cpp L375](../src/Main.cpp#L375) |
| `--smoke-empty-tuning-clip` | `size >= 2` | [Main.cpp L383](../src/Main.cpp#L383) |
| `--smoke-track-gain-envelope` | `size >= 2` | [Main.cpp L391](../src/Main.cpp#L391) |
| `--smoke-native-rendered-waveform` | `size >= 2` | [Main.cpp L399](../src/Main.cpp#L399) |
| `--smoke-native-note-move` | `size >= 2` | [Main.cpp L406](../src/Main.cpp#L406) |
| `--smoke-native-source-pitch-restore` | `size >= 2` | [Main.cpp L412](../src/Main.cpp#L412) |
| `--smoke-native-audio-overlap-focus` | `size >= 2` | [Main.cpp L418](../src/Main.cpp#L418) |
| `--smoke-native-audio-overlap` | `size >= 2` | [Main.cpp L424](../src/Main.cpp#L424) |
| `--smoke-native-audio-link` | `size >= 2` | [Main.cpp L430](../src/Main.cpp#L430) |
| `--smoke-native-audio-trim` | `size >= 2` | [Main.cpp L436](../src/Main.cpp#L436) |
| `--smoke-native-audio-disconnect` | `size >= 2` | [Main.cpp L442](../src/Main.cpp#L442) |
| `--smoke-native-note-copy-paste` | `size >= 2` | [Main.cpp L448](../src/Main.cpp#L448) |
| `--smoke-native-source-pitch` | `size >= 3` | [Main.cpp L455](../src/Main.cpp#L455) |
| `--smoke-native-waveform-preview` | `size >= 2` | [Main.cpp L463](../src/Main.cpp#L463) |
| `--smoke-timeline-notes` | `size >= 2` | [Main.cpp L471](../src/Main.cpp#L471) |
| `--smoke-clip-gain-knob` | `size >= 2` | [Main.cpp L479](../src/Main.cpp#L479) |
| `--smoke-clip-merge` | `size >= 2` | [Main.cpp L487](../src/Main.cpp#L487) |
| `--smoke-timeline-selection` | `size >= 2` | [Main.cpp L495](../src/Main.cpp#L495) |
| `--smoke-clip-trim` | `size >= 2` | [Main.cpp L503](../src/Main.cpp#L503) |
| `--smoke-clip-split` | `size >= 2` | [Main.cpp L512](../src/Main.cpp#L512) |
| `--smoke-independent-zoom` | `查看分支` | [Main.cpp L521](../src/Main.cpp#L521) |
| `--smoke-scroll-zoom` | `size >= 2` | [Main.cpp L530](../src/Main.cpp#L530) |
| `--preview-note-dance` | `size >= 2` | [Main.cpp L539](../src/Main.cpp#L539) |
| `--smoke-tempo-change` | `查看分支` | [Main.cpp L547](../src/Main.cpp#L547) |
| `--smoke-timeline-pit` | `size > 1` | [Main.cpp L556](../src/Main.cpp#L556) |
| `--mcp-live` | `查看分支` | [Main.cpp L562](../src/Main.cpp#L562) |
| `--smoke-native-envelope` | `输出目录` | [Main.cpp](../src/Main.cpp) |
| `--mcp-list-sessions` | `查看分支` | [Main.cpp L562](../src/Main.cpp#L562) |
| `--smoke-nsf-project-note` | `size >= 5` | [Main.cpp L570](../src/Main.cpp#L570) |
| `--mcp` | `查看分支` | [Main.cpp L576](../src/Main.cpp#L576) |
| `--smoke-oto-playback` | `size >= 1` | [Main.cpp L590](../src/Main.cpp#L590) |
| `--smoke-oto-editor` | `size >= 3` | [Main.cpp L790](../src/Main.cpp#L790) |
| `--smoke-splice-envelope` | `size >= 2` | [Main.cpp L913](../src/Main.cpp#L913) |
| `--smoke-dialog-enter` | `查看分支` | [Main.cpp L1023](../src/Main.cpp#L1023) |
| `--smoke-source-edit-view` | `size >= 2` | [Main.cpp L1081](../src/Main.cpp#L1081) |
| `--smoke-consonant-hold` | `size >= 1` | [Main.cpp L1143](../src/Main.cpp#L1143) |
| `--smoke-mou-panel-counts` | `size >= 2` | [Main.cpp L1363](../src/Main.cpp#L1363) |
| `--smoke-mou-consonant-ticks` | `size >= 3` | [Main.cpp L1454](../src/Main.cpp#L1454) |
| `--smoke-mou-boundary-drag` | `size >= 3` | [Main.cpp L1607](../src/Main.cpp#L1607) |
| `--smoke-prefix-map` | `size >= 3` | [Main.cpp L1696](../src/Main.cpp#L1696) |
| `--repair-mou-oto` | `size >= 2` | [Main.cpp L1737](../src/Main.cpp#L1737) |
| `--smoke-mou-oto-file` | `size >= 2` | [Main.cpp L1758](../src/Main.cpp#L1758) |
| `--smoke-utau-mode` | `size >= 2` | [Main.cpp L1957](../src/Main.cpp#L1957) |
| `--smoke-lane-marquee` | `size >= 2` | [Main.cpp L2097](../src/Main.cpp#L2097) |
| `--smoke-flag-lane` | `size >= 3` | [Main.cpp L2198](../src/Main.cpp#L2198) |
| `--smoke-keyboard-labels` | `size >= 2` | [Main.cpp L3059](../src/Main.cpp#L3059) |
| `--smoke-lane-labels` | `查看分支` | [Main.cpp L3113](../src/Main.cpp#L3113) |
| `--smoke-export-targets` | `查看分支` | [Main.cpp L3141](../src/Main.cpp#L3141) |
| `--smoke-audition-position` | `size >= 2` | [Main.cpp L3247](../src/Main.cpp#L3247) |
| `--smoke-lyric-envelope` | `size >= 4` | [Main.cpp L3289](../src/Main.cpp#L3289) |
| `--smoke-lyric-timing` | `size >= 5` | [Main.cpp L3361](../src/Main.cpp#L3361) |
| `--smoke-vibrato-bake` | `size >= 2` | [Main.cpp L3460](../src/Main.cpp#L3460) |
| `--smoke-mou-note-regions` | `size >= 4` | [Main.cpp L3573](../src/Main.cpp#L3573) |
| `--smoke-region-guides` | `size >= 2` | [Main.cpp L3859](../src/Main.cpp#L3859) |
| `--smoke-rest-playback` | `size >= 4` | [Main.cpp L3933](../src/Main.cpp#L3933) |
| `--smoke-rest-lyric` | `size >= 3` | [Main.cpp L4029](../src/Main.cpp#L4029) |
| `--smoke-missing-alias-piano` | `size >= 1` | [Main.cpp L4192](../src/Main.cpp#L4192) |
| `--smoke-flag-curve-mode` | `size >= 1` | [Main.cpp L4319](../src/Main.cpp#L4319) |
| `--smoke-flag-point-value` | `size >= 2` | [Main.cpp L4504](../src/Main.cpp#L4504) |
| `--smoke-flag-overlap` | `size >= 3` | [Main.cpp L4594](../src/Main.cpp#L4594) |
| `--smoke-load-timing` | `size >= 2` | [Main.cpp L4709](../src/Main.cpp#L4709) |
| `--smoke-flatten-pitch-line` | `size >= 1` | [Main.cpp L4822](../src/Main.cpp#L4822) |
| `--smoke-note-across-tracks` | `size >= 1` | [Main.cpp L4958](../src/Main.cpp#L4958) |
| `--smoke-clip-across-tracks` | `size >= 1` | [Main.cpp L5114](../src/Main.cpp#L5114) |
| `--smoke-reference-track` | `size >= 1` | [Main.cpp L5263](../src/Main.cpp#L5263) |
| `--smoke-native-pitch-seam` | `size >= 1` | [Main.cpp L5411](../src/Main.cpp#L5411) |
| `--smoke-native-pitch-points` | `size >= 1` | [Main.cpp L5483](../src/Main.cpp#L5483) |
| `--smoke-paste-at-pointer` | `size >= 1` | [Main.cpp L5707](../src/Main.cpp#L5707) |
| `--smoke-note-menu-split` | `size >= 1` | [Main.cpp L5850](../src/Main.cpp#L5850) |
| `--smoke-lyric-gate` | `size >= 1` | [Main.cpp L6032](../src/Main.cpp#L6032) |
| `--smoke-lyric-tab` | `size >= 1` | [Main.cpp L6129](../src/Main.cpp#L6129) |
| `--smoke-syllable-cuts` | `size >= 1` | [Main.cpp L6329](../src/Main.cpp#L6329) |
| `--smoke-single-syllable` | `size >= 1` | [Main.cpp L6511](../src/Main.cpp#L6511) |
| `--smoke-deleted-notes-silent` | `size >= 1` | [Main.cpp L6618](../src/Main.cpp#L6618) |
| `--smoke-delete-clip` | `size >= 1` | [Main.cpp L6765](../src/Main.cpp#L6765) |
| `--smoke-draw-unit` | `size >= 1` | [Main.cpp L6862](../src/Main.cpp#L6862) |
| `--smoke-new-track-draw` | `size >= 1` | [Main.cpp L6955](../src/Main.cpp#L6955) |
| `--smoke-draw-drag` | `size >= 1` | [Main.cpp L7062](../src/Main.cpp#L7062) |
| `--smoke-draw-overlap` | `size >= 1` | [Main.cpp L7233](../src/Main.cpp#L7233) |
| `--smoke-dropdown-arrow` | `size >= 1` | [Main.cpp L7417](../src/Main.cpp#L7417) |
| `--smoke-pitch-line` | `size >= 1` | [Main.cpp L7508](../src/Main.cpp#L7508) |
| `--smoke-native-unpitched-regions` | 输出目录；可选真实录音与 GAME／FCPE 模型根目录 | [Main.cpp](../src/Main.cpp) |
| `--smoke-source-breathiness` | `size >= 2`；输出目录，可选真实录音 | [Main.cpp](../src/Main.cpp#L507) |
| `--smoke-native-consonant-display` | `size >= 2`；输出目录，可选真实录音 | [Main.cpp](../src/Main.cpp) |
| `--smoke-view-menu` | `size >= 1` | [Main.cpp L7799](../src/Main.cpp#L7799) |
| `--inspect-melodyne-tracks` | `size >= 2` | [Main.cpp L7898](../src/Main.cpp#L7898) |
| `--smoke-melodyne-provider` | `size >= 1` | [Main.cpp L7909](../src/Main.cpp#L7909) |
| `--probe-melodyne-vst3-instance` | `size >= 1` | [Main.cpp L7934](../src/Main.cpp#L7934) |
| `--smoke-native-timing` | `size >= 1` | [Main.cpp L7946](../src/Main.cpp#L7946) |
| `--smoke-integrated-ui` | `size >= 2` | [Main.cpp L8003](../src/Main.cpp#L8003) |
| `--smoke-integrated` | `查看分支` | [Main.cpp L8012](../src/Main.cpp#L8012) |
| `--smoke-native-hjm` | `size >= 1` | [Main.cpp L8018](../src/Main.cpp#L8018) |
| `--smoke-batch-lyrics` | `size >= 1` | [Main.cpp L8085](../src/Main.cpp#L8085) |
| `--smoke-envelope-lanes` | `size >= 1` | [Main.cpp L8177](../src/Main.cpp#L8177) |
| `--smoke-wheel` | `size >= 1` | [Main.cpp L8236](../src/Main.cpp#L8236) |
| `--smoke-waveform-toggle` | `size >= 1` | [Main.cpp L8277](../src/Main.cpp#L8277) |
| `--smoke-envelope-base` | `size >= 1` | [Main.cpp L8387](../src/Main.cpp#L8387) |
| `--smoke-amplitude-waveform` | `size >= 1` | [Main.cpp L8708](../src/Main.cpp#L8708) |
| `--smoke-vibrato-end` | `size >= 1` | [Main.cpp L9166](../src/Main.cpp#L9166) |
| `--smoke-hanzi-pinyin` | `size >= 1` | [Main.cpp L9426](../src/Main.cpp#L9426) |
| `--smoke-shared-pitch-line` | `size >= 1` | [Main.cpp L9565](../src/Main.cpp#L9565) |
| `--smoke-waveform-alignment` | `size >= 3` | [Main.cpp L10223](../src/Main.cpp#L10223) |
| `--smoke-note-waveform` | `size >= 3` | [Main.cpp L10422](../src/Main.cpp#L10422) |
| `--smoke-utau-waveform` | `size >= 3` | [Main.cpp L10728](../src/Main.cpp#L10728) |
| `--smoke-voicebank-index` | `size >= 1` | [Main.cpp L10902](../src/Main.cpp#L10902) |
| `--smoke-ust-pitch` | `size >= 1` | [Main.cpp L11368](../src/Main.cpp#L11368) |
| `--smoke-hf-daemon-owner` | `size >= 3` | [Main.cpp L11792](../src/Main.cpp#L11792) |
| `--smoke-hf-prewarm` | `size >= 2` | [Main.cpp L11805](../src/Main.cpp#L11805) |
| `--smoke-ust-vibrato` | `size >= 1` | [Main.cpp L11938](../src/Main.cpp#L11938) |
| `--smoke-mcp-roots` | `size >= 1` | [Main.cpp L12183](../src/Main.cpp#L12183) |
| `--smoke-mcp-schema` | `size >= 1` | [Main.cpp L12290](../src/Main.cpp#L12290) |
| `--smoke-dragged-transition` | `size >= 1` | [Main.cpp L12455](../src/Main.cpp#L12455) |
| `--smoke-forward-bend` | `size >= 1` | [Main.cpp L12678](../src/Main.cpp#L12678) |
| `--smoke-midi-track-import` | `size >= 1` | [Main.cpp L12850](../src/Main.cpp#L12850) |
| `--smoke-midi-export` | `size >= 1` | [Main.cpp L13179](../src/Main.cpp#L13179) |
| `--smoke-ust-import` | `size >= 1` | [Main.cpp L13472](../src/Main.cpp#L13472) |
| `--smoke-ust-encoding` | `size >= 2` | [Main.cpp L13613](../src/Main.cpp#L13613) |
| `--smoke-ust` | `size >= 2` | [Main.cpp L13765](../src/Main.cpp#L13765) |
| `--smoke-active-resampler` | `size >= 1` | [Main.cpp L13970](../src/Main.cpp#L13970) |
| `--smoke-bundled-resampler` | `size >= 1` | [Main.cpp L14015](../src/Main.cpp#L14015) |
| `--smoke-anchor-frequency` | `size >= 1` | [Main.cpp L14073](../src/Main.cpp#L14073) |
| `--smoke-note-oto` | `size >= 1` | [Main.cpp L14259](../src/Main.cpp#L14259) |
| `--smoke-prefix-jie` | `size >= 1` | [Main.cpp L14689](../src/Main.cpp#L14689) |
| `--smoke-prefix-note` | `size >= 1` | [Main.cpp L14843](../src/Main.cpp#L14843) |
| `--smoke-utau-overlap` | `size >= 1` | [Main.cpp L15200](../src/Main.cpp#L15200) |
| `--smoke-note-stp` | `size >= 1` | [Main.cpp L15515](../src/Main.cpp#L15515) |
| `--smoke-track-toggle-tips` | `size >= 1` | [Main.cpp L15754](../src/Main.cpp#L15754) |
| `--smoke-track-area-menu` | `size >= 1` | [Main.cpp L16001](../src/Main.cpp#L16001) |
| `--smoke-stretch-items` | `size >= 1` | [Main.cpp L16102](../src/Main.cpp#L16102) |
| `--smoke-render-order` | `size >= 1` | [Main.cpp L16154](../src/Main.cpp#L16154) |
| `--smoke-llsm2-length` | `size >= 1` | [Main.cpp L16286](../src/Main.cpp#L16286) |
| `--smoke-glide-seam` | `size >= 1` | [Main.cpp L16338](../src/Main.cpp#L16338) |
| `--smoke-native-envelope-base` | `查看分支` | [Main.cpp L16402](../src/Main.cpp#L16402) |
| `--smoke-nsf-utau-phrase` | `查看分支` | [Main.cpp L16486](../src/Main.cpp#L16486) |
| `--smoke-nsf-utau-synth` | `查看分支` | [Main.cpp L16518](../src/Main.cpp#L16518) |
| `--smoke-nsf-utau-f0` | `查看分支` | [Main.cpp L16566](../src/Main.cpp#L16566) |
| `--smoke-nsf-utau-mix` | `查看分支` | [Main.cpp L16611](../src/Main.cpp#L16611) |
| `--smoke-nsf-utau-plan` | `查看分支` | [Main.cpp L16676](../src/Main.cpp#L16676) |
| `--smoke-render-capability` | `查看分支` | [Main.cpp L16733](../src/Main.cpp#L16733) |
| `--smoke-oto-hjm-discipline` | `size >= 2` | [Main.cpp L16784](../src/Main.cpp#L16784) |
| `--smoke-asset-register-dir` | `size >= 2` | [Main.cpp L16854](../src/Main.cpp#L16854) |
| `--smoke-asset-manager` | `查看分支` | [Main.cpp L16898](../src/Main.cpp#L16898) |
| `--smoke-gap-menu` | `size >= 2` | [Main.cpp L17018](../src/Main.cpp#L17018) |
| `--smoke-insert-gap` | `size >= 2` | [Main.cpp L17137](../src/Main.cpp#L17137) |
| `--smoke-ripple-delete` | `size >= 2` | [Main.cpp L17253](../src/Main.cpp#L17253) |
| `--smoke-paste-notes` | `size >= 2` | [Main.cpp L17342](../src/Main.cpp#L17342) |
| `--smoke-consonant-edge` | `size >= 2` | [Main.cpp L17518](../src/Main.cpp#L17518) |
| `--smoke-consonant-reset` | `size >= 2` | [Main.cpp L17674](../src/Main.cpp#L17674) |
| `--smoke-region-split` | `size >= 3` | [Main.cpp L17781](../src/Main.cpp#L17781) |
| `--smoke-vibrato-presets` | `查看分支` | [Main.cpp L17873](../src/Main.cpp#L17873) |
| `--smoke-follow-rule` | `查看分支` | [Main.cpp L17943](../src/Main.cpp#L17943) |
| `--smoke-play-until` | `size >= 2` | [Main.cpp L18015](../src/Main.cpp#L18015) |
| `--smoke-flags-field` | `size >= 3` | [Main.cpp L18074](../src/Main.cpp#L18074) |
| `--smoke-vertical-drag` | `size >= 2` | [Main.cpp L18139](../src/Main.cpp#L18139) |
| `--smoke-scroll-sync` | `size >= 2` | [Main.cpp L18232](../src/Main.cpp#L18232) |
| `--smoke-edit-follow` | `size >= 2` | [Main.cpp L18313](../src/Main.cpp#L18313) |
| `--smoke-note-drag-threshold` | `size >= 2` | [Main.cpp L18413](../src/Main.cpp#L18413) |
| `--smoke-roll-drag` | `size >= 2` | [Main.cpp L18488](../src/Main.cpp#L18488) |
| `--smoke-playhead-band` | `size >= 2` | [Main.cpp L18619](../src/Main.cpp#L18619) |
| `--smoke-roll-cull` | `size >= 2` | [Main.cpp L18672](../src/Main.cpp#L18672) |
| `--smoke-roll-timing` | `size >= 2` | [Main.cpp L18745](../src/Main.cpp#L18745) |
| `--smoke-envelope-shapes` | `size >= 1` | [Main.cpp L18791](../src/Main.cpp#L18791) |
| `--smoke-envelope-presets` | `size >= 2` | [Main.cpp L19114](../src/Main.cpp#L19114) |
| `--smoke-piano-roll` | `size >= 2` | [Main.cpp L19152](../src/Main.cpp#L19152) |
| `--render-project` | `size >= 3` | [Main.cpp L19200](../src/Main.cpp#L19200) |
| `--inspect-project` | `size >= 2` | [Main.cpp L19277](../src/Main.cpp#L19277) |
| `--inspect-settings` | `查看分支` | [Main.cpp L19313](../src/Main.cpp#L19313) |
| `--smoke-oto-spectrum` | `size >= 2` | [Main.cpp L19340](../src/Main.cpp#L19340) |
| `--smoke-game-defaults` | `size >= 2` | [Main.cpp L19347](../src/Main.cpp#L19347) |
| `--inspect-analysis` | `查看分支` | [Main.cpp L19353](../src/Main.cpp#L19353) |
| `--inspect-fcpe` | `size >= 3` | [Main.cpp L19375](../src/Main.cpp#L19375) |
| `--smoke-export` | `size >= 3` | [Main.cpp L19401](../src/Main.cpp#L19401) |
| `--smoke-mld5` | `size >= 4` | [Main.cpp L19431](../src/Main.cpp#L19431) |
| `--smoke-utau-selection` | `size >= 2` | [Main.cpp L19524](../src/Main.cpp#L19524) |
| `--smoke-utau-voicebank` | `size >= 2` | [Main.cpp L19638](../src/Main.cpp#L19638) |
| `--smoke-utau` | `查看分支` | [Main.cpp L19721](../src/Main.cpp#L19721) |
| `--inspect-midi` | `size >= 2` | [Main.cpp L19807](../src/Main.cpp#L19807) |
| `--inspect-audio` | `size >= 2` | [Main.cpp L19830](../src/Main.cpp#L19830) |
| `--inspect-mpd` | `size >= 2` | [Main.cpp L19865](../src/Main.cpp#L19865) |

## Python 集成测试

位于 `juce/tests/`。运行前读脚本参数；真实模型、外置引擎和运行中窗口要求不同。

- [advanced_envelope_live.py](../juce/tests/advanced_envelope_live.py)
- [advanced_envelope_settings_live.py](../juce/tests/advanced_envelope_settings_live.py)
- [consonant_untouched_smoke.py](../juce/tests/consonant_untouched_smoke.py)
- [diffsinger_expressions.py](../juce/tests/diffsinger_expressions.py)
- [diffsinger_inference.py](../juce/tests/diffsinger_inference.py)
- [diffsinger_parameters.py](../juce/tests/diffsinger_parameters.py)
- [diffsinger_pronunciation_retake.py](../juce/tests/diffsinger_pronunciation_retake.py)
- [diffsinger_runtime.py](../juce/tests/diffsinger_runtime.py)
- [diffsinger_smoke.py](../juce/tests/diffsinger_smoke.py)
- [diffsinger_timing.py](../juce/tests/diffsinger_timing.py)
- [diffsinger_variance_retake.py](../juce/tests/diffsinger_variance_retake.py)
- [envelope_stretch_smoke.py](../juce/tests/envelope_stretch_smoke.py)
- [envelope_tail_smoke.py](../juce/tests/envelope_tail_smoke.py)
- [export_tracks_smoke.py](../juce/tests/export_tracks_smoke.py)
- [flag_curve_smoke.py](../juce/tests/flag_curve_smoke.py)
- [flag_precedence_smoke.py](../juce/tests/flag_precedence_smoke.py)
- [flag_rerender_smoke.py](../juce/tests/flag_rerender_smoke.py)
- [game_default_smoke.py](../juce/tests/game_default_smoke.py)
- [game_fcpe_smoke.py](../juce/tests/game_fcpe_smoke.py)
- [hamood_context_live.py](../juce/tests/hamood_context_live.py)
- [hamood_live_smoke.py](../juce/tests/hamood_live_smoke.py)
- [hifisampler_curve_smoke.py](../juce/tests/hifisampler_curve_smoke.py)
- [mcp_live_smoke.py](../juce/tests/mcp_live_smoke.py)
- [mcp_smoke.py](../juce/tests/mcp_smoke.py)
- [melodyne_import_smoke.py](../juce/tests/melodyne_import_smoke.py)
- [mou_mixed_counts_smoke.py](../juce/tests/mou_mixed_counts_smoke.py)
- [mou_oto_smoke.py](../juce/tests/mou_oto_smoke.py)
- [native_pitch_smoke.py](../juce/tests/native_pitch_smoke.py)
- [native_source_pitch_smoke.py](../juce/tests/native_source_pitch_smoke.py)
- [native_unedited_audio_smoke.py](../juce/tests/native_unedited_audio_smoke.py)
- [nsf_hifigan_model_smoke.py](../juce/tests/nsf_hifigan_model_smoke.py)
- [project_safety_live.py](../juce/tests/project_safety_live.py)
- [region_flag_smoke.py](../juce/tests/region_flag_smoke.py)
- [rest_lyric_smoke.py](../juce/tests/rest_lyric_smoke.py)
- [splice_smoke.py](../juce/tests/splice_smoke.py)
- [ust_fidelity_live.py](../juce/tests/ust_fidelity_live.py)
- [utau4_mode_smoke.py](../juce/tests/utau4_mode_smoke.py)

## 构建与通用工具

| 文件 | 用途 |
| --- | --- |
| [CMakeLists.txt](../juce/CMakeLists.txt) | C++ 目标、依赖、资源和版本 |
| [build-local.ps1](../build-local.ps1) | 本机 MSVC 增量构建 |
| [build-windows-native.ps1](../build-windows-native.ps1) | MSYS2 clang-cl 与 xwin 路线 |
| [windows-build.yml](../.github/workflows/windows-build.yml) | Windows 无模型构建与启动验证 |
| [.gitlab-ci.yml](../.gitlab-ci.yml) | GitLab 构建与配布 |
| [check_mcp_roots.py](../tools/check_mcp_roots.py) | MCP 根路径检查 |
| [check_mcp_schemas.py](../tools/check_mcp_schemas.py) | MCP schema 检查 |
| [integration-package-smoke.py](../tools/integration-package-smoke.py) | 配布包检查 |
| [integration-regression.py](../tools/integration-regression.py) | 整合回归驱动 |
| [make_fixtures.py](../tools/make_fixtures.py) | 测试素材生成 |
| [make_mcp_doc.py](../tools/make_mcp_doc.py) | MCP 文档生成 |
| [make_pinyin_table.py](../tools/make_pinyin_table.py) | 拼音表生成 |
| [package-current-distribution.py](../tools/package-current-distribution.py) | 历史配布归档；默认仍指向旧 0.2.2，先核对路径 |
| [organize-distribution.py](../tools/organize-distribution.py) | 配布根目录整理；默认预览，`--apply` 将验证报告和旧 EXE 备份归入子目录，校验内容并记录迁移 |
| [prepare-diffsinger-pronunciation.py](../tools/prepare-diffsinger-pronunciation.py) | DS 发音数据准备 |
| [prepare-note-dance.py](../tools/prepare-note-dance.py) | 音符舞动资源准备 |
| [prepare-nsf-model.py](../tools/prepare-nsf-model.py) | NSF 模型准备 |
| [regress.sh](../tools/regress.sh) | 回归驱动 |
| [run_breaks.sh](../tools/run_breaks.sh) | 边界／分段开发检查 |
| [verify_midi.py](../tools/verify_midi.py) | MIDI 验证 |

## HAMOOD 音频工作流脚本

运行时核心是 worker.py、runtime.json 和 client.py；其余为分析、KARA2、连续和声、东京泰迪熊等任务与配布验证入口。带具体歌曲名称的脚本可能硬编码工程和输出路径，不应直接用于新工程。

- [analyse_kara2.py](../tools/hamood_audio/analyse_kara2.py)
- [analyse_reference.py](../tools/hamood_audio/analyse_reference.py)
- [apply_reference.py](../tools/hamood_audio/apply_reference.py)
- [arrange_continuous_harmony.py](../tools/hamood_audio/arrange_continuous_harmony.py)
- [build_continuous_harmony.py](../tools/hamood_audio/build_continuous_harmony.py)
- [build_kara2_project.py](../tools/hamood_audio/build_kara2_project.py)
- [client.py](../tools/hamood_audio/client.py)
- [continuous_harmony_artifacts.py](../tools/hamood_audio/continuous_harmony_artifacts.py)
- [corroborate_kara2.py](../tools/hamood_audio/corroborate_kara2.py)
- [deploy_local.py](../tools/hamood_audio/deploy_local.py)
- [export_tokyo.py](../tools/hamood_audio/export_tokyo.py)
- [export_tokyo_cached.py](../tools/hamood_audio/export_tokyo_cached.py)
- [finish_tokyo_audio.py](../tools/hamood_audio/finish_tokyo_audio.py)
- [generate_tokyo.py](../tools/hamood_audio/generate_tokyo.py)
- [generate_tokyo_final.py](../tools/hamood_audio/generate_tokyo_final.py)
- [kara2-model.json](../tools/hamood_audio/kara2-model.json)
- [kara2.cmd](../tools/hamood_audio/kara2.cmd)
- [kara2.py](../tools/hamood_audio/kara2.py)
- [kara2_artifacts.py](../tools/hamood_audio/kara2_artifacts.py)
- [package_full.py](../tools/hamood_audio/package_full.py)
- [prepare_full_package.py](../tools/hamood_audio/prepare_full_package.py)
- [prepare_reference.py](../tools/hamood_audio/prepare_reference.py)
- [prepare_tokyo.py](../tools/hamood_audio/prepare_tokyo.py)
- [recheck_chorus_artifacts.py](../tools/hamood_audio/recheck_chorus_artifacts.py)
- [recheck_chorus_filters.py](../tools/hamood_audio/recheck_chorus_filters.py)
- [recheck_chorus_report.py](../tools/hamood_audio/recheck_chorus_report.py)
- [recheck_chorus_separation.py](../tools/hamood_audio/recheck_chorus_separation.py)
- [reference_polyphony.py](../tools/hamood_audio/reference_polyphony.py)
- [render_continuous_baseline.py](../tools/hamood_audio/render_continuous_baseline.py)
- [render_continuous_harmony.py](../tools/hamood_audio/render_continuous_harmony.py)
- [render_kara2.py](../tools/hamood_audio/render_kara2.py)
- [render_reference.py](../tools/hamood_audio/render_reference.py)
- [runtime.json](../tools/hamood_audio/runtime.json)
- [validate_tokyo.py](../tools/hamood_audio/validate_tokyo.py)
- [verify_installed.py](../tools/hamood_audio/verify_installed.py)
- [verify_kara2_portable.py](../tools/hamood_audio/verify_kara2_portable.py)
- [verify_portable_live.py](../tools/hamood_audio/verify_portable_live.py)
- [worker.py](../tools/hamood_audio/worker.py)
- [tempo_worker.py](../tools/hamood_audio/tempo_worker.py)：只运行现有 Beat This! small0 的伴奏曲速任务，不写工程。

## 资源和依赖

- `juce/resources/`：编辑器资源，由 CMake 引用。
- `juce/third_party/`：WORLD、LLSM2、ciglet 等依赖实现与许可。
- `.deps/` 和构建目录 `_deps/`：获取的 JUCE、ONNX Runtime 等依赖。
- `third_party/hifisampler/`：当前为 LICENSE 和 NOTICE，适配实现位于 backend。
- `models/`：本地权重及来源信息，不保证随源码包提供。
- `engines/diffsinger/pronunciation-data/`：词典和供应的读音依赖。
- `tools/hamood_audio/deps/`、`vendor/`、`reference-deps/`：分析依赖与验证用第三方代码。

## 专题文档

- [chinese-cvvc.md](chinese-cvvc.md)：标准 UTAU 中文 CVVC 使用方法、规则、实现及当前边界。

部分专题记录历史设计或验证范围，当前行为先看总览与源码。

- [accompaniment.md](accompaniment.md)：伴奏轨道
- [advanced-envelope.md](advanced-envelope.md)：原生音频与 OTO 高级包络（0.25 / 内部更新 116）
- [audio-export.md](audio-export.md)：WAV 音频导出（0.25，文档更新 034）
- [diffsinger-audit.md](diffsinger-audit.md)：DiffSinger 使用完整性检查 · 0.2.3
- [diffsinger.md](diffsinger.md)：DiffSinger 兼容（0.25）
- [game-analysis.md](game-analysis.md)：GAME 默认识别（0.25 内部更新 060）
- [hamood-audio.md](hamood-audio.md)：HAMOOD 伴奏分析与和声 MCP
- [hamood.md](hamood.md)：HAMOOD 自动和声（0.25 / 内部更新 055）
- [hifisampler-flags.md](hifisampler-flags.md)：HiFisampler（PC-NSF-HiFiGAN）
- [integration-notes.md](integration-notes.md)：UTAU / Melodyne Integration Notes
- [kara2.md](kara2.md)：KARA2 主唱 / 背景和声分离
- [local-integration-20260924.md](local-integration-20260924.md)：HachiShifter 本机整合版 0.2.1
- [mcp-live.md](mcp-live.md)：MCP 连接当前编辑器窗口（0.25）
- [mcp.md](mcp.md)：HachiShifter Next 的 MCP 接口
- [nsf-hifigan-model.md](nsf-hifigan-model.md)：Native NSF-HiFiGAN model installation
- [pitch-timeline-fix-20260921.md](pitch-timeline-fix-20260921.md)：2026-09-21 跨音符音高时间线修复
- [playback-rendering.md](playback-rendering.md)：按播放位置优先渲染
- [tempo-change-options.md](tempo-change-options.md)：局部曲速的音符处理选项（0.2.2）
- [unified-editor-design.md](unified-editor-design.md)：Unified Native Editor — Design
- [updates.md](updates.md)：编辑器更新记录
- [ust-fidelity.md](ust-fidelity.md)：音源编码与 UST 数据保真
- [utau-feature-parity.md](utau-feature-parity.md)：UTAU Feature Parity — Checklist
- [utau-linear-flags.md](utau-linear-flags.md)：WCSNDM 线性 FLAG（0.25 · 更新 011）
- [utau-output-engine.md](utau-output-engine.md)：UTAU 输出引擎
- [windows-build.md](windows-build.md)：Native Windows Build (MSYS2 + xwin)

## 索引维护

新增源码补齐职责和链接；新增 CLI 核对参数并更新入口表。功能语义变化同步更新 AI_CODE_GUIDE.md。测试记录放到本次输出目录，不把“有入口”写成“已通过”。

更新 121：`src/tests/NativeBatchFlattenSmoke.h` 由 `--smoke-flatten-pitch-line` 调用，验证原生多选框选后双击批量拉平与最近半音吸附（普通／标点工具），跨区域、选区保留、未选中隔离和整组撤销重做。

更新 122 新增：`src/tests/NativeLinkedPitchSmoke.h`，入口 `--smoke-native-linked-pitch <输出目录>`；检查原生粘连音频的共享线、渲染一致性、控制点交互及缓存依赖。实现位于 `ProjectModel.cpp::nativeSharedPitchLines`、`ClipParts.h::expandProjectClipParts` 与 `AudioEngine.cpp::applyNativeSharedPitch/writeNativePitchContext`。

更新 123 新增：`src/NativeSharedEnvelope.h`（原生组包络合并、插值、切片、物化）；`src/tests/NativeLinkedEnvelopeSmoke.h`，入口 `--smoke-native-linked-envelope <输出目录>`，检查跨切口编辑、显示／请求一致性、源音高保留、撤销、保存重开、复制和断开。

更新 124 新增：`NativeTrimSource.h`；`NativeTrimExpandSmoke.h`，入口 `--smoke-native-trim-expand <输出目录>`，覆盖向外恢复原文件、分离后删除恢复、双向文件边界、非线性时钟、重叠限制、保存重开、旧工程后台音高补齐和实际 PCM 原音对比。

更新 125 新增：`NativeNoteJoin.h` 和 `NativeNoteJoinSmoke.h`；入口 `--smoke-native-note-join <输出目录>`，验证真实双击事件、无连接误切换、统一拉伸、源时钟、曲线、单步撤销、同源子素材、保存重开和实际 PCM。

更新 126：`NativeNoteTiming.h` 修正跨音符控制点的分段时钟；`ProjectModel::resizeClip` 修正原生整段拉伸的源锚点、响度与颤音。沿用上述合并专项及 `--smoke-native-note-move <输出目录>`，加入长拉伸、共享曲线、区域增益、子素材和源映射检查；设置 `HACHI_TEST_NSF_MODEL_DIR` 增加真实 NSF 长拉伸音频检查。

## 更新 127：原生导入性能复测

`--smoke-native-import-performance <测试音频副本.wav> <models目录>`：使用 GAME＋FCPE 实际分析，记录导入、源音高附着、渲染请求、完整离屏截图及可见区域软件重绘耗时；检查分析前原声逐采样一致和显式音高编辑不被跳过。请传入独立测试目录的音频与同名 HJM 副本，避免诊断流程接触正式素材标注。完整离屏截图包含图像后端开销，不能直接换算成实际编辑器帧率。实现：[NativeImportPerformanceSmoke.h](../src/tests/NativeImportPerformanceSmoke.h)。

## 更新 128：拉直后的原生音高独立

`NativeBatchFlattenSmoke.h`（`--smoke-flatten-pitch-line`）新增实际显示曲线／渲染目标逐点独立平直及共享响度保持检查；`NativeLinkedPitchSmoke.h`（`--smoke-native-linked-pitch <输出目录>`）检查保存重开、后续单段编辑不牵动邻段，并生成 `independent-pitch.png`。

## 更新 129：原生独立音高的最小间隔连接

`NativeLinkedPitchSmoke.h`（`--smoke-native-linked-pitch <输出目录>`）检查边界控制点相隔 1 ms、短直线连续、主体平直、合成目标一致、空隙／重叠不连接和后续邻段独立。使用现有 `SharedPitchLine::Piece` 表达分段过渡；`nativeBoundaryAnchors` 仅用于派生显示和标点操作，不替代持久化原始控制点。

更新 130：`backend/HiFiShifterMelSmoke.inc` 是 `NsfHifiganRenderer.cpp` 包含的专项测试，覆盖上游参考插值、压缩／扩展／单帧、非均匀时钟／源跳接、尾部长度对齐与真实 ONNX 的 0.5／1／2／8 倍时长。CLI：`--smoke-hifishifter-mel <输出目录> <模型目录>`。

更新 131：`backend/SourceVoicing.h` 独立源周期性与清音保护掩码；`backend/NativeNoiseSmoke.inc` 覆盖清辅音／混合气声元音、FCPE 判定、真实 HN-SEP 与用户录音、源噪声时钟、未调音辅音 PCM 和音高变换。整句零 F0 回归见 `tests/NativeLinkedPitchSmoke.h`。

更新 132：`--smoke-flatten-pitch-line` 覆盖默认工具批量规整与标点工具加点／不规整／撤销；更新 121 中普通与标点都双击规整的行为已被替换。

更新 133：`NativeLinkedPitchSmoke.h` 增加端点水平拖动、无重生控制点、越过音频边缘、插值渲染及保存撤销回归。实际端点状态 `NoteData::nativePitchHandlesPlaced` 在 `ProjectModel` 写入和保存，`AudioEngine` 纳入缓存，`PianoRollComponent` 提供统一拖动约束。

更新 134：`backend/NsfPitchTransitionsSmoke.inc`（由 NSF 渲染源包含）检查突变平滑开关的数值和真实模型；`tests/NsfPickerSmoke.h` 检查非 U NSF 复选框、按轨道设置、撤销重做、保存重开、单段／整句请求和缓存失效。新命令 `--smoke-nsf-pitch-transitions <输出目录> <nsf模型目录>`。

更新 135：`NativePitchVoicingDisplay.*` 负责可选清辅音显示的后台源分析、缓存、非线性源时钟和曲线虚线分区；`tests/NativeConsonantDisplaySmoke.h` 由 `--smoke-native-consonant-display <输出目录> [真实录音]` 调用，覆盖旧工程 F0、气声元音、普通／标点视图、缺失 F0 断线（更新 142）、显示恢复、UTAU 隔离、工程不变、裁剪／拉伸及真实录音缓存。`--smoke-view-menu` 验证菜单第 8 项和 `ui.consonantPitchDashed` 设置的往返与默认关闭。
