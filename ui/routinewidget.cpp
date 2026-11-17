#include "routinewidget.h"
#include "ui_routinewidget.h"
#include <QTreeWidgetItem>

RoutineWidget::RoutineWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RoutineWidget)
{
    ui->setupUi(this);
    
    // Set headers manually just in case .ui file configuration gets overridden
    ui->paramTree->setHeaderLabels({"Parameter", "Input Value"});
    
    connect(ui->btnExecute, &QPushButton::clicked, this, [this]() {
        emit executeRequested(m_routineName);
    });
}

RoutineWidget::~RoutineWidget() {
    delete ui;
}

void RoutineWidget::setRoutineInfo(const QString& routineName, const QString& description) {
    m_routineName = routineName;
    ui->routineTitleBox->setHtml(QString("<b>%1</b><br>%2").arg(routineName, description));
}

void RoutineWidget::addParameter(const QString& paramName, const QString& defaultValue) {
    QTreeWidgetItem* treeItem = new QTreeWidgetItem(ui->paramTree);
    treeItem->setText(0, paramName);
    
    QLineEdit* inputBox = new QLineEdit(ui->paramTree);
    inputBox->setPlaceholderText("Enter value...");
    inputBox->setText(defaultValue);
    inputBox->setStyleSheet("QLineEdit { background-color: white; color: black; }");
    
    ui->paramTree->setItemWidget(treeItem, 1, inputBox);
    m_paramInputs[paramName] = inputBox;
}

void RoutineWidget::setParameterValue(const QString& paramName, const QString& value, bool readOnly) {
    if (m_paramInputs.count(paramName)) {
        QLineEdit* inputBox = m_paramInputs[paramName];
        inputBox->setText(value);
        inputBox->setReadOnly(readOnly);
        
        if (readOnly) {
            inputBox->setStyleSheet("QLineEdit { background-color: #bdc3c7; color: #7f8c8d; }");
        } else {
            inputBox->setStyleSheet("QLineEdit { background-color: white; color: black; }");
        }
    }
}

void RoutineWidget::clearParameters() {
    ui->paramTree->clear();
    m_paramInputs.clear();
}

QString RoutineWidget::getRoutineName() const {
    return m_routineName;
}
