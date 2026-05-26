#pragma once

#include <string>

class LinuxInitialiser {
public:
  LinuxInitialiser();
  ~LinuxInitialiser();

  // Returns the icon theme root dir registered at startup,
  // or empty string if the icon was not found.
  static const std::string &getIconThemePath();

private:
  std::string iconName = "logo_256png.png";
  std::string filePath;

  static std::string s_iconThemePath;
};