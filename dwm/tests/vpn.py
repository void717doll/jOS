#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import tempfile

project = Path(__file__).resolve().parents[1]


def script(path, body):
    path.write_text('#!/bin/sh\nset -eu\n' + body)
    path.chmod(0o755)


with tempfile.TemporaryDirectory(prefix='dwm-vpn-') as directory:
    root = Path(directory)
    bindir = root / 'bin'
    bindir.mkdir()
    trace = root / 'trace'
    menu = root / 'menu'
    notice = root / 'notice'
    helper = bindir / 'dwm-vpn'
    # isolate sbin too, so missing-openvpn checks work on hosts with it installed.
    helper.write_text((project / 'scripts' / 'dwm-vpn').read_text().replace('/usr/sbin:/sbin', str(root / 'sbin')))
    helper.chmod(0o755)
    for command in ('timeout', 'tr'):
        (bindir / command).symlink_to('/usr/bin/' + command)
    env = dict(os.environ, PATH=str(bindir), TRACE=str(trace), MENU=str(menu), NOTICE=str(notice),
               NAME='office: \\vpn', CHOICE='1. connect: office: \\vpn')
    script(bindir / 'nmcli', r'''
printf '%s\n' "$*" >> "$TRACE"
case "$*" in
'-t -f UUID,TYPE connection show')
    [ "${READ_FAIL:-no}" = no ] || exit 1
    [ "${EMPTY:-no}" = no ] || exit 0
    printf 'wifi-id:802-11-wireless\nfirst:vpn\nsecond:vpn\nother:vpn\n';;
'-g UUID connection show --active') printf 'second\n';;
'-t -f TYPE,UUID connection show --active') printf 'vpn:second\nvpn:other\n';;
'-g vpn.service-type connection show uuid other') printf 'org.freedesktop.NetworkManager.openconnect\n';;
'-g vpn.service-type connection show uuid first'|'-g vpn.service-type connection show uuid second')
    printf 'org.freedesktop.NetworkManager.openvpn\n';;
'--escape no -g connection.id connection show uuid first'|'--escape no -g connection.id connection show uuid second')
    printf '%s\n' "$NAME";;
'--ask --wait 60 connection up uuid first')
    [ "${CONNECT_FAIL:-no}" = no ] || exit 1;;
'--wait 15 connection down uuid second'|'--wait 15 connection down uuid other')
    [ "${ACTION_FAIL:-no}" = no ] || exit 1;;
*) exit 9;;
esac
''')
    script(bindir / 'dmenu', '''while IFS= read -r line; do printf '%s\n' "$line"; done > "$MENU"
printf '%s\n' "$CHOICE"
exit "${CANCEL:-0}"
''')
    script(bindir / 'dunstify', 'printf "%s\\n" "$*" >> "$NOTICE"\n')
    script(bindir / 'st', '''printf 'terminal %s\n' "$*" >> "$TRACE"
[ "$1" = -e ]
shift
exec "$@"
''')

    def run(**values):
        trace.write_text('')
        menu.write_text('')
        notice.write_text('')
        return subprocess.run([helper], env=dict(env, **values), capture_output=True, text=True, timeout=5)

    assert run().returncode == 0
    assert not trace.read_text() and not menu.read_text() and not notice.read_text()
    script(bindir / 'openvpn', 'exit 99\n')
    result = run()
    assert result.returncode == 0, result.stderr
    assert menu.read_text().splitlines() == ['1. connect: office: \\vpn', '2. disconnect: office: \\vpn']
    assert f'terminal -e {helper} --connect first' in trace.read_text()
    assert '--ask --wait 60 connection up uuid first' in trace.read_text()
    assert 'connected to office: \\vpn' in notice.read_text()
    connect_trace = trace.read_text()
    connect_index = connect_trace.index('--ask --wait 60 connection up uuid first')
    assert connect_trace.index('--wait 15 connection down uuid second') < connect_index
    assert connect_trace.index('--wait 15 connection down uuid other') < connect_index

    result = run(CHOICE='2. disconnect: office: \\vpn')
    assert result.returncode == 0, result.stderr
    assert '--wait 15 connection down uuid second' in trace.read_text()
    assert 'terminal' not in trace.read_text()
    assert 'vpn disconnected' in notice.read_text()

    for values in ({'CANCEL': '1'}, {'CHOICE': 'arbitrary input'}, {'CHOICE': ''}):
        assert run(**values).returncode == 0
        assert 'connection up' not in trace.read_text() and 'connection down' not in trace.read_text()
        assert not notice.read_text()

    assert run(EMPTY='yes').returncode == 0
    assert not menu.read_text() and 'no saved openvpn profiles' in notice.read_text()
    assert run(READ_FAIL='yes').returncode != 0
    assert not menu.read_text() and 'could not read saved connections' in notice.read_text()
    assert run(CONNECT_FAIL='yes').returncode != 0
    assert 'could not connect vpn' in notice.read_text() and 'connected to' not in notice.read_text()
    assert run(CHOICE='2. disconnect: office: \\vpn', ACTION_FAIL='yes').returncode != 0
    assert 'could not disconnect vpn' in notice.read_text() and 'vpn disconnected' not in notice.read_text()

    assert run(NAME='line\nbreak\tname', CHOICE='1. connect: line break name').returncode == 0
    assert menu.read_text().splitlines()[0] == '1. connect: line break name'
    assert 'connection up uuid first' in trace.read_text()

print('optional openvpn, profile filtering, duplicate names, connect/passwords, disconnect, cancellation and errors: passed (commands mocked)')
