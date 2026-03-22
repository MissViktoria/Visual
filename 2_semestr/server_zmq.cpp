#include <zmq.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <thread>
#include <signal.h>

using namespace std;
using json = nlohmann::json;

class ZmqServer {
private:
    string host;
    int port;
    zmq::context_t context;
    zmq::socket_t socket;
    string data_file;
    int counter;

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

    void start() {
        string address = "tcp://" + host + ":" + to_string(port);
        socket.bind(address);
        
        log("INFO", "ZMQ сервер запущен на " + address);
        log("INFO", "Ожидание данных от Android...");
        
        cout << "\n" << string(80, '=') << endl;
        cout << "СЕРВЕР ДАННЫХ МЕСТОПОЛОЖЕНИЯ (PUSH/PULL режим)" << endl;
        cout << "Порт: " << port << endl;
        cout << "Статус: Ожидание данных..." << endl;
        cout << string(80, '=') << "\n" << endl;

        while (true) {
            try {
                zmq::message_t message;
                auto received = socket.recv(message, zmq::recv_flags::none);
                
                if (!received) {
                    continue;
                }

                string msg_str(static_cast<char*>(message.data()), message.size());
                
                log("INFO", "[" + getCurrentTime() + "] Получены данные №" + to_string(counter + 1));

                auto data = json::parse(msg_str);

                counter++;
                saveData(data);
                printData(data);

            } catch (const json::parse_error& e) {
                log("ERROR", string("Ошибка декодирования JSON: ") + e.what());
            } catch (const exception& e) {
                log("ERROR", string("Ошибка: ") + e.what());
            }
        }
    }

    void saveData(const json& data) {
        try {
            // Читаем существующие данные
            ifstream infile(data_file);
            json all_data;
            infile >> all_data;
            infile.close();

            // Создаем новую запись
            json data_entry = {
                {"timestamp", getCurrentISO8601()},
                {"data", data},
                {"counter", counter}
            };

            all_data.push_back(data_entry);

            // Сохраняем обратно в файл
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

            string network_type = cell_info.value("networkType", "UNKNOWN");
            cout << "   Тип сети: " << network_type << endl;

            auto cell_identity = cell_info.value("cellIdentity", json::object());
            auto signal_strength = cell_info.value("signalStrength", json::object());

            if (network_type == "LTE") {
                cout << "   Cell ID: " << cell_identity.value("cellIdentity", 0) << endl;
                cout << "   MCC: " << cell_identity.value("mcc", 0) 
                     << ", MNC: " << cell_identity.value("mnc", 0) << endl;
                cout << "   Сигнал: RSRP: " << signal_strength.value("rsrp", 0) << " dBm" << endl;
            }
            else if (network_type == "GSM") {
                cout << "   Cell ID: " << cell_identity.value("cellIdentity", 0) << endl;
                cout << "   MCC: " << cell_identity.value("mcc", 0) 
                     << ", MNC: " << cell_identity.value("mnc", 0) << endl;
                cout << "   Сигнал: DBM: " << signal_strength.value("dbm", 0) << " dBm" << endl;
            }
            else if (network_type == "5G_NR") {
                cout << "   NCI: " << cell_identity.value("nci", 0) << endl;
                cout << "   MCC: " << cell_identity.value("mcc", 0) 
                     << ", MNC: " << cell_identity.value("mnc", 0) << endl;
                cout << "   Сигнал: SS-RSRP: " << signal_strength.value("ssRsrp", 0) << " dBm" << endl;
            }

            cout << "СТАТИСТИКА: Всего сообщений: " << counter << endl;
            cout << string(80, '=') << "\n" << endl;

        } catch (const exception& e) {
            log("ERROR", string("Ошибка при выводе данных: ") + e.what());
        }
    }

    void close() {
        socket.close();
    }
};

bool running = true;

void signalHandler(int signum) {
    running = false;
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
}

int main() {
    signal(SIGINT, signalHandler);
    
    ZmqServer server;
    
    try {
        server.start();
    } catch (const zmq::error_t& e) {
        if (running) {
            cerr << "ZMQ Error: " << e.what() << endl;
        }
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
    
    server.close();
    return 0;
}



















#include <GL/glew.h>
#include <SDL2/SDL.h>

#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>


#include <cstdlib>
#include <mutex> 
#include <fstream>  


#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"
#include "implot.h"


using namespace std;

void runzmqServer() {
    // int result = system("start /B server.exe");
    int result = system("./server &");
    if (result != 0) {
        cerr << "Не удалось запустить C++ сервер!" << endl;
    }
}
// Данные с телефона
struct PhoneData {
    double latitude = 55.7558;
    double longitude = 37.1234;
    string network = "LTE";
    int signal = -65;
    string time = "N/A";
};
PhoneData g_phone_data;
mutex g_data_mutex;
// Функция чтения данных (будет работать в фоне)
void readPhoneData() {
    while (true) {
        #ifdef _WIN32
        ifstream file("android_data.json");
        #else
        ifstream file("/tmp/phone_data.txt");
        #endif

        if (file.is_open()) {
            PhoneData new_data;
            string line;
            
            while (getline(file, line)) {
                size_t pos = line.find('=');
                if (pos != string::npos) {
                    string key = line.substr(0, pos);
                    string value = line.substr(pos + 1);
                    
                    if (key == "latitude") new_data.latitude = stod(value);
                    else if (key == "longitude") new_data.longitude = stod(value);
                    else if (key == "network") new_data.network = value;
                    else if (key == "signal") new_data.signal = stoi(value);
                    else if (key == "time") new_data.time = value;
                }
            }
            file.close();
            
            lock_guard<mutex> lock(g_data_mutex);
            g_phone_data = new_data;
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }
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
        {
            PhoneData local_data;
            {
                lock_guard<mutex> lock(g_data_mutex);
                local_data = g_phone_data;
            }

            ImGui::Begin("Dannue s televona");
            
            ImGui::Text("Bremi: %s", local_data.time.c_str());
            ImGui::Separator();
            ImGui::Text("Koordinaty: %.4f, %.4f", 
                       local_data.latitude, local_data.longitude);
            ImGui::Text("Ceti: %s", local_data.network.c_str());
            ImGui::Text("Cignal: %d dBm", local_data.signal);
            
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

    // Запускаем zmq сервер
    thread zmq_thread(runzmqServer);
    //работал независимо
    zmq_thread.detach();
    
    // Даем серверу время на запуск
    this_thread::sleep_for(chrono::seconds(1));
    
    // Запускаем поток для чтения данных
    thread reader_thread(readPhoneData);
    reader_thread.detach();
    
    // Запускаем GUI
    run_gui();

    return 0;
}