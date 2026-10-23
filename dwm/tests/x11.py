#!/usr/bin/env python3
import ctypes as C
from ctypes.util import find_library
import os
from pathlib import Path
import select
import shutil
import subprocess
import tempfile
import time

project = Path(__file__).resolve().parents[1]
if not shutil.which('Xvfb'):
    raise SystemExit('x11 checks require Xvfb; install the xvfb package and rerun make check-x11')

x = C.CDLL(find_library('X11'))
test = C.CDLL(find_library('Xtst'))
D, W, I, U, L, P = C.c_void_p, C.c_ulong, C.c_int, C.c_uint, C.c_long, C.POINTER


def declare(lib, name, result, *args):
    fn = getattr(lib, name)
    fn.restype, fn.argtypes = result, args
    return fn


declare(x, 'XOpenDisplay', D, C.c_char_p)
declare(x, 'XDefaultRootWindow', W, D)
declare(x, 'XInternAtom', W, D, C.c_char_p, I)
declare(x, 'XGetWindowProperty', I, D, W, W, L, L, I, W, P(W), P(I), P(W), P(W), P(D))
declare(x, 'XCreateSimpleWindow', W, D, W, I, I, U, U, U, W, W)
declare(x, 'XStoreName', I, D, W, C.c_char_p)
declare(x, 'XSetWMProtocols', I, D, W, P(W), I)
declare(x, 'XSelectInput', I, D, W, L)
declare(x, 'XMapWindow', I, D, W)
declare(x, 'XDestroyWindow', I, D, W)
declare(x, 'XKillClient', I, D, W)
declare(x, 'XSendEvent', I, D, W, I, L, D)
declare(x, 'XGetGeometry', I, D, W, P(W), P(I), P(I), P(U), P(U), P(U), P(U))
declare(x, 'XGetInputFocus', I, D, P(W), P(I))
declare(x, 'XQueryTree', I, D, W, P(W), P(W), P(P(W)), P(U))
declare(x, 'XFree', I, D)
declare(x, 'XFlush', I, D)
declare(x, 'XSync', I, D, I)
declare(x, 'XKeysymToKeycode', C.c_ubyte, D, W)
declare(x, 'XCheckTypedWindowEvent', I, D, W, I, D)
declare(x, 'XGrabKeyboard', I, D, W, I, I, I, W)
declare(x, 'XUngrabKeyboard', I, D, W)
declare(x, 'XCloseDisplay', I, D)
declare(test, 'XTestFakeKeyEvent', I, D, U, I, W)
declare(test, 'XTestFakeButtonEvent', I, D, U, I, W)
declare(test, 'XTestFakeMotionEvent', I, D, I, I, I, W)


def wait_for(condition, message):
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        if condition():
            return
        time.sleep(0.02)
    raise AssertionError(message)


with tempfile.TemporaryDirectory(prefix='dwm-x11-') as directory:
    temporary = Path(directory)
    binpath = temporary / 'bin'
    binpath.mkdir()
    launches = temporary / 'launches'
    launcher = binpath / 'dmenu_run'
    launcher.write_text('#!/bin/sh\nprintf "launch\\n" >> "$LAUNCH_LOG"\nprintf "st\\nfirefox\\n" | dmenu "$@"\n')
    launcher.chmod(0o755)
    env = dict(os.environ, PATH=f'{binpath}:{project}:{project.parent / "dmenu"}:{project.parent / "tabbed"}:/usr/bin:/bin',
               LAUNCH_LOG=str(launches), XDG_STATE_HOME=str(temporary / 'state'))
    wm = server = None
    display = None
    readfd, writefd = os.pipe()
    try:
        with (temporary / 'xvfb.log').open('w+') as serverlog, (temporary / 'dwm.log').open('w+') as wmlog:
            server = subprocess.Popen(['Xvfb', '-displayfd', str(writefd), '-screen', '0', '1024x768x24',
                                       '-nolisten', 'tcp'], pass_fds=(writefd,), stdout=serverlog, stderr=serverlog)
            os.close(writefd)
            writefd = None
            if not select.select([readfd], [], [], 5)[0]:
                raise AssertionError('Xvfb did not start')
            number = os.read(readfd, 32).decode().strip()
            assert number.isdecimal(), 'Xvfb failed to allocate a display'
            env['DISPLAY'] = ':' + number
            display = x.XOpenDisplay(env['DISPLAY'].encode())
            assert display, 'cannot connect to the test display'
            root = x.XDefaultRootWindow(display)

            def prop(window, name):
                actual, fmt, count, remaining, data = W(), I(), W(), W(), D()
                x.XGetWindowProperty(display, window, x.XInternAtom(display, name.encode(), 0),
                                     0, 4096, 0, 0, C.byref(actual), C.byref(fmt), C.byref(count),
                                     C.byref(remaining), C.byref(data))
                if not data.value:
                    return []
                try:
                    if fmt.value == 8:
                        return C.string_at(data, count.value)
                    if fmt.value == 32:
                        return list(C.cast(data, P(W))[:count.value])
                    return []
                finally:
                    x.XFree(data)

            def menu_visible():
                parent, returned_root, children, count = W(), W(), P(W)(), U()
                x.XQueryTree(display, root, C.byref(returned_root), C.byref(parent), C.byref(children), C.byref(count))
                try:
                    return any(b'dmenu' in prop(children[i], 'WM_CLASS') for i in range(count.value))
                finally:
                    if children:
                        x.XFree(children)

            def parent_of(window):
                parent, returned_root, children, count = W(), W(), P(W)(), U()
                x.XQueryTree(display, window, C.byref(returned_root), C.byref(parent), C.byref(children), C.byref(count))
                if children:
                    x.XFree(children)
                return parent.value

            def geometry(window):
                returned_root, left, top = W(), I(), I()
                width, height, border, depth = U(), U(), U(), U()
                assert x.XGetGeometry(display, window, C.byref(returned_root),
                                      C.byref(left), C.byref(top), C.byref(width),
                                      C.byref(height), C.byref(border), C.byref(depth))
                return left.value, top.value, width.value, height.value, border.value

            def frame_geometry(window):
                left, top, width, height, border = geometry(window)
                extents = prop(window, '_NET_FRAME_EXTENTS')
                title = extents[2] - border if len(extents) == 4 else 0
                return left, top - title, width, height + title, border

            def tabs_fit(group, *tabs):
                _, _, width, height, _ = geometry(group)
                for tab in tabs:
                    left, top, tw, th, border = geometry(tab)
                    if left != 0 or tw != width or top + th != height or border != 0:
                        return False
                return True

            def focused_window():
                focused, revert = W(), I()
                x.XGetInputFocus(display, C.byref(focused), C.byref(revert))
                return focused.value

            def tab_group():
                for window in prop(root, '_NET_CLIENT_LIST'):
                    if b'dwm-tabs' in prop(window, 'WM_CLASS'):
                        return window
                return None

            def key(symbol, down):
                code = x.XKeysymToKeycode(display, symbol)
                assert code
                test.XTestFakeKeyEvent(display, code, down, 0)
                x.XFlush(display)
                time.sleep(0.04)

            def chord(*symbols):
                for symbol in symbols:
                    key(symbol, True)
                for symbol in reversed(symbols):
                    key(symbol, False)

            def drag(left, top, dx, dy):
                test.XTestFakeMotionEvent(display, 0, left, top, 0)
                x.XFlush(display)
                time.sleep(0.05)
                test.XTestFakeButtonEvent(display, 1, True, 0)
                x.XFlush(display)
                time.sleep(0.05)
                test.XTestFakeMotionEvent(display, 0, left + dx, top + dy, 0)
                x.XFlush(display)
                time.sleep(0.05)
                test.XTestFakeButtonEvent(display, 1, False, 0)
                x.XFlush(display)

            def window(name):
                win = x.XCreateSimpleWindow(display, root, 10, 10, 240, 160, 0, 0, 0)
                x.XStoreName(display, win, name.encode())
                protocol = W(x.XInternAtom(display, b'WM_DELETE_WINDOW', 0))
                x.XSetWMProtocols(display, win, C.byref(protocol), 1)
                x.XSelectInput(display, win, 1)
                x.XMapWindow(display, win)
                x.XFlush(display)
                return win

            wm = subprocess.Popen([project / 'dwm'], env=env, stdout=wmlog, stderr=wmlog)
            wait_for(lambda: prop(root, '_NET_SUPPORTING_WM_CHECK'), 'dwm startup stalled')
            first, second = window('first'), window('second')
            wait_for(lambda: len(prop(root, '_NET_CLIENT_LIST')) == 2, 'startup windows were not managed')
            wait_for(lambda: prop(root, '_NET_ACTIVE_WINDOW') == [second], 'startup focus stalled')
            wait_for(lambda: prop(second, '_NET_FRAME_EXTENTS') == [1, 1, 33, 1],
                     'ordinary window has no berry frame')
            chord(0xffeb, 0xffe3, ord(' '))
            wait_for(lambda: geometry(second)[4] == 1, 'floating border does not match tiled windows')
            for side in ('left', 'right', 'top', 'bottom'):
                left, top, width, height, border = frame_geometry(second)
                if side == 'left':
                    drag(left - 3, top + border + height // 2, 20, 0)
                    expected = (left + 20, top, width - 20, height, border)
                elif side == 'right':
                    drag(left + 2 * border + width + 3, top + border + height // 2, -20, 0)
                    expected = (left, top, width - 20, height, border)
                elif side == 'top':
                    drag(left + border + width // 2, top - 3, 0, 20)
                    expected = (left, top + 20, width, height - 20, border)
                else:
                    drag(left + border + width // 2, top + 2 * border + height + 3, 0, -20)
                    expected = (left, top, width, height - 20, border)
                wait_for(lambda: frame_geometry(second) == expected, f'{side} border did not shrink the window')
            left, top, width, height, border = frame_geometry(second)
            drag(left + width // 2, top + 10, 15, 10)
            wait_for(lambda: frame_geometry(second) == (left + 15, top + 10, width, height, border),
                     'blank berry bar did not move the window')
            chord(0xffeb, 0xffe3, ord(' '))
            wait_for(lambda: geometry(second)[4] == 1, 'tiled border was not restored')
            x.XSync(display, 1)
            chord(ord('a'))
            event = C.create_string_buffer(192)
            wait_for(lambda: x.XCheckTypedWindowEvent(display, second, 2, event), 'ordinary typing was swallowed')
            chord(0xffeb, ord('a'))
            assert not launches.exists(), 'a meta chord opened the launcher'
            chord(0xffeb)
            wait_for(menu_visible, 'bare meta did not open dmenu')
            chord(0xffeb)
            wait_for(lambda: not menu_visible(), 'second meta tap did not close dmenu')
            time.sleep(0.2)
            assert launches.read_text().splitlines() == ['launch'], 'closing dmenu launched another copy'
            chord(0xffeb)
            wait_for(menu_visible, 'meta did not reopen dmenu')
            chord(0xff1b)
            wait_for(lambda: not menu_visible(), 'escape did not close dmenu')
            for _ in range(3):
                chord(0xffe9, 0xff09)
                assert x.XGrabKeyboard(display, root, 0, 1, 1, 0) == 0, 'alt-tab left the keyboard grabbed'
                x.XUngrabKeyboard(display, 0)
                x.XFlush(display)
            chord(0xffe3, 0xffe9, 0xffff)
            wait_for(menu_visible, 'logout menu did not appear')
            third = window('created during logout')
            wait_for(lambda: third in prop(root, '_NET_CLIENT_LIST'), 'logout blocked window management')
            chord(0xff1b)
            wait_for(lambda: not menu_visible(), 'logout cancel did not release input')
            key(0xffe9, True)
            key(0xff09, True)
            key(0xff09, False)
            x.XDestroyWindow(display, first)
            x.XFlush(display)
            key(0xffe9, False)
            wait_for(lambda: len(prop(root, '_NET_CLIENT_LIST')) == 2, 'closing a switcher window stalled dwm')
            assert x.XGrabKeyboard(display, root, 0, 1, 1, 0) == 0, 'closing a switcher window left input grabbed'
            x.XUngrabKeyboard(display, 0)
            assert wm.poll() is None, 'dwm crashed'
            chord(0xffeb, 0xffe1, ord('='))
            wait_for(tab_group, 'group shortcut did not create a tab container')
            group = tab_group()
            wait_for(lambda: parent_of(second) == group and parent_of(third) == group,
                     'windows were not attached to the tab group')
            wait_for(lambda: tabs_fit(group, second, third), 'initial tabs do not fill the group')
            assert prop(group, '_NET_FRAME_EXTENTS') == [1, 1, 1, 1], 'tabbed has a duplicate title strip'
            wait_for(lambda: focused_window() in (second, third), 'no tab has keyboard focus')
            selected_tab = focused_window()
            other_tab = third if selected_tab == second else second
            chord(0xffeb, 0xffe1, 0xff53)
            wait_for(lambda: focused_window() == other_tab, 'super+shift+right did not switch tabs')
            chord(0xffeb, 0xffe1, 0xff51)
            wait_for(lambda: focused_window() == selected_tab, 'super+shift+left did not switch back')
            original_geometry = geometry(group)
            temporary_window = window('temporary window beside tab group')
            wait_for(lambda: temporary_window in prop(root, '_NET_CLIENT_LIST'), 'temporary window was not managed')
            wait_for(lambda: geometry(group) != original_geometry, 'group did not make room for the new window')
            wait_for(lambda: tabs_fit(group, second, third), 'inactive tabs kept the old layout size')
            x.XDestroyWindow(display, temporary_window)
            x.XFlush(display)
            wait_for(lambda: geometry(group) == original_geometry,
                     'group did not return to its position after closing the temporary window')
            wait_for(lambda: tabs_fit(group, second, third), 'tab sizes were not restored with the group')
            chord(0xffe3, 0xff09)
            chord(0xffeb, 0xffe1, ord('-'))
            wait_for(lambda: parent_of(second) == root and parent_of(third) == root and not tab_group(),
                     'detaching a two-tab group did not restore both windows')
            wait_for(lambda: prop(second, '_NET_FRAME_EXTENTS') == [1, 1, 33, 1],
                     'detaching did not restore the blank berry frame')
            chord(0xffeb, 0xffe1, ord('='))
            wait_for(tab_group, 'could not regroup detached windows')
            group = tab_group()
            wait_for(lambda: parent_of(second) == group and parent_of(third) == group, 'regrouping failed')
            fourth = window('added to existing group')
            wait_for(lambda: prop(root, '_NET_ACTIVE_WINDOW') == [fourth], 'new window did not receive focus')
            chord(0xffeb, 0xffe1, ord('='))
            wait_for(lambda: parent_of(fourth) == group, 'new window did not join the existing group')
            chord(0xffeb, 0xffe1, ord('-'))
            wait_for(lambda: parent_of(fourth) == root and parent_of(second) == group and parent_of(third) == group,
                     'detaching the active tab did not preserve the remaining group')

            class ClientMessage(C.Structure):
                _fields_ = [('type', I), ('serial', W), ('send_event', I), ('display', D),
                            ('window', W), ('message_type', W), ('format', I), ('data', L * 5)]

            message = ClientMessage(type=33, display=display, window=group, format=32,
                                    message_type=x.XInternAtom(display, b'WM_PROTOCOLS', 0))
            message.data[0] = x.XInternAtom(display, b'WM_DELETE_WINDOW', 0)
            x.XSendEvent(display, group, 0, 0, C.byref(message))
            x.XFlush(display)
            closed = []

            def close_requested_tab(tabs):
                for tab in tabs:
                    event = C.create_string_buffer(192)
                    if x.XCheckTypedWindowEvent(display, tab, 33, event):
                        request = C.cast(event, P(ClientMessage)).contents
                        if request.message_type == message.message_type and request.data[0] == message.data[0]:
                            closed.append(tab)
                            x.XDestroyWindow(display, tab)
                            x.XFlush(display)
                            return True
                return False

            wait_for(lambda: close_requested_tab((second, third)), 'group close did not reach a tab')
            remaining = third if closed[0] == second else second
            wait_for(lambda: tab_group() == group and parent_of(remaining) == group,
                     'closing a tab detached the remaining window')
            wait_for(lambda: tabs_fit(group, remaining), 'remaining tab stopped filling its group')
            x.XSendEvent(display, group, 0, 0, C.byref(message))
            x.XFlush(display)
            wait_for(lambda: close_requested_tab((remaining,)), 'last tab did not receive close request')
            wait_for(lambda: not tab_group(), 'empty tab container did not exit')
            second, third = window('replacement second'), window('replacement third')
            wait_for(lambda: all(w in prop(root, '_NET_CLIENT_LIST') for w in (second, third, fourth)),
                     'replacement windows were not managed')
            chord(0xffeb, 0xffe1, ord('='))
            wait_for(tab_group, 'could not create a group for the save-set check')
            group = tab_group()
            wait_for(lambda: sum(parent_of(w) == group for w in (second, third, fourth)) == 2,
                     'save-set test windows were not grouped')
            x.XKillClient(display, group)
            x.XFlush(display)
            wait_for(lambda: all(parent_of(w) == root for w in (second, third, fourth)) and not tab_group(),
                     'windows did not survive the container connection exiting')
            assert wm.poll() is None, 'dwm crashed during grouping'
            print('x11 startup, typing, launcher, alt-tab, logout, tab grouping, close, detach and container recovery: passed')
    finally:
        if display:
            x.XCloseDisplay(display)
        for process in (wm, server):
            if process and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        os.close(readfd)
        if writefd is not None:
            os.close(writefd)
        for name in ('dwm.log', 'xvfb.log'):
            path = temporary / name
            if path.exists() and path.stat().st_size:
                print(f'{name}:\n{path.read_text()[-4000:]}')
