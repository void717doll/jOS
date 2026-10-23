#!/usr/bin/env python3
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

project = Path(__file__).resolve().parents[1]


def script(path, body):
    path.write_text(body)
    path.chmod(0o755)


with tempfile.TemporaryDirectory(prefix='dwm-bluetooth-') as directory:
    root = Path(directory)
    bindir = root / 'bin'
    bindir.mkdir()
    helper = bindir / 'dwm-bluetooth'
    shutil.copy2(project / 'scripts' / 'dwm-bluetooth', helper)
    shutil.copy2(project / 'scripts' / 'dwm-action', bindir / 'dwm-action')
    trace, menus, choices, state = [root / name for name in ('trace', 'menus', 'choices', 'state')]
    address = 'AA:BB:CC:DD:EE:FF'
    name = 'headphones $(touch should-not-exist)'
    label = f'{name} [{address}]'
    env = dict(os.environ, PATH=f'{bindir}:/usr/bin:/bin', TRACE=str(trace), MENUS=str(menus),
               CHOICES=str(choices), STATE=str(state), DEVICE_NAME=name, ADDRESS=address)
    script(bindir / 'bluetoothctl', '''#!/usr/bin/env python3
import json, os, sys
from pathlib import Path
a = sys.argv[1:]
state = Path(os.environ['STATE'])
s = json.loads(state.read_text())
with open(os.environ['TRACE'], 'a') as out:
    out.write(json.dumps(a) + '\\n')
if os.environ.get('FAIL') in a:
    sys.exit(1)
if a == ['show']:
    if os.environ.get('NO_CONTROLLER'):
        print('No default controller available')
    else:
        print('Controller 00:11:22:33:44:55')
        print('Alias: Powered: yes')
        print('Powered: ' + s['powered'])
elif a == ['devices']:
    print('Device ' + os.environ['ADDRESS'] + ' ' + os.environ['DEVICE_NAME'])
    print('Device 11:22:33:44:55:66 ' + os.environ['DEVICE_NAME'])
    print('Device invalid ignored')
elif a == ['info', os.environ['ADDRESS']]:
    print('Name: ' + os.environ['DEVICE_NAME'])
    for prop in ('paired', 'connected', 'trusted'):
        print(prop.capitalize() + ': ' + s[prop])
elif a == ['--agent', 'KeyboardDisplay']:
    assert sys.stdin.readline().strip() == 'pair ' + os.environ['ADDRESS']
    assert sys.stdin.readline().strip() == 'yes'
    assert sys.stdin.readline().strip() == 'quit'
    if not os.environ.get('PAIR_CANCEL'):
        s['paired'] = 'yes'
elif a[0] == 'power':
    s['powered'] = 'yes' if a[1] == 'on' else 'no'
elif a[0] in ('connect', 'disconnect', 'trust', 'untrust', 'remove'):
    assert a[1:] == [os.environ['ADDRESS']]
    if a[0] in ('connect', 'disconnect'):
        s['connected'] = 'yes' if a[0] == 'connect' else 'no'
    elif a[0] in ('trust', 'untrust'):
        s['trusted'] = 'yes' if a[0] == 'trust' else 'no'
    else:
        s['paired'] = s['trusted'] = s['connected'] = 'no'
elif a != ['--timeout', '8', 'scan', 'on']:
    sys.exit(9)
state.write_text(json.dumps(s))
''')
    script(bindir / 'dmenu', '''#!/usr/bin/env python3
import json, os, sys
from pathlib import Path
with open(os.environ['MENUS'], 'a') as out:
    out.write(json.dumps(sys.stdin.read().splitlines()) + '\\n')
path = Path(os.environ['CHOICES'])
choices = json.loads(path.read_text())
if not choices or choices[0] is None:
    sys.exit(1)
print(choices.pop(0))
path.write_text(json.dumps(choices))
''')
    script(bindir / 'st', '#!/bin/sh\n[ "$1" = -e ] || exit 1\nshift\nexec "$@"\n')
    script(bindir / 'dunstify', '#!/bin/sh\nexit 0\n')

    def run(selections=(), args=(), **values):
        trace.write_text('')
        menus.write_text('')
        choices.write_text(json.dumps(selections))
        state.write_text(json.dumps({prop: values.pop(prop, 'no') for prop in
                                    ('paired', 'connected', 'trusted')} |
                                   {'powered': values.pop('powered', 'yes')}))
        command = [helper, *args] if args else [bindir / 'dwm-action', 'bluetooth']
        result = subprocess.run(command, env=dict(env, **values), cwd=root,
                                input='yes\nquit\n', capture_output=True, text=True, timeout=5)
        return result, [json.loads(line) for line in trace.read_text().splitlines()]

    for powered, text, color in (('yes', 'on', 4), ('no', 'off', 5)):
        result, calls = run(args=['--status'], powered=powered)
        assert result.returncode == 0 and result.stdout == f' {text}\n'
        assert calls == [['show']] and not menus.read_text()
        result, calls = run(args=['--color'], powered=powered)
        assert result.returncode == 0 and result.stdout == f'\x18{chr(color)} {text}\x19\x01\n'
    for values in ({'FAIL': 'show'}, {'NO_CONTROLLER': 'yes'}):
        result, calls = run(args=['--color'], **values)
        assert result.returncode == 0 and result.stdout == '\x18\x05 n/a\x19\x01\n'
        assert not result.stderr and not menus.read_text()

    result, calls = run([label, 'pair and connect'])
    assert result.returncode == 0, result.stderr
    assert ['--agent', 'KeyboardDisplay'] in calls
    assert calls[-2:] == [['trust', address], ['connect', address]]
    assert json.loads(state.read_text())['paired'] == 'yes'
    assert not (root / 'should-not-exist').exists()
    first_menu = json.loads(menus.read_text().splitlines()[0])
    assert first_menu == [label, f'{name} [11:22:33:44:55:66]',
                          'scan for devices', 'disable bluetooth', 'advanced']
    result, calls = run([label, 'pair and connect'], PAIR_CANCEL='yes')
    assert result.returncode != 0
    assert ['trust', address] not in calls and ['connect', address] not in calls
    result, calls = run(args=['--pair', address], paired='yes')
    assert result.returncode == 0 and ['--agent', 'KeyboardDisplay'] not in calls
    assert run(args=['--pair', 'invalid'])[0].returncode != 0

    # device names must not be mistaken for connection or pairing properties.
    misleading_name = 'speaker Connected: yes Paired: yes Trusted: yes'
    result, calls = run([f'{misleading_name} [{address}]', 'pair and connect'],
                        DEVICE_NAME=misleading_name)
    assert result.returncode == 0 and ['--agent', 'KeyboardDisplay'] in calls
    device_menu = json.loads(menus.read_text().splitlines()[1])
    assert device_menu == ['pair and connect', 'trust', 'forget']

    for action, values in (('connect', {'paired': 'yes'}), ('disconnect', {'connected': 'yes'}),
                           ('trust', {}), ('untrust', {'trusted': 'yes'})):
        result, calls = run([label, action], **values)
        assert result.returncode == 0 and calls[-1] == [action, address]
        result, calls = run([label, action], FAIL=action, **values)
        assert result.returncode != 0
    result, calls = run([label, 'forget', 'forget'], paired='yes')
    assert result.returncode == 0 and calls[-1] == ['remove', address]
    for selections in ([], [''], ['arbitrary'], [label, None], [label, 'forget', 'cancel']):
        result, calls = run(selections)
        assert result.returncode == 0
        assert all(call[0] in ('show', 'devices', 'info') for call in calls)
    result, calls = run(['scan for devices', None])
    assert result.returncode == 0 and ['--timeout', '8', 'scan', 'on'] in calls
    assert calls.count(['devices']) == 2
    assert run(['scan for devices'], FAIL='scan')[0].returncode != 0
    result, calls = run(['enable bluetooth', None], powered='no')
    assert result.returncode == 0 and ['power', 'on'] in calls
    assert json.loads(menus.read_text().splitlines()[0]) == ['enable bluetooth', 'advanced']
    result, calls = run(['disable bluetooth'])
    assert result.returncode == 0 and calls[-1] == ['power', 'off']
    for values in ({'FAIL': 'show'}, {'FAIL': 'devices'}, {'NO_CONTROLLER': 'yes'}):
        assert run(**values)[0].returncode != 0

print('bluetooth menus, pairing prompts, device actions, scanning, power, cancellation and errors: passed (commands mocked)')
