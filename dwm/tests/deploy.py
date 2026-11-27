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
    git = bindir / 'git'
    git.write_text('#!/bin/sh\nset -eu\nprintf "%s\\n" "$*" >> "$TRACE"\n'
                   'case "$1:$3" in\n'
                   'clone:1) mkdir -p "$5/.git"; touch "$5/ss" ;;\n'
                   '-C:config) printf "git:void717doll/bin.git\\n" ;;\n'
                   '-C:pull) [ "$4" = --ff-only ] ;;\n'
                   '-C:archive) tar -cf "$6" -C "$2" ss ;;\n'
                   '*) exit 1 ;;\nesac\n')
    git.chmod(0o755)
    home = root / 'home'
    home.mkdir()
    env = dict(os.environ, HOME=str(home), PATH=f'{bindir}:{os.environ["PATH"]}',
               PREFIX=str(prefix), TRACE=str(trace))
    subprocess.run([project / 'scripts/deploy.sh', '--session'], env=env,
                   check=True, capture_output=True, text=True, timeout=30)

    assert sorted(path.name for path in sessions.iterdir()) == ['jos.desktop']
    desktop = (sessions / 'jos.desktop').read_text()
    assert 'Name=jOS\n' in desktop
    assert f'Exec={prefix}/bin/dwm-session\n' in desktop
    assert 'DesktopNames=dwm\n' in desktop

    calls = trace.read_text().splitlines()
    assert calls[0] == f'clone --depth 1 https://github.com/void717doll/bin {home / "bin"}'
    assert (home / 'bin/.git').is_dir()
    assert calls[2] == 'make install'
    assert not Path(calls[1].split()[-2]).parent.exists()
    remove = next(call for call in calls if call.startswith('rm -f '))
    for name in ('dwm.desktop', 'dwm-local.desktop', 'jwm.desktop',
                 'jwm-local.desktop', 'jvm.desktop', 'jvm-local.desktop'):
        assert f'/usr/share/xsessions/{name}' in remove
    assert any(call.endswith(f'{prefix}/share/xsessions/jos.desktop /usr/share/xsessions/jos.desktop')
               for call in calls)

    trace.write_text('')
    subprocess.run([project / 'scripts/deploy.sh'], env=env,
                   check=True, capture_output=True, text=True, timeout=30)
    calls = trace.read_text().splitlines()
    assert calls[1] == f'-C {home / "bin"} pull --ff-only'
    assert calls[3] == 'make install'

print('session deployment removes duplicate wm entries and installs jOS with dwm: passed (sudo mocked)')
