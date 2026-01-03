#include <iostream>
#include <string>

#include "imgui.h"

#include "activeWindowAndDataManager.hpp"
#include "timeUtils.hpp"

void displayTimePerApplication()
{
    ImGui::Text("TIME PER APPLICATION:");
    if (ImGui::BeginTable("TimeTable", 2))
    {
        ImGui::TableSetupColumn("Application");
        ImGui::TableSetupColumn("Time (s)");
        ImGui::TableHeadersRow();

        //USE THE POINTER
        std::map<std::string, double>* logPtr = activeWindowAndDataManagement::getTimeLog();

        if (logPtr)
        {
            //LOOP THROUGH EVERY APP IN THE MAP
            for (auto const& [name, seconds] : *logPtr) 
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", name.c_str());
                ImGui::TableNextColumn();

                // Format time in hours, minutes, seconds
                timeUtils::timeFormat time = timeUtils::formatSeconds(seconds);

                ImGui::Text("%dh", time.hours);
                ImGui::SameLine();
                ImGui::Text("%dm", time.minutes);
                ImGui::SameLine();
                ImGui::Text("%ds", time.seconds);
            }
        }

        
        ImGui::EndTable();
    }
}

void displayCurrentWindowAndSwitches()
{
    std::string currentWindow = activeWindowAndDataManagement::getCurrentWindowName();
    int switches = activeWindowAndDataManagement::getCurrentSwitchCount();

    ImGui::Text("CURRENT WINDOW: ");
    ImGui::SameLine();
    ImGui::Text(currentWindow.c_str());

    ImGui::Separator();

    ImGui::Text("No. of Swicthes: ");
    ImGui::SameLine();
    ImGui::Text("%d", switches);
}

void renderMainUiElements()
{
    //Current window and switch count displaying
    displayCurrentWindowAndSwitches();

    ImGui::Separator();

    //Past windows and time displaying
    displayTimePerApplication();
    
    ImGui::Separator();
}