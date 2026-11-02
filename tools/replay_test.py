# -*- coding: utf-8 -*-
"""复盘回归测试：真机打一局 → 录像落盘 → 用录侧重跑 → 比对结算数据。

只看"能不能放"是不够的：这条链路真正要守住的是**复盘和当时是同一局**
（时间/得分/击毁一致），否则说明有非确定性漏进了 World。

用法：python tools/replay_test.py      （需要先 build.bat）
"""
import glob
import os
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import live_test as lt  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, 'build', 'SkyStriker.exe')
WM_KEYDOWN, WM_KEYUP, WM_CLOSE = 0x0100, 0x0101, 0x0010
ARROWS = {0x25, 0x26, 0x27, 0x28}
u = lt.user32


def _lp(vk, up=False):
    sc = u.MapVirtualKeyW(vk, 0) & 0xFF
    lp = 1 | (sc << 16) | ((1 << 24) if vk in ARROWS else 0)
    if up:
        lp |= (1 << 30) | (1 << 31)
    return lp


def kd(hwnd, vk):
    u.PostMessageW(hwnd, WM_KEYDOWN, vk, _lp(vk))


def ku(hwnd, vk):
    u.PostMessageW(hwnd, WM_KEYUP, vk, _lp(vk, True))


def newest_rep():
    fs = sorted(glob.glob(os.path.join(ROOT, 'replays', '*.rep')))
    return fs[-1] if fs else None


def read_head(path):
    """头 58 字节（v2 含 seed），字段顺序见 src/game/Replay.hpp"""
    with open(path, 'rb') as f:
        b = f.read(58)
    assert b[:6] == b'SKYRPL', 'magic mismatch'
    score, kills, combo, stars = struct.unpack_from('<iiii', b, 16)
    (tf,) = struct.unpack_from('<f', b, 32)
    (seed,) = struct.unpack_from('<I', b, 50)
    (count,) = struct.unpack_from('<I', b, 54)
    return dict(score=score, kills=kills, combo=combo, stars=stars, time=tf, seed=seed, frames=count,
                win=b[12])


def launch():
    """高 DPI / 沙箱环境下偶发"启动即退出"，多试几次"""
    for _ in range(5):
        p = subprocess.Popen([EXE, '--screen=game', '--force-fail=14', '--debug-title'],
                             cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        hwnd = lt.find_window(20)
        time.sleep(3.0)
        if hwnd and p.poll() is None and 'game' in lt.window_title(hwnd):
            u.ShowWindow(hwnd, 5)
            u.SetForegroundWindow(hwnd)
            return p, hwnd
        if p.poll() is None:
            p.kill()
    return None, 0


def main():
    if not os.path.exists(EXE):
        print('找不到 %s，先跑 build.bat' % EXE)
        return 1
    before = newest_rep()
    p, hwnd = launch()
    if not hwnd:
        print('FAIL: 游戏起不来（连续多次启动即退出）')
        return 1
    time.sleep(0.6)

    # 一场有操作的短局：一直开火，位置也动一动
    kd(hwnd, 0x4A)  # J
    for vk, dur in ((0x27, 2.5), (0x26, 1.0), (0x28, 1.5), (0x25, 2.5), (0x27, 2.0)):
        kd(hwnd, vk)
        time.sleep(dur)
        ku(hwnd, vk)
    ku(hwnd, 0x4A)
    time.sleep(9.0)  # 等第 14 秒的强制阵亡 + 终局演出 + 录像落盘
    u.PostMessageW(hwnd, WM_CLOSE, 0, 0)
    try:
        p.wait(timeout=10)
    except subprocess.TimeoutExpired:
        p.kill()

    rep = newest_rep()
    if not rep or rep == before:
        print('FAIL: 没有生成新录像')
        return 1
    meta = read_head(rep)
    print('[file] %s  得分=%d 击毁=%d 时长=%.2fs 帧数=%d' %
          (os.path.relpath(rep, ROOT), meta['score'], meta['kills'], meta['time'], meta['frames']))

    out = subprocess.run([EXE, '--screen=replay', '--state-log',
                          '--warp=%.1f' % (meta['time'] + 3.0), '--shot=shots/_rp_cmp.png'],
                         cwd=ROOT, capture_output=True, text=True)
    m = None
    for line in out.stdout.splitlines():
        if line.startswith('[replay]'):
            m = re.search(r'rerun t=([\d.]+) score=(-?\d+) kills=(-?\d+) \| rec t=([\d.]+) score=(-?\d+) kills=(-?\d+)',
                          line)
            if m:
                print('[rerun] ' + line[len('[replay] '):])
                break
    if not m:
        print('FAIL: 复盘没跑到结束')
        print(out.stdout[-600:])
        return 1

    rt, rs, rk, ct, cs, ck = m.groups()
    ok = (rs, rk) == (cs, ck) and abs(float(rt) - float(ct)) < 0.005
    print('PASS: 复盘与记录一致（得分/击毁/时长）' if ok else 'FAIL: 复盘结果和记录不一致')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
