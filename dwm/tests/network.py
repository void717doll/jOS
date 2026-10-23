#!/usr/bin/env python3
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

project = Path(__file__).resolve().parents[1]


def script(path, body):
    path.write_text('#!/bin/sh\nset -eu\n' + body)
    path.chmod(0o755)


with tempfile.TemporaryDirectory(prefix='dwm-network-') as directory:
    root = Path(directory)
    bindir = root / 'bin'
    bindir.mkdir()
    helper = bindir / 'dwm-network'
    shutil.copy2(project / 'scripts' / 'dwm-network', helper)
    trace = root / 'trace'
    menu = root / 'menu'
    radio = root / 'radio'
    ssid = 'home: \\ wifi $(touch should-not-exist)'
    env = dict(os.environ, PATH=f'{bindir}:/usr/bin:/bin', TRACE=str(trace), MENU=str(menu),
               RADIO=str(radio), SSID=ssid, CHOICE=f'1. connect: {ssid}')
    (bindir / 'nmcli').write_text('''#!/usr/bin/env python3
import json, os, sys
from pathlib import Path
a = sys.argv[1:]
with open(os.environ['TRACE'], 'a') as out:
    out.write(json.dumps(a) + '\\n')
if a == ['radio', 'wifi']:
    print(Path(os.environ['RADIO']).read_text())
elif a[:2] == ['radio', 'wifi']:
    Path(os.environ['RADIO']).write_text('enabled' if a[2] == 'on' else 'disabled')
elif a[-2:] == ['device', 'status']:
    print('wlan0:wifi:' + os.environ.get('STATE', 'connected'))
elif 'SSID' in a:
    if os.environ.get('SCAN_FAIL') == 'yes': sys.exit(1)
    print(os.environ['SSID'] + '\\n' + os.environ['SSID'] + '\\nCafe\\n--')
elif a == ['-t', '-f', 'UUID,TYPE', 'connection', 'show']:
    if os.environ.get('NEW') != 'yes': print('old:802-11-wireless')
    print('vpn:vpn')
elif '802-11-wireless.ssid' in a:
    print(os.environ['SSID'])
elif '802-11-wireless-security.key-mgmt' in a:
    print(os.environ.get('SECURITY', 'wpa-psk'))
elif a[0] == '--ask':
    if os.environ.get('CONNECT_FAIL') == 'yes': sys.exit(1)
elif a == ['-t', '-f', 'UUID,DEVICE', 'connection', 'show', '--active']:
    print('chosen:wlan0')
elif a[:2] == ['connection', 'modify']:
    if 'connection.autoconnect' in a and os.environ.get('SAVE_FAIL') == 'yes': sys.exit(1)
else:
    sys.exit(9)
''')
    (bindir / 'nmcli').chmod(0o755)
    script(bindir / 'dmenu', 'cat > "$MENU"\nprintf "%s\\n" "$CHOICE"\nexit "${CANCEL:-0}"\n')
    script(bindir / 'dunstify', 'printf "%s\\n" "$*" > "$TRACE.notice"\n')
    script(bindir / 'st', '[ "$1" = -e ]\nshift\nexec "$@"\n')
    script(bindir / 'nmtui', 'printf "advanced" > "$TRACE.advanced"\n')
    script(bindir / 'dwm-vpn', 'printf "vpn" > "$TRACE.vpn"\n')

    def run(**values):
        trace.write_text('')
        radio.write_text(values.pop('RADIO_STATE', 'enabled'))
        Path(str(trace) + '.notice').write_text('')
        result = subprocess.run([helper], env=dict(env, **values), cwd=root,
                                capture_output=True, text=True, timeout=5)
        calls = [json.loads(line) for line in trace.read_text().splitlines()]
        return result, calls

    result, calls = run()
    assert result.returncode == 0, result.stderr
    assert menu.read_text().splitlines() == [f'1. connect: {ssid}', '2. connect: Cafe',
        'saved networks / hidden / advanced', 'vpn', 'disable wifi']
    assert ['--ask', '--wait', '60', 'connection', 'up', 'uuid', 'old', 'ifname', 'wlan0'] in calls
    assert ['connection', 'modify', 'uuid', 'old', '802-11-wireless-security.psk-flags', '0'] in calls
    preference = ['connection', 'modify', 'uuid', 'chosen', 'connection.autoconnect', 'yes',
                  'connection.autoconnect-priority', '999']
    assert calls[-1] == preference
    assert not (root / 'should-not-exist').exists()
    result, calls = run(NEW='yes', STATE='disconnected')
    assert result.returncode == 0, result.stderr
    assert ['--ask', '--wait', '60', 'device', 'wifi', 'connect', ssid, 'ifname', 'wlan0'] in calls
    assert calls[-1] == preference
    result, calls = run(SECURITY='wpa-eap')
    assert result.returncode == 0
    assert not any('802-11-wireless-security.psk-flags' in call for call in calls)
    for values in ({'CANCEL': '1'}, {'CHOICE': ''}, {'CHOICE': 'arbitrary input'}):
        result, calls = run(**values)
        assert result.returncode == 0
        assert not any(call[0] == '--ask' or call[:2] == ['connection', 'modify'] for call in calls)
    for values in ({'CONNECT_FAIL': 'yes'}, {'SCAN_FAIL': 'yes'}):
        result, calls = run(**values)
        assert result.returncode != 0
        assert preference not in calls
    result, calls = run(SAVE_FAIL='yes')
    assert result.returncode != 0
    assert 'could not save reconnect preference' in result.stderr
    assert run(CHOICE='vpn')[0].returncode == 0
    assert Path(str(trace) + '.vpn').read_text() == 'vpn'
    assert run(CHOICE='saved networks / hidden / advanced')[0].returncode == 0
    assert Path(str(trace) + '.advanced').read_text() == 'advanced'
    result, calls = run(CHOICE='disable wifi')
    assert result.returncode == 0 and calls[-1] == ['radio', 'wifi', 'off']
    result, calls = run(RADIO_STATE='disabled', CHOICE='')
    assert result.returncode == 0 and 'enable wifi' in menu.read_text()
    assert not any('SSID' in call for call in calls)

print('wifi scanning, saved/new profiles, password storage, reconnect preference, vpn, radio and cancellation: passed (commands mocked)')
