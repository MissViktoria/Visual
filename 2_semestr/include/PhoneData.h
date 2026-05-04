#ifndef PHONE_DATA_H
#define PHONE_DATA_H

#include <string>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Структура для хранения данных одной соты
struct CellMeasurement {
    int pci = 0;
    int rsrp = -140;
    int rsrq = -20;
    int sinr = 0;
    std::string networkType = "LTE";
};

// Расширенная структура данных с телефона
struct CellIdentityLte {
    int band = 0;
    int earfcn = 0;
    int mcc = 0;
    int mnc = 0;
    int pci = 0;
    int tac = 0;
    std::string cellIdentity;
};

struct CellSignalStrengthLte {
    int asuLevel = 0;
    int cqi = 0;
    int rsrp = 0;
    int rsrq = 0;
    int rssi = 0;
    int rssnr = 0;
    int timingAdvance = 0;
};

struct CellIdentityGsm {
    int bsic = 0;
    int arfcn = 0;
    int lac = 0;
    int mcc = 0;
    int mnc = 0;
    int psc = 0;
    std::string cellIdentity;
};

struct CellSignalStrengthGsm {
    int dbm = 0;
    int rssi = 0;
    int timingAdvance = 0;
};

struct CellIdentityNr {
    int band = 0;
    int nci = 0;
    int pci = 0;
    int nrarfcn = 0;
    int tac = 0;
    int mcc = 0;
    int mnc = 0;
};

struct CellSignalStrengthNr {
    int ssRsrp = 0;
    int ssRsrq = 0;
    int ssSinr = 0;
    int timingAdvance = 0;
};

// Данные с телефона
struct PhoneData {
    // Геолокация
    double latitude = 55.7558;
    double longitude = 37.1234;
    double altitude = 0.0;
    float accuracy = 0.0f;
    std::string time = "N/A";
    
    // Тип сети
    std::string networkType = "UNKNOWN";
    
    // LTE данные
    CellIdentityLte lteIdentity;
    CellSignalStrengthLte lteSignal;
    
    // GSM данные
    CellIdentityGsm gsmIdentity;
    CellSignalStrengthGsm gsmSignal;
    
    // 5G NR данные
    CellIdentityNr nrIdentity;
    CellSignalStrengthNr nrSignal;
    
    // Трафик
    long long totalTxBytes = 0;
    long long totalRxBytes = 0;
    std::vector<std::pair<std::string, long long>> topApps;

    // Список всех сот
    std::vector<CellMeasurement> allCells;
    
    std::string deviceId = "unknown";
};

// Глобальная переменная для данных
extern PhoneData g_phone_data;
extern std::mutex g_data_mutex;

// Функция обновления данных
void updatePhoneData(const json& data);

#endif // PHONE_DATA_H