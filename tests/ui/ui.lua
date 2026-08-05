HPR.extensionName = "UI"
HPR.authorName = "Plexescor"

function init()
    HPR.sleep(2000)

    HPR.showUiPopup("Can you see this?", "No", "Yes", function(btn)
        if btn == 1 then
            HPR.log("User clicked Yes!")
            HPR.writeCsv(HPR.getExtensionDir_E() .. "output/ui.csv", "ShowUiPopup", "PASSED")
        else
            HPR.log("User clicked No!")
            HPR.writeCsv(HPR.getExtensionDir_E() .. "output/ui.csv", "ShowUiPopup", "PASSED")
        end
end)
end

function onTick(delta)

end

function onExit()

end