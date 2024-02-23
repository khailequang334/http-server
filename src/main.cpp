#include <sys/resource.h>
#include <sys/time.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "../include/http_message.h"
#include "../include/http_server.h"
#include "../include/uri.h"

using httpserver::HttpMethod;
using httpserver::HttpRequest;
using httpserver::HttpResponse;
using httpserver::HttpServer;
using httpserver::HttpStatusCode;

int main(void) {
  std::string host = "0.0.0.0";
  int port = 8080;
  HttpServer server(host, port);

  // Register a few endpoints for demo and benchmarking
  auto say_hello = [](const HttpRequest& request) -> HttpResponse {
    HttpResponse response(HttpStatusCode::OK);
    response.SetHeader("Content-Type", "text/plain");
    response.SetContent("Hello, world\n");
    return response;
  };
  auto send_html = [](const HttpRequest& request) -> HttpResponse {
    HttpResponse response(HttpStatusCode::OK);
    std::string content;
    content += "<!doctype html>\n";
    content += "<html>\n<body>\n\n";
    content += "<h1>Hello, world in an Html page</h1>\n";
    content += "<p>A Paragraph</p>\n\n";
    content += "</body>\n</html>\n";

    response.SetHeader("Content-Type", "text/html");
    response.SetContent(content);
    return response;
  };

  server.RegisterHttpRequestHandler("/", HttpMethod::HEAD, say_hello);
  server.RegisterHttpRequestHandler("/", HttpMethod::GET, say_hello);
  server.RegisterHttpRequestHandler("/hello.html", HttpMethod::HEAD, send_html);
  server.RegisterHttpRequestHandler("/hello.html", HttpMethod::GET, send_html);

  try {
    std::cout << "Starting the web server.." << std::endl;
    server.Start();
    std::cout << "Server listening on " << host << ":" << port << std::endl;

    std::cout << "Enter [q] to stop the server" << std::endl;
    std::string command;
    while (std::cin >> command, command != "q") {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "'quit' command entered. Stopping the web server.."
              << std::endl;
    server.Stop();
    std::cout << "Server stopped" << std::endl;
  } catch (std::exception& e) {
    std::cerr << "An error occurred: " << e.what() << std::endl;
    return -1;
  }

  return 0;
}
