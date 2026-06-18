/*
    SlateX - 2026
*/
#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <ctime>

// log levels, pretty self explanatory
enum class slLevel {
    Info,
    Warn,
    Err
};

// one log file per category, prefixed with session id
// e.g. Q78R_PhysicsService.log
class LogFile {
public:
    LogFile(const std::string& FilePath, const std::string& Category);
    ~LogFile();

    void WriteLine(slLevel Level, const std::string& Message, const std::string& Detail = "");
    void Info(const std::string& Message, const std::string& Detail = "");
    void Warn(const std::string& Message, const std::string& Detail = "");
    void Err(const std::string& Message, const std::string& Detail = "");

private:
    std::string   m_category;
    std::ofstream m_file;
    std::mutex    m_lock;

    // returns current time as HH:MM:SS.mmm string
    static std::string Timestamp();
};

// static logger factory, session id generated once per process
class slDBG {
public:
    // 4-char session id, same for all logs in this process
    // e.g. "Q78R"
    static const std::string SessionId;

    // create a new log file for given category
    // path is prefixed with session id automatically
    static LogFile* NewLog(const std::string& FilePath, const std::string& Category);

private:
    static std::string GenerateSessionId();
};