#include <string>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include "../include/serialize.h"
#include "../include/http_message.h"
#include "../include/uri.h"

using namespace httpserver;

template <>
std::string ToString<HttpMethod>(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::CONNECT: return "CONNECT";
        case HttpMethod::OPTIONS: return "OPTIONS";
        case HttpMethod::TRACE: return "TRACE";
        case HttpMethod::PATCH: return "PATCH";
        default: return "UNKNOWN";
    }
}

template <>
std::string ToString<HttpVersion>(HttpVersion version) {
    switch (version) {
        case HttpVersion::HTTP_1_0: return "HTTP/1.0";
        case HttpVersion::HTTP_1_1: return "HTTP/1.1";
        case HttpVersion::HTTP_2_0: return "HTTP/2.0";
        default: return "UNKNOWN";
    }
}

template <>
std::string ToString<HttpStatusCode>(HttpStatusCode status_code) {
    switch (status_code) {
        case HttpStatusCode::OK: return "200 OK";
        case HttpStatusCode::Created: return "201 Created";
        case HttpStatusCode::NoContent: return "204 No Content";
        case HttpStatusCode::BadRequest: return "400 Bad Request";
        case HttpStatusCode::Unauthorized: return "401 Unauthorized";
        case HttpStatusCode::Forbidden: return "403 Forbidden";
        case HttpStatusCode::NotFound: return "404 Not Found";
        case HttpStatusCode::MethodNotAllowed: return "405 Method Not Allowed";
        case HttpStatusCode::InternalServerError: return "500 Internal Server Error";
        case HttpStatusCode::NotImplemented: return "501 Not Implemented";
        case HttpStatusCode::BadGateway: return "502 Bad Gateway";
        case HttpStatusCode::ServiceUnavailable: return "503 Service Unavailable";
        case HttpStatusCode::GatewayTimeout: return "504 Gateway Timeout";
        default: return "UNKNOWN";
    }
}

template <>
std::string ToString<HttpRequest>(HttpRequest request) {
    std::ostringstream oss;
    oss << ToString(request.GetMethod()) << ' ';
    oss << request.GetURI().path() << ' ';
    oss << ToString(request.GetVersion()) << "\r\n";
    for (const auto& p : request.GetHeaders())
        oss << p.first << ": " << p.second << "\r\n";
    oss << "\r\n";
    oss << request.GetContent();
    return oss.str();
}

template <>
std::string ToString<HttpResponse>(HttpResponse response) {
    std::ostringstream oss;
    oss << ToString(response.GetVersion()) << " " << ToString(response.GetStatusCode()) << "\r\n";
    for (const auto& header : response.GetHeaders()) {
        oss << header.first << ": " << header.second << "\r\n";
    }
    oss << "\r\n";
    oss << response.GetContent();
    return oss.str();
}

template <>
HttpMethod FromString<HttpMethod>(const std::string& method_string) {
    if (method_string == "GET") return HttpMethod::GET;
    if (method_string == "HEAD") return HttpMethod::HEAD;
    if (method_string == "POST") return HttpMethod::POST;
    if (method_string == "PUT") return HttpMethod::PUT;
    if (method_string == "DELETE") return HttpMethod::DELETE;
    if (method_string == "CONNECT") return HttpMethod::CONNECT;
    if (method_string == "OPTIONS") return HttpMethod::OPTIONS;
    if (method_string == "TRACE") return HttpMethod::TRACE;
    if (method_string == "PATCH") return HttpMethod::PATCH;
    throw std::invalid_argument("Unknown HTTP method: " + method_string);
}

template <>
HttpVersion FromString<HttpVersion>(const std::string& version_string) {
    if (version_string == "HTTP/1.0") return HttpVersion::HTTP_1_0;
    if (version_string == "HTTP/1.1") return HttpVersion::HTTP_1_1;
    if (version_string == "HTTP/2.0") return HttpVersion::HTTP_2_0;
    throw std::invalid_argument("Unknown HTTP version: " + version_string);
}

template <>
HttpRequest FromString<HttpRequest>(const std::string& requestString) {
    HttpRequest request;

    // === Parse Start Line === //
    size_t startLineEnd = requestString.find("\r\n");
    if (startLineEnd == std::string::npos) {
        throw std::invalid_argument("Invalid HTTP request: missing start line");
    }
    std::string startLine = requestString.substr(0, startLineEnd);

    std::string method, path, version;
    std::istringstream startStream(startLine);
    if (!(startStream >> method >> path >> version)) {
        throw std::invalid_argument("Invalid start line format");
    }

    request.SetMethod(FromString<HttpMethod>(method));
    request.SetURI(URI(path));
    if (FromString<HttpVersion>(version) != request.GetVersion()) {
        throw std::logic_error("Unsupported HTTP version");
    }

    // === Parse Headers and Body === //
    size_t headersEnd = requestString.find("\r\n\r\n", startLineEnd + 2);
    std::string headerLines, messageBody;

    if (headersEnd != std::string::npos) {
        headerLines = requestString.substr(startLineEnd + 2, headersEnd - (startLineEnd + 2));
        messageBody = requestString.substr(headersEnd + 4);
    }

    std::istringstream headersStream(headerLines);
    std::string headerLine, headerKey, headerValue;

    while (std::getline(headersStream, headerLine)) {
        if (!headerLine.empty() && headerLine.back() == '\r') {
            headerLine.pop_back();
        }

        size_t colonPos = headerLine.find(':');
        if (colonPos == std::string::npos) continue;

        headerKey = headerLine.substr(0, colonPos);
        headerValue = headerLine.substr(colonPos + 1);

        // Trim whitespace
        headerKey.erase(std::remove_if(headerKey.begin(), headerKey.end(), ::isspace), headerKey.end());
        headerValue.erase(std::remove_if(headerValue.begin(), headerValue.end(), ::isspace), headerValue.end());

        request.SetHeader(headerKey, headerValue);
    }

    request.SetContent(messageBody);
    return request;
}
