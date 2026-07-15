#pragma once

#include <string>
#include <filesystem>

class LinuxInitialiser {
public:
  LinuxInitialiser();
  ~LinuxInitialiser();

  // Returns the icon theme root dir registered at startup,
  // or empty string if the icon was not found.
  static const std::string &getIconThemePath();

private:
  std::string iconName = "logo_256png.png";
  std::filesystem::path filePath;

  static std::string s_iconThemePath;
};