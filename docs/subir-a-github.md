# Cómo subir COPIA720 a GitHub (paso a paso)

Guía para publicar el proyecto en GitHub por primera vez, pensada para quien no
ha usado `git` ni GitHub antes. Sólo hay que hacer la parte de preparación una
vez.

## 1. Crear una cuenta y un repositorio en GitHub

1. Si no tienes cuenta, créala en <https://github.com>.
2. Arriba a la derecha, pulsa **+** → **New repository**.
3. Rellena:
   - **Repository name:** `copia720`
   - **Description:** algo como "Guardar, restaurar y explorar disquetes de 3½ (FAT12) en Linux. Reescritura del COPIA720 de F.J. Martos (1995)."
   - **Public** (para que sea visible) o **Private** (sólo tú), como prefieras.
   - **NO** marques "Add a README", "Add .gitignore" ni "Choose a license":
     el proyecto ya los trae. Si los marcas, darán conflicto.
4. Pulsa **Create repository**.

GitHub te mostrará una página con la dirección del repositorio. Anótala; será
algo como:

```
https://github.com/hinotori1986/copia720.git
```

## 2. Instalar git (una sola vez)

```sh
# Debian / Ubuntu / Mint / antiX
sudo apt install git

# Fedora / Nobara
sudo dnf install git
```

## 3. Configurar git con tu nombre (una sola vez)

Esto queda registrado como autor de los cambios:

```sh
git config --global user.name "Tu Nombre"
git config --global user.email "tu-email@ejemplo.com"
```

Usa el mismo email con el que te registraste en GitHub.

## 4. Preparar el proyecto y subirlo

Abre una terminal **dentro de la carpeta del proyecto** (donde está
`CMakeLists.txt` y el `README.md`) y ejecuta estos comandos, uno a uno:

```sh
# Inicializa el repositorio local
git init

# Añade todos los archivos (el .gitignore excluye build/ automáticamente)
git add .

# Primer "commit" (una foto del estado actual)
git commit -m "Primera versión: COPIA720 para Linux con Qt y Greaseweazle"

# Cambia el nombre de la rama principal a 'main' (convención actual)
git branch -M main

# Conecta tu carpeta con el repositorio de GitHub
git remote add origin https://github.com/hinotori1986/copia720.git

# Sube todo
git push -u origin main
```

En el `git push`, GitHub te pedirá autenticarte. Desde hace un tiempo **no se
usa la contraseña normal**, sino un "token de acceso personal":

1. En GitHub: foto de perfil (arriba a la derecha) → **Settings** →
   **Developer settings** (abajo del todo) → **Personal access tokens** →
   **Tokens (classic)** → **Generate new token (classic)**.
2. Dale un nombre, marca la casilla **repo**, y genera el token.
3. **Copia el token** (sólo se muestra una vez) y pégalo cuando la terminal te
   pida la contraseña. El usuario es tu nombre de usuario de GitHub.

> Alternativa más cómoda si vas a usar GitHub a menudo: instala GitHub CLI
> (`gh`) y ejecuta `gh auth login`, que gestiona la autenticación por ti.

## 5. Comprobar

Recarga la página del repositorio en GitHub: deberías ver todos los archivos,
el README mostrándose formateado abajo, y la licencia reconocida como GPL-3.0.

## Cambios posteriores

Cuando modifiques algo y quieras subir los cambios:

```sh
git add .
git commit -m "Descripción corta de lo que cambiaste"
git push
```

Eso es todo. `git add` selecciona los cambios, `git commit` los registra con un
mensaje, y `git push` los sube a GitHub.

## Consejo: publicar versiones (releases)

Cuando tengas una versión estable, en GitHub puedes ir a **Releases** →
**Create a new release**, ponerle una etiqueta (por ejemplo `v1.0`) y una
descripción. Así la gente puede descargar versiones concretas y ves un
histórico de hitos del proyecto.
