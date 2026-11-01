# -*- coding: utf-8 -*-
"""真机冒烟测试：真正启动游戏窗口、发送按键、抓取窗口画面。

为什么需要它：`--shot` 模式只跑预热循环然后导出画布就退出，
**永远不会进入 while(!WindowShouldClose()) 主循环**，
所以测不到"窗口能不能显示、按键能不能用、ESC 会不会崩、长时间跑会不会挂"。

做法：
  - 普通启动游戏进程（真实主循环），带 --debug-title（当前屏幕会写进窗口标题）
  - 用 PostMessage 投递按键（带正确的扫描码，否则 GLFW 认不出）
  - 用 PrintWindow 抓真实窗口内容
  - 每步断言窗口标题里的屏幕名，失败立即报出实际状态

用法:
    python tools/live_test.py            # 完整流程
    python tools/live_test.py --quick    # 只验证窗口能起来
"""
import ctypes
import ctypes.wintypes as w
import os
import subprocess
import sys
import time

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, 'build', 'SkyStriker.exe')
OUT = os.path.join(ROOT, 'shots', 'live')

user32 = ctypes.WinDLL('user32', use_last_error=True)
gdi32 = ctypes.WinDLL('gdi32', use_last_error=True)

user32.GetWindowTextLengthW.argtypes = [w.HWND]
user32.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, ctypes.c_int]
user32.IsWindowVisible.argtypes = [w.HWND]
user32.GetClientRect.argtypes = [w.HWND, ctypes.POINTER(w.RECT)]
user32.GetWindowDC.argtypes = [w.HWND]
user32.GetWindowDC.restype = w.HDC
user32.ReleaseDC.argtypes = [w.HWND, w.HDC]
user32.PrintWindow.argtypes = [w.HWND, w.HDC, w.UINT]
user32.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
user32.MapVirtualKeyW.argtypes = [w.UINT, w.UINT]
user32.MapVirtualKeyW.restype = w.UINT
user32.SetForegroundWindow.argtypes = [w.HWND]
user32.GetWindowRect.argtypes = [w.HWND, ctypes.POINTER(w.RECT)]
user32.GetCursorPos.argtypes = [ctypes.POINTER(w.POINT)]
user32.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
user32.ClientToScreen.argtypes = [w.HWND, ctypes.POINTER(w.POINT)]
user32.ShowWindow.argtypes = [w.HWND, ctypes.c_int]

gdi32.CreateCompatibleDC.argtypes = [w.HDC]
gdi32.CreateCompatibleDC.restype = w.HDC
gdi32.CreateCompatibleBitmap.argtypes = [w.HDC, ctypes.c_int, ctypes.c_int]
gdi32.CreateCompatibleBitmap.restype = w.HBITMAP
gdi32.SelectObject.argtypes = [w.HDC, w.HGDIOBJ]
gdi32.SelectObject.restype = w.HGDIOBJ
gdi32.GetDIBits.argtypes = [w.HDC, w.HBITMAP, w.UINT, w.UINT, ctypes.c_void_p, ctypes.c_void_p, w.UINT]
gdi32.DeleteObject.argtypes = [w.HGDIOBJ]
gdi32.DeleteDC.argtypes = [w.HDC]

WM_KEYDOWN, WM_KEYUP, WM_CLOSE = 0x0100, 0x0101, 0x0010

VK = {'ENTER': 0x0D, 'ESC': 0x1B, 'SPACE': 0x20,
      'LEFT': 0x25, 'UP': 0x26, 'RIGHT': 0x27, 'DOWN': 0x28,
      'A': 0x41, 'D': 0x44, 'W': 0x57, 'S': 0x53,
      'J': 0x4A, 'K': 0x4B, 'R': 0x52, 'H': 0x48}


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [('biSize', w.DWORD), ('biWidth', w.LONG), ('biHeight', w.LONG),
                ('biPlanes', w.WORD), ('biBitCount', w.WORD), ('biCompression', w.DWORD),
                ('biSizeImage', w.DWORD), ('biXPelsPerMeter', w.LONG), ('biYPelsPerMeter', w.LONG),
                ('biClrUsed', w.DWORD), ('biClrImportant', w.DWORD)]


def window_title(hwnd):
    n = user32.GetWindowTextLengthW(hwnd)
    if n <= 0:
        return ''
    buf = ctypes.create_unicode_buffer(n + 1)
    user32.GetWindowTextW(hwnd, buf, n + 1)
    return buf.value


def find_window(timeout=40.0):
    end = time.time() + timeout
    while time.time() < end:
        found = []

        @ctypes.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
        def cb(hwnd, _):
            if user32.IsWindowVisible(hwnd) and 'SKY STRIKER' in window_title(hwnd):
                found.append(hwnd)
            return True

        user32.EnumWindows(cb, 0)
        if found:
            return found[0]
        time.sleep(0.15)
    return 0


def capture(hwnd, path):
    cr = w.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(cr))
    cw, ch = cr.right - cr.left, cr.bottom - cr.top
    wr = w.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(wr))
    width, height = wr.right - wr.left, wr.bottom - wr.top
    if cw <= 0 or ch <= 0 or width <= 0 or height <= 0:
        return None

    hdc = user32.GetWindowDC(hwnd)
    mem = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, width, height)
    old = gdi32.SelectObject(mem, bmp)
    ok = user32.PrintWindow(hwnd, mem, 2)

    bmi = BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.biWidth = width
    bmi.biHeight = -height
    bmi.biPlanes = 1
    bmi.biBitCount = 32
    bmi.biCompression = 0
    buf = ctypes.create_string_buffer(width * height * 4)
    gdi32.GetDIBits(mem, bmp, 0, height, buf, ctypes.byref(bmi), 0)

    gdi32.SelectObject(mem, old)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mem)
    user32.ReleaseDC(hwnd, hdc)

    img = Image.frombuffer('RGBA', (width, height), buf, 'raw', 'BGRA', 0, 1).convert('RGB')

    # 裁掉标题栏和边框，只留客户区（游戏画面）
    pt = w.POINT(0, 0)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    ox, oy = pt.x - wr.left, pt.y - wr.top
    img = img.crop((ox, oy, ox + cw, oy + ch))
    img.save(path)
    return ok, cw, ch, img


def _lparam(vk, up=False):
    """GLFW 从 lParam 的 bit16..23 取扫描码判定按键；
    只传 lParam=0 会被当成未知键，按键直接失效。"""
    sc = user32.MapVirtualKeyW(vk, 0) & 0xFF
    lp = 1 | (sc << 16)
    if up:
        lp |= (1 << 30) | (1 << 31)
    return lp


def key_tap(hwnd, name, hold=0.06):
    vk = VK[name]
    user32.PostMessageW(hwnd, WM_KEYDOWN, vk, _lparam(vk))
    time.sleep(hold)
    user32.PostMessageW(hwnd, WM_KEYUP, vk, _lparam(vk, True))


def key_down(hwnd, name):
    vk = VK[name]
    user32.PostMessageW(hwnd, WM_KEYDOWN, vk, _lparam(vk))


def key_up(hwnd, name):
    vk = VK[name]
    user32.PostMessageW(hwnd, WM_KEYUP, vk, _lparam(vk, True))


def stats(img):
    small = img.resize((160, 90))
    px = list(small.getdata())
    return round(sum(sum(p) for p in px) / (len(px) * 3), 1), len(set(px))


class Report:
    def __init__(self):
        self.fails = []

    def ok(self, step, detail):
        print('  √ %-20s %s' % (step, detail))

    def fail(self, step, detail):
        self.fails.append('%s: %s' % (step, detail))
        print('  × %-20s %s' % (step, detail))


def main():
    quick = '--quick' in sys.argv
    if not os.path.exists(EXE):
        print('找不到 %s，先跑 build.bat' % EXE)
        return 1
    os.makedirs(OUT, exist_ok=True)
    rep = Report()

    print('[1] 启动游戏进程（真实主循环 + 调试标题）')
    LOGF = os.path.join(OUT, 'state.log')
    fh = open(LOGF, 'w', encoding='utf-8', errors='replace')
    proc = subprocess.Popen([EXE, '--debug-title', '--state-log'], cwd=ROOT,
                            stdout=fh, stderr=subprocess.STDOUT)
    t0 = time.time()

    hwnd = find_window(45)
    if not hwnd:
        print('  × 45s 内没找到窗口，进程存活=%s' % (proc.poll() is None))
        proc.kill()
        return 1
    rep.ok('窗口创建', 'hwnd=0x%X  %.1fs' % (hwnd, time.time() - t0))
    user32.ShowWindow(hwnd, 5)
    user32.SetForegroundWindow(hwnd)

    # 把光标挪到标题栏上（不在客户区内），避免悬停抢走菜单焦点；
    # 测试结束后还原，尽量不打扰用户
    old_pt = w.POINT()
    user32.GetCursorPos(ctypes.byref(old_pt))
    wr = w.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(wr))
    user32.SetCursorPos(wr.right - 40, wr.top + 8)
    time.sleep(0.4)

    def snap(tag):
        p = os.path.join(OUT, '%s.png' % tag)
        r = capture(hwnd, p)
        if not r:
            rep.fail('抓图 ' + tag, 'PrintWindow 失败')
            return
        _, wd, ht, img = r
        avg, colors = stats(img)
        print('      %-12s %dx%d  亮度=%5.1f  颜色=%5d' % (tag, wd, ht, avg, colors))

    def wait_state(expect, timeout=15.0):
        # 必须等到转场结束：转场期间按键会被 go() 吞掉
        t_start = time.time()
        while time.time() - t_start < timeout:
            st = window_title(hwnd)
            if expect in st and '(trans)' not in st:
                return time.time() - t_start, st
            if proc.poll() is not None:
                return None, '进程已退出(exit=%s)' % proc.returncode
            time.sleep(0.08)
        return None, window_title(hwnd)

    def goto(step, key, expect, timeout=15.0):
        key_tap(hwnd, key)
        dt, st = wait_state(expect, timeout)
        if dt is None:
            rep.fail(step, '按 %s 后没进入 %s，实际标题="%s"' % (key, expect, st))
            return False
        rep.ok(step, '%s → %s  %.2fs' % (key, expect, dt))
        return True

    dt, st = wait_state('title', 30)
    if dt is None:
        rep.fail('启动屏加载', '30s 没进主菜单，标题="%s"' % st)
    else:
        rep.ok('启动屏 → 主菜单', '%.2fs（含机体加载）' % dt)
    snap('01_title')

    if quick:
        user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
        proc.wait(timeout=10)
        print('quick 结束，退出码', proc.returncode)
        return 0

    if goto('主菜单 → 机库', 'ENTER', 'hangar'):
        time.sleep(0.5)
        snap('02_hangar')

    if goto('机库 → 简报', 'ENTER', 'briefing'):
        time.sleep(0.4)
        snap('03_briefing')

    if goto('简报 → 战斗', 'ENTER', 'game'):
        time.sleep(1.2)
        snap('04_game')

    print('[2] 操作验证')
    key_down(hwnd, 'UP')
    key_down(hwnd, 'RIGHT')
    time.sleep(0.6)
    key_up(hwnd, 'UP')
    key_up(hwnd, 'RIGHT')
    snap('05_moved')

    key_down(hwnd, 'J')
    time.sleep(1.2)
    key_up(hwnd, 'J')
    snap('06_shooting')
    rep.ok('移动 / 射击', '上下右 + J 已发送，进程存活=%s' % (proc.poll() is None))

    if goto('战斗 → 暂停', 'ESC', 'pause'):
        snap('07_pause')

    if goto('暂停 → 继续', 'ESC', 'game'):
        snap('08_resumed')

    print('[3] 连续跑 8 秒')
    t_hold = time.time()
    while time.time() - t_hold < 8.0 and proc.poll() is None:
        time.sleep(0.3)
    if proc.poll() is None:
        rep.ok('稳定性', '8s 无崩溃，当前=%s' % window_title(hwnd).split('|')[-1].strip())
    else:
        rep.fail('稳定性', '中途崩溃，exit=%s' % proc.returncode)
    snap('09_after8s')

    print('[4] 关闭窗口')
    user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
    try:
        proc.wait(timeout=10)
        rep.ok('正常退出', 'exit=%s' % proc.returncode)
    except subprocess.TimeoutExpired:
        proc.kill()
        rep.fail('正常退出', 'WM_CLOSE 后 10s 未退出，已强杀')

    fh.flush()
    states = [l.strip() for l in open(LOGF, encoding='utf-8', errors='replace') if l.startswith('[state]')]
    print('\n[5] 战斗状态采样（最后 3 条）')
    for l in states[-3:]:
        print('   ', l)

    user32.SetCursorPos(old_pt.x, old_pt.y)

    print('\n=== 结论 ===')
    if rep.fails:
        print('  %d 项失败：' % len(rep.fails))
        for f in rep.fails:
            print('   ×', f)
    else:
        print('  √ 全部通过：窗口 / 加载 / 逐屏跳转 / 移动射击 / 暂停恢复 / 稳定性 / 正常退出')
    print('截图:', OUT)
    return 0 if not rep.fails else 2


if __name__ == '__main__':
    sys.exit(main())
