#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import tempfile
import time

project = Path(__file__).resolve().parents[1]


def script(path, body):
    path.write_text('#!/bin/sh\nset -eu\n' + body)
    path.chmod(0o755)


with tempfile.TemporaryDirectory(prefix='dmenu-appimages-') as directory:
    root = Path(directory)
    bindir = root / 'bin'
    bindir.mkdir()
    applications = root / 'data/applications'
    applications.mkdir(parents=True)
    image = root / 'example app (1).AppImage'
    trace = root / 'launch'
    script(image, 'printf "%s\\n" "$@" > "$TRACE"\n')
    script(bindir / 'dmenu_path', 'printf "terminal\\n"\n')
    script(bindir / 'dmenu', 'cat > "$MENU"\n[ "${CANCEL:-0}" = 0 ] || exit 1\nprintf "%s\\n" "$CHOICE"\n')
    env = dict(os.environ, PATH=f'{bindir}:/usr/bin:/bin', SHELL='/bin/sh',
               XDG_DATA_HOME=str(root / 'data'), XDG_DATA_DIRS=str(root / 'system'),
               MENU=str(root / 'menu'), TRACE=str(trace))

    def entry(filename, name, extra=''):
        path = applications / filename
        path.write_text(f'[Desktop Entry]\nType=Application\nName={name}\n'
                        f'Exec="{image}" --no-sandbox "two words" %U\n'
                        f'TryExec={image}\n{extra}')
        return path

    entry('appimagekit_example.desktop', 'example app',
          '[Desktop Action Remove]\nName=delete this app\nExec=/bin/false\n')
    entry('appimagekit_hidden.desktop', 'hidden app', 'Hidden=true\n')
    entry('appimagekit_nodisplay.desktop', 'hidden helper', 'NoDisplay=true\n')
    missing = entry('appimagekit_missing.desktop', 'missing app')
    missing.write_text(missing.read_text().replace(f'TryExec={image}', f'TryExec={root}/missing'))

    def run(choice='', **settings):
        return subprocess.run([project / 'dmenu_run', '-i'], env=dict(env, CHOICE=choice, **settings),
                              capture_output=True, text=True, timeout=10)

    result = run(CANCEL='1')
    assert result.returncode == 0, result.stderr
    menu = (root / 'menu').read_text().splitlines()
    assert menu == ['example app [appimage]', 'terminal'], menu
    assert not trace.exists(), 'cancelling launched an app'

    result = run('example app [appimage]')
    assert result.returncode == 0, result.stderr
    for _ in range(100):
        if trace.exists():
            break
        time.sleep(0.01)
    assert trace.read_text().splitlines() == ['--no-sandbox', 'two words']

    assert run('printf command > "$TRACE"').returncode == 0
    assert trace.read_text() == 'command'
    assert run().returncode == 0
    assert trace.read_text() == 'command', 'empty input launched an app'

    added = entry('appimagekit_added.desktop', 'new app')
    assert run(CANCEL='1').returncode == 0
    assert 'new app [appimage]' in (root / 'menu').read_text()
    added.unlink()
    assert run(CANCEL='1').returncode == 0
    assert 'new app [appimage]' not in (root / 'menu').read_text()

print('appimage discovery, gio launch, arguments, cancellation and shell commands: passed (dmenu mocked)')
