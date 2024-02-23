#pragma once
#include <map>
#include <string>
#include <utility>

#include "uri.h"
#include "serialize.h"

namespace httpserver {
    enum class HttpMethod {
        GET,
        HEAD,
        POST,
        PUT,
        DELETE,
        CONNECT,
        OPTIONS,
        TRACE,
        PATCH
    };

    enum class HttpVersion {
        HTTP_1_0 = 10,
        HTTP_1_1 = 11,
        HTTP_2_0 = 20
    };

    enum class HttpStatusCode {
        OK = 200,
        Created = 201,
        NoContent = 204,
        BadRequest = 400,
        Unauthorized = 401,
        Forbidden = 403,
        NotFound = 404,
        MethodNotAllowed = 405,
        InternalServerError = 500,
        NotImplemented = 501,
        BadGateway = 502,
        ServiceUnavailable = 503,
        GatewayTimeout = 504,
        HttpVersionNotSupported = 505
    };

    class HttpMessage {
    public:
        HttpMessage();
        virtual ~HttpMessage() = default;

        void SetHeader(const std::string& key, const std::string& value);
        void RemoveHeader(const std::string& key);
        void ClearHeader();
        void SetContent(const std::string& content);
        void ClearContent();

        HttpVersion GetVersion() const;
        std::string GetHeader(const std::string& key) const;
        const std::map<std::string, std::string>& GetHeaders() const;
        const std::string& GetContent() const;
        size_t GetContentLength() const;

    protected:
        HttpVersion _version;
        std::map<std::string, std::string> _headers;
        std::string _content;
    };

    class HttpRequest : public HttpMessage {
    public:
        HttpRequest();
        ~HttpRequest() = default;

        void SetMethod(HttpMethod method);
        void SetURI(const URI& uri);

        HttpMethod GetMethod() const;
        const URI& GetURI() const;

    private:
        HttpMethod _method;
        URI _uri;
    };

    class HttpResponse : public HttpMessage {
    public:
        HttpResponse();
        HttpResponse(HttpStatusCode statusCode);
        ~HttpResponse() = default;

        void SetStatusCode(HttpStatusCode statusCode);

        HttpStatusCode GetStatusCode() const;

    private:
        HttpStatusCode _statusCode;
    };

}
