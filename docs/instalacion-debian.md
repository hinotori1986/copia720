# Instalación en Debian, Ubuntu, Linux Mint y antiX

Estas distros comparten el gestor de paquetes `apt`, así que las instrucciones
son las mismas para todas. Incluye tanto equipos de 64 bits como de 32 bits
(antiX suele usarse en máquinas antiguas de 32 bits, y funciona igual).

## 1. Dependencias

```sh
sudo apt update
sudo apt install build-essential cmake qtbase5-dev qtbase5-dev-tools
```

Esto instala el compilador, CMake y Qt5. En estas distros se usa Qt5 (Qt6
puede no estar disponible, sobre todo en 32 bits); el proyecto lo detecta y
funciona igual.

> En Debian/Ubuntu/Mint recientes con Qt6, si prefieres Qt6 puedes instalar
> `qt6-base-dev` en su lugar. No es necesario: con Qt5 basta.

## 2. Compilar

Desde la carpeta del proyecto (donde está `CMakeLists.txt`):

```sh
mkdir build && cd build
cmake ..
cmake --build .
```

En el paso `cmake ..` debe aparecer la línea `COPIA720: usando Qt5` (o Qt6).
Al terminar tendrás el ejecutable `copia720` dentro de la carpeta `build`.

## 3. Ejecutar

```sh
./copia720
```

Para volver a usarlo en el futuro, solo entra en `build` y ejecuta
`./copia720`. No hace falta recompilar salvo que cambie el código.

## Script de instalación automática (opcional)

El proyecto incluye `scripts/instalar-debian.sh`, que hace todo lo anterior de
una vez (instala dependencias, ordena, compila) y ofrece instalar también las
herramientas de Greaseweazle. Para usarlo:

```sh
sh scripts/instalar-debian.sh
```

## Permisos de la disquetera

Para leer/escribir la disquetera sin ser root, añade tu usuario al grupo
adecuado y vuelve a iniciar sesión:

```sh
sudo usermod -aG floppy $USER    # disquetera interna (/dev/fd0)
sudo usermod -aG disk $USER      # disquetera USB (/dev/sdX)
```

Si la disquetera interna no aparece como `/dev/fd0`:

```sh
sudo modprobe floppy
ls /dev/fd0
```

Para disquetes de 720 KB en una unidad que el kernel asume de 1.44 MB:

```sh
sudo setfdprm /dev/fd0 720/1440
```

## Nota sobre equipos de 32 bits (antiX y similares)

El formateo de bajo nivel funciona mejor con una **disquetera interna** (la del
cable plano conectada a la placa), habitual en equipos antiguos. Las
disqueteras USB no permiten formateo de bajo nivel en ningún caso: sirven para
leer, grabar y explorar, pero no para formatear disquetes vírgenes.
