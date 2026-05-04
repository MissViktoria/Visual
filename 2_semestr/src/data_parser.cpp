#include "PhoneData.h"
#include <ctime>
#include <chrono>

// Глобальные переменные
PhoneData g_phone_data;
std::mutex g_data_mutex;

void updatePhoneData(const json& data) {
    PhoneData new_data;
    
    // Парсинг геолокации
    if (data.contains("location")) {
        const auto& loc = data["location"];
        new_data.latitude = loc.value("latitude", 55.7558);
        new_data.longitude = loc.value("longitude", 37.1234);
        new_data.altitude = loc.value("altitude", 0.0);
        new_data.accuracy = loc.value("accuracy", 0.0f);
        
        long timestamp = loc.value("timestamp", 0L);
        if (timestamp > 0) {
            time_t seconds = timestamp / 1000;
            char buffer[80];
            strftime(buffer, sizeof(buffer), "%H:%M:%S", localtime(&seconds));
            new_data.time = std::string(buffer);
        }
    }
    
    // Парсинг информации о соте
    if (data.contains("cellInfo")) {
        const auto& cell = data["cellInfo"];
        new_data.networkType = cell.value("networkType", "UNKNOWN");

        // Заполняем список сот
        new_data.allCells.clear();
        
        // LTE данные
        if (new_data.networkType == "LTE" && cell.contains("identity")) {
            const auto& identity = cell["identity"];
            new_data.lteIdentity.band = identity.value("band", 0);
            new_data.lteIdentity.earfcn = identity.value("earfcn", 0);
            new_data.lteIdentity.mcc = identity.value("mcc", 0);
            new_data.lteIdentity.mnc = identity.value("mnc", 0);
            new_data.lteIdentity.pci = identity.value("pci", 0);
            new_data.lteIdentity.tac = identity.value("tac", 0);
            new_data.lteIdentity.cellIdentity = identity.value("cellIdentity", ""); 
        }
        
        if (new_data.networkType == "LTE" && cell.contains("signalStrength")) {
            const auto& signal = cell["signalStrength"];
            new_data.lteSignal.asuLevel = signal.value("asuLevel", 0);
            new_data.lteSignal.cqi = signal.value("cqi", 0);
            new_data.lteSignal.rsrp = signal.value("rsrp", -140);
            new_data.lteSignal.rsrq = signal.value("rsrq", -20);
            new_data.lteSignal.rssi = signal.value("rssi", -120);
            new_data.lteSignal.rssnr = signal.value("rssnr", 0);
            new_data.lteSignal.timingAdvance = signal.value("timingAdvance", 0);

            // Добавляем текущ. соту в список
            CellMeasurement currentCell;
            currentCell.pci = new_data.lteIdentity.pci;
            currentCell.rsrp = new_data.lteSignal.rsrp;
            currentCell.rsrq = new_data.lteSignal.rsrq;
            currentCell.sinr = new_data.lteSignal.rssnr;
            currentCell.networkType = "LTE";
            new_data.allCells.push_back(currentCell);
            
            // если есть сосдние соты
            if (cell.contains("neighboringCells")) {
                for (const auto& neighbor : cell["neighboringCells"]) {
                    CellMeasurement neighborCell;
                    neighborCell.pci = neighbor.value("pci", 0);
                    neighborCell.rsrp = neighbor.value("rsrp", -140);
                    neighborCell.rsrq = neighbor.value("rsrq", -20);
                    neighborCell.sinr = neighbor.value("sinr", 0);
                    neighborCell.networkType = "LTE";
                    new_data.allCells.push_back(neighborCell);
                }
            }
        }
        
        // GSM данные
        if (new_data.networkType == "GSM" && cell.contains("identity")) {
            const auto& identity = cell["identity"];
            new_data.gsmIdentity.bsic = identity.value("bsic", 0);
            new_data.gsmIdentity.arfcn = identity.value("arfcn", 0);
            new_data.gsmIdentity.lac = identity.value("lac", 0);
            new_data.gsmIdentity.mcc = identity.value("mcc", 0);
            new_data.gsmIdentity.mnc = identity.value("mnc", 0);
            new_data.gsmIdentity.psc = identity.value("psc", 0);
            new_data.gsmIdentity.cellIdentity = identity.value("cellIdentity", "");
        }
        
        if (new_data.networkType == "GSM" && cell.contains("signalStrength")) {
            const auto& signal = cell["signalStrength"];
            new_data.gsmSignal.dbm = signal.value("dbm", -120);
            new_data.gsmSignal.rssi = signal.value("rssi", 0);
            new_data.gsmSignal.timingAdvance = signal.value("timingAdvance", 0);
        }
        
        // 5G NR данные
        if (new_data.networkType == "5G_NR" && cell.contains("identity")) {
            const auto& identity = cell["identity"];
            new_data.nrIdentity.band = identity.value("band", 0);
            new_data.nrIdentity.nci = identity.value("nci", 0);
            new_data.nrIdentity.pci = identity.value("pci", 0);
            new_data.nrIdentity.nrarfcn = identity.value("nrarfcn", 0);
            new_data.nrIdentity.tac = identity.value("tac", 0);
            new_data.nrIdentity.mcc = identity.value("mcc", 0);
            new_data.nrIdentity.mnc = identity.value("mnc", 0);
        }
        
        if (new_data.networkType == "5G_NR" && cell.contains("signalStrength")) {
            const auto& signal = cell["signalStrength"];
            new_data.nrSignal.ssRsrp = signal.value("ssRsrp", -140);
            new_data.nrSignal.ssRsrq = signal.value("ssRsrq", -20);
            new_data.nrSignal.ssSinr = signal.value("ssSinr", 0);
            new_data.nrSignal.timingAdvance = signal.value("timingAdvance", 0);
        }
    }
    
    // Парсинг данных о трафике (опционально)
    if (data.contains("traffic")) {
        const auto& traffic = data["traffic"];
        new_data.totalTxBytes = traffic.value("totalTxBytes", 0LL);
        new_data.totalRxBytes = traffic.value("totalRxBytes", 0LL);
        
        if (traffic.contains("topApps")) {
            new_data.topApps.clear();
            for (const auto& app : traffic["topApps"]) {
                new_data.topApps.push_back({
                    app.value("packageName", "unknown"),
                    app.value("bytes", 0LL)
                });
            }
        }
    }
    
    new_data.deviceId = data.value("deviceId", "unknown");
    
    std::lock_guard<std::mutex> lock(g_data_mutex);
    g_phone_data = new_data;
}