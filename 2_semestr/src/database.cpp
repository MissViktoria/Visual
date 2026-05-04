#include <string>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <libpq-fe.h>
#include "PhoneData.h"

// Конфигурация БД
#define HOST "localhost"
#define PORT "5432"
#define DB_NAME "phone_db"
#define DB_USER "may"
#define DB_USER_PASSWORD "maymay"

static void log(const std::string& level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::cout << "[" << std::put_time(localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
             << "] " << level << " - " << message << std::endl;
}

static std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(localtime(&time_t), "%H:%M:%S");
    return ss.str();
}

static std::string getCurrentISO8601() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(localtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

PGconn* initDatabase() {
    const char* conn_info = "host=" HOST " port=" PORT " dbname=" DB_NAME 
                            " user=" DB_USER " password=" DB_USER_PASSWORD;
    PGconn* db_conn = PQconnectdb(conn_info);
    
    if (PQstatus(db_conn) != CONNECTION_OK) {
        log("ERROR", std::string("Ошибка подключения к БД: ") + PQerrorMessage(db_conn));
    } else {
        log("INFO", "Подключение к PostgreSQL УСПЕШНО!");
    }
    
    return db_conn;
}

bool saveToDatabase(PGconn* db_conn, const PhoneData& data) {
    if (!db_conn || PQstatus(db_conn) != CONNECTION_OK) {
        return false;
    }
    
    // Текущее время в миллисекундах
    auto now = std::chrono::system_clock::now();
    long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    // Буферы для строк
    char ts_str[32], lat_str[32], lon_str[32], alt_str[32], acc_str[32];
    char lte_band_str[16], lte_earfcn_str[16], lte_mcc_str[16], lte_mnc_str[16];
    char lte_pci_str[16], lte_tac_str[16];
    char lte_asu_str[16], lte_cqi_str[16], lte_rsrp_str[16], lte_rsrq_str[16];
    char lte_rssi_str[16], lte_rssnr_str[16], lte_ta_str[16];
    
    snprintf(ts_str, sizeof(ts_str), "%lld", timestamp);
    snprintf(lat_str, sizeof(lat_str), "%f", data.latitude);
    snprintf(lon_str, sizeof(lon_str), "%f", data.longitude);
    snprintf(alt_str, sizeof(alt_str), "%f", data.altitude);
    snprintf(acc_str, sizeof(acc_str), "%f", data.accuracy);
    
    // LTE Identity
    snprintf(lte_band_str, sizeof(lte_band_str), "%d", data.lteIdentity.band);
    snprintf(lte_earfcn_str, sizeof(lte_earfcn_str), "%d", data.lteIdentity.earfcn);
    snprintf(lte_mcc_str, sizeof(lte_mcc_str), "%d", data.lteIdentity.mcc);
    snprintf(lte_mnc_str, sizeof(lte_mnc_str), "%d", data.lteIdentity.mnc);
    snprintf(lte_pci_str, sizeof(lte_pci_str), "%d", data.lteIdentity.pci);
    snprintf(lte_tac_str, sizeof(lte_tac_str), "%d", data.lteIdentity.tac);
    
    // LTE Signal
    snprintf(lte_asu_str, sizeof(lte_asu_str), "%d", data.lteSignal.asuLevel);
    snprintf(lte_cqi_str, sizeof(lte_cqi_str), "%d", data.lteSignal.cqi);
    snprintf(lte_rsrp_str, sizeof(lte_rsrp_str), "%d", data.lteSignal.rsrp);
    snprintf(lte_rsrq_str, sizeof(lte_rsrq_str), "%d", data.lteSignal.rsrq);
    snprintf(lte_rssi_str, sizeof(lte_rssi_str), "%d", data.lteSignal.rssi);
    snprintf(lte_rssnr_str, sizeof(lte_rssnr_str), "%d", data.lteSignal.rssnr);
    snprintf(lte_ta_str, sizeof(lte_ta_str), "%d", data.lteSignal.timingAdvance);
    
    // ПОЛНЫЙ SQL запрос со ВСЕМИ полями
    const char* query = 
        "INSERT INTO data ("
        "timestamp, latitude, longitude, altitude, accuracy, location_time, network_type, "
        "lte_band, lte_earfcn, lte_mcc, lte_mnc, lte_pci, lte_tac, lte_cell_identity, "
        "lte_asu_level, lte_cqi, lte_rsrp, lte_rsrq, lte_rssi, lte_rssnr, lte_timing_advance"
        ") VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20, $21)";
    
    const char* params[] = {
        ts_str, lat_str, lon_str, alt_str, acc_str,
        data.time.c_str(), data.networkType.c_str(),
        lte_band_str, lte_earfcn_str, lte_mcc_str, lte_mnc_str,
        lte_pci_str, lte_tac_str, data.lteIdentity.cellIdentity.c_str(),
        lte_asu_str, lte_cqi_str, lte_rsrp_str, lte_rsrq_str,
        lte_rssi_str, lte_rssnr_str, lte_ta_str
    };
    
    PGresult* res = PQexecParams(db_conn, query, 21, NULL, params, NULL, NULL, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        log("ERROR", std::string("Ошибка вставки: ") + PQresultErrorMessage(res));
        PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}