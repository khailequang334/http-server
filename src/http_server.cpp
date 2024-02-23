#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#include "../include/http_server.h"
#include "../include/uri.h"

namespace httpserver {

    HttpServer::HttpServer(const std::string &host, std::uint16_t port)
        : _host(host),
          _port(port),
          _socketFd(0),
          _running(false),
          _workerEpollFd(),
          _rng(std::chrono::steady_clock::now().time_since_epoch().count()),
          _sleepTimes(10, 100) {
        CreateSocket();
    }

    void HttpServer::Start() {
        int opt = 1;
        sockaddr_in serverAddress;

        if (setsockopt(_socketFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                       sizeof(opt)) < 0) {
            throw std::runtime_error("Failed to set socket options");
        }

        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = INADDR_ANY;
        inet_pton(AF_INET, _host.c_str(), &(serverAddress.sin_addr.s_addr));
        serverAddress.sin_port = htons(_port);

        if (bind(_socketFd, (sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
            throw std::runtime_error("Failed to bind to socket");
        }

        if (listen(_socketFd, kBacklogSize) < 0) {
            std::ostringstream msg;
            msg << "Failed to listen on port " << _port;
            throw std::runtime_error(msg.str());
        }

        SetUpEpoll();
        _running = true;
        _listenerThread = std::thread(&HttpServer::Listen, this);
        for (int i = 0; i < kThreadPoolSize; i++) {
            _workerThreads[i] = std::thread(&HttpServer::ProcessEvents, this, i);
        }
    }

    void HttpServer::Stop() {
        _running = false;
        _listenerThread.join();
        for (int i = 0; i < kThreadPoolSize; i++) {
            _workerThreads[i].join();
        }
        for (int i = 0; i < kThreadPoolSize; i++) {
            close(_workerEpollFd[i]);
        }
        close(_socketFd);
    }

    void HttpServer::CreateSocket() {
        if ((_socketFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
            throw std::runtime_error("Failed to create a TCP socket");
        }
    }

    void HttpServer::SetUpEpoll() {
        for (int i = 0; i < kThreadPoolSize; i++) {
            if ((_workerEpollFd[i] = epoll_create1(0)) < 0) {
                throw std::runtime_error(
                    "Failed to create epoll file descriptor for worker");
            }
        }
    }

    void HttpServer::Listen() {
        EventData *clientData;
        sockaddr_in clientAddress;
        socklen_t clientLen = sizeof(clientAddress);
        int clientFd;
        int currentWorker = 0;
        bool active = true;

        while (_running) {
            if (!active) {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(_sleepTimes(_rng)));
            }
            clientFd = accept4(_socketFd, (sockaddr *)&clientAddress, &clientLen,
                               SOCK_NONBLOCK);
            if (clientFd < 0) {
                active = false;
                continue;
            }

            active = true;
            clientData = new EventData();
            clientData->fd = clientFd;
            ControlEpollEvent(_workerEpollFd[currentWorker], EPOLL_CTL_ADD,
                              clientFd, EPOLLIN, clientData);
            currentWorker++;
            if (currentWorker == HttpServer::kThreadPoolSize) currentWorker = 0;
        }
    }

    void HttpServer::ProcessEvents(int workerId) {
        EventData *data;
        int epollFd = _workerEpollFd[workerId];
        bool active = true;

        while (_running) {
            if (!active) {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(_sleepTimes(_rng)));
            }
            int nfds = epoll_wait(_workerEpollFd[workerId],
                                  _workerEvents[workerId], HttpServer::kMaxEvents, 0);
            if (nfds <= 0) {
                active = false;
                continue;
            }

            active = true;
            for (int i = 0; i < nfds; i++) {
                const epoll_event &currentEvent = _workerEvents[workerId][i];
                data = reinterpret_cast<EventData *>(currentEvent.data.ptr);
                if ((currentEvent.events & EPOLLHUP) ||
                    (currentEvent.events & EPOLLERR)) {
                    ControlEpollEvent(epollFd, EPOLL_CTL_DEL, data->fd);
                    close(data->fd);
                    delete data;
                } else if ((currentEvent.events == EPOLLIN) ||
                           (currentEvent.events == EPOLLOUT)) {
                    HandleEpollEvent(epollFd, data, currentEvent.events);
                } else {
                    ControlEpollEvent(epollFd, EPOLL_CTL_DEL, data->fd);
                    close(data->fd);
                    delete data;
                }
            }
        }
    }

    void HttpServer::HandleEpollEvent(int epollFd, EventData *data,
                                      std::uint32_t events) {
        int fd = data->fd;
        EventData *request, *response;

        if (events == EPOLLIN) {
            request = data;
            ssize_t byteCount = recv(fd, request->buffer, kMaxBufferSize, 0);
            if (byteCount > 0) {
                response = new EventData();
                response->fd = fd;
                HandleHttpData(*request, response);
                ControlEpollEvent(epollFd, EPOLL_CTL_MOD, fd, EPOLLOUT, response);
                delete request;
            } else if (byteCount == 0) {
                ControlEpollEvent(epollFd, EPOLL_CTL_DEL, fd);
                close(fd);
                delete request;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    request->fd = fd;
                    ControlEpollEvent(epollFd, EPOLL_CTL_MOD, fd, EPOLLIN, request);
                } else {
                    ControlEpollEvent(epollFd, EPOLL_CTL_DEL, fd);
                    close(fd);
                    delete request;
                }
            }
        } else {
            response = data;
            ssize_t byteCount =
                send(fd, response->buffer + response->cursor, response->length, 0);
            if (byteCount >= 0) {
                if (byteCount < response->length) {
                    response->cursor += byteCount;
                    response->length -= byteCount;
                    ControlEpollEvent(epollFd, EPOLL_CTL_MOD, fd, EPOLLOUT, response);
                } else {
                    request = new EventData();
                    request->fd = fd;
                    ControlEpollEvent(epollFd, EPOLL_CTL_MOD, fd, EPOLLIN, request);
                    delete response;
                }
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    ControlEpollEvent(epollFd, EPOLL_CTL_ADD, fd, EPOLLOUT, response);
                } else {
                    ControlEpollEvent(epollFd, EPOLL_CTL_DEL, fd);
                    close(fd);
                    delete response;
                }
            }
        }
    }

    void HttpServer::HandleHttpData(const EventData &rawRequest,
                                    EventData *rawResponse) {
        std::string requestString(rawRequest.buffer), responseString;
        HttpRequest httpRequest;
        HttpResponse httpResponse;

        try {
            httpRequest = FromString<HttpRequest>(requestString);
            httpResponse = HandleHttpRequest(httpRequest);
        } catch (const std::invalid_argument &e) {
            httpResponse = HttpResponse(HttpStatusCode::BadRequest);
            httpResponse.SetContent(e.what());
        } catch (const std::logic_error &e) {
            httpResponse = HttpResponse(HttpStatusCode::HttpVersionNotSupported);
            httpResponse.SetContent(e.what());
        } catch (const std::exception &e) {
            httpResponse = HttpResponse(HttpStatusCode::InternalServerError);
            httpResponse.SetContent(e.what());
        }

        responseString =
            ToString(httpResponse);
        memcpy(rawResponse->buffer, responseString.c_str(), kMaxBufferSize);
        rawResponse->length = responseString.length();
    }

    HttpResponse HttpServer::HandleHttpRequest(const HttpRequest &request) {
        auto it = _requestHandlers.find(request.GetURI());
        if (it == _requestHandlers.end()) {
            return HttpResponse(HttpStatusCode::NotFound);
        }
        auto callbackIt = it->second.find(request.GetMethod());
        if (callbackIt == it->second.end()) {
            return HttpResponse(HttpStatusCode::MethodNotAllowed);
        }
        return callbackIt->second(request);
    }

    void HttpServer::ControlEpollEvent(int epollFd, int op, int fd,
                                       std::uint32_t events, void *data) {
        if (op == EPOLL_CTL_DEL) {
            if (epoll_ctl(epollFd, op, fd, nullptr) < 0) {
                throw std::runtime_error("Failed to remove file descriptor");
            }
        } else {
            epoll_event ev;
            ev.events = events;
            ev.data.ptr = data;
            if (epoll_ctl(epollFd, op, fd, &ev) < 0) {
                throw std::runtime_error("Failed to add file descriptor");
            }
        }
    }
    void HttpServer::RegisterHttpRequestHandler(const std::string& path, HttpMethod method,
                                    const HttpRequestHandler_t callback) {
        URI uri(path);
        uri.SetPath(path);
        _requestHandlers[uri].insert(std::make_pair(method, std::move(callback)));
    }
    void HttpServer::RegisterHttpRequestHandler(const URI& uri, HttpMethod method,
                                    const HttpRequestHandler_t callback) {
        URI formattedUri = uri;
        _requestHandlers[formattedUri].insert(std::make_pair(method, std::move(callback)));
    }

    
    std::string HttpServer::GetHost() const { 
        return _host; 
    }

    std::uint16_t HttpServer::GetPort() const { 
        return _port; 
    }

    bool HttpServer::IsRunning() const { 
        return _running; 
    }
}