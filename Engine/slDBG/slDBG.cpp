/*
    SlateX - 2026
*/
#include "slDBG.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <sys/stat.h>

// --- slDBG ---

const std::string slDBG::SessionId = slDBG::GenerateSessionId();

std::string slDBG::GenerateSessionId() {
    const char Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    srand(static_cast<unsigned>(time(nullptr)));
    std::string Id;
    for (int i = 0; i < 4; i++)
        Id += Chars[rand() % (sizeof(Chars) - 1)];
    return Id;
}

LogFile* slDBG::NewLog(const std::string& FilePath, const std::string& Category) {
    return new LogFile(FilePath, Category);
}

// --- LogFile ---

LogFile::LogFile(const std::string& FilePath, const std::string& Category)
    : m_category(Category) {
    // inject session id before filename
    // e.g. "Logs/PhysicsService.log" -> "Logs/Q78R_PhysicsService.log"
    size_t Slash = FilePath.find_last_of("/\\");
    std::string Dir      = (Slash != std::string::npos) ? FilePath.substr(0, Slash + 1) : "";
    std::string FileName = (Slash != std::string::npos) ? FilePath.substr(Slash + 1) : FilePath;
    std::string FullPath = Dir + slDBG::SessionId + "_" + FileName;

    // create dir if it sucks and doesnt exist
    if (!Dir.empty()) {
#ifdef _WIN32
        _mkdir(Dir.c_str());
#else
        mkdir(Dir.c_str(), 0755);
#endif
    }

    m_file.open(FullPath, std::ios::out | std::ios::trunc);

    std::string Header = "[Starting " + Category + " logging] " + Timestamp();
    m_file << Header << "\n";
    m_file.flush();
    std::cout << Header << "\n";
}

LogFile::~LogFile() {
    if (m_file.is_open()) {
        std::string Footer = "[" + m_category + " logging stopped] " + Timestamp();
        m_file << Footer << "\n";
        m_file.close();
    }
}

void LogFile::WriteLine(slLevel Level, const std::string& Message, const std::string& Detail) {
    std::string LevelStr;
    switch (Level) {
        case slLevel::Info: LevelStr = "Info"; break;
        case slLevel::Warn: LevelStr = "Warn"; break;
        case slLevel::Err:  LevelStr = "Error"; break;
    }

    std::string Line = LevelStr + ": " + Timestamp() + ": " + Message;
    if (!Detail.empty())
        Line += ": " + Detail;

    std::lock_guard<std::mutex> Lock(m_lock);
    m_file << Line << "\n";
    m_file.flush();
}

void LogFile::Info(const std::string& Message, const std::string& Detail) {
    WriteLine(slLevel::Info, Message, Detail);
}

void LogFile::Warn(const std::string& Message, const std::string& Detail) {
    WriteLine(slLevel::Warn, Message, Detail);
}

void LogFile::Err(const std::string& Message, const std::string& Detail) {
    WriteLine(slLevel::Err, Message, Detail);
}

std::string LogFile::Timestamp() {
    auto Now = std::chrono::system_clock::now();
    auto T   = std::chrono::system_clock::to_time_t(Now);
    auto Ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
        Now.time_since_epoch()) % 1000;

    std::tm Tm{};
#ifdef _WIN32
    localtime_s(&Tm, &T);
#else
    localtime_r(&T, &Tm);
#endif

    std::ostringstream Ss;
    Ss << std::put_time(&Tm, "%H:%M:%S")
       << "." << std::setfill('0') << std::setw(3) << Ms.count();
    return Ss.str();
}