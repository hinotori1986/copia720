/* DiskButton.cpp — ver DiskButton.h. */
#include "DiskButton.h"

#include <QPainter>
#include <QSvgRenderer>

DiskButton::DiskButton(const QString &basename, const QString &tooltip,
                       QWidget *parent)
    : QAbstractButton(parent) {
    QString base = QStringLiteral(":/botones/boton_%1.svg").arg(basename);
    QString pres = QStringLiteral(":/botones/boton_%1_pulsado.svg").arg(basename);
    normal_  = new QSvgRenderer(base, this);
    pressed_ = new QSvgRenderer(pres, this);

    setToolTip(tooltip);
    setCursor(Qt::PointingHandCursor);

    // Repintar cuando cambia el estado de pulsado (para alternar el SVG).
    connect(this, &QAbstractButton::pressed,  this, [this]{ update(); });
    connect(this, &QAbstractButton::released, this, [this]{ update(); });
}

DiskButton::~DiskButton() = default;

QSize DiskButton::sizeHint() const {
    return QSize(150, 150);
}

void DiskButton::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Cuadrado centrado horizontalmente y alineado arriba, para que el hueco
    // sobrante (si el widget es más alto que ancho) quede abajo y nunca se
    // solape con lo que haya debajo del botón.
    int side = qMin(width(), height());
    QRect target((width() - side) / 2, 0, side, side);

    QSvgRenderer *r = isDown() ? pressed_ : normal_;
    if (r && r->isValid())
        r->render(&p, target);
}
