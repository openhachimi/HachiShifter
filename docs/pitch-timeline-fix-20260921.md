# 2026-09-21 跨音符音高时间线修复

## 范围

修复 UTAU 请求中跨音符编辑音高遗漏；不修改 WCSNDM 引擎、音源和用户工程文件。
控制点仍按原来的音符结构保存；实际 PIT 读取不再受读取音符是否拥有控制点限制。
重叠的显式编辑继续遵循界面现有曲线取舍，不把隐藏点或独立声部盲目串接。

## 实现

- 显式曲线能传播到相邻的无手动控制点音符，支持连续多个接收音符。
- 显式曲线恰好到达音符边界也建立共享；休止、超过 2 ms 的间隙和独立重叠音符不连接。
- 渲染请求保存不可变的音高求值器，与界面读取同一个 SharedPitchLine；单音符手动曲线也直接求值。
- 外部引擎 PIT 按实际提前量和输出长度查询曲线，不受预览数组前 0.6 秒、后 0.3 秒限制。
- 覆盖提前量时，PIT 的负时间映射到 retimeLeadIn 对应的最终时间位置。
- 预览数组补入曲线拐点；保留贝塞尔等曲线形状和原有音符颤音处理。
- 跨界可见曲线的双击加点范围跟随可见曲线，不再裁到所属音符的名义末尾。
- 拖动预览缓存包含贝塞尔控制柄；片段缓存包含整轨音高依赖，单音符缓存包含实际编码后的 PIT。
  整轨依赖是偏保守的正确性措施，编辑不相关音符可能额外触发片段缓存更新。

## 验证

最终 RelWithDebInfo 程序通过以下七组内建测试：

- --smoke-timeline-pit：18 项；包括相邻/多音符/跨片段、单侧手动点、边界、休止/间隙/独立重叠、
  可见曲线加点与撤销、长提前量/尾部、覆盖提前量、不可变快照与缓存失效。
  解码实际 PIT 字符串，逐采样与曲线求值比较，误差不超过协议的半音分舍入精度。
- --smoke-shared-pitch-line
- --smoke-pitch-line
- --smoke-ust-pitch
- --smoke-dragged-transition
- --smoke-flatten-pitch-line
- --smoke-native-pitch-points

旧 shared-pitch-line 测试中的独立曲线拖动样例增加真实间隙，保留其独立编辑测试意图；
原先“相邻接收音符无手动点所以不共享”的前提由新测试明确替代。
同时避免未发生模型修改时盲目 undo，防止测试撤销到更早的工程状态。

上述测试验证界面/请求/PIT 数据一致性，不表示所有音源的实测 F0 都能无误差跟踪目标。

## 备份与构建

旧程序及修改前源码：
`C:/Users/Asuna/Documents/Codex/2026-09-01/r/work/editor-before-pitch-timeline-20260921/`

输出：`build-gpu/HachiShifterNext_artefacts/RelWithDebInfo/HachiShifter Next.exe`。
恢复原构建缓存使用的临时目录 junction，并设置 FETCHCONTENT_FULLY_DISCONNECTED=ON；
使用现有依赖离线构建，没有下载或升级依赖。
