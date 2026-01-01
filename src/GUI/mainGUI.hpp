#pragma once

#include <SDL3/SDL.h>
#include "imgui.h"

// --- Global State (Declared as extern to avoid multiple definition errors) ---
extern SDL_Window* window;
extern SDL_Renderer* renderer;

// --- Core Lifecycle Functions ---
void initGUI();
void runGUI();
void quitGUI();

// --- Helper & Internal Functions ---
void initializeImGuiNewFrameAndViewPort();
ImGuiWindowFlags setImGuiFlags();
void renderUiScalingButtons();
void renderFinalFrame();
bool pollEvent(bool running);
void rebuildFontAndStyle();