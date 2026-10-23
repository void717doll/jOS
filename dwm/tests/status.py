#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import json
import time
import tempfile
import shutil

project = Path(__file__).resolve().parents[1]


def script(path, body):
    path.write_text('#!/bin/sh\nset -eu\n' + body)
    path.chmod(0o755)


with tempfile.TemporaryDirectory(prefix='dwm-status-') as directory:
    root = Path(directory)
    bindir = root / 'bin'
    bindir.mkdir()
    mains = root / 'power/AC'
    mains.mkdir(parents=True)
    (mains / 'type').write_text('Mains\n')
    (mains / 'online').write_text('0\n')
    battery = root / 'power/BAT0'
    battery.mkdir(parents=True)
    for name, value in {'type': 'Battery', 'present': '1', 'capacity': '73',
                        'status': 'Discharging'}.items():
        (battery / name).write_text(value + '\n')
    status = bindir / 'dwm-status'
    status.write_text((project / 'scripts' / 'dwm-status').read_text().replace('/sys/class/power_supply', str(root / 'power')))
    status.chmod(0o755)
    shutil.copy2(project / 'scripts' / 'dwm-bluetooth', bindir / 'dwm-bluetooth')
    bluetooth_power = root / 'bluetooth-power'
    bluetooth_power.write_text('yes\n')
    script(bindir / 'bluetoothctl', '''[ "$1" = show ]
printf 'Controller 00:11:22:33:44:55\nPowered: %s\n' "${BLUETOOTH_STATE:-$(cat "$BLUETOOTH_POWER")}"
''')
    script(bindir / 'dwm-wifi', "printf 'sample\\n' >> \"$TRACE.wifi\"\nprintf ' %s\\n' \"${WIFI_STATE:--}\"\n")
    script(bindir / 'dwm-weather', "printf 'sample\\n' >> \"$TRACE.weather\"\n" +
           "printf ' +72°F %s\\n' \"${WEATHER_STATE:-sunny}\"\n")
    trace = root / 'trace'
    env = dict(os.environ, PATH=f'{bindir}:/usr/bin:/bin', TRACE=str(trace),
               BLUETOOTH_POWER=str(bluetooth_power),
               XDG_STATE_HOME=str(root / 'state'), XDG_RUNTIME_DIR=str(root / 'runtime'))
    script(bindir / 'date', '''if [ "$1" = +%s ]; then printf '%s\n' "$TEST_TIME"; exit; fi
[ "$1" = '+%m/%d/%Y %I:%M %p' ]
printf '10/05/2026 09:07 PM\n'
''')
    script(bindir / 'pactl', '''case "$1" in
get-sink-volume|get-source-volume) printf 'Volume: front-left: 32768 / %s%% / -18.06 dB, front-right: 32768 / %s%% / -18.06 dB\n' "${VOLUME:-50}" "${VOLUME:-50}";;
get-sink-mute|get-source-mute) printf 'Mute: %s\n' "${MUTE:-no}";;
esac
''')
    script(bindir / 'brightnessctl', 'printf "intel_backlight,backlight,300,%s%%,1000\\n" "${LIGHT:-30}"\n')
    script(bindir / 'dunstify', 'printf "%s\\n" "$@" > "$TRACE"\n')
    script(bindir / 'paplay', 'printf "%s\\n" "$1" >> "$TRACE.sound"\n')
    script(bindir / 'xsetroot', 'printf "%s\\n" "$@" > "$TRACE.bar"\n')

    clock = 1000

    def run(*args, **values):
        global clock
        clock += 20
        values.setdefault("TEST_TIME", str(clock))
        return subprocess.run([status, *args], env=dict(env, **values), check=True,
                              capture_output=True, text=True, timeout=10).stdout.strip()

    assert run('--once') == ' 73%    50%    30%    -    on   \x1a +72°F sunny\x1b    10/05/2026 09:07 pm'
    run('--check-battery')
    assert not trace.exists(), 'initial ac state should not notify'
    (mains / 'online').write_text('1\n')
    run('--check-battery')
    assert 'charger connected' in trace.read_text() and 'laptop is charging' in trace.read_text()
    time.sleep(0.05)
    assert Path(str(trace) + '.sound').read_text().strip().endswith('power-plug.oga')
    trace.unlink()
    (mains / 'online').write_text('0\n')
    run('--check-battery')
    assert 'charger unplugged' in trace.read_text() and 'running on battery' in trace.read_text()
    time.sleep(0.05)
    assert Path(str(trace) + '.sound').read_text().strip().endswith('power-unplug.oga')
    run('--refresh', MUTE='yes')
    bar = Path(str(trace) + '.bar').read_text()
    assert '   \x01 30%\x01   ' in bar
    assert '\x01 \x0473%\x01   \x01 muted\x01' in bar
    assert '\x18\x04 on\x19\x01' in bar
    run('--refresh', BLUETOOTH_STATE='no')
    assert '\x18\x05 off\x19\x01' in Path(str(trace) + '.bar').read_text()
    for value, icon in ((100, ''), (90, ''), (89, ''), (65, ''), (64, ''), (35, ''), (34, ''), (10, ''), (9, ''), (5, ''), (0, '')):
        (battery / 'capacity').write_text(str(value) + '\n')
        run('--refresh')
        color = chr(3 if value <= 10 else 2 if value <= 25 else 6 if value <= 50 else 4)
        assert f'\x01{icon} {color}{value}%\x01' in Path(str(trace) + '.bar').read_text()
    (battery / 'capacity').write_text('73\n')
    (battery / 'status').write_text('Charging\n')
    for frame, icon in enumerate(('', '', '', '', '', '')):
        timestamp = str(2000 + frame * 2)
        assert run('--once', MUTE='yes', TEST_TIME=timestamp).startswith(f'{icon} 73%    muted   ')
        run('--refresh', TEST_TIME=timestamp)
        assert f'\x01{icon} \x0473%\x01   ' in Path(str(trace) + '.bar').read_text()
    (battery / 'status').write_text('Discharging\n')
    assert run('--once', TEST_TIME='2000').startswith(' 73%   ')
    (battery / 'status').write_text('Full\n')
    assert run('--once').startswith(' 73%   ')
    run('--refresh')
    assert '\x01 \x0473%\x01' in Path(str(trace) + '.bar').read_text()
    second = root / 'power/BAT1'
    second.mkdir()
    for name, value in {'type': 'Battery', 'capacity': '4', 'status': 'Charging'}.items():
        (second / name).write_text(value + '\n')
    run('--refresh', TEST_TIME='2000')
    bar = Path(str(trace) + '.bar').read_text()
    assert '\x01 \x0473%\x01' in bar
    assert '\x01 \x034%\x01   ' in bar
    (second / 'present').write_text('0\n')
    (battery / 'present').write_text('0\n')
    assert run('--once').startswith('   ')

    assert '\x16 10/05/2026 09:07 pm\x17' in bar
    for value, color in ((0, 3), (10, 3), (11, 2), (25, 2), (26, 6), (50, 6), (51, 4), (100, 4)):
        (battery / 'present').write_text('1\n')
        (battery / 'capacity').write_text(str(value))
        run('--refresh', VOLUME=str(value), LIGHT=str(value))
        bar = Path(str(trace) + '.bar').read_text()
        assert f'{chr(color)}{value}%\x01' in bar
        assert f'\x01 {value}%\x01' in bar
        icon = '' if value == 0 else '' if value < 50 else ''
        assert f'\x01{icon} {value}%\x01' in bar

    for volume, icon in (('0', ''), ('1', ''), ('49', ''), ('50', ''), ('100', '')):
        assert f'{icon} {volume}%' in run('--once', VOLUME=volume)

    for category, value, label in [('volume', '50', 'volume: 50%'),
                                    ('microphone', '50', 'microphone: 50%'),
                                    ('brightness', '30', 'brightness: 30%')]:
        run('--notify', category)
        args = trace.read_text().splitlines()
        assert f'string:x-dunst-stack-tag:dwm-{category}' in args
        assert f'int:value:{value}' in args and args[-1] == label
        assert args[args.index('-t') + 1] == '1500'
        assert '10/05/2026 09:07 pm' in Path(str(trace) + '.bar').read_text()
    run('--notify', 'volume', MUTE='yes')
    assert trace.read_text().splitlines()[-2:] == ['int:value:0', 'volume: muted']
    assert 'control.muted' in trace.read_text().splitlines()
    run('--notify', 'volume', VOLUME='125')
    assert trace.read_text().splitlines()[-2:] == ['int:value:100', 'volume: 125%']

    script(bindir / 'dunstify', 'printf "notice\\n%s\\n" "$*" >> "$TRACE"\n')
    trace.write_text('')
    (battery / 'present').write_text('1\n')

    ac = root / 'power/AC'
    (ac / 'online').write_text('0\n')
    run('--check-battery')
    trace.write_text('')
    (ac / 'online').write_text('1\n')
    run('--check-battery')
    assert 'power.connected' in trace.read_text() and 'charger connected' in trace.read_text()
    run('--check-battery')
    assert trace.read_text().count('notice\n') == 1, 'plugged-in alert repeated while connected'
    (ac / 'online').write_text('0\n')
    run('--check-battery')
    assert 'charger unplugged' in trace.read_text()
    (ac / 'online').write_text('1\n')
    run('--check-battery')
    assert trace.read_text().count('notice\n') == 3, 'power alert did not rearm after disconnect'
    trace.write_text('')

    def charge(value, state='Discharging'):
        (battery / 'capacity').write_text(str(value) + '\n')
        (battery / 'status').write_text(state + '\n')
        run('--check-battery')
        return trace.read_text().count('notice\n')

    assert charge(40) == 0
    assert charge(20) == 1
    assert 'battery.low' in trace.read_text()
    run('--refresh')
    assert '\x01 \x0220%\x01' in Path(str(trace) + '.bar').read_text()
    for value in (20, 19, 21, 20):
        assert charge(value) == 1, 'low battery alerts repeated around the threshold'
    assert charge(5) == 2
    assert '-u critical -t 0 -c battery.critical' in trace.read_text()
    run('--refresh')
    assert '\x01 \x035%\x01' in Path(str(trace) + '.bar').read_text()
    assert charge(4) == 2
    for state in ('Charging', 'Discharging', 'Charging', 'Discharging'):
        assert charge(5, state) == 2, 'plugging in briefly repeated a low-battery alert'
    assert charge(50) == 2
    assert charge(80, 'Full') == 3
    for value, state in ((80, 'Full'), (79, 'Discharging'), (80, 'Charging'), (80, 'Full')):
        assert charge(value, state) == 3, 'charge-limit fluctuations repeated the full alert'
    assert 'battery.full' in trace.read_text()
    assert charge(70) == 3
    assert charge(20) == 4
    assert charge(100, 'Not charging') == 5
    assert charge(100, 'Not charging') == 5
    (battery / 'present').write_text('0\n')
    run('--check-battery')
    assert trace.read_text().count('notice\n') == 5

    (battery / 'present').write_text('1\n')
    script(bindir / 'dunstify', 'exit 1\n')
    charge(30)
    charge(20)
    script(bindir / 'dunstify', 'printf "notice\\n%s\\n" "$*" >> "$TRACE"\n')
    assert charge(20) == 6, 'a failed notification was not retried'
    state_file = root / 'state/dwm/battery/BAT0'
    state_file.write_text('4 Full\n')
    assert charge(80, 'Full') == 6
    assert state_file.read_text() == '4 80\n', 'old alert state was not migrated'
    assert charge(79) == 6
    assert charge(80, 'Full') == 6
    assert charge(75) == 6
    assert charge(80, 'Full') == 7, 'full alert did not rearm after actual discharge'
    charge(30)
    (battery / 'capacity').write_text('20\n')
    script(bindir / 'dunstify', 'sleep 0.1\nprintf "notice\\n%s\\n" "$*" >> "$TRACE"\n')
    checks = [subprocess.Popen([status, '--check-battery'], env=env) for _ in range(4)]
    assert all(check.wait(timeout=3) == 0 for check in checks)
    assert trace.read_text().count('notice\n') == 8, 'concurrent checks duplicated the alert'
    (battery / 'present').write_text('0\n')

    script(bindir / 'pactl', 'exit 1\n')
    script(bindir / 'brightnessctl', 'exit 1\n')
    script(bindir / 'xbacklight', 'exit 1\n')
    assert run('--once') == '    n/a    n/a    -    on   \x1a +72°F sunny\x1b    10/05/2026 09:07 pm'

    (battery / 'present').write_text('1\n')
    (battery / 'capacity').write_text('70\n')
    (battery / 'status').write_text('Discharging\n')
    samples = Path(str(trace) + '.wifi')
    previous_samples = len(samples.read_text().splitlines())
    run('--refresh', TEST_TIME='5000')
    (battery / 'capacity').write_text('60\n')
    for second in (5001, 5005):
        run('--refresh', TEST_TIME=str(second), WIFI_STATE='+')
        assert '\x01 \x0460%' in Path(str(trace) + '.bar').read_text()
    run('--refresh', TEST_TIME='5007', WIFI_STATE='+')
    assert '\x01 \x0460%' in Path(str(trace) + '.bar').read_text()
    run('--notify', 'volume', TEST_TIME='5009', WIFI_STATE='+')
    assert ' -' in Path(str(trace) + '.bar').read_text()
    assert len(samples.read_text().splitlines()) == previous_samples + 1
    run('--refresh', TEST_TIME='5010', WIFI_STATE='+')
    assert ' +' in Path(str(trace) + '.bar').read_text()
    assert len(samples.read_text().splitlines()) == previous_samples + 2
    run('--refresh', TEST_TIME='4990')
    assert len(samples.read_text().splitlines()) == previous_samples + 3, 'clock rollback kept a stale sample'

    weather_samples = Path(str(trace) + '.weather')
    previous_weather = len(weather_samples.read_text().splitlines())
    run('--refresh', TEST_TIME='7000')
    for second in (7001, 7100, 7299):
        run('--refresh', TEST_TIME=str(second), WEATHER_STATE='cloudy')
        assert '\x1a +72°F sunny\x1b' in Path(str(trace) + '.bar').read_text()
    assert len(weather_samples.read_text().splitlines()) == previous_weather + 1
    run('--refresh', TEST_TIME='7300', WEATHER_STATE='cloudy')
    assert '\x1a +72°F cloudy\x1b' in Path(str(trace) + '.bar').read_text()
    assert len(weather_samples.read_text().splitlines()) == previous_weather + 2

    events = root / 'events'
    os.mkfifo(events)
    event_fd = os.open(events, os.O_RDWR)
    (root / 'light').write_text('30')
    script(bindir / 'brightnessctl', 'printf "intel_backlight,backlight,300,%s%%,1000\\n" "$(cat "$LIGHT")"\n')
    (bindir / 'udevadm').write_text("""#!/usr/bin/python3
import os, sys
from pathlib import Path
assert sys.argv[1:] == ['monitor', '--udev', '--subsystem-match=power_supply', '--subsystem-match=backlight']
Path(os.environ['MONITOR_PID']).write_text(str(os.getpid()))
with open(os.environ['EVENTS']) as events:
    for event in events:
        print('UDEV change ' + event.strip(), flush=True)
""")
    (bindir / 'udevadm').chmod(0o755)
    (bindir / 'xsetroot').write_text("""#!/usr/bin/python3
import json, os, sys
with open(os.environ['BAR_LOG'], 'a') as output:
    output.write(json.dumps(sys.argv[2]) + '\\n')
""")
    (bindir / 'xsetroot').chmod(0o755)
    bar_log = root / 'bar.log'
    daemon = subprocess.Popen([status], env=dict(env, TEST_TIME='6000',
                              EVENTS=str(events), LIGHT=str(root / 'light'),
                              MONITOR_PID=str(root / 'monitor.pid'), BAR_LOG=str(bar_log)))

    def wait_bar(text, limit=1.5):
        deadline = time.monotonic() + limit
        while time.monotonic() < deadline:
            if bar_log.exists():
                lines = bar_log.read_text().splitlines()
                if lines and text in json.loads(lines[-1]):
                    return
            assert daemon.poll() is None, 'status daemon exited'
            time.sleep(0.02)
        raise AssertionError('bar did not update: ' + repr(text))

    try:
        wait_bar('\x01 \x0460%', 3)
        (battery / 'capacity').write_text('5\n')
        os.write(event_fd, b'power_supply\n')
        wait_bar('\x01 \x035%\x01')
        (root / 'light').write_text('75')
        os.write(event_fd, b'backlight\n')
        wait_bar('\x01 75%\x01')
        paints = len(bar_log.read_text().splitlines())
        os.write(event_fd, b'backlight\n')
        time.sleep(0.3)
        assert len(bar_log.read_text().splitlines()) == paints, 'unchanged status repainted'
        (root / 'light').write_text('55')
        wait_bar('\x01 55%\x01', 3)
        bluetooth_power.write_text('no\n')
        wait_bar('\x18\x05 off\x19\x01', 3)
    finally:
        daemon.terminate()
        daemon.wait(timeout=3)
        os.close(event_fd)
    pid = int((root / 'monitor.pid').read_text())
    assert not Path(f'/proc/{pid}').exists(), 'device monitor survived status shutdown'

print('status colors, battery thresholds and alert suppression, device events, fallback polling, monitor cleanup and control popups: passed (commands mocked)')
