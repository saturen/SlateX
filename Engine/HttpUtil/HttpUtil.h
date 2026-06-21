/*
    SlateX - 2026
*/
#pragma once
#include <string>
#include <functional>
#include <future>

// HttpUtil — HTTP клиент на libcurl
//
//   Get(url)            — синхронный GET
//   Post(url, body)     — синхронный POST
//   GetAsync(url, cb)   — асинхронный GET
//   PostAsync(url, ...) — асинхронный POST

#ifdef _WIN32
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API
#endif

struct ENGINE_API HttpResponse {
    int         status = 0;     // HTTP код (200, 404, etc)
    std::string body;           // тело ответа
    std::string error;          // ошибка curl если не удалось
    bool        ok() const { return status >= 200 && status < 300; }
};

using HttpCallback = std::function<void(HttpResponse)>;

class ENGINE_API HttpUtil {
public:
    static HttpUtil& Get();

    // вызывается один раз при старте
    void Init();
    void Shutdown();

    // --- синхронные ---
    HttpResponse Get(const std::string& url);
    HttpResponse Post(const std::string& url, const std::string& body,
                      const std::string& contentType = "application/json");

    // --- асинхронные (коллбэк вызывается из другого потока) ---
    std::future<HttpResponse> GetAsync(const std::string& url);
    std::future<HttpResponse> PostAsync(const std::string& url,
                                         const std::string& body,
                                         const std::string& contentType = "application/json");

    void SetTimeout(long seconds) { m_timeout = seconds; }

private:
    HttpUtil() = default;

    HttpResponse DoRequest(const std::string& url,
                           const std::string& method,
                           const std::string& body,
                           const std::string& contentType);

    long m_timeout     = 10;
    bool m_initialized = false;
};