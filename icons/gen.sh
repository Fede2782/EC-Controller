#!/usr/bin/env bash
#
# gen.sh - regenerate the installable icon set from the source SVGs.
#
# Usage:
#   icons/gen.sh                          (works from any directory)
#   ICON_RENDERER=rsvg-convert icons/gen.sh
#
# Sources, next to this script:
#   ec-controller.svg                   full-colour application icon
#   ec-controller-tray-breeze.svg       tray glyph for LIGHT panels (dark, #232629)
#   ec-controller-tray-breeze-dark.svg  tray glyph for DARK panels (light, #eff0f1)
#
# The file names follow the Breeze theme they're installed into: Breeze is
# the light theme, so its tray icon is the dark glyph, and vice versa. Both
# tray SVGs style their path with Plasma's ColorScheme-Text class, so
# Plasma recolours them to the panel anyway; the fixed colours matter for
# everything else.
#
# ec-controller-flattened.svg is not used. Its path carries the
# ColorScheme-Text class but a hardcoded fill (#0d8773), so it would not
# recolour the way a symbolic icon should.
#
# Output: icons/icons/{hicolor,breeze,breeze-dark,pixmaps}, rebuilt from
# scratch on every run so no stale file survives a layout change. That tree
# is committed and installed as-is by the top-level CMakeLists.txt, so
# building and installing the project never needs an SVG renderer - only
# running this script does.
#
# Requires one of: inkscape (1.x), rsvg-convert (librsvg), resvg.
#   ICON_RENDERER=<name> forces one; otherwise the first found in that order.
# Optional:
#   ImageMagick (magick, or IM6's convert) -> pixmaps/ec-controller.ico
#   png2icns (libicns)                      -> pixmaps/ec-controller.icns
# When an optional tool is missing, the previous run's file is kept rather
# than silently dropped from the committed tree.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

APP_NAME="ec-controller"

APP_SVG="$SCRIPT_DIR/ec-controller.svg"
TRAY_FOR_LIGHT_PANELS_SVG="$SCRIPT_DIR/ec-controller-tray-breeze.svg"
TRAY_FOR_DARK_PANELS_SVG="$SCRIPT_DIR/ec-controller-tray-breeze-dark.svg"

OUT_DIR="$SCRIPT_DIR/icons"

APP_PNG_SIZES=(16 22 24 32 48 64 128 256 512)   # hicolor/<N>x<N>/apps
BREEZE_APP_SIZES=(16 22 24 32)                   # breeze*/apps/<N>
TRAY_SIZES=(16 22 24)                            # tray icons, every theme
ICO_SIZES=(16 24 32 48 64 128 256)
ICNS_SIZES=(16 32 64 128 256 512)

# -------------------------------------------------------------------
# Helpers
# -------------------------------------------------------------------

die()  { echo "gen.sh: error: $*" >&2; exit 1; }
note() { echo "gen.sh: $*"; }

have() { command -v "$1" >/dev/null 2>&1; }

pick_renderer() {
    if [[ -n "${ICON_RENDERER:-}" ]]; then
        case "$ICON_RENDERER" in
            inkscape|rsvg-convert|resvg) ;;
            *) die "ICON_RENDERER must be inkscape, rsvg-convert or resvg (got '$ICON_RENDERER')" ;;
        esac
        have "$ICON_RENDERER" || die "ICON_RENDERER=$ICON_RENDERER is not installed"
        echo "$ICON_RENDERER"
        return
    fi

    local candidate
    for candidate in inkscape rsvg-convert resvg; do
        if have "$candidate"; then
            echo "$candidate"
            return
        fi
    done
    die "no SVG renderer found - install inkscape, librsvg (rsvg-convert) or resvg"
}

# place <source> <destination>: copy an SVG into the tree as-is.
place() {
    mkdir -p "$(dirname "$2")"
    cp "$1" "$2"
}

# render <source.svg> <size> <destination.png>
render() {
    local src="$1" size="$2" dest="$3"
    mkdir -p "$(dirname "$dest")"

    case "$RENDERER" in
        inkscape)
            inkscape "$src" --export-type=png --export-filename="$dest" \
                -w "$size" -h "$size" >/dev/null 2>&1 \
                || die "inkscape failed rendering $src at ${size}px"
            ;;
        rsvg-convert)
            rsvg-convert -w "$size" -h "$size" "$src" -o "$dest" \
                || die "rsvg-convert failed rendering $src at ${size}px"
            ;;
        resvg)
            resvg -w "$size" -h "$size" "$src" "$dest" \
                || die "resvg failed rendering $src at ${size}px"
            ;;
    esac

    # A renderer that exits 0 without writing anything would otherwise
    # leave a hole in the committed tree that nobody notices.
    [[ -s "$dest" ]] || die "$RENDERER produced no output for $dest"
}

# keep_previous <path relative to the output tree> <missing tool>
keep_previous() {
    local rel="$1" tool="$2"
    if [[ -f "$OUT_DIR/$rel" ]]; then
        mkdir -p "$(dirname "$STAGE/$rel")"
        cp "$OUT_DIR/$rel" "$STAGE/$rel"
        note "$tool not found - kept the existing $rel"
    else
        note "$tool not found - skipped $rel"
    fi
}

# -------------------------------------------------------------------
# Setup
# -------------------------------------------------------------------

for src in "$APP_SVG" "$TRAY_FOR_LIGHT_PANELS_SVG" "$TRAY_FOR_DARK_PANELS_SVG"; do
    [[ -f "$src" ]] || die "missing source SVG: $src"
done

RENDERER="$(pick_renderer)"
note "rendering with $RENDERER"

# Everything is built in a staging directory next to the output and only
# swapped in once complete, so a failed run leaves the previous tree intact.
STAGE="$(mktemp -d "$SCRIPT_DIR/.icons-staging.XXXXXX")"
WORK="$(mktemp -d)"
trap 'rm -rf "$STAGE" "$WORK"' EXIT

# -------------------------------------------------------------------
# hicolor - the fallback theme every desktop searches
# -------------------------------------------------------------------

note "hicolor..."

place "$APP_SVG" "$STAGE/hicolor/scalable/apps/$APP_NAME.svg"
for size in "${APP_PNG_SIZES[@]}"; do
    render "$APP_SVG" "$size" "$STAGE/hicolor/${size}x${size}/apps/$APP_NAME.png"
done

# The tray icon's hicolor copy is the light glyph: outside Plasma, which
# recolours it anyway, panels are more often dark than light.
for name in "$APP_NAME-tray" "$APP_NAME-tray-symbolic"; do
    place "$TRAY_FOR_DARK_PANELS_SVG" "$STAGE/hicolor/scalable/status/$name.svg"
    for size in "${TRAY_SIZES[@]}"; do
        render "$TRAY_FOR_DARK_PANELS_SVG" "$size" "$STAGE/hicolor/${size}x${size}/status/$name.png"
    done
done

# Older names for the tray glyph, kept so nothing that still looks them up
# breaks. The current GUI only uses ec-controller-tray.
place "$TRAY_FOR_DARK_PANELS_SVG"  "$STAGE/hicolor/scalable/apps/${APP_NAME}indicator.svg"
place "$TRAY_FOR_LIGHT_PANELS_SVG" "$STAGE/hicolor/scalable/apps/${APP_NAME}indicatordark.svg"

# -------------------------------------------------------------------
# breeze / breeze-dark - KDE's own themes
# -------------------------------------------------------------------
# Breeze lays icons out as <context>/<size>, not hicolor's
# <size>x<size>/<context>. These directories only add files: the theme's
# index.theme belongs to breeze-icons and is never shipped here.

breeze_theme() {
    local theme="$1" tray_svg="$2" size

    note "$theme..."

    for size in "${BREEZE_APP_SIZES[@]}"; do
        place "$APP_SVG" "$STAGE/$theme/apps/$size/$APP_NAME-symbolic.svg"
    done

    for size in "${TRAY_SIZES[@]}"; do
        place "$tray_svg" "$STAGE/$theme/status/$size/$APP_NAME-tray.svg"
        place "$tray_svg" "$STAGE/$theme/status/$size/$APP_NAME-tray-symbolic.svg"
    done

    place "$APP_SVG" "$STAGE/$theme/preferences/32/$APP_NAME.svg"
}

breeze_theme breeze      "$TRAY_FOR_LIGHT_PANELS_SVG"
breeze_theme breeze-dark "$TRAY_FOR_DARK_PANELS_SVG"

# -------------------------------------------------------------------
# pixmaps - legacy /usr/share/pixmaps location
# -------------------------------------------------------------------

note "pixmaps..."

place "$APP_SVG"                  "$STAGE/pixmaps/$APP_NAME.svg"
place "$TRAY_FOR_DARK_PANELS_SVG" "$STAGE/pixmaps/$APP_NAME-white.svg"
render "$APP_SVG" 256 "$STAGE/pixmaps/$APP_NAME.png"
render "$APP_SVG" 128 "$STAGE/pixmaps/$APP_NAME-med.png"
render "$APP_SVG" 64  "$STAGE/pixmaps/$APP_NAME-small.png"

# ImageMagick 7 installs "magick"; IM6 only "convert".
IMAGEMAGICK=""
if have magick; then
    IMAGEMAGICK="magick"
elif have convert; then
    IMAGEMAGICK="convert"
fi

if [[ -n "$IMAGEMAGICK" ]]; then
    ico_frames=()
    for size in "${ICO_SIZES[@]}"; do
        render "$APP_SVG" "$size" "$WORK/ico/$size.png"
        ico_frames+=("$WORK/ico/$size.png")
    done
    "$IMAGEMAGICK" "${ico_frames[@]}" "$STAGE/pixmaps/$APP_NAME.ico" \
        || die "$IMAGEMAGICK failed building $APP_NAME.ico"
else
    keep_previous "pixmaps/$APP_NAME.ico" "ImageMagick"
fi

if have png2icns; then
    icns_frames=()
    for size in "${ICNS_SIZES[@]}"; do
        render "$APP_SVG" "$size" "$WORK/icns/icon_${size}x${size}.png"
        icns_frames+=("$WORK/icns/icon_${size}x${size}.png")
    done
    png2icns "$STAGE/pixmaps/$APP_NAME.icns" "${icns_frames[@]}" >/dev/null \
        || die "png2icns failed building $APP_NAME.icns"
else
    keep_previous "pixmaps/$APP_NAME.icns" "png2icns"
fi

# -------------------------------------------------------------------
# Swap the finished tree in
# -------------------------------------------------------------------

chmod 755 "$STAGE"
rm -rf "$OUT_DIR"
mv "$STAGE" "$OUT_DIR"

note "done - $(find "$OUT_DIR" -type f | wc -l | tr -d ' ') files in ${OUT_DIR#"$SCRIPT_DIR"/}/:"
for dir in hicolor breeze breeze-dark pixmaps; do
    printf '  %-12s %3d files\n' "$dir" "$(find "$OUT_DIR/$dir" -type f | wc -l)"
done
note "installed by the top-level CMakeLists.txt with \"cmake --install\"."
