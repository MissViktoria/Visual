#ifndef TILE_MANAGER_H
#define TILE_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstddef>

// Структура для хранения одного тайла (как в методичке)
struct Tile {
    int z, x, y;                    // координаты тайла
    std::vector<std::byte> rawData; // PNG байты (как в методичке)
    std::vector<std::byte> rgbaData; // RGBa пиксели
    int width = 256;                 // ширина (256 пикселей)
    int height = 256;                // высота
    int channels = 4;                // RGBA = 4 канала
    bool loaded = false;             // загружен или нет
};

class TileManager {
private:
    std::string cacheDir;
    std::map<std::string, Tile> cache;
    std::mutex cacheMutex;
    
    std::string getTilePath(int z, int x, int y);
    std::string getTileUrl(int z, int x, int y);
    
    bool loadFromFile(Tile& tile);
    bool saveToFile(const Tile& tile);
    bool downloadTile(Tile& tile);
    
    static size_t writeCallback(void* data, size_t size, size_t nmemb, void* userp);
    
public:
    TileManager();
    ~TileManager();
    
    // Публичные методы преобразования координат
    double lon2x(double lon, int z);
    double lat2y(double lat, int z);
    
    Tile* getTile(int z, int x, int y);
    std::vector<Tile*> getVisibleTiles(double lat, double lon, int zoom, int screenWidth, int screenHeight);
    void clearCache();
};

#endif