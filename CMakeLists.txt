# CMakeLists.txt — COPIA720 modernizado
#
# Compila:
#   · el núcleo en C17 (lógica de disco, FAT12, formateo, dispositivos), y
#   · la interfaz gráfica en C++ con Qt Widgets.
#
# Detecta Qt6 automáticamente y, si no está, usa Qt5. No hace falta editar
# nada para cambiar de una a otra.
#
# Uso:
#   mkdir build && cd build
#   cmake ..
#   cmake --build .
#   ./copia720
#
# Dependencias en Debian/Ubuntu:
#   Qt5:  sudo apt install qtbase5-dev qtbase5-dev-tools cmake g++
#   Qt6:  sudo apt install qt6-base-dev cmake g++

cmake_minimum_required(VERSION 3.16)
project(copia720 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# El núcleo usa extensiones POSIX/GNU (strtok_r, execvp, ioctls de disquete).
add_compile_definitions(_GNU_SOURCE)

# Qt genera código (moc) para las clases con Q_OBJECT: que CMake lo maneje.
set(CMAKE_AUTOMOC ON)

# --- Núcleo en C (portable, sin Qt) ---------------------------------------
add_library(copia720core STATIC
    src/core/fat12.c
    src/core/floppy_device.c
    src/core/floppy_format.c
    src/core/floppy_greaseweazle.c
    src/core/disk_image.c
    src/core/volumes.c
)
target_include_directories(copia720core PUBLIC src/core)
target_compile_options(copia720core PRIVATE -Wall -Wextra)

# --- Localizar Qt (6 preferido, 5 como alternativa) ------------------------
find_package(Qt6 QUIET COMPONENTS Widgets)
if (Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    message(STATUS "COPIA720: usando Qt6")
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
    set(QT_VERSION_MAJOR 5)
    message(STATUS "COPIA720: usando Qt5")
endif()

# --- Interfaz gráfica ------------------------------------------------------
add_executable(copia720
    src/gui/main.cpp
    src/gui/MainWindow.cpp
    src/gui/FloppyWorker.cpp
    src/gui/DeviceDialog.cpp
    src/gui/ExplorerDialog.cpp
    src/gui/ProgressDialog.cpp
)
target_include_directories(copia720 PRIVATE src/core src/gui)
target_link_libraries(copia720 PRIVATE copia720core Qt${QT_VERSION_MAJOR}::Widgets)
target_compile_options(copia720 PRIVATE -Wall -Wextra)
