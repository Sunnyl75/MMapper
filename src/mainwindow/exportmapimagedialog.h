#ifndef EXPORTMAPIMAGEDIALOG_H
#define EXPORTMAPIMAGEDIALOG_H

#include <QDialog>
#include "../display/mapcanvas.h"

namespace Ui {
class ExportMapImageDialog;
}

struct ExportImageOptions {
    int north;
    int south;
    int west;
    int east;
    int tileSize;
    bool includeBackground;
    bool includeCharacters;
    bool includeMarkers;
    bool includeSelectionIndicators = false;
};

class ExportMapImageDialog : public QDialog
{
    Q_OBJECT

    static int lastNorth;
    static int lastSouth;
    static int lastWest;
    static int lastEast;

public:
    explicit ExportMapImageDialog(MapCanvas* canvas, QWidget *parent = nullptr);
    ~ExportMapImageDialog();

    ExportImageOptions getOptions() const;
    bool validateImageSize() const;
    void saveSettings() const;

private:
    Ui::ExportMapImageDialog *ui;
    MapCanvas* m_canvas;
    void loadSettings();

public:
    int north() const;
    int south() const;
    int west() const;
    int east() const;
    int tileSize() const;
    bool includeBackground() const;

private slots:
    void updateEstimatedImageSize();
    void on_useSelectionButton_clicked();
};

#endif // EXPORTMAPIMAGEDIALOG_H
