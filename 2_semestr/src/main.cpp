#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <zmq.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdlib>
#include <mutex> 
#include <fstream>  
#include <iomanip>
#include <ctime>
#include <signal.h>
#include <atomic>
#include <vector>
#include <map>
#include <libpq-fe.h>
#include <filesystem>

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"
#include "implot.h"

#include "../include/TileManager.h"

using namespace std;
using json = nlohmann::json;

#define HOST "localhost"
#define PORT "5432"
#define DB_NAME "phone_db"
#define DB_USER "may"
#define DB_USER_PASSWORD "maymay"



// Функции преобразования координат
static double PI = 3.14159265358979323846;
static double RAD = PI / 180.0;
static double DEG = 180.0 / PI;



// Структура для хранения данных одной соты
struct CellMeasurement {
    int pci = 0;
    int rsrp = -140;
    int rsrq = -20;
    int sinr = 0;
    string networkType = "LTE";
};



// Расширенная структура данных с телефона
struct CellIdentityLte {
    int band = 0;
    int earfcn = 0;
    int mcc = 0;
    int mnc = 0;
    int pci = 0;
    int tac = 0;
    string cellIdentity;
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
    string cellIdentity;
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
    string time = "N/A";
    
    // Тип сети
    string networkType = "UNKNOWN";
    
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
    vector<pair<string, long long>> topApps; // название приложения - байты

    // Список всех сот
    vector<CellMeasurement> allCells;
    
    string deviceId = "unknown";
};

PhoneData g_phone_data;
mutex g_data_mutex;

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
            new_data.time = string(buffer);
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
    
    lock_guard<mutex> lock(g_data_mutex);
    g_phone_data = new_data;
}

// Класс сервера
class ZmqServer {
private:
    string host;
    int port;
    zmq::context_t context;
    zmq::socket_t socket;
    string data_file;
    int counter;
    atomic<bool> running{true};
    
    PGconn* db_conn = nullptr;

    void log(const string& level, const string& message) {
        auto now = chrono::system_clock::now();
        auto time_t = chrono::system_clock::to_time_t(now);
        cout << "[" << put_time(localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
             << "] " << level << " - " << message << endl;
    }

    string getCurrentTime() {
        auto now = chrono::system_clock::now();
        auto time_t = chrono::system_clock::to_time_t(now);
        stringstream ss;
        ss << put_time(localtime(&time_t), "%H:%M:%S");
        return ss.str();
    }

    string getCurrentISO8601() {
        auto now = chrono::system_clock::now();
        auto time_t = chrono::system_clock::to_time_t(now);
        stringstream ss;
        ss << put_time(localtime(&time_t), "%Y-%m-%dT%H:%M:%S");
        return ss.str();
    }

public:
    ZmqServer(string h = "*", int p = 7777) 
        : host(h), port(p), context(1), socket(context, ZMQ_PULL), counter(0) {
        data_file = "android_data.json";
        
        // Создаем файл если не существует
        ifstream infile(data_file);
        if (!infile.good()) {
            ofstream outfile(data_file);
            outfile << "[]";
            outfile.close();
        }
    }



    bool saveToDatabase(const PhoneData& data) {
        if (!db_conn || PQstatus(db_conn) != CONNECTION_OK) {
            return false;
        }
        
        // Текущее время в миллисекундах
        auto now = chrono::system_clock::now();
        long long timestamp = chrono::duration_cast<chrono::milliseconds>(
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
            log("ERROR", string("Ошибка вставки: ") + PQresultErrorMessage(res));
            PQclear(res);
            return false;
        }
        
        PQclear(res);
        return true;
    }




    void start() {
        try {
            string address = "tcp://" + host + ":" + to_string(port);
            socket.bind(address);
            
            log("INFO", "ZMQ сервер запущен на " + address);
            log("INFO", "Ожидание данных от Android...");
            
            const char* conn_info = "host=" HOST " port=" PORT " dbname=" DB_NAME 
                                    " user=" DB_USER " password=" DB_USER_PASSWORD;
            db_conn = PQconnectdb(conn_info);
            
            if (PQstatus(db_conn) != CONNECTION_OK) {
                log("ERROR", string("Ошибка подключения к БД: ") + PQerrorMessage(db_conn));
            } else {
                log("INFO", "Подключение к PostgreSQL УСПЕШНО!");
            }

            cout << "\n" << string(80, '=') << endl;
            cout << "СЕРВЕР ДАННЫХ МЕСТОПОЛОЖЕНИЯ (PUSH/PULL режим)" << endl;
            cout << "Порт: " << port << endl;
            cout << "Статус: Ожидание данных..." << endl;
            cout << string(80, '=') << "\n" << endl;

            while (running) {
                try {
                    zmq::message_t message;
                    auto received = socket.recv(message, zmq::recv_flags::none);
                    
                    if (!received || !running) {
                        continue;
                    }

                    string msg_str(static_cast<char*>(message.data()), message.size());
                    
                    log("INFO", "[" + getCurrentTime() + "] Получены данные №" + to_string(counter + 1));

                    auto data = json::parse(msg_str);

                    counter++;
                    saveData(data);
                    printData(data);
                    
                    // Обновляем данные для GUI
                    ::updatePhoneData(data);


                    PhoneData current_data;
                    {
                        lock_guard<mutex> lock(g_data_mutex);
                        current_data = g_phone_data;
                    }
                    
                    if (db_conn && PQstatus(db_conn) == CONNECTION_OK) {
                        if (saveToDatabase(current_data)) {
                            log("INFO", "Данные сохранены в PostgreSQL");
                        } else {
                            log("ERROR", "Ошибка сохранения в PostgreSQL");
                        }
                    }

                    

                } catch (const json::parse_error& e) {
                    log("ERROR", string("Ошибка декодирования JSON: ") + e.what());
                } catch (const exception& e) {
                    log("ERROR", string("Ошибка: ") + e.what());
                }
            }
        } catch (const exception& e) {
            log("ERROR", string("Фатальная ошибка сервера: ") + e.what());
        }
    }

    void stop() {
        running = false;
        socket.close();

        if (db_conn) {
        PQfinish(db_conn);
        db_conn = nullptr;
        log("INFO", "Соединение с PostgreSQL закрыто");
    }
    }

    void saveData(const json& data) {
        try {
            ifstream infile(data_file);
            json all_data;
            infile >> all_data;
            infile.close();

            json data_entry = {
                {"timestamp", getCurrentISO8601()},
                {"data", data},
                {"counter", counter}
            };

            all_data.push_back(data_entry);

            ofstream outfile(data_file);
            outfile << setw(2) << all_data << endl;
            outfile.close();

            log("INFO", "Данные сохранены в файл (всего записей: " + to_string(counter) + ")");

        } catch (const exception& e) {
            log("ERROR", string("Ошибка при сохранении: ") + e.what());
        }
    }

    void printData(const json& data) {
        try {
            cout << "\n" << string(80, '=') << endl;
            cout << " ДАННЫЕ ОТ УСТРОЙСТВА: " 
                 << data.value("deviceId", "unknown") << endl;
            cout << string(80, '=') << endl;

            auto location = data.value("location", json::object());
            cout << "МЕСТОПОЛОЖЕНИЕ:" << endl;
            cout << "   Широта: " << fixed << setprecision(6) 
                 << location.value("latitude", 0.0) << endl;
            cout << "   Долгота: " << location.value("longitude", 0.0) << endl;
            cout << "   Высота: " << location.value("altitude", 0.0) << " м" << endl;

            long timestamp = location.value("timestamp", 0L);
            if (timestamp > 0) {
                time_t seconds = timestamp / 1000;
                cout << "   Время: " << put_time(localtime(&seconds), "%H:%M:%S") << endl;
            }

            auto cell_info = data.value("cellInfo", json::object());
            cout << "ИНФОРМАЦИЯ О СОТОВОЙ СЕТИ:" << endl;
            cout << "   Тип сети: " << cell_info.value("networkType", "UNKNOWN") << endl;

        } catch (const exception& e) {
            log("ERROR", string("Ошибка при выводе данных: ") + e.what());
        }
    }
};

// Глобальный указатель на сервер для сигнала
ZmqServer* g_server = nullptr;

void signalHandler(int signum) {
    cout << "\nВСЕ СОХРАНЕННЫЕ ДАННЫЕ:" << endl;
    cout << string(80, '=') << endl;
    
    try {
        ifstream infile("android_data.json");
        json data;
        infile >> data;
        cout << "Всего записей: " << data.size() << endl;
        
        int count = 0;
        for (auto it = data.rbegin(); it != data.rend() && count < 5; ++it, ++count) {
            cout << "\n[" << (*it)["timestamp"] << "] Устройство: " 
                 << (*it)["data"].value("deviceId", "unknown") << endl;
        }
    } catch (const exception& e) {
        cout << "Ошибка чтения файла: " << e.what() << endl;
    }
    
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

void run_gui(){


    // Создаем менеджер тайлов
    static TileManager tileManager;
    
    // Переменные для карты
    static double mapLat = 55.051661;   // твоя широта
    static double mapLon = 82.914767;   // твоя долгота  
    static int mapZoom = 16;
    static bool dragging = false;
    static ImVec2 lastMousePos;
    static GLuint mapTexture = 0;

    // Хранилище текстур для тайлов 
    static map<string, GLuint> textureCache;

    // Хранилище истории сигналов для всех PCI
    static map<int, vector<float>> rsrpHistoryByPci;  // PCI - история RSRP
    static map<int, vector<float>> sinrHistoryByPci;  // PCI - история SINR



    // 1) Инициализация SDL
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    SDL_Window* window = SDL_CreateWindow(
        "Backend start", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    // 2) Инициализация контекста Dear Imgui
    ImGui::CreateContext();
    ImPlot::CreateContext();

    // Ввод/вывод
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Включить Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Включить Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Включить Docking

 // 2.1) Привязка Imgui к SDL2 и OpenGl backend'ам
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    // 3) Игра началась
// 3.0) Обработка event'ов (inputs, window resize, mouse moving, etc.);
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            cout << "Processing some event: "<< event.type << " timestamp: " << event.motion.timestamp << endl;
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

// 3.1) Начинаем создавать новый фрейм;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_None);

// 3.2) Наш виджет с кнопкой;
        {
            static int counter = 0;

            ImGui::Begin("Hello, world!"); 
            ImGui::Text("This is some useful text.");    
            if (ImGui::Button("Button"))                         
                counter++;
            ImGui::Text("counter = %d", counter);
            ImGui::End();
        }
// 3.2) Наш виджет с данными;
       // Окно с детальной информацией
    {
        PhoneData local_data;
        {
            lock_guard<mutex> lock(g_data_mutex);
            local_data = g_phone_data;
        }

        // Основное окно
        ImGui::Begin("Dannue c telefona");
        
        // Информация об устройстве
        ImGui::Text("Ystroustvo: %s", local_data.deviceId.c_str());
        ImGui::Text("Bremi: %s", local_data.time.c_str());
        ImGui::Separator();
        
        // Геолокация
        ImGui::Text("Mestopolochenie:");
        ImGui::Text("  Shirota: %.6f", local_data.latitude);
        ImGui::Text("  Dolgota: %.6f", local_data.longitude);
        ImGui::Text("  Bysota: %.2f m", local_data.altitude);
        ImGui::Text("  Tochnosti: %.2f m", local_data.accuracy);
        ImGui::Separator();
        
        // Информация о сети
        ImGui::Text("Sotovai seti: %s", local_data.networkType.c_str());
        
        // LTE данные
        if (local_data.networkType == "LTE") {
            if (ImGui::CollapsingHeader("LTE Identity")) {
                ImGui::Text("  Band: %d", local_data.lteIdentity.band);
                ImGui::Text("  EARFCN: %d", local_data.lteIdentity.earfcn);
                ImGui::Text("  MCC/MNC: %d/%d", local_data.lteIdentity.mcc, local_data.lteIdentity.mnc);
                ImGui::Text("  PCI: %d", local_data.lteIdentity.pci);
                ImGui::Text("  TAC: %d", local_data.lteIdentity.tac);
                ImGui::Text("  Cell ID: %s", local_data.lteIdentity.cellIdentity.c_str());
            }
            
            if (ImGui::CollapsingHeader("LTE Signal Strength")) {
                ImGui::Text("  RSRP: %d dBm", local_data.lteSignal.rsrp);
                ImGui::Text("  RSRQ: %d dB", local_data.lteSignal.rsrq);
                ImGui::Text("  RSSI: %d dBm", local_data.lteSignal.rssi);
                ImGui::Text("  RSSNR: %d dB", local_data.lteSignal.rssnr);
                ImGui::Text("  CQI: %d", local_data.lteSignal.cqi);
                ImGui::Text("  ASU Level: %d", local_data.lteSignal.asuLevel);
                ImGui::Text("  Timing Advance: %d", local_data.lteSignal.timingAdvance);
                
            }
        }
        
        // GSM данные
        if (local_data.networkType == "GSM") {
            if (ImGui::CollapsingHeader("GSM Identity")) {
                ImGui::Text("  BSIC: %d", local_data.gsmIdentity.bsic);
                ImGui::Text("  ARFCN: %d", local_data.gsmIdentity.arfcn);
                ImGui::Text("  LAC: %d", local_data.gsmIdentity.lac);
                ImGui::Text("  MCC/MNC: %d/%d", local_data.gsmIdentity.mcc, local_data.gsmIdentity.mnc);
                ImGui::Text("  PSC: %d", local_data.gsmIdentity.psc);
                ImGui::Text("  Cell ID: %s", local_data.gsmIdentity.cellIdentity.c_str());
            }
            
            if (ImGui::CollapsingHeader("GSM Signal Strength")) {
                ImGui::Text("  DBM: %d dBm", local_data.gsmSignal.dbm);
                ImGui::Text("  RSSI: %d", local_data.gsmSignal.rssi);
                ImGui::Text("  Timing Advance: %d", local_data.gsmSignal.timingAdvance);
            }
        }
        
        // 5G NR данные
        if (local_data.networkType == "5G_NR") {
            if (ImGui::CollapsingHeader("5G NR Identity")) {
                ImGui::Text("  Band: %d", local_data.nrIdentity.band);
                ImGui::Text("  NCI: %d", local_data.nrIdentity.nci);
                ImGui::Text("  PCI: %d", local_data.nrIdentity.pci);
                ImGui::Text("  NRARFCN: %d", local_data.nrIdentity.nrarfcn);
                ImGui::Text("  TAC: %d", local_data.nrIdentity.tac);
                ImGui::Text("  MCC/MNC: %d/%d", local_data.nrIdentity.mcc, local_data.nrIdentity.mnc);
            }
            
            if (ImGui::CollapsingHeader("5G NR Signal Strength")) {
                ImGui::Text("  SS-RSRP: %d dBm", local_data.nrSignal.ssRsrp);
                ImGui::Text("  SS-RSRQ: %d dB", local_data.nrSignal.ssRsrq);
                ImGui::Text("  SS-SINR: %d dB", local_data.nrSignal.ssSinr);
                ImGui::Text("  Timing Advance: %d", local_data.nrSignal.timingAdvance);
            }
        }
        
        // Данные о трафике (опционально)
        if (local_data.totalTxBytes > 0 || local_data.totalRxBytes > 0) {
            ImGui::Separator();
            ImGui::Text("Sotovui trafik:");
            ImGui::Text("  Peredano: %.2f MB", local_data.totalTxBytes / (1024.0 * 1024.0));
            ImGui::Text("  Polucheno: %.2f MB", local_data.totalRxBytes / (1024.0 * 1024.0));
            
            if (!local_data.topApps.empty()) {
                ImGui::Text("Top prilocheiy po trafiku:");
                for (const auto& app : local_data.topApps) {
                    ImGui::Text("  %s: %.2f MB", 
                            app.first.c_str(), 
                            app.second / (1024.0 * 1024.0));
                }
            }
        }
        
        ImGui::End();
    }

        //График изменения сигнала
    {
        static vector<float> lte_rsrp_history;
        static vector<float> lte_rsrq_history;
        static vector<float> gsm_dbm_history;
        static vector<float> nr_ssrsrp_history;
        static vector<float> lte_sinr_history;
        
        PhoneData local_data;
        {
            lock_guard<mutex> lock(g_data_mutex);
            local_data = g_phone_data;
        }
        
        // Сохраняем историю в зависимости от типа сети
        if (local_data.networkType == "LTE") {
            lte_rsrp_history.push_back(local_data.lteSignal.rsrp);
            lte_rsrq_history.push_back(local_data.lteSignal.rsrq);
            lte_sinr_history.push_back(local_data.lteSignal.rssnr);
            
            // Ограничиваем размер истории (последние 100 значений)
            if (lte_rsrp_history.size() > 100) {
                lte_rsrp_history.erase(lte_rsrp_history.begin());
                lte_rsrq_history.erase(lte_rsrq_history.begin());
                lte_sinr_history.erase(lte_sinr_history.begin());
            }
        }
        else if (local_data.networkType == "GSM") {
            gsm_dbm_history.push_back(local_data.gsmSignal.dbm);
            
            if (gsm_dbm_history.size() > 100) {
                gsm_dbm_history.erase(gsm_dbm_history.begin());
            }
        }
        else if (local_data.networkType == "5G_NR") {
            nr_ssrsrp_history.push_back(local_data.nrSignal.ssRsrp);
            
            if (nr_ssrsrp_history.size() > 100) {
                nr_ssrsrp_history.erase(nr_ssrsrp_history.begin());
            }
        }

        // Обновляем историю для каждой соты
        for (const auto& cell : local_data.allCells) {
            if (cell.pci > 0) {
                // RSRP история
                rsrpHistoryByPci[cell.pci].push_back(cell.rsrp);
                if (rsrpHistoryByPci[cell.pci].size() > 100) {
                    rsrpHistoryByPci[cell.pci].erase(rsrpHistoryByPci[cell.pci].begin());
                }
                
                // SINR история (только для LTE/5G)
                if (cell.sinr != 0) {
                    sinrHistoryByPci[cell.pci].push_back(cell.sinr);
                    if (sinrHistoryByPci[cell.pci].size() > 100) {
                        sinrHistoryByPci[cell.pci].erase(sinrHistoryByPci[cell.pci].begin());
                    }
                }
            }
        }

        
        ImGui::Begin("Grafiki signala");
        
        // Графики для LTE
        if (local_data.networkType == "LTE") {
            if (ImPlot::BeginPlot("RSRP History", ImVec2(-1, 250))) {
                ImPlot::SetupAxes("Vremia", "RSRP (dBm)");
                ImPlot::SetupAxisLimits(ImAxis_Y1, -140, -40);
                ImPlot::PlotLine("RSRP", lte_rsrp_history.data(), lte_rsrp_history.size());
                ImPlot::EndPlot();
            }
            
            if (ImPlot::BeginPlot("RSRQ History", ImVec2(-1, 250))) {
                ImPlot::SetupAxes("Vremia", "RSRQ (dB)");
                ImPlot::SetupAxisLimits(ImAxis_Y1, -20, -3);
                ImPlot::PlotLine("RSRQ", lte_rsrq_history.data(), lte_rsrq_history.size());
                ImPlot::EndPlot();
            }

            //График SINR
            if (ImPlot::BeginPlot("SINR History", ImVec2(-1, 200))) {
                ImPlot::SetupAxes("Vremia", "SINR (dB)");
                ImPlot::SetupAxisLimits(ImAxis_Y1, -10, 35);
                ImPlot::PlotLine("SINR", lte_sinr_history.data(), lte_sinr_history.size());
                ImPlot::EndPlot();
            }
        }
        // Графики для GSM
        else if (local_data.networkType == "GSM") {
            if (ImPlot::BeginPlot("Signal Strength History", ImVec2(-1, 300))) {
                ImPlot::SetupAxes("Vremia", "Signal (dBm)");
                ImPlot::SetupAxisLimits(ImAxis_Y1, -120, -30);
                ImPlot::PlotLine("DBM", gsm_dbm_history.data(), gsm_dbm_history.size());
                ImPlot::EndPlot();
            }
        }
        // Графики для 5G NR
        else if (local_data.networkType == "5G_NR") {
            if (ImPlot::BeginPlot("SS-RSRP History", ImVec2(-1, 300))) {
                ImPlot::SetupAxes("Vremia", "SS-RSRP (dBm)");
                ImPlot::SetupAxisLimits(ImAxis_Y1, -140, -40);
                ImPlot::PlotLine("SS-RSRP", nr_ssrsrp_history.data(), nr_ssrsrp_history.size());
                ImPlot::EndPlot();
            }
        }




        // График RSRP для всех PCI
        if (!rsrpHistoryByPci.empty()) {
            if (ImPlot::BeginPlot("RSRP po PCI", ImVec2(-1, 300))) {
                ImPlot::SetupAxes("Vremia", "RSRP (dBm)");
                ImPlot::SetupAxisLimits(ImAxis_Y1, -140, -40);
                
                // Рисуем линию для каждого PCI
                for (auto& [pci, history] : rsrpHistoryByPci) {
                    if (!history.empty()) {
                        string label = "PCI " + to_string(pci);
                        ImPlot::PlotLine(label.c_str(), history.data(), history.size());
                    }
                }
                
                ImPlot::EndPlot();
            }
        } else {
            ImGui::Text("Net dannyh o sotah dlya grafika RSRP");
        }
        
        // График SINR для всех PCI
        bool hasSinrData = false;
        for (const auto& [pci, history] : sinrHistoryByPci) {
            if (!history.empty()) {
                hasSinrData = true;
                break;
            }
        }
        
        if (hasSinrData) {
            if (ImPlot::BeginPlot("SINR po PCI", ImVec2(-1, 300))) {
                ImPlot::SetupAxes("Vremia", "SINR (dB)");
                ImPlot::SetupAxisLimits(ImAxis_Y1, -10, 35);
                
                for (auto& [pci, history] : sinrHistoryByPci) {
                    if (!history.empty()) {
                        string label = "PCI " + to_string(pci);
                        ImPlot::PlotLine(label.c_str(), history.data(), history.size());
                    }
                }
                
                ImPlot::EndPlot();
            }
        }
        
        // Дополнительно показываем текущие значения всех сот
        if (!local_data.allCells.empty()) {
            ImGui::Separator();
            ImGui::Text("Tekushie znachenia po sotam:");
            for (const auto& cell : local_data.allCells) {
                ImGui::Text("  PCI %d: RSRP=%d dBm, RSRQ=%d dB, SINR=%d dB",
                        cell.pci, cell.rsrp, cell.rsrq, cell.sinr);
            }
        }


        // Если нет данных о сети
        else {
            ImGui::Text("Net dannyh o seti dlya postroenia grafika");
        }
        
        ImGui::End();
    }




    // Новое окно с картой OSM
    {
        ImGui::Begin("OpenStreetMap Card", nullptr);
        
        // Получаем размеры окна
        ImVec2 windowPos = ImGui::GetCursorScreenPos();
        ImVec2 windowSize = ImGui::GetContentRegionAvail();
        
        if (windowSize.x > 100 && windowSize.y > 100) {
            
            // Обработка колесика мыши для zoom
            ImGuiIO& io = ImGui::GetIO();
            if (ImGui::IsWindowHovered()) {
                float scroll = io.MouseWheel;
                if (scroll != 0) {
                    mapZoom += (scroll > 0 ? 1 : -1);
                    mapZoom = max(0, min(18, mapZoom));
                }
                
                // Перетаскивание карты
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    if (!dragging) {
                        dragging = true;
                        lastMousePos = ImGui::GetMousePos();
                    } else {
                        ImVec2 delta;
                        delta.x = ImGui::GetMousePos().x - lastMousePos.x;
                        delta.y = ImGui::GetMousePos().y - lastMousePos.y;
                        
                        // Конвертируем пиксельное смещение в градусы
                        double lonPerPixel = 360.0 / (1 << mapZoom) / 256.0;
                        double latPerPixel = 170.0 / (1 << mapZoom) / 256.0;
                        
                        mapLon -= delta.x * lonPerPixel;
                        mapLat += delta.y * latPerPixel;
                        
                        // Ограничиваем широту
                        mapLat = max(-85.0, min(85.0, mapLat));
                        
                        lastMousePos = ImGui::GetMousePos();
                    }
                } else {
                    dragging = false;
                }
            }
            
            // Получаем тайлы для текущей области
            vector<Tile*> visibleTiles = tileManager.getVisibleTiles(
                mapLat, mapLon, mapZoom, 
                (int)windowSize.x, (int)windowSize.y
            );
            
            // Рисуем каждый тайл
            for (Tile* tile : visibleTiles) {
                if (tile && tile->loaded && !tile->rgbaData.empty()) {
                    
                    string tileKey = to_string(tile->z) + "_" + to_string(tile->x) + "_" + to_string(tile->y);
                    
                    // Создаем текстуру если её нет в кэше
                    if (textureCache.find(tileKey) == textureCache.end()) {
                        GLuint texId;
                        glGenTextures(1, &texId);
                        glBindTexture(GL_TEXTURE_2D, texId);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
                                    tile->width, tile->height, 0, GL_RGBA,
                                    GL_UNSIGNED_BYTE, tile->rgbaData.data());
                        
                        textureCache[tileKey] = texId;
                        
                        // Проверка ошибок OpenGL
                        GLenum err = glGetError();
                        if (err != GL_NO_ERROR) {
                            cout << "OpenGL error creating texture: " << err << endl;
                        }
                    }
                    
                    // Вычисляем позицию тайла на экране
                    double centerX = tileManager.lon2x(mapLon, mapZoom);
                    double centerY = tileManager.lat2y(mapLat, mapZoom);
                    
                    double tileX = (double)tile->x;
                    double tileY = (double)tile->y;
                    
                    int offsetX = (int)((tileX - centerX) * 256 + windowSize.x/2);
                    int offsetY = (int)((tileY - centerY) * 256 + windowSize.y/2);
                    
                    // Отображаем тайл
                    ImVec2 pmin = ImVec2(windowPos.x + offsetX, windowPos.y + offsetY);
                    ImVec2 pmax = ImVec2(pmin.x + 256, pmin.y + 256);
                    
                    ImGui::GetWindowDrawList()->AddImage(
                        (void*)(intptr_t)textureCache[tileKey], pmin, pmax
                    );
                }
            }
            
            // Показываем информацию
            ImGui::SetCursorScreenPos(windowPos);
            ImGui::Text("Zoom: %d | Lat: %.6f | Lon: %.6f | Tiles: %zu", 
                        mapZoom, mapLat, mapLon, visibleTiles.size());
        }
        
        ImGui::End();
    }





// 3.3) Отправляем на рендер;
        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    // Очистка текстур
    for (auto& [key, texId] : textureCache) {
        glDeleteTextures(1, &texId);
    }
    textureCache.clear();

// 4) Закрываем приложение безопасно.
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}


int main(int argc, char *argv[]) {
    // Устанавливаем обработчик сигнала
    signal(SIGINT, signalHandler);
    
    // Создаем сервер
    ZmqServer server("*", 7777);  // Порт 7777
    g_server = &server;
    
    // Запускаем сервер в отдельном потоке
    thread server_thread([&server](){
        server.start();
    });
    
    // Даем серверу время на запуск
    this_thread::sleep_for(chrono::seconds(1));
    
    // Запускаем GUI
    run_gui();
    
    // Останавливаем сервер при выходе
    server.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
    
    return 0;
}