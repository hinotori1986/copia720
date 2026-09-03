# COPIA720

Herramienta para guardar, restaurar y explorar disquetes de 3½ (720 KB y
1.44 MB, sistema de archivos FAT12) en Linux, con interfaz gráfica Qt.

Reescritura moderna del programa **COPIA720** original de **F.J. Martos (1995)**,
que en MS-DOS guardaba y restauraba disquetes de 720 KB mediante llamadas
directas a la BIOS (INT 13h). Esta versión reimplementa esa idea desde cero en
C moderno sobre Linux, y añade exploración del contenido FAT12 y soporte para
**Greaseweazle**.

> Basado en el COPIA720 original de F.J. Martos (1995), que lo distribuyó de
> forma gratuita. Este proyecto es una reescritura independiente por
> **hinotori1986**, no contiene código del original, y se publica bajo licencia
> GPL-3.0.

## Características

- **Crear imagen desde disquete real** — vuelca un disquete completo a un
  archivo `.img`.
- **Copiar imagen a disquete real** — graba un `.img` en un disquete, con
  opción de formatear cada pista al vuelo y de verificar la grabación.
- **Explorar disquete** — navega el contenido FAT12 de una imagen o de un
  disquete físico, y extrae archivos.
- **Formatear** — formateo de bajo nivel pista a pista, para disquetes vírgenes.

## Dos formas de acceder al disquete

Se eligen en el desplegable «Usar:» de la ventana principal:

| | Disquetera clásica (FDC) | Greaseweazle (USB) |
|---|:---:|:---:|
| Crear imagen (leer) | ✓ | ✓ |
| Grabar imagen | ✓ | ✓ |
| Explorar FAT12 | ✓ | ✓ |
| Formatear a bajo nivel | ✓ | ✗ |

- **Disquetera clásica (FDC):** la conectada al controlador de la placa base
  (`/dev/fd0`). Es la única que permite formateo de bajo nivel.
- **Greaseweazle (USB):** dispositivo USB que permite usar disqueteras en
  equipos modernos sin controlador de disquete. Requiere la herramienta `gw`
  (ver [docs/greaseweazle.md](docs/greaseweazle.md)).

También puede elegirse el formato: 720 KB (doble densidad) o 1.44 MB (alta
densidad).

## Instalación

### Opción rápida: AppImage (Linux 64 bits)

Si sólo quieres usar el programa en un equipo de 64 bits sin compilar, descarga
el archivo `copia720-x86_64.AppImage` de la sección
[Releases](https://github.com/hinotori1986/copia720/releases), dale permiso de
ejecución y ábrelo:

```sh
chmod +x copia720-x86_64.AppImage
./copia720-x86_64.AppImage
```

Lleva todo lo necesario dentro (incluido Qt). Para equipos de 32 bits (como
antiX) o para desarrollar, compila desde el código fuente (abajo).

### Compilar desde el código fuente

Descarga el proyecto (o clónalo con git):

```sh
git clone https://github.com/hinotori1986/copia720.git
cd copia720
```

El proyecto usa CMake y detecta Qt6 automáticamente, con Qt5 como alternativa.
El mismo código compila en 32 y 64 bits: basta compilarlo en la máquina donde
se vaya a usar.

### Fedora, Nobara

```sh
sudo dnf install cmake gcc-c++ qt6-qtbase-devel
```

Guía detallada: [docs/instalacion-fedora.md](docs/instalacion-fedora.md)

### Debian, Ubuntu, Linux Mint, antiX

```sh
sudo apt update
sudo apt install build-essential cmake qtbase5-dev qtbase5-dev-tools
```

Guía detallada: [docs/instalacion-debian.md](docs/instalacion-debian.md)

### Compilar (igual en todas las distros)

Desde la carpeta del proyecto:

```sh
mkdir build && cd build
cmake ..
cmake --build .
./copia720
```

En `cmake ..` debe aparecer `COPIA720: usando Qt6` (o Qt5).

## Uso con la disquetera

Para acceder a la disquetera sin ser root, añade tu usuario al grupo
correspondiente y vuelve a iniciar sesión:

```sh
sudo usermod -aG floppy $USER    # disquetera interna (/dev/fd0)
sudo usermod -aG disk $USER      # disquetera USB (/dev/sdX)
```

Si la disquetera interna no aparece como `/dev/fd0`:

```sh
sudo modprobe floppy
```

## Arquitectura

Dos capas separadas, lo que facilita portar a otras plataformas:

- **Núcleo en C17** (`src/core/`), sin dependencias de la interfaz:
  - `fat12.*` — lectura del sistema de archivos FAT12.
  - `floppy_device.*` — lectura/escritura/verificación vía `/dev/fdX`.
  - `floppy_format.*` — formateo de bajo nivel (ioctls de `<linux/fd.h>`).
  - `floppy_greaseweazle.*` — backend Greaseweazle (orquesta la herramienta `gw`).
  - `disk_image.*` — carga/guardado de archivos `.img`.
  - `volumes.*` — detección de unidades y montaje seguro.
- **Interfaz en C++/Qt** (`src/gui/`), una capa fina sobre el núcleo.

## Licencia

GPL-3.0. Ver [LICENSE](LICENSE).

## Créditos

- **F.J. Martos** — autor del COPIA720 original para MS-DOS (1995).
- **hinotori1986** — reescritura para Linux y añadidos (FAT12, Qt, Greaseweazle).
