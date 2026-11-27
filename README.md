# jOS

a small x11 desktop bundle. 

configs are bundled in `dwm/dunstrc` and `dwm/redshift.conf`.

## install

on debian, ubuntu, or arch, run:

```sh
./deploy.sh --deps --session --lock --fonts
```

log out, choose **jOS** from the x11 session menu, and log back in. without a
display manager, add this to `~/.xinitrc`:

```sh
exec "$HOME/.local/bin/dwm-session"
```

install updates with `./deploy.sh`. the default prefix is `~/.local`; set
`PREFIX` to change it. deploy clones [bin](https://github.com/void717doll/bin)
into `~/bin` or updates the existing checkout, then runs `sudo make install`
to install capture helpers in `/usr/local/bin`.
use `DESTDIR` to stage a package without installing the helpers. run
`./deploy.sh --help` for options.

## tools

- `dwm`: customized window manager with cherry, berry, the status bar, and
  desktop helpers.
- `dmenu`: application launcher.
- `st`: terminal.
- `tabbed`: grouped windows.
- `slock`: screen locker.

settings live in the source configs. user settings and session state go under
`~/.config/dwm` and `~/.local/state/dwm`.

## keybinds

`super` is the windows key. `alt` is left alt.

| keys | action |
| --- | --- |
| `super` + `enter` | open terminal |
| `super` + `space` | open app launcher |
| `super` + `shift` + `t` / `f` | cherry tiling / berry floating |
| `super` + `shift` + `p` | show or hide window titles |
| `super` + `shift` + `c` | center the floating window |
| `super` + `ctrl` + `space` | toggle floating for the focused window |
| `super` + arrow keys | snap the focused window to an edge |
| `alt` + `tab` / `shift` + `alt` + `tab` | switch visible windows |
| `super` + `w` / top-left screen corner | open the clickable dmenu window list |
| `super` + `1`–`5` | switch workspace |
| `super` + `shift` + `1`–`5` | move window to workspace |
| `super` + `n` / `super` + `shift` + `n` | wifi / bluetooth menu |
| `super` + `l` | lock screen |
| `super` + `shift` + `y` | take a screenshot with `ss` |
| `super` + `shift` + `r` | start or stop recording with `rec` |

grouped windows use tabbed controls: `ctrl` + `tab` switches tabs,
`ctrl` + `shift` + `tab` switches backwards, and `ctrl` + `shift` + `w`
closes the selected tab. in floating mode, hold `super` and drag a tabbed window
to move the group.

dmenu results can be selected by hovering and clicking. the dwm bar leaves
window titles to tabbed for grouped windows, so the active tab title is not
shown twice. closing the last tab shuts down its container cleanly.

## credits

- dwm, dmenu, st, tabbed, and slock: suckless and their contributors.
- berry: joshua ervin; floating movement and frames. see `dwm/LICENSE.berry`.
- dwm: alttab (viliamkovac1223), brightlight (elbachir-one), fancybar,
  colorfly, i3 master toggle, [fullgaps](https://dwm.suckless.org/patches/fullgaps/),
  and [systray](https://dwm.suckless.org/patches/systray/) (jan christoph
  ebersbach and contributors).
- dmenu: priority and bumbers by miles alan.
- st: xresources, alpha, anysize, blinking cursor, desktopentry, scrollback,
  autocomplete, clickurl, and drag-and-drop.
- slock: [auto-timeout](https://tools.suckless.org/slock/patches/auto-timeout/).

changes are integrated into the tools; no patch files are bundled. licenses
stay beside each tool's source.
