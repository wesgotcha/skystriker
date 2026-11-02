# -*- coding: utf-8 -*-
"""把模型素材预处理成运行时更快的形态（幂等，可重复跑，缺了也能跑）。

为什么要它：启动时 11 架机体一共要 1.8 秒，其中每一架都是两份 2048² 贴图在拖时间。

  1) glTF 内嵌贴图：每架模型的 .gltf 里塞了一张 2048² 的 PNG（bufferView 形式）。
     运行时根本用不到 —— 代码总会把材质换成 assets 里的 5 套配色。
     但 raylib 的 LoadModel 照旧解码 + 上传显存 + 再被我们丢掉，单架约 80ms。
     处理：删掉 images / textures / samplers 和材质里的贴图引用，
           生成 <名字>_mesh.gltf（网格数据原样保留）。

  2) 配色贴图：Textures/*.png 是 2048²，运行时只用 512²（Boss 1024²）。
     LoadSkin 每次都要解码 2048² 再缩到 512²，单架约 80ms。
     处理：预生成 Textures/opt/ 下目标尺寸的版本。

产物都是派生文件（.gitignore 已忽略）。删掉它们游戏照样能跑，只是回到慢速路径。

用法:
    python tools/prep_assets.py          # 增量生成（已是最新就跳过）
    python tools/prep_assets.py --force  # 全部重做
"""
import json
import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODELS = os.path.join(ROOT, 'models')

# 贴图键名：glTF 里所有指向 textures[] 的字段都以 Texture 结尾
TEX_KEYS = ('normalTexture', 'occlusionTexture', 'emissiveTexture')


def fresh(src, dst, force):
    if force or not os.path.exists(dst):
        return True
    return os.path.getmtime(dst) < os.path.getmtime(src)


def strip_gltf(src, dst):
    with open(src, 'r', encoding='utf-8') as f:
        d = json.load(f)

    removed_tex = len(d.get('textures', []))
    for k in ('images', 'textures', 'samplers'):
        d.pop(k, None)

    for mat in d.get('materials', []):
        pbr = mat.get('pbrMetallicRoughness')
        if isinstance(pbr, dict):
            for k in [k for k in pbr if k.endswith('Texture')]:
                pbr.pop(k)
        for k in TEX_KEYS:
            mat.pop(k, None)
        # 扩展里也可能挂贴图（KHR_materials_*），一并清掉
        for ext in (mat.get('extensions') or {}).values():
            if isinstance(ext, dict):
                for k in [k for k in ext if k.endswith('Texture')]:
                    ext.pop(k)

    # 紧凑写：少几百 KB，cgltf 解析也快一点
    with open(dst, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(d, f, separators=(',', ':'), ensure_ascii=False)
    return removed_tex


def skin_target(path):
    # Boss 用 1024²，其余机体 512²（与 Assets.cpp 里的 skinSize 对应）
    return 1024 if os.sep + 'boss' + os.sep in path else 512


def main():
    force = '--force' in sys.argv
    if not os.path.isdir(MODELS):
        print('[prep] 找不到 models/，跳过')
        return 0

    n_gltf = n_skin = n_skip = 0
    for group in sorted(os.listdir(MODELS)):
        gdir = os.path.join(MODELS, group)
        if not os.path.isdir(gdir):
            continue
        for name in sorted(os.listdir(gdir)):
            mdir = os.path.join(gdir, name)
            if not os.path.isdir(mdir):
                continue

            # ---- glTF ----
            gltf_dir = os.path.join(mdir, 'glTF')
            if os.path.isdir(gltf_dir):
                for fn in sorted(os.listdir(gltf_dir)):
                    if not fn.endswith('.gltf') or fn.endswith('_mesh.gltf'):
                        continue
                    src = os.path.join(gltf_dir, fn)
                    dst = os.path.join(gltf_dir, fn[:-5] + '_mesh.gltf')
                    if not fresh(src, dst, force):
                        n_skip += 1
                    else:
                        strip_gltf(src, dst)
                        n_gltf += 1

            # ---- 配色贴图 ----
            tex_dir = os.path.join(mdir, 'Textures')
            if not os.path.isdir(tex_dir):
                continue
            out_dir = os.path.join(tex_dir, 'opt')
            for fn in sorted(os.listdir(tex_dir)):
                if not fn.lower().endswith('.png'):
                    continue
                src = os.path.join(tex_dir, fn)
                dst = os.path.join(out_dir, fn)
                if not fresh(src, dst, force):
                    n_skip += 1
                    continue
                os.makedirs(out_dir, exist_ok=True)
                size = skin_target(src)
                with Image.open(src) as im:
                    im = im.convert('RGBA')
                    if im.size != (size, size):
                        im = im.resize((size, size), Image.LANCZOS)
                    im.save(dst, optimize=True)
                n_skin += 1

    print('[prep] gltf=%d skins=%d skipped=%d' % (n_gltf, n_skin, n_skip))
    return 0


if __name__ == '__main__':
    sys.exit(main())
