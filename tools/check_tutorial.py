# -*- coding: utf-8 -*-
"""校验脚本: 检查生成的教程文档结构 + 嵌入源码与工程一致性。

用法:
    python tools/check_tutorial.py docs/裸机教程.md            # 常规结构校验
    python tools/check_tutorial.py docs/裸机教程.md extract     # 额外提取 build/burn 脚本供语法解析
    python tools/check_tutorial.py docs/裸机教程-CSDN导入版.md csdn  # CSDN 版校验(无围栏 + 缩进代码块一致性)
"""
import io
import os
import re
import sys
from collections import Counter


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def extract_embedded(doc_text):
    """返回 {源码相对路径: 嵌入正文}。只处理 ``` 围栏型嵌入块。"""
    out = {}
    lines = doc_text.split("\n")
    i = 0
    while i < len(lines):
        m = re.match(r"^#### 完整文件：(.+)$", lines[i])
        if m:
            path = m.group(1).strip()
            j = i + 1
            while j < len(lines) and lines[j].strip() == "":
                j += 1
            if j < len(lines) and lines[j].strip().startswith("```"):
                lang = lines[j].strip()[3:]
                j += 1
                block = []
                while j < len(lines) and lines[j].strip() != "```":
                    block.append(lines[j])
                    j += 1
                out[path] = (lang, "\n".join(block))
                i = j + 1
                continue
        i += 1
    return out


def extract_indented(doc_text):
    """返回 {源码相对路径: 嵌入正文}。只处理 #### 完整文件 后的 4 空格缩进块。"""
    out = {}
    lines = doc_text.split("\n")
    i = 0
    while i < len(lines):
        m = re.match(r"^#### 完整文件：(.+)$", lines[i])
        if m:
            path = m.group(1).strip()
            j = i + 1
            while j < len(lines) and lines[j].strip() == "":
                j += 1
            if j < len(lines) and re.match(r"^    ", lines[j]):
                block = []
                while j < len(lines) and re.match(r"^    ", lines[j]):
                    block.append(lines[j][4:])
                    j += 1
                out[path] = "\n".join(block)
                i = j
                continue
        i += 1
    return out


def check_integrity(doc_text):
    embedded = extract_embedded(doc_text)
    bad = []
    for path, (lang, block) in embedded.items():
        src_path = os.path.join(ROOT, path.replace("\\", os.sep))
        if not os.path.exists(src_path):
            bad.append("MISSING SRC: " + path)
            continue
        src = io.open(src_path, encoding="utf-8").read().rstrip("\n")
        expected = re.sub(r"(?m)^# +", "#", src)
        if block != expected:
            bad.append("MISMATCH: " + path)
    return embedded, bad


def check_integrity_indented(doc_text):
    embedded = extract_indented(doc_text)
    bad = []
    for path, block in embedded.items():
        src_path = os.path.join(ROOT, path.replace("\\", os.sep))
        if not os.path.exists(src_path):
            bad.append("MISSING SRC: " + path)
            continue
        src = io.open(src_path, encoding="utf-8").read().rstrip("\n")
        expected = re.sub(r"(?m)^# +", "#", src)
        if block != expected:
            bad.append("MISMATCH: " + path)
    return embedded, bad


def main():
    path = sys.argv[1]
    mode = sys.argv[2] if len(sys.argv) > 2 else "normal"
    if mode == "extract":
        extract_mode(path)
        return
    if mode == "csdn":
        csdn_check(path)
        return
    text = io.open(path, encoding="utf-8").read()
    lines = text.split("\n")

    fence = False
    issues = []
    fence_count = 0
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        if fence:
            if line.strip() == "```":
                fence = False
            else:
                if re.match(r"^# ", line):
                    issues.append("FENCE line %d starts with '# ': %r" % (i + 1, line[:60]))
            i += 1
            continue
        if line.strip().startswith("```"):
            fence = True
            fence_count += 1
            i += 1
            continue
        if re.match(r"^    ", line):
            j = i
            while j < n and (lines[j].strip() == "" or re.match(r"^    ", lines[j])):
                if re.match(r"^    # ", lines[j]):
                    issues.append("INDENT line %d starts with '    # ': %r" % (j + 1, lines[j][:70]))
                j += 1
            i = j
            continue
        i += 1

    print("doc:", path)
    print("fence delimiters:", fence_count, "(should be even)")
    print("unterminated fence at EOF:", fence)
    print("issues inside code blocks:", len(issues))
    for it in issues[:60]:
        print(" -", it)

    headings = []
    for idx, line in enumerate(lines, 1):
        m = re.match(r"^(#{1,6}) (.+)$", line)
        if m:
            headings.append((idx, len(m.group(1)), m.group(2)))
    print("heading count:", len(headings))
    cnt = Counter(h[2] for h in headings)
    dups = {k: v for k, v in cnt.items() if v > 1}
    print("duplicate headings:", dups if dups else "none")

    anchors = re.findall(r"\]\(#([^)]+)\)", text)

    def slugify(h):
        s = h.lower()
        s = re.sub(r"[^\w\u4e00-\u9fff \-]", "", s)
        return s.replace(" ", "-")

    slugs = set()
    for _, lvl, h in headings:
        slugs.add(slugify(h))
    missing = [a for a in anchors if a not in slugs]
    print("TOC anchor count:", len(anchors), "missing:", missing if missing else "none")

    embedded, bad = check_integrity(text)
    print("embedded full-file blocks:", len(embedded))
    print("integrity mismatches:", len(bad))
    for b in bad[:40]:
        print(" -", b)


def csdn_check(path):
    text = io.open(path, encoding="utf-8").read()
    lines = text.split("\n")
    fences = [i + 1 for i, l in enumerate(lines) if l.strip().startswith("```")]
    print("doc:", path)
    print("lines starting with ``` :", len(fences), "(should be 0)")
    for f in fences[:20]:
        print(" - line", f, repr(lines[f - 1][:60]))

    headings = []
    for idx, line in enumerate(lines, 1):
        m = re.match(r"^(#{1,6}) (.+)$", line)
        if m:
            headings.append((idx, len(m.group(1)), m.group(2)))
    cnt = Counter(h[2] for h in headings)
    dups = {k: v for k, v in cnt.items() if v > 1}
    print("heading count:", len(headings), "duplicates:", dups if dups else "none")

    anchors = re.findall(r"\]\(#([^)]+)\)", text)

    def slugify(h):
        s = h.lower()
        s = re.sub(r"[^\w\u4e00-\u9fff \-]", "", s)
        return s.replace(" ", "-")

    slugs = set()
    for _, lvl, h in headings:
        slugs.add(slugify(h))
    missing = [a for a in anchors if a not in slugs]
    print("TOC anchor count:", len(anchors), "missing:", missing if missing else "none")

    embedded, bad = check_integrity_indented(text)
    print("embedded full-file blocks (indented):", len(embedded))
    print("integrity mismatches:", len(bad))
    for b in bad[:40]:
        print(" -", b)


def extract_mode(path):
    """把文档中嵌入的 build.ps1 / burn_sd.ps1 提取到临时目录, 供语法解析验证。"""
    import tempfile
    text = io.open(path, encoding="utf-8").read()
    embedded, _ = check_integrity(text)
    outdir = tempfile.mkdtemp(prefix="tut_check_")
    for name in ("tools/build.ps1", "tools/burn_sd.ps1"):
        lang, block = embedded[name]
        f = os.path.join(outdir, os.path.basename(name))
        if block.startswith("\ufeff"):
            block = block[1:]  # 源文件自带 BOM, 避免写出双重 BOM
        io.open(f, "w", encoding="utf-8-sig").write(block)
        print("EXTRACT", f, len(block))
    print("OUTDIR", outdir)


if __name__ == "__main__":
    main()
