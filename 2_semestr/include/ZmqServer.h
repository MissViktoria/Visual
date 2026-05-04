#ifndef ZMQ_SERVER_H
#define ZMQ_SERVER_H

#include <zmq.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <atomic>
#include <libpq-fe.h>
#include "PhoneData.h"

using namespace std;
using json = nlohmann::json;

// Forward declarations функций из database.cpp
PGconn* initDatabase();
bool saveToDatabase(PGconn* db_conn, const PhoneData& data);

class ZmqServer {
private:
    string host;
    int port;
    zmq::context_t context;
    zmq::socket_t socket;
    string data_file;
    int counter;
    
    PGconn* db_conn = nullptr;

    void log(const string& level, const string& message);
    string getCurrentTime();
    string getCurrentISO8601();
    void saveData(const json& data);
    void printData(const json& data);

public:
    atomic<bool> running{true};
    
    ZmqServer(string h = "*", int p = 7777);
    void start();
    void stop();
};

#endif // ZMQ_SERVER_H