#!/bin/sh
# construir-appimage.sh — Genera COPIA720-x86_64.AppImage (Linux 64 bits).
#
# Un AppImage es un único archivo ejecutable que lleva dentro todas sus
# dependencias (Qt incluido), así que funciona en la mayoría de distros de
# 64 bits (Fedora, Debian, Ubuntu, Mint...) sin instalar nada.
#
# Uso, desde la raíz del proyecto:
#   sh scripts/construir-appimage.sh
#
# Este script incorpora los arreglos necesarios para que funcione de forma
# fiable (aprendidos construyéndolo en Fedora/Nobara):
#   - fuerza el uso de qmake6 (si no, el plugin de Qt coge qmake5 por error),
#   - extrae linuxdeploy y su plugin de Qt cuando no hay FUSE,
#   - coloca el plugin donde linuxdeploy lo busca.
#
# Requisitos: lo necesario para compilar (compilador, cmake, Qt incluido el
# modulo Svg) mas 'wget' o 'curl'.
#
# NOTA: el AppImage generado es SOLO para 64 bits (x86_64). Para equipos de
# 32 bits, compila el programa alli desde el codigo fuente.

set -e

cd "$(dirname "$0")/.."
ROOT="$(pwd)"

echo "==> COPIA720 - construccion del AppImage (x86_64)"

ARCH="$(uname -m)"
if [ "$ARCH" != "x86_64" ]; then
    echo "AVISO: estas en $ARCH. El AppImage sera para $ARCH, no para 64 bits Intel/AMD."
fi

# --- 1. Localizar qmake6 (imprescindible para que el plugin use Qt6) --------
QMAKE_BIN="$(command -v qmake6 || true)"
if [ -z "$QMAKE_BIN" ]; then
    QMAKE_BIN="$(command -v qmake-qt6 || true)"
fi
if [ -z "$QMAKE_BIN" ]; then
    echo "ERROR: no encuentro 'qmake6'. Instala el desarrollo de Qt6:"
    echo "  Fedora:  sudo dnf install qt6-qtbase-devel"
    echo "  Debian:  sudo apt install qmake6 qt6-base-dev"
    exit 1
fi
echo "==> Usando qmake: $QMAKE_BIN"
export QMAKE="$QMAKE_BIN"

# --- 2. Herramienta de descarga -------------------------------------------
if command -v wget >/dev/null 2>&1; then
    DL="wget -q -O"
elif command -v curl >/dev/null 2>&1; then
    DL="curl -sL -o"
else
    echo "ERROR: necesito 'wget' o 'curl'."
    echo "  Fedora:  sudo dnf install wget"
    echo "  Debian:  sudo apt install wget"
    exit 1
fi

# --- 3. Descargar linuxdeploy y su plugin de Qt ---------------------------
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

# --- 4. Extraer las herramientas (para funcionar sin FUSE) -----------------
LD_DIR="$TOOLS/linuxdeploy.extracted"
LDQT_DIR="$TOOLS/linuxdeploy-plugin-qt.extracted"

if [ ! -d "$LD_DIR" ]; then
    echo "==> Extrayendo linuxdeploy..."
    ( cd "$TOOLS" && "$LD" --appimage-extract >/dev/null && mv squashfs-root "$LD_DIR" )
fi
if [ ! -d "$LDQT_DIR" ]; then
    echo "==> Extrayendo el plugin de Qt..."
    ( cd "$TOOLS" && "$LDQT" --appimage-extract >/dev/null && mv squashfs-root "$LDQT_DIR" )
fi

# Colocar el ejecutable del plugin donde linuxdeploy lo busca (junto al suyo).
cp -f "$LDQT_DIR/AppRun" "$LD_DIR/usr/bin/linuxdeploy-plugin-qt"
chmod +x "$LD_DIR/usr/bin/linuxdeploy-plugin-qt"

# --- 5. Compilar e instalar en un AppDir ----------------------------------
echo "==> Compilando..."
rm -rf build-appimage AppDir
mkdir build-appimage
cd build-appimage
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr >/dev/null
cmake --build . -j"$(nproc)"
DESTDIR="$ROOT/AppDir" cmake --install . >/dev/null
cd "$ROOT"

# --- 6. Empaquetar el AppImage --------------------------------------------
echo "==> Empaquetando dependencias (Qt) y generando el AppImage..."
QMAKE="$QMAKE_BIN" \
"$LD_DIR/AppRun" \
    --appdir AppDir \
    --plugin qt \
    --output appimage \
    --desktop-file AppDir/usr/share/applications/copia720.desktop \
    --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/copia720.png

# --- 7. Normalizar el nombre ----------------------------------------------
OUT="$(ls -1 COPIA720*.AppImage copia720*.AppImage 2>/dev/null | head -n1 || true)"
if [ -n "$OUT" ]; then
    mv -f "$OUT" copia720-x86_64.AppImage
    chmod +x copia720-x86_64.AppImage
    echo
    echo "==> Listo!  ->  $ROOT/copia720-x86_64.AppImage"
    echo "    Pruebalo con:   ./copia720-x86_64.AppImage"
    echo "    Comprueba en Ayuda -> Acerca de que muestra la version correcta,"
    echo "    y subelo como binario en tu release de GitHub."
else
    echo "ERROR: no se genero el AppImage. Revisa los mensajes anteriores."
    exit 1
fi
