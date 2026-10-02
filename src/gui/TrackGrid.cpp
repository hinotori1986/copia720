/* TrackGrid.cpp — ver TrackGrid.h. */
#include "TrackGrid.h"

#include <QPainter>

/* Estados (coinciden con TrackStatus del núcleo, más CUR para la actual). */
enum { ST_PENDING = 0, ST_OK = 1, ST_RETRY = 2, ST_BAD = 3, ST_CUR = 4 };

static const int GAP = 3;

TrackGrid::TrackGrid(QWidget *parent) : QWidget(parent) {
    // La altura depende del ancho (celdas cuadradas): se lo decimos al layout.
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Minimum);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
    reset(80);
}

void TrackGrid::reset(int tracks) {
    if (tracks < 1) tracks = 1;
    states_ = QVector<int>(tracks, ST_PENDING);
    updateGeometry();   // la altura necesaria puede haber cambiado
    update();
}

void TrackGrid::setTrack(int index, int status) {
    if (index < 0 || index >= states_.size())
        return;
    states_[index] = status;
    update();
}

/* Lado de celda para un ancho dado. */
static int cellSideFor(int width, int cols) {
    int s = (width - (cols - 1) * GAP) / cols;
    return s < 6 ? 6 : s;
}

int TrackGrid::heightForWidth(int w) const {
    int rows = (states_.size() + cols_ - 1) / cols_;
    int side = cellSideFor(w, cols_);
    return rows * side + (rows - 1) * GAP;
}

bool TrackGrid::hasHeightForWidth() const {
    return true;
}

QSize TrackGrid::sizeHint() const {
    // Ancho de referencia; la altura se ajustará vía heightForWidth.
    int w = cols_ * 16;
    return QSize(w, heightForWidth(w));
}

QSize TrackGrid::minimumSizeHint() const {
    int rows = (states_.size() + cols_ - 1) / cols_;
    int side = 6;
    return QSize(cols_ * side, rows * side + (rows - 1) * GAP);
}

void TrackGrid::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    int n = states_.size();
    if (n == 0) return;

    int side = cellSideFor(width(), cols_);

    for (int i = 0; i < n; i++) {
        int r = i / cols_;
        int c = i % cols_;
        int x = c * (side + GAP);
        int y = r * (side + GAP);

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
