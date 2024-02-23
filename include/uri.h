#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace httpserver {
    class URI {
    public:
        URI() = default;
        explicit URI(const std::string& path) { 
            SetPath(path); 
        }

        ~URI() = default;

        bool operator==(const URI& other) const { 
            return _path == other._path; 
        }
        
        bool operator<(const URI& other) const { 
            return _path < other._path; 
            }

        void SetPath(const std::string& path) {
            _path = path;
            // to lowercase
            std::transform(_path.begin(), _path.end(), _path.begin(),
                           [](unsigned char c) { return std::tolower(c); });
        }

        std::string path() const { return _path; }

    private:
        std::string _path;
    };
}