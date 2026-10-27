# 翻译文件是唯一运行时语言来源

## 使用与编辑

正式文件位于程序旁的 `lang/translations.tsv`，即本地 `bin/lang/translations.tsv`。可在“设置 → 界面 → 打开翻译文件”打开，修改后选择“重新载入翻译”；已有工具窗口若没有立即更新全部标题，重新打开即可。请使用 UTF-8 制表符分隔文本编辑，不要改成逗号 CSV。

当前目录包含 **1509 条、7545 个五语言单元格**：简体中文、繁体中文、日文、韩文、英文。

**便携复制必须成套：** EXE 与旁边的 `lang` 文件夹一起复制。模型可留在目标目录；只替换 EXE 而保留旧版语言文件，会让新增标签显示 `[key]`。用户实际 G 盘运行目录已更新为成套文件，不需要把模型挪回 D 盘。

列顺序：

```
key    zh-CN    zh-TW    ja-JP    ko-KR    en-US    note    aliases
```

- `key`：稳定取词键。不要因修改翻译而改变它。
- 五种语言列：实际显示文本，运行时全部从此文件读取。
- `note`：备注，不显示为界面译文。
- `aliases`：JSON 字符串数组，保留历史源码字面量及英文短语的稳定映射，通常不需要修改。它不是额外语言列。
- 单元格内换行写为 `\n`，制表符写为 `\t`，反斜杠写为 `\\`。
- `{count}`、`{percent}`、`{interval}` 等占位符必须保留，数量也须一致。

## 读取与故障行为

- 已移除 C++ 内置五语言翻译表。`I18n::text`、`uiText` 和旧 `pairText` 都读取同一份文件目录，`pairText` 不再从代码里的中文参数取回退值。
- 空语言单元格仅可使用**同一文件的英文列**，不会回读代码译文。发放的目录无空格。
- 缺键显示 `[key]` 并可由诊断记录，不能用内置中文／英文偷偷掩盖缺译。
- 文件损坏／缺失会明确报告，保留本进程最后一份有效**文件**目录。新启动且没有任何有效文件时，只能显示缺键标记和启动技术诊断，因此请将翻译文件与程序一起保留。
- 重复键报告错误，首行生效；破坏占位符的格子被拒绝；未知格式的现有文件不会被“导出”覆盖。
- 文件重载不改音乐模型或音频请求。设备名、模型名、音源别名、歌词、路径、数字、算法品牌等是数据／专名，保留原样；外部后端的原始技术详情不伪装成应用译文。

## 维护约定

源码规范目录是 `resources/lang/translations.tsv`，CMake 将它复制到构建程序的 `lang` 子目录。它是外部数据，不编译进 EXE。增加界面短语时，加入文件并通过既有 I18n 入口取词；不要新增内置语言表或中英二选一 helper。模型字段、JSON 键、音高／flag ID 不能随界面语言改变。

内置振音／包络预设保留稳定数据身份，只有显示名称读取翻译文件；用户保存的自定义名称不翻译。HAMOOD 调性与和声报告在显示层翻译，不修改后端／工程的自动命名或 JSON 数据。

检查工具：

```
python tools/validate-translations.py resources/lang/translations.tsv
python tools/generate-ui-literals.py
python tools/generate-interface-reference.py
```

旧 `generate-ui-literals.py` 现在只输出缺键 JSON 审计，不再生成 C++ 翻译数据。`UiLiterals.inc` 是未被运行时引用的历史文件，不是语言回退源。

专项：`--smoke-translation-file`（文件哨兵、五语、别名、重载、占位符、坏文件等）、`--smoke-translation-no-file`（新进程无编译翻译回退）、`--smoke-file-only-translation-ui`（五语言设置／HAMOOD／DS 发音／素材选择实际组件）。环境变量 `HACHI_TRANSLATION_AUDIT_FILE` 可将实际访问但缺失的键输出至指定的自有诊断文件，默认不写。
