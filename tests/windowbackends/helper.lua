HPR.extensionName = "WindowBackendHelper"
HPR.authorName = "Plexescor"
HPR.versionSupport = { "0.9.7", "0.9.8" }

function init()

    --Windows
    if string.find(HPR.getOsName() or "", "Windows", 1, true) then
        HPR.registerBackend(
            "Windows",
            function(env) return true end,
            function() print("Windows backend is active!") end,
            function() return "custom_name" end,
            function() return "custom_title" end,
            function() return "69" end
        )
    
    elseif string.find(HPR.getEnvironmentName() or "", "Hyprland", 1, true) then
        HPR.registerBackend(
            "Hyprland",
            function(env) return true end,
            function() print("Hyprland backend is active!") end,
            function() return "custom_name" end,
            function() return "custom_title" end,
            function() return "69" end
        )
    
    elseif string.find(HPR.getEnvironmentName() or "", "KDE", 1, true) then
        HPR.registerBackend(
            "KDE",
            function(env) return true end,
            function() print("KDE backend is active!") end,
            function() return "custom_name" end,
            function() return "custom_title" end,
            function() return "69" end
        )
    
    elseif string.find(HPR.getEnvironmentName() or "", "GNOME", 1, true) then
        HPR.registerBackend(
            "GNOME",
            function(env) return true end,
            function() print("GNOME backend is active!") end,
            function() return "custom_name" end,
            function() return "custom_title" end,
            function() return "69" end
        )
    
    elseif string.find(HPR.getEnvironmentName() or "", "Cinnamon", 1, true) then
        HPR.registerBackend(
            "Cinnamon",
            function(env) return true end,
            function() print("Cinnamon backend is active!") end,
            function() return "custom_name" end,
            function() return "custom_title" end,
            function() return "69" end
        )
    --niri manager
    elseif string.find(HPR.getEnvironmentName() or "", "niri", 1, true) then
        HPR.registerBackend(
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