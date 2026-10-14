#pragma once
#include "map/roomid.h"

#include <QDialog>

class QPlainTextEdit;
class QLabel;
class QDialogButtonBox;
class MapData;

class RoomXmlEditorDlg : public QDialog
{
    Q_OBJECT
public:
    explicit RoomXmlEditorDlg(QWidget *parent = nullptr);
    void openForRoom(MapData *mapData, RoomId id);

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onSave();   // Save = apply and close (logic later)
    void onRevert(); // Revert to original buffer
    void onCancel(); // Close without applying

private:
    void loadFromModel(RoomId id); // (logic later)
    bool applyToModel();           // (logic later)

private:
    QPlainTextEdit *xmlEdit = nullptr;
    QLabel *statusLabel = nullptr;
    QDialogButtonBox *buttonBox = nullptr;

    RoomId m_roomId{};
    QString m_originalXml;

    MapData *m_mapData = nullptr;

signals:
    void sig_applyRoomFields(RoomId id,
                             const QString &name,
                             const QString &desc,
                             const QString &contents);
};
