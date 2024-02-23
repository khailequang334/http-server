#pragma once
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <chrono>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <thread>
#include <utility>

#include "http_message.h"
#include "uri.h"

namespace httpserver {
    constexpr size_t kMaxBufferSize = 4096;

    struct EventData {
        int fd;
        size_t length;
        size_t cursor;
        char buffer[kMaxBufferSize];
        EventData() : fd(0), length(0), cursor(0), buffer() {}
    };

    using HttpRequestHandler_t = std::function<HttpResponse(const HttpRequest&)>;

    class HttpServer {
    public:
        explicit HttpServer(const std::string& host, std::uint16_t port);
        HttpServer(HttpServer&&) noexcept;
        HttpServer& operator=(HttpServer&&) noexcept;
        HttpServer() = default;
        ~HttpServer() = default;

        void Start();
        void Stop();
        void RegisterHttpRequestHandler(const std::string& path, HttpMethod method,
                                        const HttpRequestHandler_t callback);
        void RegisterHttpRequestHandler(const URI& uri, HttpMethod method,
                                        const HttpRequestHandler_t callback);

        std::string GetHost() const;
        std::uint16_t GetPort() const;
        bool IsRunning() const;

    private:
        static constexpr int kBacklogSize = 1000;
        static constexpr int kMaxConnections = 10000;
        static constexpr int kMaxEvents = 10000;
        static constexpr int kThreadPoolSize = 5;

        std::string _host;
        std::uint16_t _port;
        int _socketFd;
        bool _running;
        std::thread _listenerThread;
        std::thread _workerThreads[kThreadPoolSize];
        int _workerEpollFd[kThreadPoolSize];
        epoll_event _workerEvents[kThreadPoolSize][kMaxEvents];
        std::map<URI, std::map<HttpMethod, HttpRequestHandler_t>> _requestHandlers;
        std::mt19937 _rng;
        std::uniform_int_distribution<int> _sleepTimes;

        void CreateSocket();
        void SetUpEpoll();
        void Listen();
        void ProcessEvents(int workerId);
        void HandleEpollEvent(int epollFd, EventData* event, std::uint32_t events);
        void HandleHttpData(const EventData& request, EventData* response);
        HttpResponse HandleHttpRequest(const HttpRequest& request);

        void ControlEpollEvent(int epollFd, int op, int fd,
                            std::uint32_t events = 0, void* data = nullptr);
    };

}