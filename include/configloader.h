#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#include <map>
#include <utility>

struct AxisConfig {
    int hwIndex;
    QString label;
    int defaultMin;
    int defaultMax;
    int defaultMaxSpeed = 0;
};

struct DeviceConfigInfo {
    QString name;
    QString resource;
    bool isMotor;
    std::vector<AxisConfig> axes;
};

struct StationConfigData {
    QString yamlPath;
    QString stationName;
    std::vector<DeviceConfigInfo> devices;
    std::map<QString, QStringList> routines; // routineName -> list of parameter names
    QStringList products;
};

class ConfigLoader {
public:
    static QString readStationName(const QString& yamlFilePath);
    static StationConfigData loadStationConfig(const QString& yamlFilePath);

    // Evaluates Fixture baselines and Product BoundaryAdjustments
    static std::map<QString, std::map<int, std::pair<int, int>>> getProductBoundaries(
        const QString& yamlFilePath,
        const QString& productName,
        const std::map<QString, std::map<int, std::pair<int, int>>>& defaultLimits,
        const std::map<QString, std::map<int, QString>>& axisNames
        );

    // Reads product-specific routine parameter overrides
    static std::map<QString, std::map<QString, QString>> getProductRoutineParams(
        const QString& yamlFilePath,
        const QString& productName
        );
};
