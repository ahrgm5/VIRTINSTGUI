#include "configloader.h"
#include <yaml-cpp/yaml.h>
#include <QFileInfo>
#include <QDebug>
#include <algorithm>

QString ConfigLoader::readStationName(const QString& yamlFilePath)
{
    QFileInfo fileInfo(yamlFilePath);
    QString stationName = fileInfo.completeBaseName();
    try {
        YAML::Node config = YAML::LoadFile(yamlFilePath.toStdString());
        if (config["General"] && config["General"]["StationName"]) {
            stationName = QString::fromStdString(config["General"]["StationName"].as<std::string>());
        }
    } catch (const YAML::Exception& e) {
        qDebug() << "Could not parse StationName from" << yamlFilePath << ":" << e.what();
    }
    return stationName;
}

StationConfigData ConfigLoader::loadStationConfig(const QString& yamlFilePath)
{
    StationConfigData data;
    data.yamlPath = yamlFilePath;
    data.stationName = readStationName(yamlFilePath);

    YAML::Node config;
    try {
        config = YAML::LoadFile(yamlFilePath.toStdString());
    } catch (const YAML::Exception& e) {
        qDebug() << "Failed to parse YAML:" << e.what();
        return data;
    }

    if (config["Devices"]) {
        YAML::Node devices = config["Devices"];
        for (auto it = devices.begin(); it != devices.end(); ++it) {
            DeviceConfigInfo dev;
            dev.name = QString::fromStdString(it->first.as<std::string>());
            dev.resource = QString::fromStdString(it->second.as<std::string>());
            QString devUpper = dev.name.toUpper();
            dev.isMotor = devUpper.contains("ZABER") || devUpper.contains("VELMEX") || devUpper.contains("VXM");

            if (dev.isMotor) {
                std::vector<std::pair<int, QString>> rawAxes;
                if (devUpper.contains("ZABER")) {
                    rawAxes = { {3, "X Axis"}, {4, "Y Axis"}, {2, "Z Axis"}, {1, "Gripper"} };
                } else {
                    // Velmex Hardware Mapping: Motor 1 = X Axis (Master), Motor 3 = Y Axis (Slave), Motor 2 = Z Axis (Master)
                    rawAxes = { {1, "X Axis"}, {3, "Y Axis"}, {2, "Z Axis"} };
                }

                for (const auto& ax : rawAxes) {
                    AxisConfig axCfg;
                    axCfg.hwIndex = ax.first;
                    axCfg.label = ax.second;
                    axCfg.defaultMin = 0;

                    if (devUpper.contains("ZABER")) {
                        axCfg.defaultMax = axCfg.label.contains("Gripper", Qt::CaseInsensitive) ? 50000 : 1511811;
                        axCfg.defaultMaxSpeed = 200000;
                    } else {
                        axCfg.defaultMax = 10000;
                        axCfg.defaultMaxSpeed = 4000;
                    }

                    if (config["HardwareLimits"] && config["HardwareLimits"][dev.name.toStdString()]) {
                        YAML::Node hwNode = config["HardwareLimits"][dev.name.toStdString()][axCfg.label.toStdString()];
                        if (hwNode) {
                            if (hwNode["Min"]) axCfg.defaultMin = hwNode["Min"].as<int>();
                            if (hwNode["Max"]) axCfg.defaultMax = hwNode["Max"].as<int>();
                            if (hwNode["Speed"]) axCfg.defaultMaxSpeed = hwNode["Speed"].as<int>();
                        }
                    }

                    dev.axes.push_back(axCfg);
                }
            }
            data.devices.push_back(dev);
        }

        std::stable_partition(data.devices.begin(), data.devices.end(), [](const DeviceConfigInfo& d) {
            return !d.isMotor;
        });
    }

    if (config["Routines"]) {
        YAML::Node routines = config["Routines"];
        for (auto it = routines.begin(); it != routines.end(); ++it) {
            QString routineName = QString::fromStdString(it->first.as<std::string>());
            QStringList paramList;
            YAML::Node params = it->second;
            for (auto paramIt = params.begin(); paramIt != params.end(); ++paramIt) {
                paramList.append(QString::fromStdString(paramIt->as<std::string>()));
            }
            data.routines[routineName] = paramList;
        }
    }

    if (config["Products"]) {
        YAML::Node products = config["Products"];
        for (auto it = products.begin(); it != products.end(); ++it) {
            data.products.append(QString::fromStdString(it->first.as<std::string>()));
        }
    }

    return data;
}

std::map<QString, std::map<int, std::pair<int, int>>> ConfigLoader::getProductBoundaries(
    const QString& yamlFilePath,
    const QString& productName,
    const std::map<QString, std::map<int, std::pair<int, int>>>& defaultLimits,
    const std::map<QString, std::map<int, QString>>& axisNames)
{
    auto finalLimits = defaultLimits;
    if (productName.isEmpty() || yamlFilePath.isEmpty()) return finalLimits;

    YAML::Node config;
    try {
        config = YAML::LoadFile(yamlFilePath.toStdString());
    } catch (...) {
        return finalLimits;
    }

    if (!config["Products"] || !config["Products"][productName.toStdString()]) return finalLimits;
    YAML::Node prodNode = config["Products"][productName.toStdString()];

    std::string fixtureName;
    if (prodNode["Fixture"]) {
        fixtureName = prodNode["Fixture"].as<std::string>();
    }

    YAML::Node fixtureNode;
    if (!fixtureName.empty() && config["Fixtures"] && config["Fixtures"][fixtureName]) {
        fixtureNode = config["Fixtures"][fixtureName];
    }

    for (auto& [devName, axisMap] : finalLimits) {
        for (auto& [axIdx, limits] : axisMap) {
            QString axisLabel = axisNames.at(devName).at(axIdx);
            int finalMin = limits.first;
            int finalMax = limits.second;

            // Step A: Load Baseline from Fixture
            if (fixtureNode) {
                for (auto fDevIt = fixtureNode.begin(); fDevIt != fixtureNode.end(); ++fDevIt) {
                    QString fDevName = QString::fromStdString(fDevIt->first.as<std::string>());
                    if (devName.contains(fDevName, Qt::CaseInsensitive) || fDevName.contains(devName, Qt::CaseInsensitive)) {
                        YAML::Node axisDef = fDevIt->second[axisLabel.toStdString()];
                        if (axisDef) {
                            if (axisDef["Min"]) finalMin = axisDef["Min"].as<int>();
                            if (axisDef["Max"]) finalMax = axisDef["Max"].as<int>();
                        }
                        break;
                    }
                }
            }

            // Step B: Apply Product Geometry Displacements/Offsets
            if (prodNode["BoundaryAdjustments"]) {
                YAML::Node adjNode = prodNode["BoundaryAdjustments"];
                for (auto aDevIt = adjNode.begin(); aDevIt != adjNode.end(); ++aDevIt) {
                    QString aDevName = QString::fromStdString(aDevIt->first.as<std::string>());
                    if (devName.contains(aDevName, Qt::CaseInsensitive) || aDevName.contains(devName, Qt::CaseInsensitive)) {
                        YAML::Node adjAxis = aDevIt->second[axisLabel.toStdString()];
                        if (adjAxis) {
                            if (adjAxis["MinOffset"]) finalMin += adjAxis["MinOffset"].as<int>();
                            if (adjAxis["MaxOffset"]) finalMax += adjAxis["MaxOffset"].as<int>();
                            if (adjAxis["Min"]) finalMin = adjAxis["Min"].as<int>();
                            if (adjAxis["Max"]) finalMax = adjAxis["Max"].as<int>();
                        }
                        break;
                    }
                }
            }
            limits = { finalMin, finalMax };
        }
    }
    return finalLimits;
}

std::map<QString, std::map<QString, QString>> ConfigLoader::getProductRoutineParams(
    const QString& yamlFilePath,
    const QString& productName)
{
    std::map<QString, std::map<QString, QString>> result;
    if (productName.isEmpty() || yamlFilePath.isEmpty()) return result;

    try {
        YAML::Node config = YAML::LoadFile(yamlFilePath.toStdString());
        YAML::Node prodNode = config["Products"][productName.toStdString()];
        if (!prodNode) return result;

        if (config["Routines"]) {
            for (auto rIt = config["Routines"].begin(); rIt != config["Routines"].end(); ++rIt) {
                QString rName = QString::fromStdString(rIt->first.as<std::string>());
                YAML::Node paramList = rIt->second;
                for (auto pIt = paramList.begin(); pIt != paramList.end(); ++pIt) {
                    QString pName = QString::fromStdString(pIt->as<std::string>());
                    try {
                        std::string val = prodNode[rName.toStdString()][pName.toStdString()].as<std::string>();
                        result[rName][pName] = QString::fromStdString(val);
                    } catch (...) {
                        result[rName][pName] = "N/A";
                    }
                }
            }
        }
    } catch (...) {}

    return result;
}
