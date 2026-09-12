#!/usr/bin/env python3
"""
生成 LVGL 位图字体所需的字符集（symbols），输出到 stdout。

字符集组成：
  - ASCII 可见字符 0x20-0x7E
  - GB2312 一级汉字（0xB0A1-0xD7F9，共 3755 字）—— 覆盖日常用语绰绰有余
  - 中文标点与常用符号（LVGL 内置字库缺的正是这部分之外的东西）

用法：
  python3 tools/gen_font_charset.py            # 输出字符集（一行）
  python3 tools/gen_font_charset.py --stats    # 只打印统计
  python3 tools/gen_font_charset.py -o chars.txt
"""

import sys

# 中文标点/常用符号（GB2312 二级区里的常用部分 + 全角标点）
PUNCT = "，。、；：？！“”‘’（）《》〈〉【】—…·～％＋－×÷＝"
EXTRA = "℃℉°±µΩ√≈≠≤≥→←↑↓★☆●○■□▲▼◆◇§¥$€£№"

# 罗马数字/希腊字母等偶尔用到的
MISC = "ΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩαβγδεζηθικλμνξοπρστυφχψω"


def build() -> str:
    chars = set()

    # ASCII 可见字符
    for c in range(0x20, 0x7F):
        chars.add(chr(c))

    # GB2312 一级汉字：区 0xB0-0xD7，位 0xA1-0xFE
    # 减去每区末尾 0xFF 之前不存在的位；直接尝试解码最稳妥
    for hi in range(0xB0, 0xD8):
        for lo in range(0xA1, 0xFF):
            try:
                ch = bytes([hi, lo]).decode("gb2312")
            except UnicodeDecodeError:
                continue
            chars.add(ch)

    chars.update(PUNCT)
    chars.update(EXTRA)
    chars.update(MISC)

    # 去掉控制字符和重复
    return "".join(sorted(c for c in chars if c.isprintable()))


if __name__ == "__main__":
    s = build()
    if "--stats" in sys.argv:
        hanzi = sum(1 for c in s if "一" <= c <= "鿿")
        print(f"总字符数: {len(s)}", file=sys.stderr)
        print(f"其中汉字: {hanzi}", file=sys.stderr)
        print(f"非汉字  : {len(s) - hanzi}", file=sys.stderr)
        sys.exit(0)

    if "-o" in sys.argv:
        path = sys.argv[sys.argv.index("-o") + 1]
        with open(path, "w", encoding="utf-8") as f:
            f.write(s)
        print(f"已写入 {path}（{len(s)} 字符）", file=sys.stderr)
    else:
        sys.stdout.write(s)
