/* ExplorerDialog.h — Explorador del contenido de un disquete FAT12.
 *
 * Muestra el árbol de directorios de una imagen (leída de un archivo .img o
 * directamente del disquete físico) usando el parser fat12 del núcleo, y
 * permite extraer cualquier archivo al disco duro.
 */
#ifndef EXPLORERDIALOG_H
#define EXPLORERDIALOG_H

#include <QDialog>
#include <QString>
#include <vector>
#include <cstdint>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;

extern "C" {
#include "fat12.h"
}

class ExplorerDialog : public QDialog {
    Q_OBJECT
public:
    /* Explora los bytes de una imagen ya en memoria. `title` describe el
     * origen (nombre de archivo o "disquete físico"). */
    ExplorerDialog(std::vector<uint8_t> image, QString title,
                   QWidget *parent = nullptr);
    ~ExplorerDialog() override;

    bool parsedOk() const { return parsedOk_; }

private slots:
    void onExtractSelected();
    void onSelectionChanged();

private:
    void populateTree(QTreeWidgetItem *parent, const Fat12Entry *entries,
                      size_t n);

    std::vector<uint8_t> image_;
    Fat12Image fat_{};
    bool parsedOk_ = false;

    QTreeWidget *tree_ = nullptr;
    QLabel *summary_ = nullptr;
};

#endif // EXPLORERDIALOG_H
