#!/usr/bin/env bash
set -euo pipefail

#
# generate-icons.sh
#
# Generates Linux app icons from:
#
#   msicontroller.svg
#   msicontroller-tray-dark.svg
#   msicontroller-tray-light.svg
#
# Requires:
#   inkscape OR rsvg-convert
#
# Usage:
#   ./generate-icons.sh
#

APP_NAME="msicontroller"

MAIN_SVG="msicontroller.svg"
TRAY_DARK_SVG="msicontroller-tray-breeze.svg" # the one with dark wallpaper
TRAY_LIGHT_SVG="msicontroller-tray-breeze-dark.svg" # the one with light wallpaper



# -------------------------------------------------------------------
# Output structure
# -------------------------------------------------------------------

OUT_DIR="icons"

# Standard app icon sizes
PNG_SIZES=(16 22 24 32 48 64 128 256 512)

# Breeze symbolic sizes
SYM_SIZES=(16 22 24 32)

# Tray icon sizes
TRAY_SIZES=(16 22 24)

# -------------------------------------------------------------------
# Detect renderer
# -------------------------------------------------------------------

render_svg() {
    local input="$1"
    local output="$2"
    local size="$3"

    if command -v inkscape >/dev/null 2>&1; then
        inkscape \
            "$input" \
            --export-type=png \
            --export-filename="$output" \
            -w "$size" \
            -h "$size" \
            >/dev/null 2>&1

    elif command -v rsvg-convert >/dev/null 2>&1; then
        rsvg-convert \
            -w "$size" \
            -h "$size" \
            "$input" \
            -o "$output"

    else
        echo "ERROR: install inkscape or librsvg (rsvg-convert)"
        exit 1
    fi
}

mkdir -p "$OUT_DIR"

# -------------------------------------------------------------------
# Hicolor tray/status icons
# -------------------------------------------------------------------

echo "Generating hicolor tray icons..."

mkdir -p "$OUT_DIR/hicolor/scalable/status"

# scalable SVG tray icons
cp "$TRAY_LIGHT_SVG" \
   "$OUT_DIR/hicolor/scalable/status/${APP_NAME}-tray.svg"

cp "$TRAY_LIGHT_SVG" \
   "$OUT_DIR/hicolor/scalable/status/${APP_NAME}-tray-symbolic.svg"

# raster tray PNGs
for size in "${TRAY_SIZES[@]}"; do

    dir="$OUT_DIR/hicolor/${size}x${size}/status"
    mkdir -p "$dir"

    render_svg \
        "$TRAY_LIGHT_SVG" \
        "$dir/${APP_NAME}-tray.png" \
        "$size"

    render_svg \
        "$TRAY_LIGHT_SVG" \
        "$dir/${APP_NAME}-tray-symbolic.png" \
        "$size"

done

# -------------------------------------------------------------------
# Hicolor scalable SVGs
# -------------------------------------------------------------------

echo "Generating scalable SVG icons..."

mkdir -p "$OUT_DIR/hicolor/scalable/apps"

cp "$MAIN_SVG" \
   "$OUT_DIR/hicolor/scalable/apps/${APP_NAME}.svg"

cp "$TRAY_LIGHT_SVG" \
   "$OUT_DIR/hicolor/scalable/apps/${APP_NAME}indicator.svg"

cp "$TRAY_DARK_SVG" \
   "$OUT_DIR/hicolor/scalable/apps/${APP_NAME}indicatordark.svg"

# -------------------------------------------------------------------
# Breeze symbolic app icons
# -------------------------------------------------------------------

echo "Generating Breeze symbolic icons..."

for theme in breeze breeze-dark; do
    for size in "${SYM_SIZES[@]}"; do

        dir="$OUT_DIR/$theme/apps/$size"
        mkdir -p "$dir"

        cp "$MAIN_SVG" \
           "$dir/${APP_NAME}-symbolic.svg"



    done
done

# -------------------------------------------------------------------
# Breeze tray icons
# -------------------------------------------------------------------

echo "Generating Breeze tray icons..."

for size in "${TRAY_SIZES[@]}"; do

    # -------------------------
    # Light theme
    # -------------------------

    mkdir -p "$OUT_DIR/breeze/status/$size"

    cp "$TRAY_DARK_SVG" \
       "$OUT_DIR/breeze/status/$size/${APP_NAME}-tray.svg"

    cp "$TRAY_DARK_SVG" \
       "$OUT_DIR/breeze/status/$size/${APP_NAME}-tray-symbolic.svg"

    # -------------------------
    # Dark theme
    # -------------------------

    mkdir -p "$OUT_DIR/breeze-dark/status/$size"

    cp "$TRAY_LIGHT_SVG" \
       "$OUT_DIR/breeze-dark/status/$size/${APP_NAME}-tray.svg"

    cp "$TRAY_LIGHT_SVG" \
       "$OUT_DIR/breeze-dark/status/$size/${APP_NAME}-tray-symbolic.svg"

done

# -------------------------------------------------------------------
# Preferences icon
# -------------------------------------------------------------------

echo "Generating preferences icons..."

for theme in breeze breeze-dark; do

    mkdir -p "$OUT_DIR/$theme/preferences/32"

    cp "$MAIN_SVG" \
       "$OUT_DIR/$theme/preferences/32/${APP_NAME}.svg"

done

# -------------------------------------------------------------------
# PNG icons (hicolor)
# -------------------------------------------------------------------

echo "Generating PNG icons..."

for size in "${PNG_SIZES[@]}"; do

    dir="$OUT_DIR/hicolor/${size}x${size}/apps"
    mkdir -p "$dir"

    render_svg \
        "$MAIN_SVG" \
        "$dir/${APP_NAME}.png" \
        "$size"

done

# -------------------------------------------------------------------
# Pixmaps
# -------------------------------------------------------------------

echo "Generating pixmaps..."

mkdir -p "$OUT_DIR/pixmaps"

# Main pixmap PNGs
render_svg "$MAIN_SVG" "$OUT_DIR/pixmaps/${APP_NAME}.png" 256
render_svg "$MAIN_SVG" "$OUT_DIR/pixmaps/${APP_NAME}-small.png" 64
render_svg "$MAIN_SVG" "$OUT_DIR/pixmaps/${APP_NAME}-med.png" 128

# SVG pixmaps
cp "$MAIN_SVG" \
   "$OUT_DIR/pixmaps/${APP_NAME}.svg"

cp "$TRAY_LIGHT_SVG" \
   "$OUT_DIR/pixmaps/${APP_NAME}-white.svg"

# -------------------------------------------------------------------
# Optional ICO generation
# -------------------------------------------------------------------

if command -v convert >/dev/null 2>&1; then

    echo "Generating ICO..."

    TMPDIR="$(mktemp -d)"

    for size in 16 24 32 48 64 128 256; do
        render_svg \
            "$MAIN_SVG" \
            "$TMPDIR/${size}.png" \
            "$size"
    done

    convert \
        "$TMPDIR/"*.png \
        "$OUT_DIR/pixmaps/${APP_NAME}.ico"

    rm -rf "$TMPDIR"

fi

# -------------------------------------------------------------------
# Optional ICNS generation (macOS)
# -------------------------------------------------------------------

if command -v png2icns >/dev/null 2>&1; then

    echo "Generating ICNS..."

    TMPDIR="$(mktemp -d)"

    for size in 16 32 64 128 256 512; do
        render_svg \
            "$MAIN_SVG" \
            "$TMPDIR/icon_${size}x${size}.png" \
            "$size"
    done

    png2icns \
        "$OUT_DIR/pixmaps/${APP_NAME}.icns" \
        "$TMPDIR/"*.png

    rm -rf "$TMPDIR"

fi

# -------------------------------------------------------------------
# Desktop install hints
# -------------------------------------------------------------------

cat <<EOF

Done.

Generated icon hierarchy:

$OUT_DIR/

Example install locations:

  /usr/share/icons/hicolor/
  /usr/share/icons/breeze/
  /usr/share/icons/breeze-dark/
  /usr/share/pixmaps/

Install example:

  sudo cp -r $OUT_DIR/hicolor /usr/share/icons/
  sudo cp -r $OUT_DIR/breeze /usr/share/icons/
  sudo cp -r $OUT_DIR/breeze-dark /usr/share/icons/
  sudo cp -r $OUT_DIR/pixmaps/* /usr/share/pixmaps/

Refresh cache:

  sudo gtk-update-icon-cache /usr/share/icons/hicolor
  sudo gtk-update-icon-cache /usr/share/icons/breeze
  sudo gtk-update-icon-cache /usr/share/icons/breeze-dark

EOF
