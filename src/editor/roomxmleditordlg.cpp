#include "roomxmleditordlg.h"

#include "global/TaggedString.h"
#include "map/RawRoom.h"
#include "map/RoomFields.h"
#include "mapdata/mapdata.h"

#include <string>
#include <string_view>

#include <QCloseEvent>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDomDocument>
#include <QDomElement>
#include <QFontDatabase>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QString>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QXmlStreamWriter>

static QString toQString(const QString &s)
{
    return s;
}
static QString toQString(const char *s)
{
    return QString::fromUtf8(s ? s : "");
}
static QString toQString(const std::string &s)
{
    return QString::fromStdString(s);
}
static QString toQString(std::string_view sv)
{
    return QString::fromUtf8(sv.data(), int(sv.size()));
}
static QString toQString(const QByteArray &a)
{
    return QString::fromUtf8(a);
}

RoomXmlEditorDlg::RoomXmlEditorDlg(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Room XML Editor"));

    auto *layout = new QVBoxLayout(this);

    xmlEdit = new QPlainTextEdit(this);
    xmlEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    xmlEdit->setFont(mono);

    statusLabel = new QLabel(this);
    statusLabel->setText(QString());

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Reset
                                         | QDialogButtonBox::Cancel,
                                     this);

    layout->addWidget(xmlEdit);
    layout->addWidget(statusLabel);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &RoomXmlEditorDlg::onSave);
    connect(buttonBox->button(QDialogButtonBox::Reset),
            &QPushButton::clicked,
            this,
            &RoomXmlEditorDlg::onRevert);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &RoomXmlEditorDlg::onCancel);

    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, &RoomXmlEditorDlg::onSave);
}

void RoomXmlEditorDlg::openForRoom(MapData *mapData, RoomId id)
{
    m_mapData = mapData; // store for later use
    m_roomId = id;
    loadFromModel(id); // still shows the placeholder XML for now
    show();
    raise();
    activateWindow();
}

void RoomXmlEditorDlg::onSave()
{
    const QString xml = xmlEdit->toPlainText();

    QDomDocument doc;
    QString parseErrorMsg;
    int parseErrorLine = 0, parseErrorColumn = 0;
    if (!doc.setContent(xml, &parseErrorMsg, &parseErrorLine, &parseErrorColumn)) {
        QMessageBox::warning(this,
                             tr("Invalid XML"),
                             tr("Could not parse XML (line %1, col %2): %3")
                                 .arg(parseErrorLine)
                                 .arg(parseErrorColumn)
                                 .arg(parseErrorMsg));
        return;
    }

    const auto root = doc.documentElement();
    if (root.tagName() != QLatin1String("room")) {
        QMessageBox::warning(this, tr("Invalid XML"), tr("Root element must be <room>."));
        return;
    }

    auto firstText = [&](const QString &tag) -> QString {
        const auto nodes = root.elementsByTagName(tag);
        if (nodes.isEmpty())
            return QString();
        const auto e = nodes.at(0).toElement();
        return e.isNull() ? QString() : e.text();
    };

    const QString name = firstText(QStringLiteral("name"));
    const QString desc = firstText(QStringLiteral("description"));
    const QString contents = firstText(QStringLiteral("contents"));

    // Emit to MainWindow (Step 2 handles the apply)
    emit sig_applyRoomFields(m_roomId, name, desc, contents);

    // Close dialog and clear modified flag
    xmlEdit->document()->setModified(false);
    accept();
}

void RoomXmlEditorDlg::onRevert()
{
    xmlEdit->setPlainText(m_originalXml);
    xmlEdit->moveCursor(QTextCursor::End);
    xmlEdit->ensureCursorVisible();

    qDebug() << "[xedit] xml length =" << m_originalXml.size();

    xmlEdit->document()->setModified(false);
    statusLabel->setText(tr("Reverted."));
}

void RoomXmlEditorDlg::onCancel()
{
    reject();
}

void RoomXmlEditorDlg::closeEvent(QCloseEvent *e)
{
    if (!xmlEdit->document()->isModified()) {
        e->accept();
        return;
    }

    const auto r = QMessageBox::question(this,
                                         tr("Unsaved changes"),
                                         tr("Save changes to this room?"),
                                         QMessageBox::Save | QMessageBox::Discard
                                             | QMessageBox::Cancel,
                                         QMessageBox::Save);

    if (r == QMessageBox::Save) {
        if (applyToModel())
            e->accept();
        else
            e->ignore(); // keep open on error
    } else if (r == QMessageBox::Discard) {
        e->accept();
    } else {
        e->ignore();
    }
}

static QString buildRoomXml(RoomId id,
                            const QString &name,
                            const QString &description,
                            const QString &contents)
{
    QString out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement(QStringLiteral("room"));
    w.writeAttribute(QStringLiteral("id"), QString::number(id.value()));

    w.writeStartElement(QStringLiteral("name"));
    w.writeCharacters(name);
    w.writeEndElement();

    w.writeStartElement(QStringLiteral("description"));
    w.writeCharacters(description);
    w.writeEndElement();

    w.writeStartElement(QStringLiteral("contents"));
    w.writeCharacters(contents);
    w.writeEndElement();

    w.writeEndElement(); // room
    w.writeEndDocument();
    return out;
}

void RoomXmlEditorDlg::loadFromModel(RoomId id)
{
    qDebug() << "[xedit] loadFromModel() called with RoomId =" << id.value();

    QString name;
    QString description;
    QString contents;

    if (m_mapData) {
        if (const auto handle = m_mapData->findRoomHandle(id)) {
            const RawRoom &r = handle.getRaw();
            const RoomFields &f = r.getRoomFields();

            // Convert TaggedBoxedStringUtf8 -> std::string_view (UTF-8) -> QString
            const auto nsv = f.getName().getStdStringViewUtf8();
            const auto dsv = f.getDescription().getStdStringViewUtf8();
            const auto csv = f.getContents().getStdStringViewUtf8();

            name = QString::fromUtf8(nsv.data(), static_cast<int>(nsv.size()));
            description = QString::fromUtf8(dsv.data(), static_cast<int>(dsv.size()));
            contents = QString::fromUtf8(csv.data(), static_cast<int>(csv.size()));

            qDebug() << "[xedit] loaded fields:"
                     << "name len=" << name.size() << "desc len=" << description.size()
                     << "contents len=" << contents.size();
        } else {
            qWarning() << "[xedit] Room handle not found for id" << id.value();
        }
    } else {
        qWarning() << "[xedit] m_mapData is null";
    }

    // Build a simple, escaped XML preview
    QString xml;
    xml += QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    xml += QString("<room id=\"%1\">\n").arg(QString::number(id.value()));
    xml += "  <name>" + name.toHtmlEscaped() + "</name>\n";
    xml += "  <description>" + description.toHtmlEscaped() + "</description>\n";
    xml += "  <contents>" + contents.toHtmlEscaped() + "</contents>\n";
    xml += "</room>\n";

    m_originalXml = xml;

    // Push into UI
    xmlEdit->setPlainText(m_originalXml);
    xmlEdit->document()->setModified(false);
    statusLabel->setText(QString());
    setWindowTitle(QString("Room XML Editor — id=%1").arg(QString::number(id.value())));
    xmlEdit->moveCursor(QTextCursor::Start);
    xmlEdit->ensureCursorVisible();

    qDebug() << "[xedit] xml length =" << m_originalXml.size();
}

bool RoomXmlEditorDlg::applyToModel()
{
    // Stub for now—Step 3 will parse XML, update the room, and mark map dirty.
    // Returning true lets Save/Close proceed so we can test the UI plumbing.
    statusLabel->setText(tr("Saved (stub)."));
    xmlEdit->document()->setModified(false);
    return true;
}
