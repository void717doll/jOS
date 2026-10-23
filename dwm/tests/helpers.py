#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import sys
import tempfile

project = Path(__file__).resolve().parents[1]


def script(path, body):
    path.write_text('#!/bin/sh\nset -eu\n' + body)
    path.chmod(0o755)


with tempfile.TemporaryDirectory(prefix='dwm-helpers-') as directory:
    root = Path(directory)
    bindir = root / 'bin'
    bindir.mkdir()
    trace = root / 'trace'
    env = dict(os.environ, HOME=str(root), XDG_CONFIG_HOME=str(root / 'config'),
               PATH=f'{bindir}:/usr/bin:/bin', TRACE=str(trace), DWM_WALLPAPER='/missing-wallpaper')
    script(bindir / 'nmcli', r'''
[ "${FAILED:-no}" = no ] || exit 1
case "$*" in
*device\ status) printf 'wlan0:wifi:%s\n' "${STATE:-connected}";;
*connection\ show\ --active) [ "${VPN:-no}" = yes ] && printf 'vpn:11111111-1111-1111-1111-111111111111\n';;
*-g\ vpn.service-type\ connection\ show\ uuid*) [ "${VPN:-no}" = yes ] && printf '%s\n' "${VPN_SERVICE:-org.freedesktop.NetworkManager.openvpn}";;
*device\ wifi\ list*) printf "yes:%s\n" "${SIGNAL-75}";;
*device\ show*) printf '192.0.2.1\n';;
esac
''')
    script(bindir / 'ping', '''
printf '%s\n' "$*" > "$TRACE"
[ "${UNREACHABLE:-no}" = no ] || exit 1
printf '64 bytes from 192.0.2.1: icmp_seq=1 ttl=64 time=43.1 ms\n'
''')

    wireless = root / 'wireless'
    wireless.write_text('')
    wifi_script = bindir / 'dwm-wifi'
    wifi_script.write_text((project / 'scripts' / 'dwm-wifi').read_text().replace('/proc/net/wireless', str(wireless)))
    wifi_script.chmod(0o755)

    def wifi(*args, **values):
        return subprocess.check_output([wifi_script, *args],
                                       env=dict(env, **values), text=True).strip()

    assert wifi() == ' 43ms'
    assert '-I wlan0 192.0.2.1' in trace.read_text()
    assert wifi('--color') == '\x07\x04\x14\x05 43ms\x08\x01'
    assert wifi(VPN='yes') == ' 43ms'
    assert wifi('--color', VPN='yes') == '\x07\x04\x05 43ms\x08\x01'
    assert wifi(VPN='yes', VPN_SERVICE='org.freedesktop.NetworkManager.openconnect') == ' 43ms'
    assert wifi(UNREACHABLE='yes') == ' n/a'
    assert wifi('--color', UNREACHABLE='yes') == '\x07\x04\x14\x05 n/a\x08\x01'
    trace.write_text('')
    assert wifi(STATE='disconnected') == ''
    assert wifi(FAILED='yes') == ''
    assert wifi('--color', STATE='disconnected') == '\x07\x05\x05\x08\x01'
    assert wifi('--color', FAILED='yes') == '\x07\x05\x05\x08\x01'
    assert not trace.read_text(), 'disconnected wifi sent a ping'

    for signal, level in ((0, 1), (19, 1), (20, 2), (39, 2), (40, 3), (59, 3), (60, 4), (79, 4), (80, 5), (100, 5)):
        result = wifi('--color', SIGNAL=str(signal))
        color = chr(3 if signal <= 10 else 2 if signal <= 25 else 6 if signal <= 50 else 4)
        assert result == f'\x07{color}{chr(16 + level)}\x05 43ms\x08\x01'
    for signal in ('', '--', '-1', '101', 'bad'):
        assert wifi('--color', SIGNAL=signal) == '\x07\x05\x05 43ms\x08\x01'

    for signal, color in ((10, 3), (11, 2), (25, 2), (26, 6), (50, 6), (51, 4)):
        assert wifi('--color', SIGNAL=str(signal)).startswith(f'\x07{chr(color)}')
    wireless.write_text('wlan0: 0000 35. -60. -256 0 0 0 0 0 0\n')
    assert wifi(SIGNAL='') == ' 43ms'
    assert wifi('--color', SIGNAL='') == '\x07\x06\x13\x05 43ms\x08\x01'
    script(bindir / 'curl', "printf '%s\\n' \"$@\" > \"$TRACE.curl\"\n" +
           '[ "${WEATHER_FAIL:-no}" = no ] || exit 28\n' +
           "printf '%s' \"$WEATHER_RESPONSE\"\n")

    def weather(response, **values):
        return subprocess.check_output([project / 'scripts' / 'dwm-weather'],
                                       env=dict(env, WEATHER_RESPONSE=response, **values), text=True).strip()

    for condition, icon in (('Sunny', ''), ('Clear', ''), ('Partly cloudy', ''),
                            ('Light rain', ''), ('Light drizzle', ''), ('Snow', ''),
                            ('Mist', ''), ('Thunderstorm', '')):
        assert weather('+72°F|' + condition) == f'{icon} +72°F {condition.lower()}'
    curl_args = Path(str(trace) + '.curl').read_text().splitlines()
    assert 'https://wttr.in/Oceanside?u&lang=en' in curl_args
    assert 'format=%t|%C' in curl_args
    assert curl_args[curl_args.index('--max-time') + 1] == '4'
    assert weather('-5°F|Snow') == ' -5°F snow'
    for response in ('', '<html>error</html>', 'oops|Sunny', '+72°F|bad\x03colour',
                     '+72°F|Sunny\nerror', '+72°F|' + 'a' * 100):
        assert weather(response) == ' n/a'
    assert weather('+72°F|Sunny', WEATHER_FAIL='yes') == ' n/a'
    script(bindir / 'st', 'printf "%s\\n" "$@" > "$TRACE.st"\n')
    subprocess.run([project / 'scripts' / 'dwm-weather', '--forecast'], env=env, check=True)
    assert Path(str(trace) + '.st').read_text().splitlines() == [
        '-c', 'dwm-weather', '-t', 'weather', '-g', '80x30', '-e',
        sys.executable, str(project / 'scripts' / 'dwm-weather'), '--show-forecast']
    result = subprocess.run([project / 'scripts' / 'dwm-weather', '--show-forecast'],
                            env=dict(env, WEATHER_RESPONSE='forecast text'), input='\n',
                            capture_output=True, text=True, check=True)
    assert result.stdout == 'forecast text\npress enter to close\n'
    result = subprocess.run([project / 'scripts' / 'dwm-weather', '--show-forecast'],
                            env=dict(env, WEATHER_FAIL='yes'), input='\n',
                            capture_output=True, text=True, check=True)
    assert result.stdout == 'could not load forecast\n\npress enter to close\n'

    script(bindir / 'st', 'printf "%s\\n" "$@" > "$TRACE"\n')
    subprocess.run([project / 'scripts' / 'dwm-calendar'], env=env, check=True)
    assert trace.read_text().splitlines() == ['-c', 'dwm-calendar', '-t', 'calendar',
                                            '-g', '80x30', '-e', str(project / 'scripts' / 'dwm-calendar'), '--show']
    calendar = subprocess.run([project / 'scripts' / 'dwm-calendar', '--show'], input='\n',
                              capture_output=True, text=True, check=True)
    header = calendar.stdout.splitlines()[1]
    assert all(day in header for day in ('su', 'mo', 'tu', 'we', 'th', 'fr', 'sa'))
    assert 'us federal holidays' in calendar.stdout
    assert '\x1b[7m' in calendar.stdout and 'enter to close' in calendar.stdout
    fixed_calendar = bindir / 'calendar-fixed-date'
    fixed_calendar.write_text((project / 'scripts' / 'dwm-calendar').read_text().replace(
        'today = date.today()', 'today = date(2026, 10, 1)'))
    fixed_calendar.chmod(0o755)
    fixed = subprocess.run([fixed_calendar, '--show'], input='\n', capture_output=True,
                           text=True, check=True).stdout.splitlines()
    assert any('columbus' in line for line in fixed[:fixed.index('* us federal holidays')])

    wallpapers = root / 'Pictures/wallpapers'
    (wallpapers / 'flowers').mkdir(parents=True)
    for name in ('flowers/red flower.jpg', 'blue.PNG', 'not an image.txt', 'line\nbreak.png'):
        (wallpapers / name).touch()
    script(bindir / 'dmenu', 'cat > "$TRACE.menu"\nprintf "%s\n" "$CHOICE"\nexit "${CANCEL:-0}"\n')
    script(bindir / 'xwallpaper', '[ "${FAILED:-no}" = no ] || exit 1\nprintf "%s\n" "$@" > "$TRACE"\n')
    script(bindir / 'xsetroot', 'printf "%s\\n" "$@" > "$TRACE"\n')
    script(bindir / 'dunstify', '[ "${NOTICE_FAIL:-no}" = no ] || exit 1\nprintf "%s\\n" "$@" > "$TRACE.notice"\n')
    selected = 'flowers/red flower.jpg'
    subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE=selected), check=True)
    assert trace.read_text().splitlines() == ['--zoom', str(wallpapers / selected)]
    assert Path(str(trace) + '.menu').read_text().splitlines() == ['[random]', 'blue.PNG', selected]
    saved = root / 'config/dwm/wallpaper'
    assert saved.read_text() == f'{wallpapers / selected}\n'
    history = root / 'config/dwm/wallpaper.previous'
    notice = Path(str(trace) + '.notice')
    assert history.read_text() == '\n'
    assert 'wallpaper changed' in notice.read_text()
    assert 'undo' not in notice.read_text().lower()
    for choice, cancel in (('', '1'), ('../outside.jpg', '0'), ('arbitrary text', '0')):
        trace.write_text('')
        notice.write_text('')
        subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE=choice, CANCEL=cancel), check=True)
        assert not notice.read_text()
        assert not trace.read_text() and saved.read_text() == f'{wallpapers / selected}\n'
    result = subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE='blue.PNG', FAILED='yes'))
    assert result.returncode != 0 and saved.read_text() == f'{wallpapers / selected}\n'


    assert not notice.read_text(), 'failed apply sent a success notification'
    assert history.read_text() == '\n'
    subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE='blue.PNG'), check=True)
    assert history.read_text() == f'{wallpapers / selected}\n'
    assert Path(str(trace) + '.menu').read_text().splitlines()[0] == '[undo last change]'
    subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE='[undo last change]'), check=True)
    assert saved.read_text() == f'{wallpapers / selected}\n'
    assert trace.read_text().splitlines() == ['--zoom', str(wallpapers / selected)]
    assert not history.exists()
    assert 'wallpaper restored' in notice.read_text()

    # returning to the original solid background is also undoable.
    saved.write_text('\n')
    subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE='blue.PNG', NOTICE_FAIL='yes'), check=True)
    assert saved.read_text() == f'{wallpapers / "blue.PNG"}\n'
    subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE='[undo last change]'), check=True)
    assert trace.read_text().splitlines() == ['-solid', '#222222']
    assert saved.read_text() == '\n' and not history.exists()

    subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE=selected), check=True)
    subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE='blue.PNG'), check=True)
    subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE='blue.PNG'), check=True)
    assert history.read_text() == f'{wallpapers / selected}\n', 'same selection erased undo'
    subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE='[random]'), check=True)
    assert saved.read_text() == f'{wallpapers / selected}\n', 'random repeated the current image'
    assert history.read_text() == f'{wallpapers / "blue.PNG"}\n'
    subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE='[undo last change]'), check=True)
    assert saved.read_text() == f'{wallpapers / "blue.PNG"}\n'
    subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE=selected), check=True)
    subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE='blue.PNG'), check=True)
    (wallpapers / selected).unlink()
    result = subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE='[undo last change]'), capture_output=True)
    assert result.returncode != 0
    assert history.read_text() == f'{wallpapers / selected}\n'
    assert saved.read_text() == f'{wallpapers / "blue.PNG"}\n'

    trace.write_text('')
    subprocess.run([project / 'scripts' / 'dwm-wallpaper'], env=dict(env, CHOICE='[random]'), check=True)
    assert not trace.read_text(), 'single-image random selection reapplied the current image'

print('wifi states, latency, wallpaper random selection, notifications, undo, cancellation and failed apply: passed (commands mocked)')
