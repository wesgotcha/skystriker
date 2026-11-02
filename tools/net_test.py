# -*- coding: utf-8 -*-
"""联机（局域网）真机测试：本机开两个进程，主机 + 客户端直连 127.0.0.1。

测什么：
  - 主机开房、客户端连接、双方进入战斗（锁步对上第一帧）
  - 两端模拟结果一致（血量 / 波次 / 敌机数不会分叉）
  - ESC 暂停是否同步给对方，再按一次是否同步取消
  - 对端掉线后本机是否弹出"连接断开"

用法:
    python tools/net_test.py                # 合作模式，全套流程
    python tools/net_test.py --mode=versus  # 对战模式
    python tools/net_test.py --quick        # 只验证连上并进入战斗

注意：跑之前会先杀掉残留的 SkyStriker.exe（联机端口是独占的）。
"""
import ctypes
import ctypes.wintypes as w
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, 'build', 'SkyStriker.exe')
OUT = os.path.join(ROOT, 'shots', 'live')

user32 = ctypes.WinDLL('user32', use_last_error=True)
user32.SetForegroundWindow.argtypes = [w.HWND]
user32.ShowWindow.argtypes = [w.HWND, ctypes.c_int]
user32.MapVirtualKeyW.argtypes = [ctypes.c_uint, ctypes.c_uint]
user32.MapVirtualKeyW.restype = ctypes.c_uint
user32.GetWindowThreadProcessId.argtypes = [w.HWND, ctypes.POINTER(w.DWORD)]
user32.PostMessageW.argtypes = [w.HWND, ctypes.c_uint, w.WPARAM, w.LPARAM]

WM_KEYDOWN, WM_KEYUP = 0x0100, 0x0101


def lparam(vk, up=False):
    """GLFW 从 lParam 的 bit16..23 取扫描码，没填就被当成未知键。"""
    sc = user32.MapVirtualKeyW(vk, 0) & 0xFF
    lp = 1 | (sc << 16)
    if up:
        lp |= (1 << 30) | (1 << 31)
    return lp


def key_tap(hwnd, vk, hold=0.15):
    user32.PostMessageW(hwnd, WM_KEYDOWN, vk, lparam(vk))
    time.sleep(hold)
    user32.PostMessageW(hwnd, WM_KEYUP, vk, lparam(vk, True))


def windows_by_pid():
    out = {}

    @ctypes.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
    def cb(hwnd, _):
        pid = w.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        n = user32.GetWindowTextLengthW(hwnd)
        if n <= 0:
            return True
        buf = ctypes.create_unicode_buffer(n + 1)
        user32.GetWindowTextW(hwnd, buf, n + 1)
        if 'SKY STRIKER' in buf.value:
            out[pid.value] = (hwnd, buf.value)
        return True

    user32.EnumWindows(cb, 0)
    return out


def title_of(proc, wins):
    e = wins.get(proc.pid)
    return e[1] if e else ''


def kill(p):
    if p.poll() is None:
        subprocess.run(['taskkill', '/PID', str(p.pid), '/F'],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def parse_states(path):
    """取 [state] 行里的关键字段，方便两端对比。"""
    rows = []
    try:
        text = open(path, encoding='utf-8', errors='replace').read()
    except IOError:
        return rows
    for line in text.splitlines():
        if '[state]' not in line:
            continue
        d = {}
        for kv in line.split():
            if '=' in kv:
                k, v = kv.split('=', 1)
                d[k] = v
        rows.append(d)
    return rows


def main():
    mode = 'coop'
    quick = False
    for a in sys.argv[1:]:
        if a.startswith('--mode='):
            mode = a.split('=', 1)[1].strip('"')
        elif a == '--quick':
            quick = True

    os.makedirs(OUT, exist_ok=True)
    subprocess.run(['taskkill', '/IM', 'SkyStriker.exe', '/F'],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1)

    base = [EXE, '--screen=lobby', '--mp=' + mode, '--autostart', '--debug-title',
            '--state-log', '--size=960x540']
    host_log = os.path.join(OUT, 'net_host.log')
    client_log = os.path.join(OUT, 'net_client.log')
    hf = open(host_log, 'w')
    cf = open(client_log, 'w')

    host = subprocess.Popen(base + ['--host'], cwd=ROOT, stdout=hf, stderr=subprocess.STDOUT)
    time.sleep(2)
    client = subprocess.Popen(base + ['--join=127.0.0.1'], cwd=ROOT, stdout=cf,
                              stderr=subprocess.STDOUT)

    fails = []

    def ok(tag, msg):
        print('  v %-16s %s' % (tag, msg))

    def bad(tag, msg):
        print('  x %-16s %s' % (tag, msg))
        fails.append(tag)

    print('[1] 连接并进入战斗')
    joined = False
    end = time.time() + 25
    while time.time() < end:
        wins = windows_by_pid()
        if 'game' in title_of(host, wins) and 'game' in title_of(client, wins):
            joined = True
            break
        time.sleep(0.2)
    if joined:
        ok('进入战斗', '主机 + 客户端都已开打')
    else:
        bad('进入战斗', '主机=%r 客户端=%r' % (title_of(host, windows_by_pid()),
                                        title_of(client, windows_by_pid())))

    if not joined or quick:
        kill(host)
        kill(client)
        hf.close()
        cf.close()
        print('=== 结论 ===')
        print('  ' + ('全部通过' if not fails else '%d 项失败' % len(fails)))
        return 1 if fails else 0

    print('[2] 锁步一致性')
    time.sleep(10)
    hs = parse_states(host_log)
    cs = parse_states(client_log)
    if len(hs) < 3 or len(cs) < 3:
        bad('状态采样', 'host=%d 条 client=%d 条' % (len(hs), len(cs)))
    else:
        a, b = hs[-1], cs[-1]
        diff = [k for k in ('wave', 'phase', 'hp', 'hp2', 'enemies', 'versus', 'winner')
                if a.get(k) != b.get(k)]
        if diff:
            bad('状态一致', '字段分叉: %s (host=%s client=%s)' %
                (diff, [a.get(k) for k in diff], [b.get(k) for k in diff]))
        else:
            ok('状态一致', 'wave=%s phase=%s hp2=%s' %
               (a.get('wave'), a.get('phase'), a.get('hp2')))

    print('[3] 暂停同步')
    wins = windows_by_pid()
    hw = wins[host.pid][0]
    user32.ShowWindow(hw, 5)
    user32.SetForegroundWindow(hw)
    key_tap(hw, 0x1B)  # ESC
    time.sleep(1.0)
    t1 = (title_of(host, windows_by_pid()), title_of(client, windows_by_pid()))
    if 'pause' in t1[0] and 'pause' in t1[1]:
        ok('暂停同步', '两端都进入暂停')
    else:
        bad('暂停同步', 'host=%r client=%r' % t1)
    key_tap(hw, 0x1B)
    time.sleep(1.0)
    t2 = (title_of(host, windows_by_pid()), title_of(client, windows_by_pid()))
    if 'pause' not in t2[0] and 'pause' not in t2[1]:
        ok('恢复同步', '两端都回到战斗')
    else:
        bad('恢复同步', 'host=%r client=%r' % t2)

    print('[4] 掉线处理')
    kill(client)
    got = False
    end = time.time() + 12
    while time.time() < end:
        if 'duel' in title_of(host, windows_by_pid()):
            got = True
            break
        time.sleep(0.3)
    if got:
        ok('对端掉线', '本机弹出结算/断开提示')
    else:
        bad('对端掉线', '实际标题=%r' % title_of(host, windows_by_pid()))

    kill(host)
    hf.close()
    cf.close()

    print('\n=== 结论 ===')
    if fails:
        print('  %d 项失败：%s' % (len(fails), ' / '.join(fails)))
    else:
        print('  全部通过：连接 / 一致性 / 暂停同步 / 掉线')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
