#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QLabel>
#include <QTreeWidgetItem>
#include <QLineEdit>
#include <QList>
#include <QPoint>
#include <map>
#include <utility>

#include "axiscontrolwidget.h"
#include "routinewidget.h"
#include "configloader.h"
#include "hardwaremanager.h"
#include "remoteinputmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void executeSequenceRequested(const QString& sequenceKey, const QString& sequenceDisplayName);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onStationSelected(int index);
    void onProductSequenceChanged(int index);
    void onExecuteSequenceClicked();
    void onControllerTargetChanged(int index);

private:
    void clearStationUi();
    void loadStationConfig(const QString& yamlFilePath);
    void updateControllerTargetCombo();
    void appendLogMessage(const QString& htmlMsg);
    QComboBox* activePopupCombo() const;

    // Controller Overlay & Video Viewport Helpers
    void setupControllerLabels();
    void updateControllerLabelPositions();
    void updateControllerOverlayLabels(const QString& devName);
    void updateVideoContainerGeometry();
    void setupCornerSplitterGrip();

    // Boundary & Parameter Lock Management
    void applyParameterLockState();
    void restoreMechanicalLimits();

    Ui::MainWindow *ui;
    HardwareManager *m_hardwareManager;
    RemoteInputManager *m_inputManager;
    QWidget *m_cornerGrip = nullptr;
    bool m_draggingCorner = false;
    QPoint m_cornerDragStartGlobal;
    QList<int> m_startHorizSizes;
    QList<int> m_startVertSizes;
    int m_remotePanelLaunchHeight = 230;

    struct ControllerLabelInfo {
        QLabel *label;
        double normX;
        double normY;
        double normW;
        double normH;
    };
    QList<ControllerLabelInfo> m_controllerLabels;
    int m_viewToggleState = 0;

    // Configuration State
    StationConfigData m_currentConfig;
    std::map<QString, std::map<int, std::pair<int, int>>> m_defaultMechanicalLimits;
    std::map<QString, std::map<int, int>> m_defaultMaxSpeeds;
    std::map<QString, std::map<int, QString>> m_axisNames;

    // Dynamic Device UI Pointers
    std::map<QString, QTreeWidgetItem*> m_deviceTopItems;
    std::map<QString, std::map<int, QLineEdit*>> m_axisLivePosOutputs;
    std::map<QString, std::map<int, AxisControlWidget*>> m_axisControlWidgets;
    std::map<QString, RoutineWidget*> m_routineWidgets;
};
