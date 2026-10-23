#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import tempfile

project = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix='dwm-deploy-') as directory:
    root = Path(directory)
    prefix = root / 'prefix'
    sessions = prefix / 'share/xsessions'
    sessions.mkdir(parents=True)
    for name in ('dwm', 'dwm-local', 'jwm', 'jwm-local', 'jvm', 'jvm-local'):
        (sessions / f'{name}.desktop').touch()

    bindir = root / 'bin'
    bindir.mkdir()
    trace = root / 'sudo'
    sudo = bindir / 'sudo'
    sudo.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$TRACE"\n')
    sudo.chmod(0o755)
    env = dict(os.environ, PATH=f'{bindir}:{os.environ["PATH"]}', PREFIX=str(prefix), TRACE=str(trace))
    subprocess.run([project / 'scripts/deploy.sh', '--session'], env=env,
                   check=True, capture_output=True, text=True, timeout=30)

    assert sorted(path.name for path in sessions.iterdir()) == ['jos.desktop']
    desktop = (sessions / 'jos.desktop').read_text()
    assert 'Name=jOS\n' in desktop
    assert f'Exec={prefix}/bin/dwm-session\n' in desktop
    assert 'DesktopNames=dwm\n' in desktop

    calls = trace.read_text().splitlines()
    remove = next(call for call in calls if call.startswith('rm -f '))
    for name in ('dwm.desktop', 'dwm-local.desktop', 'jwm.desktop',
                 'jwm-local.desktop', 'jvm.desktop', 'jvm-local.desktop'):
        assert f'/usr/share/xsessions/{name}' in remove
    assert any(call.endswith(f'{prefix}/share/xsessions/jos.desktop /usr/share/xsessions/jos.desktop')
               for call in calls)

print('session deployment removes duplicate wm entries and installs jOS with dwm: passed (sudo mocked)')
