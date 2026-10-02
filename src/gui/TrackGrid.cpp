/* TrackGrid.cpp — ver TrackGrid.h. */
#include "TrackGrid.h"

#include <QPainter>

/* Estados (coinciden con TrackStatus del núcleo, más CUR para la actual). */
enum { ST_PENDING = 0, ST_OK = 1, ST_RETRY = 2, ST_BAD = 3, ST_CUR = 4 };

static const int GAP = 3;
/* Alto reservado para la rejilla. Todas las celdas se ajustan para caber aquí,
 * sea cual sea el formato (80, 160 o 320 pistas), al estilo X-Copy: el disco
 * entero visible de un vistazo, sin barras de desplazamiento. */
static const int GRID_HEIGHT = 150;

TrackGrid::TrackGrid(QWidget *parent) : QWidget(parent) {
    setFixedHeight(GRID_HEIGHT);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    reset(80);
}

/* Elige cuántas columnas usar según el número de pistas, para que las celdas
 * no queden ni enormes (pocas pistas) ni diminutas (muchas). */
static int colsFor(int tracks) {
    if (tracks <= 90)  return 20;   /* 360 KB: 4 filas */
    if (tracks <= 170) return 20;   /* 720 KB: 8 filas */
    return 32;                       /* 1.44 MB: 10 filas */
}

void TrackGrid::reset(int tracks) {
    if (tracks < 1) tracks = 1;
    states_ = QVector<int>(tracks, ST_PENDING);
    cols_ = colsFor(tracks);
    update();
}

void TrackGrid::setTrack(int index, int status) {
    if (index < 0 || index >= states_.size())
        return;
    states_[index] = status;
    update();
}

QSize TrackGrid::sizeHint() const {
    return QSize(cols_ * 16, GRID_HEIGHT);
}

QSize TrackGrid::minimumSizeHint() const {
    return QSize(cols_ * 8, GRID_HEIGHT);
}

void TrackGrid::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    int n = states_.size();
    if (n == 0) return;
    int rows = (n + cols_ - 1) / cols_;

    /* Tamaño de celda que cabe tanto a lo ancho como a lo alto: el menor de
     * los dos, para que ninguna celda se salga de la rejilla. */
    int sideW = (width()  - (cols_ - 1) * GAP) / cols_;
    int sideH = (height() - (rows  - 1) * GAP) / rows;
    int side = sideW < sideH ? sideW : sideH;
    if (side < 3) side = 3;

    /* Centrar la cuadrícula en el espacio disponible. */
    int gridW = cols_ * side + (cols_ - 1) * GAP;
    int gridH = rows  * side + (rows  - 1) * GAP;
    int ox = (width()  - gridW) / 2; if (ox < 0) ox = 0;
    int oy = (height() - gridH) / 2; if (oy < 0) oy = 0;

    for (int i = 0; i < n; i++) {
        int r = i / cols_;
        int c = i % cols_;
        int x = ox + c * (side + GAP);
        int y = oy + r * (side + GAP);

        QColor fill, border;
        switch (states_[i]) {
        case ST_OK:    fill = QColor("#2e9e6f"); border = fill; break;
        case ST_RETRY: fill = QColor("#e89a2c"); border = fill; break;
        case ST_BAD:   fill = QColor("#d84a47"); border = fill; break;
        case ST_CUR:   fill = QColor("#f2b705"); border = QColor("#ffffff"); break;
        default:       fill = QColor("#34557a"); border = QColor("#42648c"); break;
        }
        p.setBrush(fill);
        p.setPen(border);
        p.drawRoundedRect(x, y, side, side, 2, 2);
    }
}
