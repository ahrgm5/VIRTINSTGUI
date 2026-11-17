#pragma once
#include <QWidget>
#include <utility>

namespace Ui {
class AxisControlWidget;
}

class AxisControlWidget : public QWidget {
    Q_OBJECT

public:
    explicit AxisControlWidget(QWidget *parent = nullptr);
    ~AxisControlWidget();

    void setLimits(int min, int max);
    std::pair<int, int> getLimits() const;

    void setSetpoint(int val);
    int getSetpoint() const;
    void setLocked(bool locked);

    // Per-axis step size and speed controls
    int getStepSize() const;
    int getSpeed() const;
    void setStepSize(int step);
    void setSpeed(int speed);

signals:
    void moveRequested(int targetPos);
    void limitsChanged(int minVal, int maxVal);

private slots:
    void onMinButtonClicked();
    void onMaxButtonClicked();
    void onExecuteMove();

private:
    Ui::AxisControlWidget *ui;
};
