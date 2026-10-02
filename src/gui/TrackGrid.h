/* TrackGrid.h — Rejilla de pistas estilo X-Copy.
 *
 * Muestra una cuadrícula de celdas, una por pista del disquete, que se van
 * coloreando a medida que se leen o escriben: gris = pendiente, amarillo =
 * la que se está procesando ahora, verde = correcta, naranja = con reintentos,
 * rojo = error. Inspirada en la mítica pantalla de copia del X-Copy del Amiga.
 */
#ifndef TRACKGRID_H
#define TRACKGRID_H

#include <QWidget>
#include <QVector>

class TrackGrid : public QWidget {
    Q_OBJECT
public:
    explicit TrackGrid(QWidget *parent = nullptr);

    /* Prepara la rejilla para `tracks` pistas (todas a pendiente). */
    void reset(int tracks);

    /* Marca el estado de una pista. status usa los valores de TrackStatus del
     * núcleo (1=ok, 2=retry, 3=bad); el valor 4 lo usamos para "actual". */
    void setTrack(int index, int status);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int w) const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<int> states_;   // estado por pista
    int cols_ = 20;         // celdas por fila
};

#endif // TRACKGRID_H
