/*
    SlateX - 2026
*/
#pragma once
#include <string>
#include <cstdint>

// JoinScriptLoader
//
//   SXS:    -h <путь>  -p <порт>   -> LoadHost(Port, Endpoint)
//   Client: -j <путь>  -p <порт>   -> LoadJoin(Port, Endpoint)
//
// <путь> — путь эндпоинта относительно BaseUrl (по умолчанию
// /GameApi/Generic/Host и /GameApi/Generic/Join соответственно).
// Полный URL всегда BaseUrl + Endpoint, BaseUrl фиксирован и
// проверяется в ValidateBaseUrl() — поменять домен через CLI нельзя,
// только через AppSettings.ini (и он должен совпасть с kHardcodedBaseUrl).
//
// Алгоритм:
//   1. ValidateBaseUrl() — AppSettings.BaseUrl должен == kHardcodedBaseUrl
//   2. HTTP GET BaseUrl + Endpoint
//   3. Crypt::VerifySign
//   4. __JoinPort__ / __JoinAddress__ выставляются в Lua (Address — это
//      хост из BaseUrl, без схемы, см. JoinScriptLoader.cpp)
//   5. KakaScheduler::SpawnCode на Layer::Join
//
// Само решение host'ить или коннектиться принимает скрипт
// (HostServer/ConnectServer доступны только Layer::Join — см. LuaSandbox).

#ifdef _WIN32
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API
#endif

class ENGINE_API JoinScriptLoader {
public:
    static bool LoadHost(uint16_t Port, const std::string& Endpoint = kHostEndpoint);
    static bool LoadJoin(uint16_t Port, const std::string& Endpoint = kJoinEndpoint);

    static const char* kHardcodedBaseUrl;
    static const char* kHostEndpoint;
    static const char* kJoinEndpoint;

private:
    static bool LoadFromEndpoint(const std::string& Endpoint, const std::string& LogTag, uint16_t Port);
    static void ValidateBaseUrl();
    static std::string StripScheme(const std::string& Url);
};