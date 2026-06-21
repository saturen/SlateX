/*
    SlateX - 2026
    HttpUtil — реализация на libcurl
*/

#include "HttpUtil.h"
#include <curl/curl.h>
#include <iostream>

HttpUtil& HttpUtil::Get() {
    static HttpUtil instance;
    return instance;
}

// libcurl write callback — пишет данные в std::string
static size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* str = reinterpret_cast<std::string*>(userdata);
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

void HttpUtil::Init() {
    if (m_initialized) return;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    m_initialized = true;
    std::cout << "[HttpUtil] Initialized (libcurl " << curl_version() << ")\n";
}

void HttpUtil::Shutdown() {
    if (!m_initialized) return;
    curl_global_cleanup();
    m_initialized = false;
    std::cout << "[HttpUtil] Shutdown\n";
}

HttpResponse HttpUtil::DoRequest(const std::string& url,
                                  const std::string& method,
                                  const std::string& body,
                                  const std::string& contentType) {
    HttpResponse resp;

    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.error = "curl_easy_init failed";
        return resp;
    }

    // URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // Таймаут
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_timeout);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, m_timeout);

    // Пишем ответ в строку
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);

    // User-Agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "SlateX/1.0");

    // SSL — проверяем сертификат
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    // Заголовки
    struct curl_slist* headers = nullptr;
    if (!contentType.empty()) {
        std::string ct = "Content-Type: " + contentType;
        headers = curl_slist_append(headers, ct.c_str());
    }

    // Метод
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    } else if (method != "GET") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    }

    if (headers)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Выполняем
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        resp.error  = curl_easy_strerror(res);
        resp.status = 0;
    } else {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        resp.status = static_cast<int>(httpCode);
    }

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return resp;
}

HttpResponse HttpUtil::Get(const std::string& url) {
    return DoRequest(url, "GET", "", "");
}

HttpResponse HttpUtil::Post(const std::string& url,
                             const std::string& body,
                             const std::string& contentType) {
    return DoRequest(url, "POST", body, contentType);
}

std::future<HttpResponse> HttpUtil::GetAsync(const std::string& url) {
    return std::async(std::launch::async, [this, url]() {
        return Get(url);
    });
}

std::future<HttpResponse> HttpUtil::PostAsync(const std::string& url,
                                               const std::string& body,
                                               const std::string& contentType) {
    return std::async(std::launch::async, [this, url, body, contentType]() {
        return Post(url, body, contentType);
    });
}