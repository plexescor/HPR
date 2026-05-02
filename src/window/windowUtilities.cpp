#include <cstdio> //For piping
#include <iostream>
#include <string>

#include "windowUtilities.hpp"

std::string runSystemCommand(std::string &command) {
#ifdef __linux__

    FILE *pipe = popen(command.c_str(), "r"); // Get only read access

    if (!pipe) {
      std::cerr << "Opening the pipe failed!\n";
      return "";
    }

    std::string output = "";
    char buffer[256]; // 255 normal + 1 nullterm

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      output += buffer;
    }

    int exitCode = pclose(pipe);

    if (exitCode != 0) {
      std::cerr << "Running the command failed! Exit Code: " << exitCode;
    }

    // std::cout << "Output; " << output << std::endl;
    return output;

#endif
    return "";
}