#!/bin/sh
# instalar-fedora.sh — Instala dependencias y compila COPIA720 en
# Fedora y Nobara.
#
# Uso, desde la raíz del proyecto:
#   sh scripts/instalar-fedora.sh

set -e

echo "==> COPIA720 — instalación para Fedora / Nobara"
echo

# Situarse en la raíz del proyecto (este script vive en scripts/).
cd "$(dirname "$0")/.."

# --- 1. Dependencias -------------------------------------------------------
echo "==> Instalando dependencias (pedirá tu contraseña de administrador)..."
sudo dnf install -y cmake gcc-c++ qt6-qtbase-devel

# --- 2. Compilar -----------------------------------------------------------
echo "==> Compilando..."
rm -rf build
mkdir build
cd build
cmake ..
cmake --build .
cd ..

# --- 3. Greaseweazle (opcional) -------------------------------------------
echo
printf "==> ¿Vas a usar una Greaseweazle USB? Instalo su herramienta 'gw'. [s/N] "
read -r resp
case "$resp" in
    s|S|y|Y)
        echo "==> Instalando pipx y git..."
        sudo dnf install -y pipx git
        pipx ensurepath
        echo "==> Instalando gw (Greaseweazle host tools)..."
        pipx install "git+https://github.com/keirf/greaseweazle@latest"
        echo "==> Hecho. Abre una terminal NUEVA y comprueba con:  gw --version"
        ;;
    *)
        echo "==> Omitido. Si más adelante quieres la Greaseweazle, instala 'gw' con:"
        echo "        sudo dnf install pipx git"
        echo "        pipx ensurepath"
        echo "        pipx install git+https://github.com/keirf/greaseweazle@latest"
        ;;
esac

echo
echo "==> ¡Listo! El programa está en:  build/copia720"
echo "    Lánzalo con:  ./build/copia720"
echo
echo "==> Para usar la disquetera sin ser root:"
echo "        sudo usermod -aG floppy \$USER    # disquetera interna"
echo "        sudo usermod -aG disk \$USER      # disquetera USB"
echo "    Después cierra sesión y vuelve a entrar."
