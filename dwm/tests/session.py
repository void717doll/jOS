#!/usr/bin/env python3
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

project = Path(__file__).resolve().parents[1]


def script(path, body):
    path.write_text('#!/bin/sh\nset -eu\n' + body)
    path.chmod(0o755)


with tempfile.TemporaryDirectory(prefix='dwm-check-') as directory:
    root = Path(directory)
    bindir = root / 'bin'
    bindir.mkdir()
    trace = root / 'trace'
    trace.touch()
    env = dict(os.environ, HOME=str(root / 'home'), PATH=f'{bindir}:/usr/bin:/bin', TRACE=str(trace),
               XDG_CONFIG_HOME=str(root / 'config'), XDG_STATE_HOME=str(root / 'state'),
               XDG_CACHE_HOME=str(root / 'cache'), XDG_SESSION_TYPE='x11',
               XDG_CONFIG_DIRS=str(root / 'system-config'), XDG_MENU_PREFIX='',
               DISPLAY=':99', DBUS_SESSION_BUS_ADDRESS='test', PIDFILE=str(root / 'pid'))
    menus = root / 'system-config/menus'
    menus.mkdir(parents=True)
    (menus / 'plasma-applications.menu').write_text('<Menu/>')
    script(bindir / 'kbuildsycoca6', 'printf "cache %s\\n" "$XDG_MENU_PREFIX" >> "$TRACE"\nexit "${CACHE_RESULT:-0}"\n')
    script(bindir / 'xinput', '''case "$1" in
list) printf '10\\n11\\n';;
list-props)
    if [ "$2" = 10 ]; then
        printf 'libinput Disable While Typing Enabled (300): 1\\n'
    else
        printf 'libinput Accel Speed (301): 0\\n'
    fi;;
set-prop)
    printf 'touchpad %s %s %s\\n' "$2" "$3" "$4" >> "$TRACE"
    exit "${TOUCHPAD_RESULT:-0}";;
esac
''')
    locker = bindir / 'dwm-slock'
    shutil.copy2(project / 'scripts' / 'lock', bindir / 'dwm-lock')
    lock_command = bindir / 'dwm-lock'
    lock_command.write_text(lock_command.read_text().replace('/usr/local/bin/dwm-slock', str(locker)))
    for name in ('dwm-session', 'dwm-action', 'dwm-background', 'dwm-check'):
        shutil.copy2(project / 'scripts' / name, bindir / name)
        target = bindir / name
        target.write_text(target.read_text().replace('/usr/local/bin/dwm-slock', str(locker)))
    for name in ('xset', 'xsetroot', 'copyq', 'nextcloud', 'slock', 'i3lock', 'dwm-slock', 'xss-lock',
                 'lxpolkit', 'nm-applet', 'redshift', 'dbus-update-activation-environment'):
        script(bindir / name, 'printf "%s\\n" "${0##*/} $*" >> "$TRACE"\n')
    locker.chmod(0o4755)
    script(bindir / 'dunstify', 'printf "notification %s\\n" "$*" >> "$TRACE"\n')
    script(bindir / 'picom', 'printf "picom %s\\n" "$*" >> "$TRACE"\nexec sleep 10\n')
    script(bindir / 'dunst', 'printf "dunst %s\\n" "$*" >> "$TRACE"\nprintf "%s" "$$" > "$TRACE.dunst"\nsleep 10\n')
    script(bindir / 'gnome-keyring-daemon', '''printf 'keyring %s\\n' "$*" >> "$TRACE"
[ "${KEYRING_FAIL:-no}" = no ] || exit 1
touch "$TRACE.keyring"
''')
    script(bindir / 'dbus-send', '''case "$*" in
*NameHasOwner*)
    if [ -f "$TRACE.keyring" ]; then echo 'boolean true'; else echo 'boolean false'; fi;;
*) printf 'uint32 %s\\n' "$(cat "$TRACE.dunst")";;
esac
''')
    script(bindir / 'dwm', 'printf "%s\\n" "$$" > "$PIDFILE"\nprintf "wm started\\n" >> "$TRACE"\nsleep 1\n')
    script(bindir / 'xprop', '''case "$*" in
    '-root _NET_SUPPORTING_WM_CHECK') echo '_NET_SUPPORTING_WM_CHECK(WINDOW): window id # 0x123';;
    '-id 0x123 _NET_WM_PID') printf '_NET_WM_PID(CARDINAL) = %s\\n' "$(cat "$PIDFILE")";;
    *) printf '%s\\n' "$*" >> "$TRACE";;
esac
''')
    script(bindir / 'dwm-background', 'printf "background\\n" >> "$TRACE"\n')
    script(bindir / 'dwm-status', 'printf "status %s\\n" "$*" >> "$TRACE"\n')
    script(bindir / 'dwm-mount', 'printf "mount helper started\\n" >> "$TRACE"\n')
    subprocess.run([bindir / 'dwm-session'], env=env, check=True, timeout=5)
    lines = trace.read_text().splitlines()
    assert lines[0] == 'wm started'
    assert lines[2] == 'cache plasma-', lines
    assert [line for line in lines if line.startswith('touchpad ')] == [
        'touchpad 10 libinput Disable While Typing Enabled 0']
    assert any(line.startswith('dbus-update-activation-environment ') and 'XDG_MENU_PREFIX' in line for line in lines)
    assert 'background' in lines and 'xset s reset' in lines
    assert 'mount helper started' in lines
    assert 'xset s 300 300' in lines
    assert f'xss-lock -- {bindir}/dwm-action lock' in lines
    assert f'dunst -conf {bindir}/../share/dwm/dunstrc' in lines
    assert f'picom --config {bindir}/../share/dwm/picom.conf' in lines
    assert f'redshift -c {bindir}/../share/dwm/redshift.conf' in lines
    assert 'status ' in lines
    assert 'lxpolkit ' in lines
    assert 'nextcloud --background' in lines
    assert lines.index('keyring --start --components=secrets') < lines.index('nextcloud --background')
    assert 'password keyring unavailable' not in (root / 'state/dwm/session.log').read_text()
    assert not any(line.startswith('nm-applet') for line in lines)
    assert lines[1].startswith('dbus-update-activation-environment --systemd DISPLAY XAUTHORITY')
    assert lines.index(f'dunst -conf {bindir}/../share/dwm/dunstrc') < lines.index('lxpolkit ')
    assert 'dunst ready:' in (root / 'state/dwm/session.log').read_text()
    assert 'dwm ready:' in (root / 'state/dwm/session.log').read_text()
    assert not any('compositor unavailable' in line for line in lines)

    # reuse an existing secret service without starting a competing backend.
    trace.write_text('')
    subprocess.run([bindir / 'dwm-session'], env=env, check=True, timeout=5)
    assert 'keyring --start' not in trace.read_text()
    assert 'nextcloud --background' in trace.read_text()

    Path(str(trace) + '.keyring').unlink()
    trace.write_text('')
    subprocess.run([bindir / 'dwm-session'],
                   env=dict(env, KEYRING_FAIL='yes', XDG_MENU_PREFIX='custom-', CACHE_RESULT='1', TOUCHPAD_RESULT='1'),
                   check=True, timeout=5)
    assert 'cache custom-' in trace.read_text()
    assert 'could not refresh kde application cache' in (root / 'state/dwm/session.log').read_text()
    assert 'could not enable touchpad movement while typing' in (root / 'state/dwm/session.log').read_text()
    assert 'password keyring unavailable' in (root / 'state/dwm/session.log').read_text()
    assert 'nextcloud --background' in trace.read_text()

    trace.write_text('')
    script(bindir / 'picom', 'printf "glx initialization failed\\n" >&2\nexit 1\n')
    subprocess.run([bindir / 'dwm-session'], env=env, check=True, timeout=5)
    assert 'compositor unavailable' in trace.read_text()
    assert 'picom exited during startup' in trace.read_text()
    assert 'background' in trace.read_text(), 'compositor failure stopped session startup'
    assert 'glx initialization failed' in (root / 'state/dwm/session.log').read_text()

    # hide picom even on machines that have the real compositor installed.
    session = bindir / 'dwm-session'
    original = session.read_text()
    session.write_text(original.replace('command -v picom', 'command -v missing-test-picom'))
    trace.write_text('')
    subprocess.run([session], env=env, check=True, timeout=5)
    assert 'compositor unavailable' in trace.read_text()
    assert 'install it with your package manager' in trace.read_text()
    session.write_text(original)
    script(bindir / 'picom', 'exec sleep 10\n')

    trace.write_text('')
    subprocess.run([bindir / 'dwm-action', 'lock'], env=env, check=True)
    assert trace.read_text() == 'dwm-slock \n'
    result = subprocess.run([bindir / 'dwm-check'], env=env, capture_output=True, text=True)
    assert 'dwm-slock setup' not in result.stdout
    for installed in (True, False):
        if installed:
            locker.chmod(0o755)
        else:
            locker.unlink()
        trace.write_text('')
        result = subprocess.run([bindir / 'dwm-action', 'lock'], env=env,
                                capture_output=True, text=True)
        assert result.returncode != 0 and 'the screen is not locked' in result.stderr
        assert './deploy.sh --lock' in result.stderr
        assert trace.read_text().startswith('notification ')
        assert not any(line.startswith(('slock ', 'i3lock ', 'dwm-slock '))
                       for line in trace.read_text().splitlines())
        result = subprocess.run([bindir / 'dwm-check'], env=env, capture_output=True, text=True)
        assert result.returncode != 0 and 'missing: dwm-slock setup' in result.stdout

    script(bindir / 'dunst', 'exit 1\n')
    subprocess.run([bindir / 'dwm-session'], env=env, check=True, timeout=5)
    assert 'dunst did not acquire' in (root / 'state/dwm/session.log').read_text()
    assert 'dwm-slock needs setup' in (root / 'state/dwm/session.log').read_text()

    trace.write_text('')
    script(bindir / 'dwm', 'exit 7\n')
    result = subprocess.run([bindir / 'dwm-session'], env=env, timeout=5)
    assert result.returncode == 7
    assert not trace.read_text(), 'helpers started after the window manager failed'
    assert (root / 'state/dwm/session.previous.log').is_file()

    script(bindir / 'dmenu', 'printf "%s\\n" "$CHOICE"\n')
    for choice in ('cancel', 'log out'):
        trace.write_text('')
        subprocess.run([bindir / 'dwm-action', 'logout'], env=dict(env, CHOICE=choice), check=True)
        assert bool(trace.read_text()) == (choice == 'log out')

    script(bindir / 'st', 'printf "st %s\\n" "$*" >> "$TRACE"\n')
    script(bindir / 'dwm-network', 'printf "network menu\\n" >> "$TRACE"\n')
    trace.write_text('')
    subprocess.run([bindir / 'dwm-action', 'network'], env=env, check=True)
    assert trace.read_text().strip() == 'network menu'

    script(bindir / 'brightnessctl', 'printf "%s\\n" "$@" >> "$TRACE"\n')
    for action, value in (('brightness-up', '+10%'), ('brightness-down', '10%-'),
                          ('brightness-up-small', '+1%'), ('brightness-down-small', '1%-')):
        trace.write_text('')
        subprocess.run([bindir / 'dwm-action', action], env=env, check=True)
        assert trace.read_text().splitlines() == ['-c', 'backlight', '--min-value=1', 'set', value,
                                                 'status --notify brightness']

    script(bindir / 'pactl', '''case "$1" in
get-sink-volume) printf 'Volume: front-left: 32768 / %s%% / -18.06 dB, front-right: 32768 / %s%% / -18.06 dB\\n' "${LEFT:-50}" "${RIGHT:-40}";;
*) printf '%s\\n' "$*" >> "$TRACE";;
esac
''')
    for action, command in (('volume-up', 'set-sink-volume @DEFAULT_SINK@ 55% 45%'),
                            ('volume-down', 'set-sink-volume @DEFAULT_SINK@ 45% 35%'),
                            ('volume-up-small', 'set-sink-volume @DEFAULT_SINK@ 51% 41%'),
                            ('volume-down-small', 'set-sink-volume @DEFAULT_SINK@ 49% 39%'),
                            ('mute', 'set-sink-mute @DEFAULT_SINK@ toggle'),
                            ('mic-mute', 'set-source-mute @DEFAULT_SOURCE@ toggle')):
        trace.write_text('')
        subprocess.run([bindir / 'dwm-action', action], env=env, check=True)
        category = 'microphone' if action == 'mic-mute' else 'volume'
        assert trace.read_text().splitlines() == [command, f'status --notify {category}']

    for action, left, right, expected in (
            ('volume-up', '98', '100', '100% 100%'),
            ('volume-up-small', '100', '99', '100% 100%'),
            ('volume-down', '2', '0', '0% 0%'),
            ('volume-down-small', '0', '1', '0% 0%'),
            ('volume-up', '150', '200', '100% 100%'),
            ('volume-down', '150', '200', '100% 100%')):
        trace.write_text('')
        subprocess.run([bindir / 'dwm-action', action], env=dict(env, LEFT=left, RIGHT=right), check=True)
        assert trace.read_text().splitlines()[0] == f'set-sink-volume @DEFAULT_SINK@ {expected}'

    script(bindir / 'brightnessctl', 'exit 1\n')
    script(bindir / 'dunstify', 'exit 0\n')
    trace.write_text('')
    result = subprocess.run([bindir / 'dwm-action', 'brightness-up'], env=env)
    assert result.returncode != 0 and not trace.read_text(), 'failed controls showed a success popup'

    shutil.copy2(project / 'scripts' / 'dwm-background', bindir / 'dwm-background')
    config = root / 'config'
    config.mkdir()
    wallpaper = root / 'wall paper.jpg'
    wallpaper.touch()
    (config / 'dwm').mkdir()
    (config / 'dwm/wallpaper').write_text(f'{wallpaper}\n')
    script(bindir / 'xwallpaper', 'printf "%s\\n" "$@" >> "$TRACE"\n')
    trace.write_text('')
    subprocess.run([bindir / 'dwm-background'], env=env, check=True)
    lines = trace.read_text().splitlines()
    assert lines[0].startswith('xsetroot -solid #222222')
    assert lines[1:] == ['--zoom', str(wallpaper)]

    script(bindir / 'dbus-send', '''case "$*" in
*ListNames)
    [ "${PLAYERS:-yes}" = yes ] || exit 0
    printf 'string "org.mpris.MediaPlayer2.first"\\nstring "org.mpris.MediaPlayer2.second"\\n';;
*Properties.Get*)
    case "$*" in
    *MediaPlayer2.second*) printf 'variant string "%s"\\n' "${PLAYING:-Playing}";;
    *) echo 'variant string "Paused"';;
    esac;;
*) printf '%s\\n' "$*" >> "$TRACE";;
esac
''')
    for action in ('PlayPause', 'Pause', 'Next', 'Previous', 'Stop'):
        trace.write_text('')
        subprocess.run([bindir / 'dwm-action', action], env=env, check=True)
        assert '--dest=org.mpris.MediaPlayer2.second' in trace.read_text()
        assert f'org.mpris.MediaPlayer2.Player.{action}' in trace.read_text()
    trace.write_text('')
    subprocess.run([bindir / 'dwm-action', 'PlayPause'], env=dict(env, PLAYING='Paused'), check=True)
    assert '--dest=org.mpris.MediaPlayer2.first' in trace.read_text()
    trace.write_text('')
    subprocess.run([bindir / 'dwm-action', 'PlayPause'], env=dict(env, PLAYERS='no'), check=True)
    assert not trace.read_text()

    script(bindir / 'dmenu', 'cat > "$TRACE.menu"\nprintf "%s\\n" "$CHOICE"\n')
    script(bindir / 'xclip', 'cat > "$TRACE.clip"\n')
    subprocess.run([bindir / 'dwm-action', 'emoji'], env=dict(env, CHOICE='😀 grinning face'), check=True)
    assert '😀 grinning face' in Path(str(trace) + '.menu').read_text()
    assert Path(str(trace) + '.clip').read_text() == '😀'

    script(bindir / 'flameshot', 'printf "%s\\n" "$*" >> "$TRACE"\n')
    trace.write_text('')
    subprocess.run([bindir / 'dwm-action', 'screenshot'], env=env, check=True)
    assert trace.read_text() == 'gui\n'

    env['PATH'] = f'{project.parent / "dmenu"}:/usr/bin:/bin'
    result = subprocess.run([project.parent / 'dmenu/dmenu_path'], env=env,
                            check=True, capture_output=True, text=True)
    assert not result.stderr, result.stderr
    assert 'dmenu' in result.stdout.splitlines()
    assert (root / 'cache/dmenu_run').is_file()

print('startup, locking, failure cleanup, controls, wallpaper, media, emoji and flameshot capture: passed (desktop commands mocked)')
