#include "axiscontrolwidget.h"
#include "ui_axiscontrolwidget.h"

AxisControlWidget::AxisControlWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AxisControlWidget)
{
    ui->setupUi(this);

    QString btnStyle =
        "QPushButton { background-color: #ecf0f1; color: #2c3e50; border: 1px solid #bdc3c7; "
        "border-radius: 3px; padding: 2px 6px; font-weight: bold; }"
        "QPushButton:hover { background-color: #d0d7de; border-color: #3498db; }"
        "QPushButton:pressed { background-color: #3498db; color: white; }";
    ui->inputMin->setStyleSheet(btnStyle);
    ui->inputMax->setStyleSheet(btnStyle);

    ui->inputStepSize->setText("10000");
    ui->inputSpeed->setText("100000");

    connect(ui->inputMin, &QPushButton::clicked, this, &AxisControlWidget::onMinButtonClicked);
    connect(ui->inputMax, &QPushButton::clicked, this, &AxisControlWidget::onMaxButtonClicked);

    connect(ui->inputSetpoint, &QLineEdit::returnPressed, this, &AxisControlWidget::onExecuteMove);
    connect(ui->slider, &QSlider::sliderReleased, this, [this]() {
        ui->inputSetpoint->setText(QString::number(ui->slider->value()));
        onExecuteMove();
    });
}

AxisControlWidget::~AxisControlWidget() {
    delete ui;
}

void AxisControlWidget::setLimits(int min, int max) {
    ui->inputMin->setText(QString::number(min));
    ui->inputMax->setText(QString::number(max));

    ui->slider->setMinimum(min);
    ui->slider->setMaximum(max);

    emit limitsChanged(min, max);
}

std::pair<int, int> AxisControlWidget::getLimits() const {
    return { ui->slider->minimum(), ui->slider->maximum() };
}

void AxisControlWidget::setSetpoint(int val) {
    ui->inputSetpoint->setText(QString::number(val));
    ui->slider->blockSignals(true);
    ui->slider->setValue(val);
    ui->slider->blockSignals(false);
}

int AxisControlWidget::getSetpoint() const {
    ui->slider->setSliderPosition(ui->inputSetpoint->text().toInt());
    return ui->inputSetpoint->text().toInt();
}

void AxisControlWidget::setLocked(bool locked) {
    Q_UNUSED(locked);
}

int AxisControlWidget::getStepSize() const {
    bool ok;
    int val = ui->inputStepSize->text().toInt(&ok);
    return ok && val > 0 ? val : 10000;
}

int AxisControlWidget::getSpeed() const {
    bool ok;
    int val = ui->inputSpeed->text().toInt(&ok);
    return ok && val > 0 ? val : 100000;
}

void AxisControlWidget::setStepSize(int step) {
    ui->inputStepSize->setText(QString::number(step));
}

void AxisControlWidget::setSpeed(int speed) {
    ui->inputSpeed->setText(QString::number(speed));
}

void AxisControlWidget::onMinButtonClicked() {
    int minVal = ui->slider->minimum();
    ui->slider->blockSignals(true);
    ui->slider->setValue(minVal);
    ui->slider->blockSignals(false);

    emit moveRequested(minVal);
}

void AxisControlWidget::onMaxButtonClicked() {
    int maxVal = ui->slider->maximum();
    ui->slider->blockSignals(true);
    ui->slider->setValue(maxVal);
    ui->slider->blockSignals(false);

    emit moveRequested(maxVal);
}

void AxisControlWidget::onExecuteMove() {
    bool okPos;
    int targetPos = ui->inputSetpoint->text().toInt(&okPos);
    int minVal = ui->slider->minimum();
    int maxVal = ui->slider->maximum();

    if (okPos) {
        if (targetPos < minVal) targetPos = minVal;
        if (targetPos > maxVal) targetPos = maxVal;

        ui->slider->blockSignals(true);
        ui->slider->setValue(targetPos);
        ui->slider->blockSignals(false);

        emit moveRequested(targetPos);
        ui->inputSetpoint->clear();
        ui->inputSetpoint->clearFocus();
    }
}
