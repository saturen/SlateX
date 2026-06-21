#pragma once
#include <string>
#include <utility>

// =============================================
//  SlateX - 2026
//  Crypt — верификация подписи джойнскрипта
//
//  Формат входной строки:
//    --|pslx|sign|<base64 подпись>|<lua код>
//
//  VerifySign(content):
//    -> {true,  lua_code}  если подпись верна
//    -> {false, ""}        если подпись неверна или формат неправильный
// =============================================

#ifdef _WIN32
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API
#endif

class ENGINE_API Crypt {
public:
    // Парсит и верифицирует подписанный скрипт
    static std::pair<bool, std::string> VerifySign(const std::string& content);

private:
    // ed25519 публичный ключ (32 байта)
    static const unsigned char kPublicKey[32];

    // base64 декодирование
    static std::string Base64Decode(const std::string& input);
};