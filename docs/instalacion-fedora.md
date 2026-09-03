# Instalación en Fedora y Nobara

Fedora y sus derivadas (como Nobara) usan el gestor de paquetes `dnf` y suelen
traer Qt6.

## 1. Dependencias

```sh
sudo dnf install cmake gcc-c++ qt6-qtbase-devel
```

Si prefieres Qt5, sería `qt5-qtbase-devel` en su lugar. El proyecto detecta
cualquiera de las dos automáticamente.

## 2. Compilar

Desde la carpeta del proyecto (donde está `CMakeLists.txt`):

```sh
mkdir build && cd build
cmake ..
cmake --build .
```

En el paso `cmake ..` debe aparecer la línea `COPIA720: usando Qt6`. Al
terminar tendrás el ejecutable `copia720` dentro de la carpeta `build`.

## 3. Ejecutar

```sh
./copia720
```

Para volver a usarlo, solo entra en `build` y ejecuta `./copia720`. No hace
falta recompilar salvo que cambie el código.

## Permisos de la disquetera

Para leer/escribir la disquetera sin ser root, añade tu usuario al grupo
adecuado y vuelve a iniciar sesión:

```sh
sudo usermod -aG floppy $USER    # disquetera interna (/dev/fd0)
sudo usermod -aG disk $USER      # disquetera USB (/dev/sdX)
```

En Fedora, comprueba con `groups` (tras reiniciar sesión) que aparece el grupo.

Si la disquetera interna no aparece como `/dev/fd0`:

```sh
sudo modprobe floppy
ls /dev/fd0
```

## Greaseweazle

Para usar una Greaseweazle, instala su herramienta `gw`:

```sh
sudo dnf install pipx git
pipx ensurepath
pipx install git+https://github.com/keirf/greaseweazle@latest
```

Reabre la terminal y comprueba con `gw --version`. Más detalles en
[greaseweazle.md](greaseweazle.md).
