#include "exportmapimagedialog.h"
#include "ui_exportmapimagedialog.h"

ExportMapImageDialog::ExportMapImageDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ExportMapImageDialog)
{
    ui->setupUi(this);

    // Optional: set default values
    ui->northSpinBox->setValue(100);
    ui->southSpinBox->setValue(-100);
    ui->westSpinBox->setValue(-100);
    ui->eastSpinBox->setValue(100);
    ui->zoomSpinBox->setValue(100);  // 100%
    ui->backgroundCheckBox->setChecked(true);
    ui->labelsCheckBox->setChecked(true);
    ui->markersCheckBox->setChecked(true);
    ui->notesCheckBox->setChecked(true);
    ui->connectionsCheckBox->setChecked(true);
}

ExportMapImageDialog::~ExportMapImageDialog()
{
    delete ui;
}

ExportImageOptions ExportMapImageDialog::getOptions() const
{
    ExportImageOptions options;
    options.north = ui->northSpinBox->value();
    options.south = ui->southSpinBox->value();
    options.west = ui->westSpinBox->value();
    options.east = ui->eastSpinBox->value();
    options.zoomLevel = ui->zoomSpinBox->value();
    options.includeBackground = ui->backgroundCheckBox->isChecked();
    options.includeLabels = ui->labelsCheckBox->isChecked();
    options.includeMarkers = ui->markersCheckBox->isChecked();
    options.includeNotes = ui->notesCheckBox->isChecked();
    options.includeConnections = ui->connectionsCheckBox->isChecked();
    return options;
}

int ExportMapImageDialog::north() const {
    return ui->northSpinBox->value();
}

int ExportMapImageDialog::south() const {
    return ui->southSpinBox->value();
}

int ExportMapImageDialog::west() const {
    return ui->westSpinBox->value();
}

int ExportMapImageDialog::east() const {
    return ui->eastSpinBox->value();
}

int ExportMapImageDialog::zoomLevel() const {
    return ui->zoomSpinBox->value();
}

bool ExportMapImageDialog::includeBackground() const {
    return ui->backgroundCheckBox->isChecked();
}

void ExportMapImageDialog::on_buttonBox_accepted()
{
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

