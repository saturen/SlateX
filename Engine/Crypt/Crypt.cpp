/*
    SlateX - 2026
    Crypt — верификация ed25519 подписи
*/

#include "Crypt.h"
#include "ed25519.h"
#include <iostream>
#include <sstream>
#include <vector>

const unsigned char Crypt::kPublicKey[32] = {
    0x5c, 0x7e, 0xbb, 0x0e, 0x21, 0xc6, 0x1a, 0xfb,
    0x65, 0xcc, 0xe2, 0x19, 0x50, 0xec, 0x40, 0xee,
    0xe8, 0xc0, 0x9e, 0xc5, 0xe3, 0x12, 0xf9, 0xde,
    0x47, 0x5d, 0x90, 0xce, 0xea, 0xec, 0x6a, 0xbe
};

// =============================================
//  Base64 декодирование
// =============================================
static const std::string kBase64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Crypt::Base64Decode(const std::string& input) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++)
        T[(unsigned char)kBase64Chars[i]] = i;

    int val = 0, bits = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        bits += 6;
        if (bits >= 0) {
            out += (char)((val >> bits) & 0xFF);
            bits -= 8;
        }
    }
    return out;
}

// =============================================
//  VerifySign
//  Формат: --|pslx|sign|<base64sig>|<lua code>
// =============================================
std::pair<bool, std::string> Crypt::VerifySign(const std::string& content) {
    const std::string prefix = "--|pslx|sign|";

    // Проверяем префикс
    if (content.substr(0, prefix.size()) != prefix) {
        std::cerr << "[Crypt] Invalid format: missing prefix\n";
        return {false, ""};
    }

    size_t sigStart = prefix.size();
    size_t sigEnd = content.find('|', sigStart);
    if (sigEnd == std::string::npos) {
        std::cerr << "[Crypt] Invalid format: missing second pipe\n";
        return {false, ""};
    }

    std::string sigBase64 = content.substr(sigStart, sigEnd - sigStart);
    std::string code      = content.substr(sigEnd + 1);

    // Декодируем подпись
    std::string sigRaw = Base64Decode(sigBase64);
    if (sigRaw.size() != 64) {
        std::cerr << "[Crypt] Invalid signature length: "
                  << sigRaw.size() << " (expected 64)\n";
        return {false, ""};
    }

    // Верифицируем ed25519
    int ok = ed25519_verify(
        reinterpret_cast<const unsigned char*>(sigRaw.data()),
        reinterpret_cast<const unsigned char*>(code.data()),
        code.size(),
        kPublicKey
    );

    if (!ok) {
        std::cerr << "[Crypt] Signature verification FAILED\n";
        return {false, ""};
    }

    std::cout << "[Crypt] Signature OK\n";
    return {true, code};
}