#!/bin/sh
set -eu

prefix=${PREFIX:-"$HOME/.local"}
destination=${DESTDIR:-}$prefix/share/fonts/ttf-all-the-icons
source_url=https://raw.githubusercontent.com/domtronn/all-the-icons.el/master
for command in curl fc-scan fc-cache; do
	command -v "$command" >/dev/null 2>&1 || { printf 'missing: %s\n' "$command" >&2; exit 1; }
done
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM
# finish and validate all downloads before replacing installed fonts.
for font in all-the-icons file-icons fontawesome material-design-icons octicons weathericons; do
	curl --fail --location --silent --show-error --connect-timeout 10 --max-time 120 \
		"$source_url/fonts/$font.ttf" -o "$temporary/$font.ttf"
	family=$(fc-scan --format '%{family}' "$temporary/$font.ttf")
	[ -n "$family" ] || { printf 'invalid font: %s\n' "$font" >&2; exit 1; }
done
curl --fail --location --silent --show-error --connect-timeout 10 --max-time 60 \
	"$source_url/README.md" -o "$temporary/README.md"
mkdir -p "$destination"
install -m 644 "$temporary/"*.ttf "$temporary/README.md" "$destination/"
if [ -z "${DESTDIR:-}" ]; then fc-cache -f "$destination"; fi
printf 'installed ttf-all-the-icons to %s\n' "$destination"
