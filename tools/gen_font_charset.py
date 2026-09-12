#!/usr/bin/env python3
"""
生成 LVGL 位图字体所需的字符集（symbols），输出到 stdout。

字符集组成：
  1. ASCII 可见字符 0x20-0x7E
  2. GB2312 一级汉字（0xB0A1-0xD7F9，共 3755 字）—— 覆盖日常用语
  3. 中文标点与常用符号（GB2312 一级区外的那些）
  4. **应用源码里实际用到的所有非 ASCII 字符**（关键！）

第 4 项是踩过坑加的：GB2312 一级字库按拼音排序，收录的是常用字，
"阈"（久坐阈值）、"「」（用药页提示）这类字不在里面，
字库生成时看不出来，烧到板子上才显示成方框。
所以直接扫源码，用到的字一个都不能少。

用法：
  python3 tools/gen_font_charset.py            # 输出字符集（一行）
  python3 tools/gen_font_charset.py --stats    # 只打印统计
  python3 tools/gen_font_charset.py -o chars.txt
"""

import os
import sys

# 中文标点/常用符号（GB2312 符号区里常用的 + 全角标点）
PUNCT = "，。、；：？！“”‘’（）《》〈〉【】「」『』〔〕—…·～％＋－×÷＝"
EXTRA = "℃℉°±µΩ√≈≠≤≥→←↑↓★☆●○■□▲▼◆◇§¥$€£№"
MISC = "ΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩαβγδεζηθικλμνξοπρστυφχψω"

# 扫描源码的目录（相对仓库根）
APP_DIRS = ["app/silver_guardian_hub"]


def gb2312_level1() -> set:
    """GB2312 一级汉字：区 0xB0-0xD7，位 0xA1-0xFE"""
    chars = set()
    for hi in range(0xB0, 0xD8):
        for lo in range(0xA1, 0xFF):
            try:
                chars.add(bytes([hi, lo]).decode("gb2312"))
            except UnicodeDecodeError:
                continue
    return chars


def scan_sources(repo_root: str) -> dict:
    """
    扫描应用源码，返回 {字符: {文件名}}。
    只扫 .c/.h，跳过生成的字库目录（那里面全是位图数据，不是界面文案）。
    """
    found = {}
    for rel in APP_DIRS:
        base = os.path.join(repo_root, rel)
        if not os.path.isdir(base):
            continue
        for root, dirs, files in os.walk(base):
            if os.sep + "fonts" in root:
                continue
            for fn in files:
                if not fn.endswith((".c", ".h")):
                    continue
                path = os.path.join(root, fn)
                try:
                    with open(path, encoding="utf-8") as f:
                        src = f.read()
                except (OSError, UnicodeDecodeError):
                    continue
                for ch in src:
                    if ord(ch) > 0x2000 and ch.isprintable():
                        found.setdefault(ch, set()).add(fn)
    return found


def build(repo_root: str = None, verbose: bool = False) -> str:
    if repo_root is None:
        # tools/ 的上一级就是仓库根
        repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    chars = set()

    for c in range(0x20, 0x7F):
        chars.add(chr(c))

    chars |= gb2312_level1()
    chars.update(PUNCT)
    chars.update(EXTRA)
    chars.update(MISC)

    from_source = scan_sources(repo_root)
    extra = {c for c in from_source if c not in chars}
    chars |= from_source.keys()

    if verbose:
        print(f"  GB2312 一级 + ASCII + 标点 = {len(chars) - len(extra)} 字符",
              file=sys.stderr)
        print(f"  源码补充（不在上面集合里的）= {len(extra)} 字符", file=sys.stderr)
        for c in sorted(extra):
            print(f"    {c!r} U+{ord(c):04X}  <- {', '.join(sorted(from_source[c]))}",
                  file=sys.stderr)

    return "".join(sorted(c for c in chars if c.isprintable()))


if __name__ == "__main__":
    verbose = "--stats" in sys.argv or "--verbose" in sys.argv
    s = build(verbose=verbose)

    if "--stats" in sys.argv or "--verbose" in sys.argv:
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
