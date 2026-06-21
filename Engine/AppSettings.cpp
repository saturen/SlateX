/*
    SlateX - 2026
*/
#include "AppSettings.h"
#include <fstream>
#include <sstream>
#include <iostream>

AppSettings& AppSettings::Get() {
    static AppSettings instance;
    return instance;
}

bool AppSettings::Load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[AppSettings] Failed to open: " << path << "\n";
        return false;
    }

    std::string line;
    std::string section;

    while (std::getline(file, line)) {
        // trim leading whitespace
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
            line.erase(line.begin());
        // trim trailing whitespace
        while (!line.empty() && (line.back() == ' ' || line.back() == '\r' || line.back() == '\n'))
            line.pop_back();

        // skip blank lines and comments
        if (line.empty() || line[0] == ';') continue;

        // section header
        if (line[0] == '[') {
            section = line.substr(1, line.find(']') - 1);
            continue;
        }

        // key = value ;; comment
        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key   = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        while (!key.empty() && key.back() == ' ') key.pop_back();
        while (!value.empty() && value.front() == ' ') value.erase(value.begin());

        auto commentPos = value.find(";;");
        if (commentPos != std::string::npos)
            value = value.substr(0, commentPos);
        while (!value.empty() && value.back() == ' ') value.pop_back();

        if (section == "Generic") {
            if (key == "BaseUrl")       BaseUrl       = value;
            if (key == "ContentFolder") ContentFolder = value;
        } else if (section == "FUN") {
            if (key == "fun") Fun = std::stoi(value);
        }
    }

    std::cout << "[AppSettings] Loaded\n";
    std::cout << "[AppSettings] BaseUrl: "       << BaseUrl       << "\n";
    std::cout << "[AppSettings] ContentFolder: " << ContentFolder << "\n";
    return true;
}