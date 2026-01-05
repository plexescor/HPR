#include <iostream>
#include <string>
#include <map>

#include "imgui.h"

#include "activeWindowAndDataManager.hpp"
#include "timeUtils.hpp"

bool showSwitch = false;
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
    ImGui::Text("%s", currentWindow.c_str());

    ImGui::Separator();

    ImGui::Text("No. of Swicthes: ");
    ImGui::SameLine();
    ImGui::Text("%d", switches);
}

void displayWindowSwitches()
{
        if (ImGui::BeginTable("SwitchTable", 2)) // 2 columns
        {
            ImGui::TableSetupColumn("From");
            ImGui::TableSetupColumn("To");
            ImGui::TableHeadersRow();

            std::vector<std::pair<std::string, std::string>>* switchPtr = activeWindowAndDataManagement::getAllSwitchedWindowName();
            if (switchPtr && !switchPtr->empty())
            {
                for (auto const& [fromApp, toApp] : *switchPtr) 
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", fromApp.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", toApp.c_str());
                }
            }
            

            ImGui::EndTable();
        }
}


void renderMainUiElements()
{
    //Current window and switch count displaying
    displayCurrentWindowAndSwitches();

    ImGui::Separator();

    //Past windows and time displaying
    displayTimePerApplication();

    ImGui::Separator();

    //Display Window Switches name
    if (ImGui::Button("Show Window Switches")) showSwitch = !showSwitch;
    if (showSwitch) displayWindowSwitches();    
    
    ImGui::Separator();
}