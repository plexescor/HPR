#include <iostream>
#include <SDL3/SDL.h>

#include "GUIElements.hpp"
#include "imguiStyling.hpp" //Some custom functions for styling and font etc
#include "InterFont.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

struct GuiState{
    SDL_Window* window;
    SDL_Renderer* renderer;
    float uiScale = 1.3f;
    bool pendingFontRebuild = false; // Flag to rebuild fonts outside the frame block
} vars;


void initializeImGuiNewFrameAndViewPort()
{
    // ImGui new frame
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // -------- FULLSCREEN IMGUI WINDOW SETUP --------
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

}

ImGuiWindowFlags setImGuiFlags()
{
    ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;

    return flags;
}

void renderUiScalingButtons()
{
    if (ImGui::Button("Increase UI Size"))
    {
        vars.uiScale += 0.1f;
        vars.pendingFontRebuild = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Decrease UI SIZE"))
    {
        vars.uiScale -= 0.1f;
        if (vars.uiScale < 0.4f) vars.uiScale = 0.4f; // clamp
        vars.pendingFontRebuild = true; //IDK, GPT
    }
}

void renderFinalFrame()
{
    // Render
    ImGui::Render();

    // Dark-ish background
    SDL_SetRenderDrawColor(vars.renderer, 50, 50, 50, 255);
    SDL_RenderClear(vars.renderer);

    ImGui_ImplSDLRenderer3_RenderDrawData(
        ImGui::GetDrawData(),
        vars.renderer
    );

    SDL_RenderPresent(vars.renderer);
}

void initGUI()
{
    // Init SDL
    SDL_Init(SDL_INIT_VIDEO);

    // Init SDL Window (RESIZABLE)
    vars.window = SDL_CreateWindow(
        "HPR",
        1280, 720,
        SDL_WINDOW_RESIZABLE
    );

    // Init SDL Renderer
    vars.renderer = SDL_CreateRenderer(vars.window, nullptr);

    // Vsync
    SDL_SetRenderVSync(vars.renderer, 1);

    // Init ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    //APPLY CUSTOM STYLE and load custom font
    setUpImGuiStyle();
    ImGui::GetStyle().ScaleAllSizes(vars.uiScale);
    loadFonts(1.0f);

    // Init ImGui backends
    ImGui_ImplSDL3_InitForSDLRenderer(vars.window, vars.renderer);
    ImGui_ImplSDLRenderer3_Init(vars.renderer);
}

bool pollEvent(bool running)
{
    running = true;
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        ImGui_ImplSDL3_ProcessEvent(&e);
        if (e.type == SDL_EVENT_QUIT)
        {
            running = false;
            return running;
        }
    }
    return running;
}

void rebuildFontAndStyle()
{
    loadFonts(vars.uiScale);
            
    // The backend will update the texture 
    // automatically on the next NewFrame().
    ImGui::GetIO().Fonts->Build(); 
            
    // Re-apply style scaling
    ImGui::GetStyle() = ImGuiStyle(); 
    setUpImGuiStyle();
    ImGui::GetStyle().ScaleAllSizes(vars.uiScale);
            
    vars.pendingFontRebuild = false; //Accessing global var
}

void runGUI()
{
    bool running = true;

    while (running)
    {
        running = pollEvent(running); //Poll and return the result whether to continue

        // --- Safe Font/Style Rebuilding ---
        // We do this before NewFrame so the atlas isn't in use by the GPU
        if (vars.pendingFontRebuild) rebuildFontAndStyle();

        initializeImGuiNewFrameAndViewPort(); //Initialise new Imgui fram and view port

        // ImGuiWindowFlags flags = setImGuiFlags(); //Get the Essential flags

        ImGui::Begin("HPR", nullptr, setImGuiFlags());

        // -----------------------------------------------------------------------------------------------
        //
        //         TO BE RENDERED, ITS THE MAIN UI AREA
        //
        // -----------------------------------------------------------------------------------------------
        
        renderMainUiElements(); //Modularity is whats this called (resides in another cpp)
        renderUiScalingButtons(); //For user (testing nows)

        // ----------------------------------------------------------------------------------------------------
        //
        //              MAIN UI AREA END
        //
        //-------------------------------------------------------------------------------------------------
        
        ImGui::End();
        renderFinalFrame();
    }
}

void quitGUI()
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(vars.renderer);
    SDL_DestroyWindow(vars.window);
    SDL_Quit();
    std::cout << "Done!\n";
}