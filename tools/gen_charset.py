#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""扫描 src/ 下所有「字符串字面量」里的中文，检查字体是否覆盖。
注释里的中文不参与检查（注释不上屏）。
用法: python tools/gen_charset.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, 'assets', 'fonts', 'SourceHanSansSC-Medium-subset.ttf')

try:
    from fontTools.ttLib import TTFont
except ImportError:
    print('[charset] 未安装 fontTools，跳过校验 (pip install fonttools)')
    sys.exit(0)

# 去掉块注释与行注释，再抓取字符串字面量
BLOCK = re.compile(r'/\*.*?\*/', re.S)
LINE = re.compile(r'//[^\n]*')
STR = re.compile(r'"((?:[^"\\\n]|\\.)*)"')


def font_codepoints(path):
    return set(TTFont(path).getBestCmap().keys())


def scan_sources():
    hits = {}
    for base, _, files in os.walk(os.path.join(ROOT, 'src')):
        for fn in files:
            if not fn.endswith(('.cpp', '.hpp')):
                continue
            p = os.path.join(base, fn)
            with open(p, encoding='utf-8') as f:
                text = f.read()
            text = BLOCK.sub(' ', text)
            text = LINE.sub(' ', text)
            for m in STR.finditer(text):
                for ch in m.group(1):
                    if ord(ch) > 0x2000:
                        hits.setdefault(ch, set()).add(os.path.relpath(p, ROOT))
    return hits


def main():
    cmap = font_codepoints(FONT)
    hits = scan_sources()
    missing = {c: v for c, v in hits.items() if ord(c) not in cmap}
    cjk = [c for c in hits if 0x4E00 <= ord(c) <= 0x9FFF]
    print(f'[charset] 上屏汉字 {len(cjk)} 个，字体覆盖 {len([c for c in cjk if ord(c) in cmap])} 个')
    if missing:
        print(f'[charset] 字体缺少 {len(missing)} 个字符:')
        for c, files in sorted(missing.items()):
            print(f'    {c!r} U+{ord(c):04X}  <- {sorted(files)[:2]}')
        sys.exit(1)
    print('[charset] 全部字符均已覆盖')


if __name__ == '__main__':
    main()
