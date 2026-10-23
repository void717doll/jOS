#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import tempfile

project = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix='dwm-mount-') as directory:
    root = Path(directory)
    bindir = root / 'bin'
    bindir.mkdir()
    trace = root / 'trace'
    notice = root / 'notice'
    mounted = root / 'mounted'
    unmounted = root / 'unmounted'
    helper = bindir / 'dwm-mount'
    helper.write_text((project / 'scripts' / 'dwm-mount').read_text())
    helper.chmod(0o755)
    (bindir / 'lsblk').write_text(r'''#!/bin/sh
case "$*" in
'--noheadings --paths --output NAME,TYPE,FSTYPE')
    printf '/dev/sdb disk \n/dev/sdb1 part ext4\n/dev/nvme0n1p2 part ext4\n';;
'--noheadings --nodeps --output TYPE /dev/sdb1') printf 'part\n';;
'--noheadings --nodeps --output TYPE /dev/sdb') printf 'disk\n';;
'--noheadings --nodeps --output TYPE /dev/nvme0n1p2') printf 'part\n';;
'--noheadings --nodeps --output FSTYPE /dev/sdb1'|'--noheadings --nodeps --output FSTYPE /dev/nvme0n1p2') printf 'ext4\n';;
'--noheadings --nodeps --output FSTYPE /dev/sdb') printf '\n';;
'--noheadings --nodeps --output LABEL /dev/sdb1') printf 'USB_DATA\n';;
*) exit 1;;
esac
''')
    (bindir / 'lsblk').chmod(0o755)
    (bindir / 'findmnt').write_text(r'''#!/bin/sh
[ -f "$MOUNTED" ] && [ ! -f "$UNMOUNTED" ] || exit 1
[ "$2" = /dev/sdb1 ] || exit 1
printf '/run/media/mia/USB_DATA\n'
''')
    (bindir / 'findmnt').chmod(0o755)
    (bindir / 'udevadm').write_text(r'''#!/bin/sh
case "$*" in
'info --query=property --name=/dev/sdb') printf 'ID_BUS=usb\nID_MODEL=USB_Drive\n';;
'info --query=property --name=/dev/sdc') printf 'ID_BUS=usb\nID_MODEL=New_Drive\n';;
'info --query=property --name=/dev/sdb1') printf 'ID_BUS=usb\nID_FS_LABEL=USB_DATA\n';;
'info --query=property --name=/dev/nvme0n1p2') printf 'ID_BUS=ata\n';;
monitor*)
    touch "$UNMOUNTED"
    printf 'ACTION=change\nDEVNAME=/dev/sdb1\nDEVTYPE=partition\nID_BUS=usb\n\n'
    printf 'ACTION=add\nDEVNAME=/dev/sdc\nDEVTYPE=disk\nID_BUS=usb\n\n'
    printf 'ACTION=remove\nDEVNAME=/dev/sdb\nDEVTYPE=disk\nID_BUS=usb\n\n';;
*) exit 1;;
esac
''')
    (bindir / 'udevadm').chmod(0o755)
    (bindir / 'udisksctl').write_text(r'''#!/bin/sh
case "$*" in
'mount --block-device /dev/sdb1')
    printf '%s\n' "$*" >> "$TRACE"
    touch "$MOUNTED"
    printf 'Mounted /dev/sdb1 at /run/media/mia/USB_DATA.\n';;
*) exit 1;;
esac
''')
    (bindir / 'udisksctl').chmod(0o755)
    (bindir / 'dunstify').write_text('#!/bin/sh\nprintf "%s\n" "$*" >> "$NOTICE"\n')
    (bindir / 'dunstify').chmod(0o755)
    env = dict(os.environ, PATH=str(bindir) + ':/usr/bin:/bin', TRACE=str(trace), NOTICE=str(notice),
               MOUNTED=str(mounted), UNMOUNTED=str(unmounted), XDG_STATE_HOME=str(root / 'state'))

    result = subprocess.run([helper, '--once'], env=env, capture_output=True, text=True, timeout=5)
    assert result.returncode == 0, result.stderr
    assert trace.read_text().splitlines() == ['mount --block-device /dev/sdb1']
    assert 'disk mounted' in notice.read_text() and '/run/media/mia/USB_DATA' in notice.read_text()

    notice.write_text('')
    result = subprocess.run([helper], env=env, capture_output=True, text=True, timeout=5)
    assert result.returncode == 0, result.stderr
    messages = notice.read_text()
    assert 'disk unmounted' in messages and 'USB_DATA' in messages
    assert 'device connected' in messages and 'USB Drive' in messages
    assert 'device disconnected' in messages

print('mount, unmount, usb connection, removal and non-usb filtering: passed (commands mocked)')
