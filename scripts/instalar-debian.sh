#!/bin/sh
# instalar-debian.sh — Instala dependencias, ordena los archivos y compila
# COPIA720 en Debian, Ubuntu, Linux Mint y antiX (32 o 64 bits).
#
# Uso, desde la raíz del proyecto:
#   sh scripts/instalar-debian.sh
#
# El script es idempotente: puedes ejecutarlo varias veces sin problema.

set -e

echo "==> COPIA720 — instalación para Debian / Ubuntu / Mint / antiX"
echo

# Situarse en la raíz del proyecto (este script vive en scripts/).
cd "$(dirname "$0")/.."

# --- 1. Dependencias -------------------------------------------------------
# antiX trae Qt5. Necesitamos el compilador, CMake y los -dev de Qt5 Widgets.
echo "==> Instalando dependencias (pedirá tu contraseña de administrador)..."
sudo apt update
sudo apt install -y build-essential cmake qtbase5-dev qtbase5-dev-tools

# --- 2. Ordenar los archivos en src/core y src/gui -------------------------
# Si los archivos están sueltos en esta carpeta (como al descargarlos), los
# recolocamos en la estructura que espera CMake. Si ya están en su sitio, no
# se toca nada.
if [ ! -d src/core ] || [ ! -f src/core/fat12.c ]; then
    echo "==> Ordenando los archivos fuente en src/core y src/gui..."
    mkdir -p src/core src/gui

    for f in fat12 floppy_device floppy_format disk_image volumes; do
        [ -f "$f.c" ] && mv "$f.c" src/core/ 2>/dev/null || true
        [ -f "$f.h" ] && mv "$f.h" src/core/ 2>/dev/null || true
    done

    [ -f main.cpp ] && mv main.cpp src/gui/ 2>/dev/null || true
    for f in MainWindow FloppyWorker DeviceDialog ExplorerDialog ProgressDialog; do
        [ -f "$f.cpp" ] && mv "$f.cpp" src/gui/ 2>/dev/null || true
        [ -f "$f.h" ]   && mv "$f.h"   src/gui/ 2>/dev/null || true
    done
else
    echo "==> Los archivos ya están ordenados."
fi

# Comprobación
if [ ! -f src/core/fat12.c ] || [ ! -f src/gui/main.cpp ]; then
    echo "ERROR: no encuentro los archivos fuente. ¿Están todos descargados"
    echo "       en esta carpeta junto a CMakeLists.txt?"
    exit 1
fi

# --- 3. Compilar -----------------------------------------------------------
echo "==> Compilando..."
rm -rf build
mkdir build
cd build
cmake ..
cmake --build .
cd ..

# --- 4. Greaseweazle (opcional) -------------------------------------------
# Si vas a usar una Greaseweazle USB, hace falta la herramienta `gw`. La
# ofrecemos aquí para no tener que buscarla luego. Es de código abierto y
# gratuita, y se instala de forma aislada con pipx.
echo
printf "==> ¿Vas a usar una Greaseweazle USB? Instalo su herramienta 'gw'. [s/N] "
read -r resp
case "$resp" in
    s|S|y|Y)
        echo "==> Instalando pipx y git..."
        sudo apt install -y pipx git
        pipx ensurepath
        echo "==> Instalando gw (Greaseweazle host tools)..."
        pipx install "git+https://github.com/keirf/greaseweazle@latest"
        echo "==> Hecho. Abre una terminal NUEVA y comprueba con:  gw --version"
        echo "    (Para usar la Greaseweazle sin ser root puede hacer falta"
        echo "     una regla udev; la app te avisa en Ayuda si hace falta.)"
        ;;
    *)
        echo "==> Omitido. Si más adelante quieres la Greaseweazle, instala 'gw' con:"
        echo "        sudo apt install pipx git"
        echo "        pipx ensurepath"
        echo "        pipx install git+https://github.com/keirf/greaseweazle@latest"
        ;;
esac

echo
echo "==> ¡Listo! El programa está en:  build/copia720"
echo "    Lánzalo con:  ./build/copia720   (desde la carpeta del proyecto)"
echo
echo "==> IMPORTANTE — permisos y disquetera:"
echo "    Para usar la disquetera sin ser root, añádete al grupo 'floppy'"
echo "    (disquetera interna) o 'disk' (disquetera USB):"
echo
echo "        sudo usermod -aG floppy \$USER"
echo "        sudo usermod -aG disk \$USER"
echo
echo "    Después CIERRA SESIÓN y vuelve a entrar para que tenga efecto."
echo
echo "    Si la disquetera interna no aparece como /dev/fd0, prueba:"
echo "        sudo modprobe floppy"
