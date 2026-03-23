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

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"
#include "implot.h"

using namespace std;
using json = nlohmann::json;

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
    vector<pair<string, long long>> topApps; // название приложения -> байты
    
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

    // void updatePhoneData(const json& data) {
    //     PhoneData new_data;
        
    //     if (data.contains("location")) {
    //         new_data.latitude = data["location"].value("latitude", 55.7558);
    //         new_data.longitude = data["location"].value("longitude", 37.1234);
    //         new_data.altitude = data["location"].value("altitude", 0.0);
            
    //         long timestamp = data["location"].value("timestamp", 0L);
    //         if (timestamp > 0) {
    //             time_t seconds = timestamp / 1000;
    //             char buffer[80];
    //             strftime(buffer, sizeof(buffer), "%H:%M:%S", localtime(&seconds));
    //             new_data.time = string(buffer);
    //         }
    //     }
        
    //     if (data.contains("cellInfo")) {
    //         new_data.network = data["cellInfo"].value("networkType", "LTE");
            
    //         if (data["cellInfo"].contains("signalStrength")) {
    //             const auto& signal = data["cellInfo"]["signalStrength"];
    //             if (new_data.network == "LTE") {
    //                 new_data.signal = signal.value("rsrp", -65);
    //             } else if (new_data.network == "GSM") {
    //                 new_data.signal = signal.value("dbm", -65);
    //             } else if (new_data.network == "5G_NR") {
    //                 new_data.signal = signal.value("ssRsrp", -65);
    //             }
    //         }
    //     }
        
    //     new_data.deviceId = data.value("deviceId", "unknown");
        
    //     lock_guard<mutex> lock(g_data_mutex);
    //     g_phone_data = new_data;
    // }

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

    void start() {
        try {
            string address = "tcp://" + host + ":" + to_string(port);
            socket.bind(address);
            
            log("INFO", "ZMQ сервер запущен на " + address);
            log("INFO", "Ожидание данных от Android...");
            
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

        //График изменения сигнала - поддержка всех типов сетей
    {
        static vector<float> lte_rsrp_history;
        static vector<float> lte_rsrq_history;
        static vector<float> gsm_dbm_history;
        static vector<float> nr_ssrsrp_history;
        
        PhoneData local_data;
        {
            lock_guard<mutex> lock(g_data_mutex);
            local_data = g_phone_data;
        }
        
        // Сохраняем историю в зависимости от типа сети
        if (local_data.networkType == "LTE") {
            lte_rsrp_history.push_back(local_data.lteSignal.rsrp);
            lte_rsrq_history.push_back(local_data.lteSignal.rsrq);
            
            // Ограничиваем размер истории (последние 100 значений)
            if (lte_rsrp_history.size() > 100) {
                lte_rsrp_history.erase(lte_rsrp_history.begin());
                lte_rsrq_history.erase(lte_rsrq_history.begin());
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
        // Если нет данных о сети
        else {
            ImGui::Text("Net dannyh o seti dlya postroenia grafika");
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