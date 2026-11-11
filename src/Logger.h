// Logger.h - Writes the auto-generated log file (and optionally echoes it).
#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <iostream>
#include <string>

class Logger {
private:
    std::ofstream file;
    bool echo;
    std::string path;

public:
    Logger(const std::string& path, bool echoToConsole);

    bool isOpen() const { return file.is_open(); }
    const std::string& getPath() const { return path; }

    // One line in the log (newline added).
    void log(const std::string& line);

    // Direct access for multi-line blocks (stats, tables).
    std::ostream& stream() { return file; }
};

#endif
