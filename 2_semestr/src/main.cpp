#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <fstream>
#include <nlohmann/json.hpp>
#include "../include/ZmqServer.h"

// Forward declaration для GUI
void run_gui();

// Глобальный указатель на сервер для сигнала
ZmqServer* g_server = nullptr;

void signalHandler(int signum) {
    std::cout << "\nВСЕ СОХРАНЕННЫЕ ДАННЫЕ:" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    try {
        std::ifstream infile("android_data.json");
        nlohmann::json data;
        infile >> data;
        std::cout << "Всего записей: " << data.size() << std::endl;
        
        int count = 0;
        for (auto it = data.rbegin(); it != data.rend() && count < 5; ++it, ++count) {
            std::cout << "\n[" << (*it)["timestamp"] << "] Устройство: " 
                     << (*it)["data"].value("deviceId", "unknown") << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Ошибка чтения файла: " << e.what() << std::endl;
    }
    
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    // Устанавливаем обработчик сигнала
    signal(SIGINT, signalHandler);
    
    // Создаем сервер
    ZmqServer server("*", 7777);
    g_server = &server;
    
    // Запускаем сервер в отдельном потоке
    std::thread server_thread([&server](){
        server.start();
    });
    
    // Даем серверу время на запуск
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Запускаем GUI
    run_gui();
    
    // Останавливаем сервер при выходе
    server.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
    
    return 0;
}