#!/bin/sh
# construir-appimage.sh — Genera copia720-x86_64.AppImage (Linux 64 bits).
#
# Un AppImage es un único archivo ejecutable que lleva dentro todas sus
# dependencias (Qt incluido), así que funciona en la mayoría de distros de
# 64 bits (Fedora, Debian, Ubuntu, Mint...) sin instalar nada.
#
# Uso, desde la raíz del proyecto:
#   sh scripts/construir-appimage.sh
#
# Requisitos previos (además del compilador, cmake y Qt para compilar):
#   - wget o curl (para descargar las herramientas de empaquetado)
#   - FUSE (para ejecutar los AppImage). En algunas distros: 'sudo dnf install
#     fuse' o 'sudo apt install libfuse2'. Si no tienes FUSE, el script usa
#     un modo alternativo de extracción automáticamente.
#
# NOTA: el AppImage generado es SÓLO para 64 bits (x86_64). Para el equipo de
# 32 bits (antiX), compila el programa allí desde el código fuente.

set -e

# Situarse en la raíz del proyecto (este script vive en scripts/).
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

echo "==> COPIA720 — construcción del AppImage (x86_64)"

# Comprobar arquitectura
ARCH="$(uname -m)"
if [ "$ARCH" != "x86_64" ]; then
    echo "AVISO: estás en $ARCH, no en x86_64. El AppImage resultante será"
    echo "       para $ARCH, no para 64 bits Intel/AMD."
fi

# --- 1. Herramienta de descarga -------------------------------------------
if command -v wget >/dev/null 2>&1; then
    DL="wget -q -O"
elif command -v curl >/dev/null 2>&1; then
    DL="curl -sL -o"
else
    echo "ERROR: necesito 'wget' o 'curl' para descargar las herramientas."
    echo "  Fedora:  sudo dnf install wget"
    echo "  Debian:  sudo apt install wget"
    exit 1
fi

# --- 2. Descargar linuxdeploy y su plugin de Qt ---------------------------
TOOLS="$ROOT/.appimage-tools"
mkdir -p "$TOOLS"

LD="$TOOLS/linuxdeploy-x86_64.AppImage"
LDQT="$TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -f "$LD" ]; then
    echo "==> Descargando linuxdeploy..."
    $DL "$LD" "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "$LD"
fi
if [ ! -f "$LDQT" ]; then
    echo "==> Descargando linuxdeploy-plugin-qt..."
    $DL "$LDQT" "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "$LDQT"
fi

# Si no hay FUSE, los AppImage de las herramientas no se ejecutan
# directamente: se extraen y se usa el binario interno. Lo detectamos.
run_tool() {
    tool="$1"; shift
    if "$tool" --appimage-extract-and-run "$@" 2>/dev/null; then
        return 0
    fi
    # Fallback: extraer manualmente
    dir="${tool}.extracted"
    if [ ! -d "$dir" ]; then
        (cd "$(dirname "$tool")" && "$tool" --appimage-extract >/dev/null 2>&1 && mv squashfs-root "$dir")
    fi
    "$dir/AppRun" "$@"
}

# --- 3. Compilar e instalar en un AppDir ----------------------------------
echo "==> Compilando..."
rm -rf build-appimage AppDir
mkdir build-appimage
cd build-appimage
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr >/dev/null
cmake --build . -j"$(nproc)"
DESTDIR="$ROOT/AppDir" cmake --install . >/dev/null
cd "$ROOT"

# --- 4. Empaquetar el AppImage --------------------------------------------
echo "==> Empaquetando dependencias (Qt) y generando el AppImage..."
export QMAKE="$(command -v qmake6 || command -v qmake || true)"

run_tool "$LD" \
    --appdir AppDir \
    --plugin qt \
    --output appimage \
    --desktop-file AppDir/usr/share/applications/copia720.desktop \
    --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/copia720.png

# linuxdeploy deja el archivo con un nombre tipo COPIA720-x86_64.AppImage.
# Lo normalizamos.
OUT="$(ls -1 *.AppImage 2>/dev/null | head -n1 || true)"
if [ -n "$OUT" ]; then
    mv -f "$OUT" copia720-x86_64.AppImage
    chmod +x copia720-x86_64.AppImage
    echo
    echo "==> ¡Listo!  ->  $ROOT/copia720-x86_64.AppImage"
    echo "    Pruébalo con:   ./copia720-x86_64.AppImage"
    echo "    Súbelo como binario en tu release de GitHub."
else
    echo "ERROR: no se generó el AppImage. Revisa los mensajes anteriores."
    exit 1
fi
