/*
    SlateX - 2026
*/
#pragma once
#include <string>

// AppSettings — парсер AppSettings.ini
struct AppSettings {
    std::string BaseUrl;
    std::string ContentFolder;
    int         Fun = 0;

    static AppSettings& Get();
    bool Load(const std::string& path = "AppSettings.ini");
};