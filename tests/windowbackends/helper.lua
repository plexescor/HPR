HPR.extensionName = "WindowBackendHelper"
HPR.authorName = "Plexescor"

function init()

    --Windows
    if string.find(HPR.getOsName_E() or "", "Windows", 1, true) then
        HPR.registerBackend_E(
            "Windows",
            function(env) return true end,
            function() print("Windows backend is active!") end,
            function() return "custom_name" end,
            function() return "custom_title" end,
            function() return "69" end
        )
    
    elseif string.find(HPR.getEnvironmentName_E() or "", "Hyprland", 1, true) then
        HPR.registerBackend_E(
            "Hyprland",
            function(env) return true end,
            function() print("Hyprland backend is active!") end,
            function() return "custom_name" end,
            function() return "custom_title" end,
            function() return "69" end
        )
    
    elseif string.find(HPR.getEnvironmentName_E() or "", "KDE", 1, true) then
        HPR.registerBackend_E(
            "KDE",
            function(env) return true end,
            function() print("KDE backend is active!") end,
            function() return "custom_name" end,
            function() return "custom_title" end,
            function() return "69" end
        )
    
    elseif string.find(HPR.getEnvironmentName_E() or "", "GNOME", 1, true) then
        HPR.registerBackend_E(
            "GNOME",
            function(env) return true end,
            function() print("GNOME backend is active!") end,
            function() return "custom_name" end,
            function() return "custom_title" end,
            function() return "69" end
        )
    
    elseif string.find(HPR.getEnvironmentName_E() or "", "Cinnamon", 1, true) then
        HPR.registerBackend_E(
            "Cinnamon",
            function(env) return true end,
            function() print("Cinnamon backend is active!") end,
            function() return "custom_name" end,
            function() return "custom_title" end,
            function() return "69" end
        )
    --niri manager
    elseif string.find(HPR.getEnvironmentName_E() or "", "niri", 1, true) then
        HPR.registerBackend_E(
            "niri",
            function(env) return true end,
            function() print("niri backend is active!") end,
            function() return "custom_name" end,
            function() return "custom_title" end,
            function() return "69" end
        )
    else
        print("No backend found for this environment.")
    end
    
end

function onTick(delta)

end

function onExit()

end