#include "exportmapimagedialog.h"
#include "ui_exportmapimagedialog.h"
#include "../display/mapcanvas.h"

#include <QMessageBox>
#include <QDebug>
#include <QSettings>

ExportMapImageDialog::ExportMapImageDialog(MapCanvas* canvas, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ExportMapImageDialog)
    , m_canvas(canvas)
{
    ui->setupUi(this);

    // Set sensible defaults
    ui->northSpinBox->setValue(lastNorth);
    ui->southSpinBox->setValue(lastSouth);
    ui->westSpinBox->setValue(lastWest);
    ui->eastSpinBox->setValue(lastEast);
    ui->backgroundCheckBox->setChecked(getConfig().canvas.showBackgroundImage.get());
    // remove any lines using notesCheckBox
    QSettings settings;
    ui->charactersCheckBox->setChecked(settings.value("ExportDialog/includeLabels", true).toBool());
    ui->selectionIndicatorsCheckBox->setChecked(false);

    // Associate integer data with slider entries (index => value)
    ui->tileSizeSlider->setMinimum(16);
    ui->tileSizeSlider->setMaximum(128);
    ui->tileSizeSlider->setTickInterval(16);
    ui->tileSizeSlider->setValue(64);
    ui->tileSizeSlider->setTickPosition(QSlider::TicksAbove);

    // Ensure dialog works
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Connect bounds andtile size to update label
    connect(ui->northSpinBox,  QOverload<int>::of(&QSpinBox::valueChanged), this, &ExportMapImageDialog::updateEstimatedImageSize);
    connect(ui->southSpinBox,  QOverload<int>::of(&QSpinBox::valueChanged), this, &ExportMapImageDialog::updateEstimatedImageSize);
    connect(ui->eastSpinBox,   QOverload<int>::of(&QSpinBox::valueChanged), this, &ExportMapImageDialog::updateEstimatedImageSize);
    connect(ui->westSpinBox,   QOverload<int>::of(&QSpinBox::valueChanged), this, &ExportMapImageDialog::updateEstimatedImageSize);
    connect(ui->tileSizeSlider, &QSlider::valueChanged, this, [this](int value) {
        qDebug() << "[Slider] Tile size changed to:" << value;
        ui->tileSizeLabel->setText(QString("Tile Size: %1 px/room").arg(value));
        updateEstimatedImageSize();  // <-- Update size estimate live
    });

    // Trigger once at startup
    updateEstimatedImageSize();
    ui->tileSizeLabel->setText(QString("Tile Size: %1 px/room").arg(ui->tileSizeSlider->value()));

    loadSettings();

}

ExportMapImageDialog::~ExportMapImageDialog()
{
    saveSettings();
    delete ui;
}

void ExportMapImageDialog::on_useSelectionButton_clicked()
{
    if (m_canvas) {
        if (auto boundsOpt = m_canvas->getSelectedRoomBounds()) {
            QRect bounds = *boundsOpt;
            ui->westSpinBox->setValue(bounds.left());
            ui->eastSpinBox->setValue(bounds.right());
            ui->southSpinBox->setValue(bounds.top());
            ui->northSpinBox->setValue(bounds.bottom());
            updateEstimatedImageSize();
        } else {
            QMessageBox::information(this, tr("No Rooms Selected"), tr("There are no selected rooms to use as bounds."));
        }
    }
}

ExportImageOptions ExportMapImageDialog::getOptions() const
{
    ExportImageOptions options;
    options.north = ui->northSpinBox->value();
    options.south = ui->southSpinBox->value();
    options.west = ui->westSpinBox->value();
    options.east = ui->eastSpinBox->value();
    options.tileSize = ui->tileSizeSlider->value();

    options.includeBackground = ui->backgroundCheckBox->isChecked();
    options.includeCharacters = ui->charactersCheckBox->isChecked();
    options.includeMarkers = ui->markersCheckBox->isChecked();
    options.includeSelectionIndicators = ui->selectionIndicatorsCheckBox->isChecked();

    return options;
}

// Optional individual accessors if needed
int ExportMapImageDialog::north() const { return ui->northSpinBox->value(); }
int ExportMapImageDialog::south() const { return ui->southSpinBox->value(); }
int ExportMapImageDialog::west() const { return ui->westSpinBox->value(); }
int ExportMapImageDialog::east() const { return ui->eastSpinBox->value(); }
bool ExportMapImageDialog::includeBackground() const { return ui->backgroundCheckBox->isChecked(); }
int ExportMapImageDialog::lastNorth = -62;
int ExportMapImageDialog::lastSouth = -83;
int ExportMapImageDialog::lastWest = 203;
int ExportMapImageDialog::lastEast = 232;

bool ExportMapImageDialog::validateImageSize() const
{
    const int numRoomsX = ui->eastSpinBox->value() - ui->westSpinBox->value() + 1;
    const int numRoomsY = ui->northSpinBox->value() - ui->southSpinBox->value() + 1;

    const int tileSize = ui->tileSizeSlider->value();

    const int pixelWidth = static_cast<int>(numRoomsX * tileSize);
    const int pixelHeight = static_cast<int>(numRoomsY * tileSize);
    const int megapixels = (pixelWidth * pixelHeight) / 1'000'000;

    qDebug() << "[Export Preview] Image:" << pixelWidth << "x" << pixelHeight << " (~" << megapixels << " MP)";

    // Just return whether it's large, don't show a message
    const int MAX_DIM = 16384;
    return (pixelWidth <= MAX_DIM && pixelHeight <= MAX_DIM);
}

void ExportMapImageDialog::updateEstimatedImageSize()
{
    int numRoomsX = ui->eastSpinBox->value() - ui->westSpinBox->value() + 1;
    int numRoomsY = ui->northSpinBox->value() - ui->southSpinBox->value() + 1;
    const int tileSize = ui->tileSizeSlider->value();
    int width = static_cast<int>(numRoomsX * tileSize);
    int height = static_cast<int>(numRoomsY * tileSize);

    // 4 bytes per pixel (RGBA32)
    qint64 totalBytes = static_cast<qint64>(width) * static_cast<qint64>(height) * 4;

    double sizeMB = static_cast<double>(totalBytes) / (1024.0 * 1024.0);

    QString text = QString("Estimated Size: %1 MB").arg(QString::number(sizeMB, 'f', 2));
    ui->imageSizeLabel->setText(text);
}

void ExportMapImageDialog::loadSettings() {
    QSettings settings;

    lastNorth = settings.value("ExportDialog/north", 0).toInt();
    lastSouth = settings.value("ExportDialog/south", 0).toInt();
    lastWest  = settings.value("ExportDialog/west",  0).toInt();
    lastEast  = settings.value("ExportDialog/east",  0).toInt();

    int tileSize = settings.value("ExportDialog/tileSize", 64).toInt();
    ui->tileSizeSlider->setValue(tileSize);

    ui->backgroundCheckBox->setChecked(settings.value("ExportDialog/includeBackground", true).toBool());
    ui->charactersCheckBox->setChecked(settings.value("ExportDialog/includeCharacters", true).toBool());
    ui->markersCheckBox->setChecked(settings.value("ExportDialog/includeMarkers", true).toBool());
    ui->selectionIndicatorsCheckBox->setChecked(settings.value("ExportDialog/includeSelectionIndicators", false).toBool());
}

void ExportMapImageDialog::saveSettings() const {
    QSettings settings;

    settings.setValue("ExportDialog/north", ui->northSpinBox->value());
    settings.setValue("ExportDialog/south", ui->southSpinBox->value());
    settings.setValue("ExportDialog/west",  ui->westSpinBox->value());
    settings.setValue("ExportDialog/east",  ui->eastSpinBox->value());

    settings.setValue("ExportDialog/tileSize", ui->tileSizeSlider->value());

    settings.setValue("ExportDialog/includeBackground", ui->backgroundCheckBox->isChecked());
    settings.setValue("ExportDialog/includeCharacters", ui->charactersCheckBox->isChecked());
    settings.setValue("ExportDialog/includeMarkers", ui->markersCheckBox->isChecked());
    settings.setValue("ExportDialog/includeSelectionIndicators", ui->selectionIndicatorsCheckBox->isChecked());
}
