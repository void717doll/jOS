#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
prefix=${PREFIX:-"$HOME/.local"}
dependencies=false
session=false
locker=false
fonts=false
while [ "$#" -gt 0 ]; do
	case "$1" in
	--deps) dependencies=true ;;
	--session) session=true ;;
	--lock) locker=true ;;
	--fonts) fonts=true ;;
	--help)
		printf 'usage: ./deploy.sh [--deps] [--session] [--lock] [--fonts]\n\n'
		printf 'build and install the jOS desktop bundle to PREFIX (default: ~/.local).\n'
		printf '  --deps     install debian, ubuntu, or arch dependencies using sudo\n'
		printf '  --session  add jOS to the system x11 session menu using sudo\n'
		printf '  --fonts    download and install the ttf-all-the-icons font collection\n'
		printf '  --lock     enable the bundle screen locker using sudo\n'
		printf 'DESTDIR stages files; it cannot be combined with --deps, --session or --lock.\n'
		exit 0 ;;
	*) printf 'unknown option: %s\n' "$1" >&2; exit 1 ;;
	esac
	shift
done
if [ -n "${DESTDIR:-}" ] && { [ "$dependencies" = true ] || [ "$session" = true ] || [ "$locker" = true ]; }; then
	printf 'jOS: staging cannot change system packages, login sessions or ~/bin\n' >&2
	exit 1
fi
if [ "$(id -u)" = 0 ]; then
	printf 'jOS: run as your desktop user; sudo is used only for system changes\n' >&2
	exit 1
fi
case "$prefix" in
/*) ;;
*) printf 'jOS: PREFIX must be an absolute path\n' >&2; exit 1 ;;
esac
case "$prefix" in
*[!a-zA-Z0-9_./-]*) printf 'jOS: PREFIX must not contain spaces or shell characters\n' >&2; exit 1 ;;
esac
for project in "$source_dir" "$source_dir/../dmenu" "$source_dir/../tabbed" "$source_dir/../st" "$source_dir/../slock"; do
	[ -f "$project/Makefile" ] || { printf 'jOS: missing bundle component: %s\n' "$project" >&2; exit 1; }
done
if [ "$dependencies" = true ]; then
	. /etc/os-release
	case "$ID ${ID_LIKE:-}" in
	*debian*|*ubuntu*)
		sudo apt-get update
		sudo apt-get install -y build-essential pkg-config ncurses-bin libx11-dev libxinerama-dev libxft-dev \
			libxrandr-dev libxext-dev libcrypt-dev perl python3 xdg-utils \
			libfontconfig1-dev libxrender-dev bash util-linux udev udisks2 libglib2.0-bin dbus-x11 dunst picom redshift xwallpaper \
			x11-utils x11-xserver-utils xinput xinit xserver-xorg-core fonts-dejavu-core fontconfig \
			network-manager bluez lxpolkit iputils-ping \
			pulseaudio-utils sound-theme-freedesktop brightnessctl copyq xss-lock flameshot xclip curl unicode-data \
			dolphin gimp nextcloud-desktop gnome-keyring libpam-gnome-keyring
		;;
	*arch*)
		sudo pacman -S --needed --noconfirm base-devel pkgconf ncurses libx11 libxinerama libxft libxrandr libxext \
			libxrender libxcrypt fontconfig freetype2 perl python xdg-utils bash util-linux systemd udisks2 glib2 dbus \
			dunst picom redshift xwallpaper xorg-xrandr xorg-xsetroot xorg-xset xorg-xinput xorg-xinit xorg-server \
			ttf-dejavu networkmanager bluez lxsession iputils libpulse sound-theme-freedesktop brightnessctl copyq xss-lock flameshot \
			xclip curl unicode-character-database dolphin gimp nextcloud-client gnome-keyring
		;;
	*) printf 'jOS: --deps supports debian, ubuntu, and arch\n' >&2; exit 1 ;;
	esac
	if command -v systemctl >/dev/null 2>&1; then
		sudo systemctl enable --now NetworkManager.service
		sudo systemctl enable --now bluetooth.service
	fi
fi
if [ "$fonts" = true ]; then
	PREFIX="$prefix" DESTDIR="${DESTDIR:-}" "$script_dir/install-fonts.sh"
fi
make -C "$source_dir" -f desktop.mk install PREFIX="$prefix" DESTDIR="${DESTDIR:-}"
if [ "$locker" = true ]; then
	sudo install -o root -g root -m 4755 "$source_dir/../slock/slock" /usr/local/bin/dwm-slock
fi
if [ "$session" = true ]; then
	for name in dwm dwm-local jwm jwm-local jvm jvm-local; do
		rm -f "$prefix/share/xsessions/$name.desktop"
	done
	sudo rm -f /usr/share/xsessions/dwm.desktop /usr/share/xsessions/dwm-local.desktop \
		/usr/share/xsessions/jwm.desktop /usr/share/xsessions/jwm-local.desktop \
		/usr/share/xsessions/jvm.desktop /usr/share/xsessions/jvm-local.desktop
	sudo install -Dm644 "$prefix/share/xsessions/jos.desktop" /usr/share/xsessions/jos.desktop
fi
printf '\njOS installed to %s%s\n' "${DESTDIR:-}" "$prefix"
if [ -z "${DESTDIR:-}" ]; then
	"$prefix/bin/dwm-check" || :
	printf '\nlog out and select jOS in the x11 session menu.\n'
fi
