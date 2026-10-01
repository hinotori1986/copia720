# Crear el AppImage y publicarlo en una release

Un **AppImage** es un único archivo ejecutable que lleva dentro todas sus
dependencias (Qt incluido). Sirve para que cualquiera en Linux de 64 bits
(Fedora, Debian, Ubuntu, Mint...) pueda usar COPIA720 descargando un solo
archivo, sin compilar ni instalar nada.

## Generar el AppImage

Desde la raíz del proyecto, en un equipo de **64 bits**:

```sh
sh scripts/construir-appimage.sh
```

Un solo comando. El script hace todo automáticamente:
1. Localiza `qmake6` y fuerza su uso (evita que el empaquetado coja Qt5 por error).
2. Descarga las herramientas de empaquetado (`linuxdeploy` y su plugin de Qt).
3. Las extrae para funcionar aunque el sistema no tenga FUSE.
4. Compila el proyecto.
5. Mete Qt y demás dependencias dentro (incluida la librería SVG de los botones).
6. Genera `copia720-x86_64.AppImage` en la raíz del proyecto.

Requisitos: lo necesario para compilar (compilador, CMake y Qt6 incluido el
módulo Svg y `qmake6`), más `wget` o `curl`. En Fedora/Nobara:

```sh
sudo dnf install qt6-qtbase-devel qt6-qtsvg-devel qt6-qtdeclarative-devel cmake gcc-c++ wget
```


Requisitos: además de lo necesario para compilar (compilador, CMake, Qt), hace
falta `wget` o `curl`. Si tu sistema no trae FUSE, el script usa un modo de
extracción alternativo automáticamente.

## Probarlo

```sh
chmod +x copia720-x86_64.AppImage
./copia720-x86_64.AppImage
```

Debe abrirse la ventana de COPIA720.

## Importante: una arquitectura por AppImage

El AppImage generado es **sólo para 64 bits (x86_64)**. No sirve para el equipo
de 32 bits (antiX): en esa máquina hay que compilar desde el código fuente
(que es, además, lo recomendado para hardware antiguo).

Conviene generar el AppImage en un sistema **relativamente antiguo** dentro de
los que quieras soportar: un binario compilado en un Linux muy nuevo puede no
arrancar en uno más viejo (por la versión de glibc), pero al revés sí funciona.
Si lo generas en tu Fedora actual, funcionará en esa Fedora y en sistemas de
antigüedad similar o posterior.

## Subirlo a una release de GitHub

1. En tu repositorio, ve a **Releases** → **Draft a new release** (o edita una
   existente).
2. Elige o crea la etiqueta (por ejemplo `v1.0`).
3. En **Attach binaries by dropping them here or selecting them**, arrastra el
   archivo `copia720-x86_64.AppImage`.
4. Publica.

A partir de ahí, en la página de la release aparecerá el AppImage como archivo
descargable, junto al código fuente que GitHub adjunta automáticamente.

> Recuerda: **no** subas el AppImage al árbol de código (el `.gitignore` ya lo
> excluye). Va sólo como binario adjunto en la release.
