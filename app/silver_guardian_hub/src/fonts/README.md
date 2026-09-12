# 中文字库（自动生成，请勿手工编辑）

生成命令（在仓库根目录执行）：

    bash tools/gen_lvgl_fonts.sh

- 源字体：Noto Sans SC Regular
- 授权：SIL Open Font License 1.1 (Noto Sans SC / Google Noto Project)
- 字符集：GB2312 一级汉字(3755) + ASCII + 中文标点，由 `tools/gen_font_charset.py` 生成
- 生成工具：lv_font_conv

| 文件 | 变量名 | 字号 | bpp | 用途 |
|---|---|---|---|---|
| sg_font_16.c | `sg_font_16` | 16 | 4 | 正文、列表、按钮 |
| sg_font_24.c | `sg_font_24` | 24 | 2 | 页面标题 |

改了 `gen_font_charset.py` 里的字符集后要重新生成，否则新字会渲染成空白。
