/* DiskButton.h — Botón con forma de disquete.
 *
 * Muestra una imagen SVG de un disquete (con el texto de la acción dentro) y,
 * mientras se mantiene pulsado, cambia a una segunda imagen con el marco
 * resaltado en amarillo. Al soltar, vuelve a la normal.
 *
 * Los SVG se cargan desde los recursos empotrados en el binario (ver
 * assets/recursos.qrc), así que no dependen de archivos externos: el botón
 * funciona igual en el ejecutable normal y en el AppImage.
 */
#ifndef DISKBUTTON_H
#define DISKBUTTON_H

#include <QAbstractButton>
#include <QString>

class QSvgRenderer;

class DiskButton : public QAbstractButton {
    Q_OBJECT
public:
    /* `basename` es el nombre base del recurso, p. ej. "crear": el widget
     * cargará ":/botones/boton_crear.svg" y ":/botones/boton_crear_pulsado.svg".
     * `tooltip` es el texto de ayuda emergente. */
    DiskButton(const QString &basename, const QString &tooltip,
               QWidget *parent = nullptr);
    ~DiskButton() override;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QSvgRenderer *normal_ = nullptr;
    QSvgRenderer *pressed_ = nullptr;
};

#endif // DISKBUTTON_H
