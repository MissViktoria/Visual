#include "../include/ZmqServer.h"

ZmqServer::ZmqServer(string h, int p) 
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

void ZmqServer::start() {
    try {
        string address = "tcp://" + host + ":" + to_string(port);
        socket.bind(address);
        
        log("INFO", "ZMQ сервер запущен на " + address);
        log("INFO", "Ожидание данных от Android...");
        
        db_conn = initDatabase();

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
                    if (saveToDatabase(db_conn, current_data)) {
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

void ZmqServer::stop() {
    running = false;
    socket.close();

    if (db_conn) {
        PQfinish(db_conn);
        db_conn = nullptr;
        log("INFO", "Соединение с PostgreSQL закрыто");
    }
}

void ZmqServer::log(const string& level, const string& message) {
    auto now = chrono::system_clock::now();
    auto time_t = chrono::system_clock::to_time_t(now);
    cout << "[" << put_time(localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
         << "] " << level << " - " << message << endl;
}

string ZmqServer::getCurrentTime() {
    auto now = chrono::system_clock::now();
    auto time_t = chrono::system_clock::to_time_t(now);
    stringstream ss;
    ss << put_time(localtime(&time_t), "%H:%M:%S");
    return ss.str();
}

string ZmqServer::getCurrentISO8601() {
    auto now = chrono::system_clock::now();
    auto time_t = chrono::system_clock::to_time_t(now);
    stringstream ss;
    ss << put_time(localtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

void ZmqServer::saveData(const json& data) {
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

void ZmqServer::printData(const json& data) {
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