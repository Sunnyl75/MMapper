#ifndef EXPORTMAPIMAGEDIALOG_H
#define EXPORTMAPIMAGEDIALOG_H

#include <QDialog>

namespace Ui {
class ExportMapImageDialog;
}

struct ExportImageOptions {
    int north;
    int south;
    int west;
    int east;
    int zoomLevel;
    bool includeBackground;
    bool includeLabels;
    bool includeMarkers;
    bool includeNotes;
    bool includeConnections;
};

class ExportMapImageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportMapImageDialog(QWidget *parent = nullptr);
    ~ExportMapImageDialog();

    ExportImageOptions getOptions() const;

private:
    Ui::ExportMapImageDialog *ui;

public:
    int north() const;
    int south() const;
    int west() const;
    int east() const;
    int zoomLevel() const;
    bool includeBackground() const;

private slots:
    void on_buttonBox_accepted();
};

#endif // EXPORTMAPIMAGEDIALOG_H
