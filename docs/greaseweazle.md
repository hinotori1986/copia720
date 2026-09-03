# Uso con Greaseweazle

La [Greaseweazle](https://github.com/keirf/greaseweazle) es un dispositivo USB
de código abierto que lee y escribe disquetes a nivel de flujo magnético. Con
ella puedes usar una disquetera en equipos modernos que ya no tienen
controlador de disquete en la placa base.

COPIA720 no reimplementa el protocolo de la Greaseweazle: usa por debajo su
herramienta oficial `gw`, igual que usa `lsblk` o `udisksctl` para otras
tareas. `gw` decodifica el disquete a una imagen de sectores `.img`, que el
núcleo FAT12 explora como cualquier otra imagen.

## Instalar la herramienta `gw`

`gw` se instala con `pipx` (que aísla cada programa de Python del sistema).

**1. Instala pipx y git:**

```sh
# Debian / Ubuntu / Mint / antiX
sudo apt install pipx git

# Fedora / Nobara
sudo dnf install pipx git
```

**2. Prepara el PATH y instala gw:**

```sh
pipx ensurepath
pipx install git+https://github.com/keirf/greaseweazle@latest
```

**3. Reabre la terminal** y comprueba:

```sh
gw --version
```

Si responde con un número de versión, está lista.

> **Importante:** Greaseweazle **no** se publica en PyPI con el nombre
> `greaseweazle` a secas, por eso se instala desde GitHub con `git+https://...`.
> El comando `pipx install greaseweazle` (sin la URL) falla.

## Comprobar el estado desde la aplicación

En la ventana principal, menú **Ayuda → Comprobar herramientas**, verás si `gw`
está detectada y su versión. Si no lo está, te muestra el comando exacto para
instalarla.

## Permisos USB (regla udev)

En algunos sistemas, usar la Greaseweazle sin ser root requiere una regla udev
que dé permiso al dispositivo USB. Si `gw --version` funciona pero la lectura
falla por permisos, consulta la
[documentación de Greaseweazle](https://github.com/keirf/greaseweazle/wiki)
sobre reglas udev para tu sistema.

## Formatos

COPIA720 indica a `gw` el formato según el que elijas en la interfaz:

| Formato en la app | Formato de `gw` |
|---|---|
| 720 KB | `ibm.720` |
| 1.44 MB | `ibm.1440` |

## Qué puede y qué no puede hacer la Greaseweazle en COPIA720

- **Sí:** crear imágenes (leer), grabar imágenes, explorar el contenido FAT12.
- **No:** formateo de bajo nivel pista a pista. Para dejar un disquete vacío
  con la Greaseweazle, graba una imagen ya formateada (por ejemplo, una imagen
  en blanco del tamaño correcto).
