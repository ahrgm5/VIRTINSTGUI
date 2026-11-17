#pragma once
#include <QWidget>
#include <QLineEdit>
#include <map>
#include <QString>

namespace Ui {
class RoutineWidget;
}

class RoutineWidget : public QWidget {
    Q_OBJECT

public:
    explicit RoutineWidget(QWidget *parent = nullptr);
    ~RoutineWidget();

    void setRoutineInfo(const QString& routineName, const QString& description = "Routine execution parameters.");
    void addParameter(const QString& paramName, const QString& defaultValue = "");
    void setParameterValue(const QString& paramName, const QString& value, bool readOnly = false);
    void clearParameters();
    
    QString getRoutineName() const;

signals:
    void executeRequested(const QString& routineName);

private:
    Ui::RoutineWidget *ui;
    QString m_routineName;
    std::map<QString, QLineEdit*> m_paramInputs;
};
