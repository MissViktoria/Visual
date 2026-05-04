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
#include <filesystem>

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"
#include "implot.h"

#include "../include/TileManager.h"
#include "../include/PhoneData.h"

using namespace std;
using json = nlohmann::json;

// Функции преобразования координат
static double PI = 3.14159265358979323846;
static double RAD = PI / 180.0;
static double DEG = 180.0 / PI;

void run_gui(){
    // Создаем менеджер тайлов
    static TileManager tileManager;

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
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            std::cout << "Processing some event: " << event.type << " timestamp: " << event.motion.timestamp << std::endl;
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
                
                // Переменные для управления картой
                static double mapLat = 55.051661;   // твоя широта
                static double mapLon = 82.914767;   // твоя долгота  
                static int mapZoom = 16;
                static bool dragging = false;
                static ImVec2 lastMousePos;
                
                // Обработка ввода мыши
                ImGuiIO& io = ImGui::GetIO();
                if (ImGui::IsWindowHovered()) {
                    // Зум колесиком
                    float scroll = io.MouseWheel;
                    if (scroll != 0) {
                        int newZoom = mapZoom + (scroll > 0 ? 1 : -1);
                        newZoom = std::max(0, std::min(18, newZoom));
                        if (newZoom != mapZoom) {
                            tileManager.clearQueue();  // ОЧИЩАЕМ ОЧЕРЕДЬ ПРИ СМЕНЕ ЗУМА
                            mapZoom = newZoom;
                        }
                    }
                    
                    // Перетаскивание правой кнопкой
                    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.5f)) {
                        if (!dragging) {
                            dragging = true;
                            lastMousePos = ImGui::GetMousePos();
                        } else {
                            ImVec2 delta;
                            delta.x = ImGui::GetMousePos().x - lastMousePos.x;
                            delta.y = ImGui::GetMousePos().y - lastMousePos.y;
                            
                            double lonPerPixel = 360.0 / (1 << mapZoom) / 256.0;
                            double latPerPixel = 170.0 / (1 << mapZoom) / 256.0;
                            
                            mapLon -= delta.x * lonPerPixel;
                            mapLat += delta.y * latPerPixel;
                            mapLat = std::max(-85.0, std::min(85.0, mapLat));
                            
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