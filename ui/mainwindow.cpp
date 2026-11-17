#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QTime>
#include <QDir>
#include <QFileInfo>
#include <QBrush>
#include <QColor>
#include <QTextCursor>
#include <QMenu>
#include <QMouseEvent>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <algorithm>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Capture the exact launch minimum height of the remote console from mainwindow.ui (230px)
    // and cap its maximum height to that value so it never balloons past its .ui launch height.
    m_remotePanelLaunchHeight = std::max(ui->frameBottomConsole->minimumHeight(), 230);
    ui->frameBottomConsole->setMaximumHeight(m_remotePanelLaunchHeight);

    // Ensure both videoViewport and videoContainer have native HWNDs for clean clipping & SDK rendering
    ui->videoViewport->setAttribute(Qt::WA_NativeWindow, true);
    ui->videoContainer->setAttribute(Qt::WA_NativeWindow, true);
    ui->videoContainer->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    ui->videoViewport->installEventFilter(this);
    ui->videoFeedFrame->installEventFilter(this);

    // Keep the exact launch proportions from the original setup while allowing full-range smooth collapsing
    ui->verticalMainSplitter->setStretchFactor(0, 1);
    ui->verticalMainSplitter->setStretchFactor(1, 0);
    ui->mainSplitter->setStretchFactor(0, 3);
    ui->mainSplitter->setStretchFactor(1, 2);
    ui->mainSplitter->setSizes({760, 500});
    ui->verticalMainSplitter->setSizes({550, m_remotePanelLaunchHeight});

    setupCornerSplitterGrip();

    connect(ui->mainSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        updateVideoContainerGeometry();
    });
    connect(ui->verticalMainSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        updateVideoContainerGeometry();
    });

    setupControllerLabels();
    updateControllerOverlayLabels("");

    // --- Instantiate Managers ---
    m_hardwareManager = new HardwareManager(this);

    // After the initial .ui geometry is established, unlock minimum sizes and enable collapsible splitters
    // so sliders glide smoothly to 0 without sticking at 360px/480px walls.
    QTimer::singleShot(0, this, [this]() {
        QList<int> hSizes = ui->mainSplitter->sizes();
        int totalH = ui->verticalMainSplitter->height();
        int bottomH = m_remotePanelLaunchHeight;
        int topH = std::max(0, totalH - bottomH);

        ui->videoFeedFrame->setMinimumSize(0, 0);
        ui->videoViewport->setMinimumSize(0, 0);
        ui->tabWidgetPanels->setMinimumSize(0, 0);
        ui->frameBottomConsole->setMinimumSize(0, 0);
        ui->labelControllerImage->setMinimumSize(0, 0);
        ui->textControllerStream->setMinimumSize(0, 0);

        ui->mainSplitter->setChildrenCollapsible(true);
        ui->verticalMainSplitter->setChildrenCollapsible(true);
        ui->xboxHuiSplitter->setChildrenCollapsible(true);

        if (hSizes.size() >= 2 && (hSizes[0] + hSizes[1]) > 0) {
            ui->mainSplitter->setSizes(hSizes);
        }
        if (totalH > bottomH) {
            ui->verticalMainSplitter->setSizes({topH, bottomH});
        }

        updateVideoContainerGeometry();
    });

    m_inputManager = new RemoteInputManager(
        // Callback 1: Provide current step distance & speed from UI inputs (via AxisControlWidget or defaults)
        [this](int& stepDistance, int& speed) {
            QString targetName = ui->comboRemoteTarget->currentData().toString();
            stepDistance = 10000;
            speed = 100000;
            if (m_axisControlWidgets.count(targetName)) {
                for (auto& [axIdx, widget] : m_axisControlWidgets[targetName]) {
                    if (widget) {
                        stepDistance = widget->getStepSize();
                        speed = widget->getSpeed();
                        break;
                    }
                }
            }
            m_hardwareManager->setPositionPollingPaused(m_inputManager->isInputActive());
        },
        // Callback 2: Check if a dropdown popup is currently active
        [this]() -> bool {
            return activePopupCombo() != nullptr;
        },
        this
        );

    // --- Wire Hardware Manager Signals ---
    connect(m_hardwareManager, &HardwareManager::logMessage, this, &MainWindow::appendLogMessage);
    connect(m_hardwareManager, &HardwareManager::cameraStreamStateChanged, this, [this](bool streaming) {
        ui->btnToggleStream->setText(streaming ? "Stop Stream" : "Start Stream");
        ui->btnToggleStream->setStyleSheet(streaming
                                               ? "QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 4px; }"
                                               : "QPushButton { background-color: #2ecc71; color: white; font-weight: bold; border-radius: 4px; }");
        updateVideoContainerGeometry();
    });
    connect(m_hardwareManager, &HardwareManager::deviceConnectionChanged, this, [this](const QString& devName, const QString& resStr, bool connected) {
        if (m_deviceTopItems.count(devName)) {
            QTreeWidgetItem* item = m_deviceTopItems[devName];
            item->setText(1,(connected ? "   ● Connected" : "   ● Disconnected ") + QString("(%1)").arg(resStr) );
            item->setForeground(1, QBrush(QColor(connected ? "#2ecc71" : "#e74c3c")));
        }
    });
    connect(m_hardwareManager, &HardwareManager::axisPositionUpdated, this, [this](const QString& devName, int axisIdx, const QString& posText) {
        if (m_axisLivePosOutputs.count(devName) && m_axisLivePosOutputs[devName].count(axisIdx)) {
            m_axisLivePosOutputs[devName][axisIdx]->setText(posText);
        }
    });

    // --- Wire Remote Input Manager Signals ---
    connect(m_inputManager, &RemoteInputManager::logMessage, this, &MainWindow::appendLogMessage);
    connect(m_inputManager, &RemoteInputManager::connectionStateChanged, this, [this](bool connected, const QString&) {
        ui->lblRemoteStatus->setText(connected
                                         ? "<span style='color: #2ecc71; font-size: 16pt;'>&#9679;</span>"
                                         : "<span style='color: #e74c3c; font-size: 16pt;'>&#9679;</span>");
        ui->comboRemoteTarget->setEnabled(connected);
        if (connected) updateControllerTargetCombo();
    });
    connect(m_inputManager, &RemoteInputManager::targetPopupRequested, this, [this]() {
        ui->comboRemoteTarget->setFocus();
        ui->comboRemoteTarget->showPopup();
        if (ui->comboRemoteTarget->view()) {
            QModelIndex idx = ui->comboRemoteTarget->model()->index(
                ui->comboRemoteTarget->currentIndex(),
                ui->comboRemoteTarget->modelColumn(),
                ui->comboRemoteTarget->rootModelIndex());
            ui->comboRemoteTarget->view()->setCurrentIndex(idx);
        }
    });
    connect(m_inputManager, &RemoteInputManager::toggleStreamRequested, this, [this]() {
        updateVideoContainerGeometry();
        m_hardwareManager->toggleStreaming(ui->videoContainer->winId());
    });
    connect(m_inputManager, &RemoteInputManager::toggleSplitterViewRequested, this, [this]() {
        int totalH = ui->verticalMainSplitter->height();
        if (m_viewToggleState == 0) {
            ui->frameBottomConsole->setVisible(false);
            ui->verticalMainSplitter->setSizes({totalH, 0});
            m_viewToggleState = 1;
        } else {
            ui->frameBottomConsole->setVisible(true);
            int bottomH = m_remotePanelLaunchHeight;
            int topH = std::max(0, totalH - bottomH);
            ui->verticalMainSplitter->setSizes({topH > 0 ? topH : 550, bottomH});
            m_viewToggleState = 0;
        }
        updateVideoContainerGeometry();
    });
    connect(m_inputManager, &RemoteInputManager::activeTabPopupRequested, this, [this]() {
        QComboBox* targetCombo = (ui->tabWidgetPanels->currentIndex() == 0) ? ui->comboStation : ui->comboSequence;
        targetCombo->setFocus();
        targetCombo->showPopup();
        if (targetCombo->view()) {
            QModelIndex idx = targetCombo->model()->index(
                targetCombo->currentIndex(),
                targetCombo->modelColumn(),
                targetCombo->rootModelIndex());
            targetCombo->view()->setCurrentIndex(idx);
        }
    });
    connect(m_inputManager, &RemoteInputManager::switchTabRequested, ui->tabWidgetPanels, &QTabWidget::setCurrentIndex);

    // --- Wire Active Dropdown Popup Navigation (D-Pad Up/Down, A to Select, B to Escape) ---
    connect(m_inputManager, &RemoteInputManager::comboPopupNavigateRequested, this, [this](int delta) {
        QComboBox* combo = activePopupCombo();
        if (!combo || !combo->view() || combo->count() == 0) return;

        int currentRow = combo->view()->currentIndex().row();
        if (currentRow < 0) currentRow = combo->currentIndex();

        int nextRow = std::clamp(currentRow + delta, 0, combo->count() - 1);
        QModelIndex nextIdx = combo->model()->index(nextRow, combo->modelColumn(), combo->rootModelIndex());
        if (combo->view()->selectionModel()) {
            combo->view()->selectionModel()->setCurrentIndex(nextIdx, QItemSelectionModel::ClearAndSelect);
        }
        combo->view()->setCurrentIndex(nextIdx);
        combo->view()->scrollTo(nextIdx);
    });
    connect(m_inputManager, &RemoteInputManager::comboPopupSelectRequested, this, [this]() {
        QComboBox* combo = activePopupCombo();
        if (!combo || !combo->view()) return;

        int selectedRow = combo->view()->currentIndex().row();
        combo->hidePopup();
        if (selectedRow >= 0 && selectedRow < combo->count()) {
            combo->setCurrentIndex(selectedRow);
        }
    });
    connect(m_inputManager, &RemoteInputManager::comboPopupCloseRequested, this, [this]() {
        if (QComboBox* combo = activePopupCombo()) {
            combo->hidePopup();
        }
    });

    // --- Wire UI Controls ---
    connect(ui->chkParameterLock, &QCheckBox::toggled, this, [this](bool) { applyParameterLockState(); });
    connect(ui->btnToggleStream, &QPushButton::clicked, this, [this]() {
        updateVideoContainerGeometry();
        m_hardwareManager->toggleStreaming(ui->videoContainer->winId());
    });
    connect(ui->comboRemoteTarget, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onControllerTargetChanged);
    connect(ui->comboSequence, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onProductSequenceChanged);

    ui->motorHomeBtn->setVisible(false);
    connect(ui->motorHomeBtn, &QPushButton::clicked, this, [this]() {
        m_hardwareManager->homeDevice(ui->comboRemoteTarget->currentData().toString());
    });

    ui->textControllerStream->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->textControllerStream, &QTextEdit::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu *menu = ui->textControllerStream->createStandardContextMenu();
        menu->addSeparator();
        QAction *clearAction = menu->addAction("Clear Log Window");
        connect(clearAction, &QAction::triggered, ui->textControllerStream, &QTextEdit::clear);
        menu->exec(ui->textControllerStream->mapToGlobal(pos));
        delete menu;
    });

    QTimer::singleShot(100, this, [this]() {
        updateVideoContainerGeometry();
        m_hardwareManager->initCamera(ui->videoContainer->winId());
    });
    m_inputManager->initializeController();

    // --- Populate Station Dropdown ---
    connect(ui->comboStation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onStationSelected);
    ui->comboStation->blockSignals(true);
    ui->comboStation->clear();
    ui->comboStation->addItem("-- Select Station --", "");

    QDir configDir("C:/Users/aron.rezene/Documents/Work/Weld_Room_Projects/SFW/FORCE/VIRTINSTGUI_improv");
    for (const QFileInfo& fileInfo : configDir.entryInfoList({"*.yaml", "*.yml"}, QDir::Files)) {
        QString filePath = fileInfo.absoluteFilePath();
        ui->comboStation->addItem(ConfigLoader::readStationName(filePath), filePath);
    }
    ui->comboStation->setCurrentIndex(0);
    ui->comboStation->blockSignals(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupCornerSplitterGrip()
{
    // Invisible 2D corner hit-zone right where mainSplitter and verticalMainSplitter intersect
    m_cornerGrip = new QWidget(ui->centralwidget);
    m_cornerGrip->setFixedSize(20, 20);
    m_cornerGrip->setCursor(Qt::SizeFDiagCursor);
    m_cornerGrip->setStyleSheet("background: transparent; border: none;");
    m_cornerGrip->installEventFilter(this);
    m_cornerGrip->raise();
}

QComboBox* MainWindow::activePopupCombo() const
{
    if (ui->comboRemoteTarget->view() && ui->comboRemoteTarget->view()->isVisible()) {
        return ui->comboRemoteTarget;
    }
    if (ui->comboStation->view() && ui->comboStation->view()->isVisible()) {
        return ui->comboStation;
    }
    if (ui->comboSequence->view() && ui->comboSequence->view()->isVisible()) {
        return ui->comboSequence;
    }
    return nullptr;
}

void MainWindow::clearStationUi()
{
    ui->treeWidget->clear();
    ui->comboSequence->blockSignals(true);
    ui->comboSequence->clear();
    ui->comboSequence->blockSignals(false);

    m_hardwareManager->clearDevices();
    m_deviceTopItems.clear();
    m_defaultMechanicalLimits.clear();
    m_defaultMaxSpeeds.clear();
    m_axisNames.clear();
    m_axisLivePosOutputs.clear();
    m_axisControlWidgets.clear();

    QLayoutItem* child;
    while ((child = ui->verticalLayout_ProcedureSteps->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }
    m_routineWidgets.clear();

    m_inputManager->clearTargetMotor();
    updateControllerTargetCombo();
}

void MainWindow::onStationSelected(int index)
{
    QString yamlPath = ui->comboStation->itemData(index).toString();
    if (!yamlPath.isEmpty()) {
        loadStationConfig(yamlPath);
    } else {
        clearStationUi();
    }
}

void MainWindow::loadStationConfig(const QString& yamlFilePath)
{
    clearStationUi();
    m_currentConfig = ConfigLoader::loadStationConfig(yamlFilePath);

    // 1. Build Device Tree UI
    for (const auto& devConfig : m_currentConfig.devices) {
        QString devName = devConfig.name;
        bool isConnected = m_hardwareManager->connectAndSetupDevice(devConfig, m_defaultMechanicalLimits[devName]);

        QTreeWidgetItem* item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, devName);
        item->setText(1, (isConnected ? "   ● Connected" : "   ● Disconnected ") + QString("(%1)").arg(devConfig.resource));
        item->setForeground(1, QBrush(QColor(isConnected ? "#2ecc71" : "#e74c3c")));
        m_deviceTopItems[devName] = item;

        if (devConfig.isMotor) {
            for (const auto& ax : devConfig.axes) {
                m_axisNames[devName][ax.hwIndex] = ax.label;
                m_defaultMaxSpeeds[devName][ax.hwIndex] = ax.defaultMaxSpeed;
                auto [defMin, defMax] = m_defaultMechanicalLimits[devName][ax.hwIndex];

                QTreeWidgetItem* axisHeader = new QTreeWidgetItem(item);
                axisHeader->setText(0, ax.label);

                QLineEdit* livePosOutput = new QLineEdit("0", ui->treeWidget);
                livePosOutput->setReadOnly(true);
                livePosOutput->setStyleSheet("QLineEdit { background-color: #f8f9fa; color: #2c3e50; border: 1px solid #bdc3c7; font-weight: bold; }");
                ui->treeWidget->setItemWidget(axisHeader, 1, livePosOutput);
                m_axisLivePosOutputs[devName][ax.hwIndex] = livePosOutput;

                QTreeWidgetItem* rangeItem = new QTreeWidgetItem(axisHeader);
                rangeItem->setText(0, "Range & Target");

                AxisControlWidget* axisWidget = new AxisControlWidget(ui->treeWidget);
                ui->treeWidget->setItemWidget(rangeItem, 1, axisWidget);
                m_axisControlWidgets[devName][ax.hwIndex] = axisWidget;

                int hwIdx = ax.hwIndex;
                connect(axisWidget, &AxisControlWidget::moveRequested, this, [this, devName, hwIdx](int targetPos) {
                    m_hardwareManager->moveAxisAbsolute(devName, hwIdx, targetPos);
                });

                connect(axisWidget, &AxisControlWidget::limitsChanged, this, [this, devName, hwIdx](int minVal, int maxVal) {
                    int maxSpd = m_defaultMaxSpeeds[devName][hwIdx];
                    m_hardwareManager->setAxisLimits(devName, hwIdx, minVal, maxVal, maxSpd);
                });

                axisWidget->setLimits(defMin, defMax);
            }
        }
    }

    // 2. Build Routine Widgets
    for (const auto& [routineName, paramList] : m_currentConfig.routines) {
        RoutineWidget* routineWidget = new RoutineWidget(ui->scrollAreaWidgetContents);
        routineWidget->setRoutineInfo(routineName);
        for (const QString& paramName : paramList) {
            routineWidget->addParameter(paramName);
        }
        ui->verticalLayout_ProcedureSteps->addWidget(routineWidget);
        m_routineWidgets[routineName] = routineWidget;

        connect(routineWidget, &RoutineWidget::executeRequested, this, [this](const QString& rName) {
            emit executeSequenceRequested(rName, rName);
        });
    }
    ui->verticalLayout_ProcedureSteps->addSpacerItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // 3. Populate Products Dropdown
    ui->comboSequence->blockSignals(true);
    ui->comboSequence->addItem("-- Manual Input / No Product --", "");
    for (const QString& prodName : m_currentConfig.products) {
        ui->comboSequence->addItem(prodName, prodName);
    }
    ui->comboSequence->setCurrentIndex(0);
    ui->comboSequence->blockSignals(false);

    onProductSequenceChanged(0);
    updateControllerTargetCombo();
}

void MainWindow::onProductSequenceChanged(int index)
{
    bool hasProduct = (index > 0);
    QString prodName = ui->comboSequence->itemData(index).toString();

    if (hasProduct) {
        auto paramOverrides = ConfigLoader::getProductRoutineParams(m_currentConfig.yamlPath, prodName);
        for (auto& [routineName, rWidget] : m_routineWidgets) {
            for (const auto& [pName, pVal] : paramOverrides[routineName]) {
                rWidget->setParameterValue(pName, pVal, true);
            }
        }

        auto boundaries = ConfigLoader::getProductBoundaries(
            m_currentConfig.yamlPath, prodName, m_defaultMechanicalLimits, m_axisNames);
        for (auto& [devName, axisMap] : m_axisControlWidgets) {
            for (auto& [axIdx, widget] : axisMap) {
                auto [minVal, maxVal] = boundaries[devName][axIdx];
                widget->setLimits(minVal, maxVal);
            }
        }
        ui->chkParameterLock->setChecked(true);
    } else {
        for (auto& [routineName, rWidget] : m_routineWidgets) {
            for (const QString& pName : m_currentConfig.routines[routineName]) {
                rWidget->setParameterValue(pName, "", false);
            }
        }
        restoreMechanicalLimits();
        ui->chkParameterLock->setChecked(false);
    }
    applyParameterLockState();
}

void MainWindow::restoreMechanicalLimits()
{
    for (auto& [devName, axisMap] : m_axisControlWidgets) {
        for (auto& [axIdx, widget] : axisMap) {
            auto [defMin, defMax] = m_defaultMechanicalLimits[devName][axIdx];
            widget->setLimits(defMin, defMax);
        }
    }
}

void MainWindow::applyParameterLockState()
{
    bool locked = ui->chkParameterLock->isChecked();
    for (auto& [devName, axisMap] : m_axisControlWidgets) {
        for (auto& [axIdx, widget] : axisMap) {
            if (widget) widget->setLocked(locked);
        }
    }
}

void MainWindow::updateControllerTargetCombo()
{
    ui->comboRemoteTarget->blockSignals(true);
    ui->comboRemoteTarget->clear();
    ui->comboRemoteTarget->addItem("UI Navigation (Default)", "");

    for (const QString& motorName : m_hardwareManager->getConnectedMotors()) {
        ui->comboRemoteTarget->addItem(motorName, motorName);
    }

    ui->comboRemoteTarget->setCurrentIndex(0);
    ui->comboRemoteTarget->blockSignals(false);
    m_inputManager->clearTargetMotor();
    ui->motorHomeBtn->setVisible(false);
}

void MainWindow::onControllerTargetChanged(int index)
{
    ui->textControllerStream->clear();
    QString devName = ui->comboRemoteTarget->itemData(index).toString();
    updateControllerOverlayLabels(devName);

    auto inst = m_hardwareManager->getDevice(devName);
    if (devName.isEmpty() || !inst) {
        m_inputManager->clearTargetMotor();
        ui->motorHomeBtn->setVisible(false);
        appendLogMessage(QString("[%1] <span style='color: #9b59b6;'><b>Target Bound: UI Navigation</b></span><br>"
                                 "<b>Instructions / Controls:</b><br>"
                                 "• Use D-Pad to navigate tabs and menus.<br>"
                                 "• Press A to select options and B to escape.")
                             .arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
        return;
    }

    if (!inst->isOpen()) {
        appendLogMessage(QString("[%1] <span style='color: #e74c3c;'>Cannot bind target %2: Serial session closed.</span>")
                             .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName));
        m_inputManager->clearTargetMotor();
        ui->motorHomeBtn->setVisible(false);
        return;
    }

    m_inputManager->bindTargetDevice(devName, inst);
    ui->motorHomeBtn->setVisible(true);
    appendLogMessage(QString("[%1] <span style='color: #3498db;'><b>Target Bound: %2</b></span><br>"
                             "<b>Instructions / Controls:</b><br>"
                             "• Use mapped controller thumbsticks or triggers to adjust position.<br>"
                             "• Click the <b>Home</b> button above to zero out target axes.")
                         .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName));
}

void MainWindow::appendLogMessage(const QString& htmlMsg)
{
    ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
    ui->textControllerStream->append(htmlMsg);
    QTextCursor cursor = ui->textControllerStream->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->textControllerStream->setTextCursor(cursor);
}

void MainWindow::onExecuteSequenceClicked()
{
    QString seqKey = ui->comboSequence->currentData().toString();
    QString seqText = ui->comboSequence->currentText();
    if (seqKey.isEmpty()) {
        QMessageBox::warning(this, "No Sequence Selected", "Please select a product sequence before executing.");
        return;
    }
    emit executeSequenceRequested(seqKey, seqText);
}

void MainWindow::setupControllerLabels()
{
    if (!ui->labelControllerImage) return;
    ui->labelControllerImage->installEventFilter(this);
    m_controllerLabels.clear();

    for (QLabel* lbl : ui->labelControllerImage->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
        m_controllerLabels.append({lbl, lbl->x() / 400.0, lbl->y() / 400.0, lbl->width() / 400.0, lbl->height() / 400.0});
    }
    updateControllerLabelPositions();
}

void MainWindow::updateControllerLabelPositions()
{
    if (!ui->labelControllerImage) return;
    int pw = ui->labelControllerImage->width();
    int ph = ui->labelControllerImage->height();
    for (const auto& item : m_controllerLabels) {
        item.label->setGeometry(item.normX * pw, item.normY * ph, item.normW * pw, item.normH * ph);
    }
}

void MainWindow::updateVideoContainerGeometry()
{
    if (!ui->videoViewport || !ui->videoContainer) return;

    int availW = ui->videoViewport->width();
    int availH = ui->videoViewport->height();
    if (availW > 0 && availH > 0) {
        ui->videoContainer->setGeometry(0, 0, availW, availH);
        if (m_hardwareManager) {
            m_hardwareManager->updateCameraDisplayRect(ui->videoContainer->winId(), availW, availH);
        }
    }

    // Center the invisible 2D corner grip right on the splitter handle intersection
    if (m_cornerGrip && ui->videoFeedFrame) {
        int hHandleOffset = ui->mainSplitter ? (ui->mainSplitter->handleWidth() / 2) : 2;
        int vHandleOffset = ui->verticalMainSplitter ? (ui->verticalMainSplitter->handleWidth() / 2) : 2;
        QPoint intersection = ui->videoFeedFrame->mapTo(
            ui->centralwidget,
            QPoint(ui->videoFeedFrame->width() + hHandleOffset,
                   ui->videoFeedFrame->height() + vHandleOffset)
        );
        m_cornerGrip->move(intersection.x() - m_cornerGrip->width() / 2,
                           intersection.y() - m_cornerGrip->height() / 2);
        m_cornerGrip->raise();
    }
}

void MainWindow::updateControllerOverlayLabels(const QString& devName)
{
    QString devUpper = devName.toUpper();
    if (devName.isEmpty()) {
        ui->lbl_slow->setText(""); ui->lbl_bump_L->setText("Device Tab"); ui->lbl_L_analog->setText("");
        ui->lbl_dpad->setText("Navigate Menus"); ui->lbl_view->setText("Toggle View"); ui->lbl_menu->setText("Tab Pull Down Menu");
        ui->lbl_fast->setText(""); ui->lbl_bump_L->setText("Device Tab"); ui->lbl_bump_R->setText("Procedure Tab"); ui->lbl_Y->setText("Device Menu");
        ui->lbl_X->setText("Trigger Video Feed"); ui->lbl_B->setText("Escape Menu"); ui->lbl_A->setText("Select Option");
        ui->lbl_R_analog->setText("");
    } else if (devUpper.contains("ZABER")) {
        ui->lbl_slow->setText(""); ui->lbl_bump_L->setText("Z - Step"); ui->lbl_L_analog->setText("XY Jog (Toggle Speed)");
        ui->lbl_dpad->setText("XY Step"); ui->lbl_view->setText("Toggle View"); ui->lbl_menu->setText("");
        ui->lbl_fast->setText(""); ui->lbl_bump_R->setText("Z + Step"); ui->lbl_Y->setText("Device Menu");
        ui->lbl_X->setText("Trigger Video Feed"); ui->lbl_B->setText("Gripper Close"); ui->lbl_A->setText("Gripper Open");
        ui->lbl_R_analog->setText("Z Jog (Toggle Speed)");
    } else if (devUpper.contains("VELMEX") || devUpper.contains("VXM")) {
        ui->lbl_slow->setText(""); ui->lbl_bump_L->setText(""); ui->lbl_L_analog->setText("XY Jog (Toggle Speed)");
        ui->lbl_dpad->setText("XY Step"); ui->lbl_view->setText("Toggle View"); ui->lbl_menu->setText("");
        ui->lbl_fast->setText(""); ui->lbl_bump_R->setText(""); ui->lbl_Y->setText("Device Menu");
        ui->lbl_X->setText("Trigger Video Feed"); ui->lbl_B->setText("Z - Step"); ui->lbl_A->setText("Z + Step");
        ui->lbl_R_analog->setText("Z Jog (Toggle Speed)");
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_cornerGrip) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_draggingCorner = true;
                m_cornerDragStartGlobal = me->globalPosition().toPoint();
                m_startHorizSizes = ui->mainSplitter->sizes();
                m_startVertSizes = ui->verticalMainSplitter->sizes();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_draggingCorner) {
            auto *me = static_cast<QMouseEvent*>(event);
            QPoint delta = me->globalPosition().toPoint() - m_cornerDragStartGlobal;

            if (m_startHorizSizes.size() >= 2) {
                int totalW = m_startHorizSizes[0] + m_startHorizSizes[1];
                int newW0 = std::clamp(m_startHorizSizes[0] + delta.x(), 0, totalW);
                ui->mainSplitter->setSizes({newW0, totalW - newW0});
            }
            if (m_startVertSizes.size() >= 2) {
                int totalH = m_startVertSizes[0] + m_startVertSizes[1];
                int minTopH = std::max(0, totalH - m_remotePanelLaunchHeight);
                int newH0 = std::clamp(m_startVertSizes[0] + delta.y(), minTopH, totalH);
                ui->verticalMainSplitter->setSizes({newH0, totalH - newH0});
            }
            updateVideoContainerGeometry();
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_draggingCorner = false;
            return true;
        }
    }

    if (watched == ui->labelControllerImage && event->type() == QEvent::Resize) {
        updateControllerLabelPositions();
    } else if ((watched == ui->videoViewport || watched == ui->videoFeedFrame) &&
               (event->type() == QEvent::Resize || event->type() == QEvent::Move ||
                event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest)) {
        updateVideoContainerGeometry();
    }
    return QMainWindow::eventFilter(watched, event);
}
