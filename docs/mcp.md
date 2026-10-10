# HachiShifter Next 的 MCP 接口

当前窗口连接请使用 **`--mcp-live`**，完整配置与 DS 接口见 [当前窗口 MCP](mcp-live.md)。下面记录的是保留的 **`--mcp` 独立工程模式**，不连接已打开的窗口。

编辑器自带一个 MCP 服务器：外部程序（Claude Code、Claude Desktop、自己写的脚本）可以用它新建工程、导入音频/MIDI/UST、改音轨和音符、渲染试听、导出 WAV/MIDI、读写音源 oto。现有接口见下表；DS 专用操作使用当前窗口模式。

## 启动

```
"HachiShifter Next.exe" --mcp [--roots=D:\一个目录;D:\另一个目录]
```

stdio 上的 JSON-RPC，一行一条消息（MCP 的标准 stdio 传输）。协议版本 `2025-06-18`，服务器名 `hachishifter-next`。

**注意**：这个 exe 是 GUI 子系统程序，自己不带控制台。客户端用管道启动它完全正常；但你在命令行里手敲 `--mcp` 会看不到任何输出，那不是坏了，是没有管道。要手工试，就用管道喂进去：

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' | "HachiShifter Next.exe" --mcp
```

## 接进客户端

仓库根目录的 `.mcp.json` 就是现成的配置（Claude Code 会读它）。先运行 `build-local.ps1` 构建整合版；其他构建目录请相应修改 `command`：

```json
{
  "mcpServers": {
    "hachishifter": {
      "command": "build-integrated/HachiShifterNext_artefacts/RelWithDebInfo/HachiShifter Next.exe",
      "args": ["--mcp"]
    }
  }
}
```

别的客户端（Claude Desktop 等）用绝对路径写同样的三项：

```json
{
  "mcpServers": {
    "hachishifter": {
      "command": "C:\\src\\HachiShifter\\build-integrated\\HachiShifterNext_artefacts\\RelWithDebInfo\\HachiShifter Next.exe",
      "args": ["--mcp", "--roots=D:\\我的工程"]
    }
  }
}
```

## 服务器提供什么

- `tools/list`、`tools/call`：48 个工具，见下表。
- `resources/list`、`resources/read`：一个资源 `hachishifter://project/current`，读回当前工程的完整 JSON（和 `project_snapshot` 同样的内容）。
- `ping`。

错误分两种，按 MCP 的约定：协议层面的错误走 JSON-RPC 错误码（方法不存在 `-32601`、资源不存在 `-32002`），工具自己失败则是正常结果加 `isError: true`，文本里写明原因。

## 工具

每个工具的参数都写在 schema 里（名字、类型、说明、必填、取值只有几个词时列出那几个词），客户端里能直接看到。schema 保留 `additionalProperties: true`：几个会分析音频的工具共用一组分析设置参数，收紧了反而挡掉正常调用。

分析类参数（`project_open` 打开录音、`import_audio`、`analyse_audio`、`analysis_status`、`import_melodyne` 都收）：`game_model_dir`、`fcpe_model`、`game_model`（medium/large/small，便携包默认 medium）、`inference`（automatic/cpu/directml/cuda/coreml）、`device_index`。不传就用环境里配置好的；没有环境覆盖时自动发现 EXE 同目录的 GAME Medium + FCPE。

<!-- TOOLS -->

粗体是必填的参数。

### 工程

| 工具 | 做什么 | 参数 |
|---|---|---|
| `project_new` | 新建工程 | — |
| `project_open` | 打开工程或素材 | **path** |
| `project_save` | 保存工程 | **path** |
| `project_snapshot` | 读取全部工程内容 | — |

### 导入与导出

| 工具 | 做什么 | 参数 |
|---|---|---|
| `import_audio` | 导入音频 | **path**、start_seconds、track_id |
| `import_midi` | 导入 MIDI | **path** |
| `import_ust` | 导入 UST | **path** |
| `import_melodyne` | 导入 Melodyne 工程并控制素材搜索、工程编辑与原始 F0 | **path**、recursive_media、preserve_edits、source_pitch |
| `export_midi` | 导出 MIDI | **path** |
| `export_wav` | 渲染全曲并导出 WAV，track_id 可单独导出一个轨道 | **path**、track_id、from_seconds、to_seconds、timeout_seconds |
| `analyse_audio` | 执行 GAME+FCPE 分析并报告实际后端 | **path** |
| `analysis_status` | 查看 GAME、FCPE 与推理状态 | （分析参数） |

### 速度与音轨

| 工具 | 做什么 | 参数 |
|---|---|---|
| `set_tempo` | 设置速度与拍号 | bpm、numerator、denominator |
| `add_track` | 新建空旋律、普通或伴奏轨道 | name、compose、accompaniment（true 时关闭调音） |
| `set_track` | 设置轨道及算法 | **track_id**、name、compose、muted、solo、volume、pan、smooth_overlaps、normalize_volume、nsf_smooth_pitch_transitions、nsf_noise_protection、pitch_algorithm、stretch_algorithm、render_order、utau_global_flags、voicebank_directory |
| `remove_track` | 删除轨道 | **track_id** |

### 采样

| 工具 | 做什么 | 参数 |
|---|---|---|
| `set_clip` | 设置采样增益、淡入淡出和静音 | **clip_id**、gain、fade_in_seconds、fade_out_seconds、muted |
| `move_clip` | 移动采样 | **clip_id**、**start_seconds** |
| `resize_clip` | 整体拉伸采样并保留原始素材 | **clip_id**、**start_seconds**、duration_seconds |
| `duplicate_clip` | 深度复制采样及其音符到指定位置 | **clip_id**、start_seconds、track_id |
| `remove_clip` | 删除采样 | **clip_id** |

### 音符

| 工具 | 做什么 | 参数 |
|---|---|---|
| `add_note` | 在采样中创建音符 | **clip_id**、**start_seconds**、duration_seconds、midi |
| `set_note` | 设置稳健音高线及全部音符参数 | **note_id**、label、gain、tension、breath、formant_semitones、drift、modulation、robust_pitch_curve、consonant_seconds、attack_speed、amplitude_envelope、utau_flags、utau_consonant_velocity、utau_splice、jie_split、region_flags、flag_split、flag_curve_enabled、flag_curve、flag_curve_g |
| `resize_note` | 修改音符时间 | **note_id**、**start_seconds**、duration_seconds |
| `transpose_note` | 整体移动音高线 | **note_id**、**semitones** |
| `edit_notes_pitch` | 批量移调、设置、平均或量化音符 | note_ids、note_id、action、cents、midi、step_semitones |
| `duplicate_notes` | 深度复制所选音符到目标采样 | note_ids、note_id、**clip_id**、start_seconds |
| `set_pitch_curve` | 绘制目标音高线并保留原始 F0 | **note_id**、**points** |
| `toggle_note_connection` | 连接或分离相邻音符 | **note_id** |
| `remove_note` | 删除音符 | **note_id** |

### 撤销

| 工具 | 做什么 | 参数 |
|---|---|---|
| `undo` | 撤销工程编辑 | — |
| `redo` | 重做工程编辑 | — |

### 渲染与播放

| 工具 | 做什么 | 参数 |
|---|---|---|
| `utau_render_selection` | 选择参与 UTAU 渲染与试听的音符 | note_ids、note_id |
| `set_utau_resampler` | 指定 UTAU 重采样器 | **path** |
| `render_prepare` | 按当前所选算法预渲染工程 | wait、timeout_seconds |
| `render_status` | 读取预渲染进度与实际后端 | — |
| `transport_play` | 必要时预渲染并开始播放 | position_seconds、play_until_seconds、timeout_seconds |
| `transport_stop` | 停止播放 | — |
| `transport_seek` | 跳转播放位置 | **position_seconds** |
| `transport_status` | 读取播放位置与渲染状态 | — |

### 音源与分段

| 工具 | 做什么 | 参数 |
|---|---|---|
| `sample_settings_read` | 读取或生成音频 .hjm.csv 分段 | **audio_path** |
| `sample_settings_save` | 保存音频分段到 .hjm.csv | **audio_path**、**rows** |
| `oto_import` | 从 UTAU oto.ini 导入单个音频分段 | **audio_path**、**oto_path**、save_sidecar |
| `oto_export` | 将单个音频分段导出为 UTAU oto.ini | **audio_path**、**oto_path** |
| `jie_oto_create` | 按 oto.ini 生成界•OTO | **voicebank_path** |
| `voicebank_import` | 导入 UTAU 音源并生成 .hjm.csv | **path** |

### 文件

| 工具 | 做什么 | 参数 |
|---|---|---|
| `read_file` | 读取任意文件内容 | **path**、offset、max_bytes |
| `list_directory` | 列出目录内容 | **path** |

## 文件访问范围

`read_file` 和 `list_directory` 是通用的文件工具，**只能看见这个会话自己的工作所在的目录**：

1. 启动时用 `--roots=A;B` 或环境变量 `HACHISHIFTER_MCP_ROOTS` 指定的目录；
2. 会话里某个工具**成功**用过的路径所在目录（打开工程、导入 UST/音频/MIDI、导出 WAV/MIDI、读写 oto、导入音源……）。失败的路径不算，所以拿一个读不动的文件去试探不会打开它所在的目录；
3. 当前工程自己的目录：各音轨绑定的音源目录，以及各采样素材所在的目录。

"在目录里"按**整个目录**算，不是按名字前缀：允许了 `D:\song` 并不会连 `D:\song-private` 一起允许。子目录跟着父目录一起允许，反过来不行。

被拒绝时返回的文本会写明是哪个路径、当前允许了哪些目录、以及怎么放行（先打开工程或导入素材，或者用 `--roots=` / 环境变量启动）。

**已知限制**：路径本身是符号链接/junction 时会先解析一层再判断；但路径**中间某一级**是链接的情况不解析。同一台机器上能自己建链接的人绕得过去——这条规则挡的是接入的客户端乱翻硬盘，不是本机上蓄意的绕过。

其余工具（打开工程、导入、导出、音源相关）不受这个限制：它们本来就是编辑器该做的事，而且正是它们把目录变成"已允许"的。

## 一个完整的例子

把一首 UST 变成 MIDI，中间绑上音源：

```json
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"project_new","arguments":{}}}
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"import_ust","arguments":{"path":"F:\\UTAU\\voice\\雪融2.ust"}}}
{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"project_snapshot","arguments":{}}}
{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"set_track","arguments":{"track_id":"track_…","voicebank_directory":"F:\\UTAU\\voice\\某音源","pitch_algorithm":"utau"}}}
{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"export_midi","arguments":{"path":"D:\\out.mid"}}}
```

`project_snapshot` 回来的 JSON 里有 `track_id`、`clip_id`、`note_id`，后面的调用都靠它们指认对象。

渲染和导出音频要注意：UTAU 轨的渲染是**按选中的音符**走的，所以脚本里先 `utau_render_selection`（不传参数就是全选），再 `render_prepare` 或 `export_wav`。`export_wav` 自己会先全选再渲染。

## 维护

改了 MCP 接口，这四样会盯着：

| | 跑什么 | 看什么 |
|---|---|---|
| `--smoke-mcp-schema` | 回归套件 | 每个工具有独立 schema，参数都有类型和说明，required 都是已声明的属性，列表说明元素结构 |
| `tools/check_mcp_schemas.py` | 回归套件末尾 | 两边对账：分发器读的参数名 ↔ schema 声明的参数名 |
| `--smoke-mcp-roots` | 回归套件 | "在目录里"这条规则本身（前缀陷阱、`..`、子目录、空目录集） |
| `tools/check_mcp_roots.py` | 回归套件末尾 | 真的起一个服务器，让它读不该读的东西，看它拒绝 |

加工具时：在 `McpServer::diagnosticTools()` 里加一条（带参数），在 `McpServer::dispatch()` 里加一个分支。两边的参数名必须一致，否则 `check_mcp_schemas.py` 会报出来。
