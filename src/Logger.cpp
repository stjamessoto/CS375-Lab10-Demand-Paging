// Logger.cpp
#include "Logger.h"

Logger::Logger(const std::string& p, bool echoToConsole) : file(p), echo(echoToConsole), path(p) {
    if (!file) std::cerr << "Warning: cannot write log file '" << p << "'\n";
}

void Logger::log(const std::string& line) {
    if (file) file << line << '\n';
    if (echo) std::cout << line << '\n';
}
