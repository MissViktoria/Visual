#define STB_IMAGE_IMPLEMENTATION
#include "../include/TileManager.h"
#include <curl/curl.h>
#include <stb/stb_image.h>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <cmath>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <mutex>
#include <unistd.h> 
#include <thread>
#include <chrono>

#define PI 3.14159265358979323846
#define RAD (PI / 180.0)
#define DEG (180.0 / PI)

using namespace std;

// Callback для curl
size_t TileManager::writeCallback(void* data, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    auto& blob = *static_cast<vector<std::byte>*>(userp);
    auto* dataptr = static_cast<std::byte*>(data);
    blob.insert(blob.end(), dataptr, dataptr + realsize);
    return realsize;
}

// Формулы из методички
double TileManager::lon2x(double lon, int z) {
    return (lon + 180.0) / 360.0 * (1 << z);
}

double TileManager::lat2y(double lat, int z) {
    double latRad = lat * PI / 180.0;
    return (1.0 - asinh(tan(latRad)) / PI) / 2.0 * (1 << z);
}

TileManager::TileManager() {
    cacheDir = "tile_cache";
    // Создаем директорию для кэша
    mkdir(cacheDir.c_str(), 0777);
    
    // Инициализируем curl глобально
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Запускаем фоновый поток для загрузки тайлов
    workerRunning = true;
    workerThread = std::thread(&TileManager::workerFunction, this);
}

TileManager::~TileManager() {
    // Останавливаем фоновый поток
    workerRunning = false;
    if (workerThread.joinable()) {
        workerThread.join();
    }
    
    // Очищаем curl
    curl_global_cleanup();
}

string TileManager::getTilePath(int z, int x, int y) {
    string dir = cacheDir + "/" + to_string(z) + "/" + to_string(x);
    mkdir(dir.c_str(), 0777); // создаем вложенные директории
    return dir + "/" + to_string(y) + ".png";
}

string TileManager::getTileUrl(int z, int x, int y) {
    // Используем другой сервер для лучшей стабильности
    return "https://tile.openstreetmap.org/" + to_string(z) + "/" + to_string(x) + "/" + to_string(y) + ".png";
}

bool TileManager::loadFromFile(Tile& tile) {
    string path = getTilePath(tile.z, tile.x, tile.y);
    ifstream file(path, ios::binary);
    if (!file.is_open()) return false;
    
    // Читаем файл
    file.seekg(0, ios::end);
    size_t size = file.tellg();
    file.seekg(0, ios::beg);
    
    tile.rawData.resize(size);
    file.read(reinterpret_cast<char*>(tile.rawData.data()), size);
    file.close();
    
    // Преобразуем PNG в RGBA
    stbi_set_flip_vertically_on_load(false);
    int width, height, channels;
    auto* ptr = stbi_load_from_memory(
        reinterpret_cast<stbi_uc const*>(tile.rawData.data()),
        tile.rawData.size(), &width, &height, &channels, STBI_rgb_alpha
    );
    
    if (ptr) {
        tile.width = width;
        tile.height = height;
        tile.channels = STBI_rgb_alpha;
        size_t nbytes = width * height * STBI_rgb_alpha;
        tile.rgbaData.resize(nbytes);
        auto* byteptr = reinterpret_cast<std::byte*>(ptr);
        tile.rgbaData.assign(byteptr, byteptr + nbytes);
        stbi_image_free(ptr);
        tile.loaded = true;
        return true;
    }
    
    return false;
}

bool TileManager::saveToFile(const Tile& tile) {
    string path = getTilePath(tile.z, tile.x, tile.y);
    ofstream file(path, ios::binary);
    if (!file.is_open()) return false;
    
    file.write(reinterpret_cast<const char*>(tile.rawData.data()), tile.rawData.size());
    file.close();
    return true;
}

bool TileManager::downloadTile(Tile& tile) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    string url = getTileUrl(tile.z, tile.x, tile.y);
    cout << "Downloading: " << url << endl;
    
    // Очищаем rawData перед загрузкой
    tile.rawData.clear();
    
    // Настройки curl
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &tile.rawData);
    
    // User-Agent (необходим для OSM)
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Для простоты, в проде нужно включить
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    
    // Добавляем заголовки
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: image/webp,image/apng,image/*,*/*;q=0.8");
    headers = curl_slist_append(headers, "Accept-Language: ru-RU,ru;q=0.9,en;q=0.8");
    headers = curl_slist_append(headers, "Referer: https://www.openstreetmap.org/");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Задержка между запросами (уважаем сервер OSM)
    static std::chrono::steady_clock::time_point lastRequest = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRequest).count();
    if (elapsed < 200) { // Минимум 200 мс между запросами
        std::this_thread::sleep_for(std::chrono::milliseconds(200 - elapsed));
    }
    lastRequest = std::chrono::steady_clock::now();
    
    // Выполняем запрос
    CURLcode res = curl_easy_perform(curl);
    bool ok = (res == CURLE_OK);
    
    if (!ok) {
        cerr << "CURL error: " << curl_easy_strerror(res) << " for URL: " << url << endl;
    }
    
    // Очищаем заголовки и curl
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (!ok || tile.rawData.empty()) return false;
    
    // Конвертируем PNG в RGBA
    stbi_set_flip_vertically_on_load(false);
    int width, height, channels;
    auto* ptr = stbi_load_from_memory(
        reinterpret_cast<stbi_uc const*>(tile.rawData.data()),
        tile.rawData.size(), &width, &height, &channels, STBI_rgb_alpha
    );
    
    if (ptr) {
        tile.width = width;
        tile.height = height;
        tile.channels = STBI_rgb_alpha;
        size_t nbytes = width * height * STBI_rgb_alpha;
        tile.rgbaData.resize(nbytes);
        auto* byteptr = reinterpret_cast<std::byte*>(ptr);
        tile.rgbaData.assign(byteptr, byteptr + nbytes);
        stbi_image_free(ptr);
        tile.loaded = true;
        
        // Сохраняем в файл для кэша
        saveToFile(tile);
        return true;
    }
    
    cerr << "Failed to decode PNG for tile: " << url << endl;
    return false;
}

void TileManager::workerFunction() {
    cout << "TileManager worker thread started" << endl;
    
    while (workerRunning) {
        TileJob job;
        bool hasJob = false;
        
        {
            lock_guard<mutex> lock(jobMutex);
            if (!jobQueue.empty()) {
                job = jobQueue.front();
                jobQueue.pop();
                hasJob = true;
            }
        }
        
        if (hasJob) {
            processTileJob(job);
        } else {
            // Нет задач - спим немного
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
    
    cout << "TileManager worker thread stopped" << endl;
}

void TileManager::processTileJob(const TileJob& job) {
    Tile tile;
    tile.z = job.z;
    tile.x = job.x;
    tile.y = job.y;
    tile.loaded = false;
    
    string key = to_string(job.z) + "/" + to_string(job.x) + "/" + to_string(job.y);
    
    // Сначала пробуем загрузить из файла (дисковый кэш)
    if (loadFromFile(tile)) {
        lock_guard<mutex> lock(cacheMutex);
        cache[key] = move(tile);
        
        // Убираем из pending
        auto it = pendingTiles.find(key);
        if (it != pendingTiles.end()) {
            it->second.isLoading = false;
            it->second.isQueued = false;
        }
        return;
    }
    
    // Иначе скачиваем из сети
    if (downloadTile(tile)) {
        lock_guard<mutex> lock(cacheMutex);
        cache[key] = move(tile);
        
        // Убираем из pending
        auto it = pendingTiles.find(key);
        if (it != pendingTiles.end()) {
            it->second.isLoading = false;
            it->second.isQueued = false;
        }
    } else {
        // Загрузка не удалась, убираем из очереди ожидания
        lock_guard<mutex> lock(cacheMutex);
        auto it = pendingTiles.find(key);
        if (it != pendingTiles.end()) {
            it->second.isLoading = false;
            it->second.isQueued = false;
        }
    }
}

Tile* TileManager::getTile(int z, int x, int y) {
    string key = to_string(z) + "/" + to_string(x) + "/" + to_string(y);
    
    // Проверяем уже загруженные тайлы в кэше
    {
        lock_guard<mutex> lock(cacheMutex);
        auto it = cache.find(key);
        if (it != cache.end() && it->second.loaded) {
            return &it->second;
        }
    }
    
    // Проверяем, не в процессе ли уже загрузки или в очереди
    {
        lock_guard<mutex> lock(cacheMutex);
        auto pendingIt = pendingTiles.find(key);
        if (pendingIt != pendingTiles.end()) {
            if (pendingIt->second.isLoading || pendingIt->second.isQueued) {
                return nullptr; // Уже загружается или в очереди
            }
        }
        
        // Отмечаем как ожидающий загрузки
        pendingTiles[key].isQueued = true;
        pendingTiles[key].isLoading = false;
    }
    
    // Добавляем задачу в очередь на загрузку
    {
        lock_guard<mutex> lock(jobMutex);
        jobQueue.push({z, x, y});
    }
    
    return nullptr; // Пока не загружен
}

vector<Tile*> TileManager::getVisibleTiles(double lat, double lon, int zoom, int screenWidth, int screenHeight) {
    vector<Tile*> tiles;
    
    // Конвертируем центр в тайл-координаты
    double centerX = lon2x(lon, zoom);
    double centerY = lat2y(lat, zoom);
    
    // Сколько тайлов помещается на экране
    int tilesWide = ceil(screenWidth / 256.0) + 2;
    int tilesHigh = ceil(screenHeight / 256.0) + 2;
    
    int centerTileX = (int)centerX;
    int centerTileY = (int)centerY;
    
    int maxTile = (1 << zoom) - 1;
    
    // Загружаем тайлы вокруг центра
    for (int dy = -tilesHigh/2; dy <= tilesHigh/2; dy++) {
        for (int dx = -tilesWide/2; dx <= tilesWide/2; dx++) {
            int x = centerTileX + dx;
            int y = centerTileY + dy;
            
            if (x >= 0 && x <= maxTile && y >= 0 && y <= maxTile) {
                Tile* tile = getTile(zoom, x, y); // Неблокирующий вызов
                if (tile && tile->loaded) {
                    tiles.push_back(tile);
                }
            }
        }
    }
    
    return tiles;
}

void TileManager::clearCache() {
    lock_guard<mutex> lock(cacheMutex);
    
    // Очищаем кэш в памяти
    cache.clear();
    
    // Также можно очистить pending
    pendingTiles.clear();
    
    // Очищаем очередь задач
    {
        lock_guard<mutex> lock(jobMutex);
        while (!jobQueue.empty()) {
            jobQueue.pop();
        }
    }
}

void TileManager::clearQueue() {
    lock_guard<mutex> lock(jobMutex);
    while (!jobQueue.empty()) {
        jobQueue.pop();
    }
    // Также очищаем pending-статусы
    lock_guard<mutex> lock2(cacheMutex);
    pendingTiles.clear();
}

double TileManager::mercatorXToTileX(double mercatorX, int zoom) {
    return (0.5 + mercatorX / 360.0) * (1 << zoom);
}

double TileManager::mercatorYToTileY(double mercatorY, int zoom) {
    return (0.5 - mercatorY / 360.0) * (1 << zoom);
}

double TileManager::tileXToMercatorX(int tileX, int zoom) {
    return (tileX / static_cast<double>(1 << zoom) - 0.5) * 360.0;
}

double TileManager::tileYToMercatorY(int tileY, int zoom) {
    return (0.5 - tileY / static_cast<double>(1 << zoom)) * 360.0;
}