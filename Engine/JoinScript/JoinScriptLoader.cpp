/*
    SlateX - 2026
*/
#include "JoinScriptLoader.h"
#include "../HttpUtil/HttpUtil.h"
#include "../Crypt/Crypt.h"
#include "../Scripting/KakaScheduler.hpp"
#include "../AppSettings.h"
#include <iostream>
#include <cstdlib>

// =============================================
//  defaults
// =============================================
const char* JoinScriptLoader::kHardcodedBaseUrl = "https://sltworx.su";
const char* JoinScriptLoader::kHostEndpoint     = "/GameApi/Generic/Host";
const char* JoinScriptLoader::kJoinEndpoint     = "/GameApi/Generic/Join";

void JoinScriptLoader::ValidateBaseUrl() {
    const auto& cfgBase = AppSettings::Get().BaseUrl;
    if (cfgBase != kHardcodedBaseUrl) {
        std::exit(0);
    }
}

// trims "https://"/"http://" required __JoinAddress__
// just host, nothing special
std::string JoinScriptLoader::StripScheme(const std::string& Url) {
    auto pos = Url.find("://");
    return (pos == std::string::npos) ? Url : Url.substr(pos + 3);
}

bool JoinScriptLoader::LoadFromEndpoint(const std::string& Endpoint, const std::string& LogTag, uint16_t Port) {
    ValidateBaseUrl();

    const std::string url     = std::string(kHardcodedBaseUrl) + Endpoint;
    const std::string address = StripScheme(kHardcodedBaseUrl);

    std::cout << "[" << LogTag << "] Fetching: " << url << "\n";

    auto resp = HttpUtil::Get().Get(url);
    if (!resp.ok()) {
        std::cerr << "[" << LogTag << "] HTTP failed (status " << resp.status
                  << "): " << resp.error << "\n";
        return false;
    }
    std::cout << "[" << LogTag << "] Got response, " << resp.body.size() << " bytes\n";

    auto [valid, code] = Crypt::VerifySign(resp.body);
    if (!valid) {
        std::exit(0);
    }
    std::cout << "[" << LogTag << "] Signature OK, " << code.size() << " bytes of code\n";

    KakaScheduler::Get().GetLua()["__JoinPort__"]    = Port;
    KakaScheduler::Get().GetLua()["__JoinAddress__"] = address;

    bool spawned = KakaScheduler::Get().SpawnCode(code, 0.0, LogTag, ScriptLayer::Join);
    if (!spawned) {
        std::cerr << "[" << LogTag << "] Failed to spawn (syntax error?)\n";
        return false;
    }

    std::cout << "[" << LogTag << "] Running on layer 0, tasks now: "
              << KakaScheduler::Get().TaskCount() << "\n";
    return true;
}

bool JoinScriptLoader::LoadHost(uint16_t Port, const std::string& Endpoint) {
    return LoadFromEndpoint(Endpoint, "HostScript", Port);
}

bool JoinScriptLoader::LoadJoin(uint16_t Port, const std::string& Endpoint) {
    return LoadFromEndpoint(Endpoint, "JoinScript", Port);
}