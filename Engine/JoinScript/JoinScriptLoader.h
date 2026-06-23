/*
    SlateX - 2026
*/
#pragma once
#include <string>
#include <cstdint>

// JoinScriptLoader
//
//   Server:    -h <endpoint>  -p <port>   -> LoadHost(Port, Endpoint)
//   Client: -j <endpoint>  -p <port>   -> LoadJoin(Port, Endpoint)
//
// <endpoint> — endpoint path relative to base url (
// defaults:
// /GameApi/Generic/Host 
// /GameApi/Generic/Join
// Resolves URL as: BaseUrl + Endpoint

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