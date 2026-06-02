#pragma once
#include <string>

std::string runSystemCommand(std::string &command);
std::string runSystemCommand_UNSAFE(std::string &command);
void showNotification(const std::string &title, const std::string &msg);
std::string getCpuUsage();
std::string getRamUsage();
std::string getThreadCount();