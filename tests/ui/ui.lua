HPR.extensionName = "UI"
HPR.authorName = "Plexescor"

function init()
    HPR.sleep(2000)

    HPR.showUiPopup("Can you see this?", "No", "Yes", function(btn)
        if btn == 1 then
            HPR.log( "UI","User clicked Yes!")
            HPR.writeCsv(HPR.getExtensionDir() .. "output/ui.csv", "ShowUIPopup", "PASSED")
        elseif btn == 0 then
            HPR.log("UI", "User clicked No!")
            HPR.writeCsv(HPR.getExtensionDir() .. "output/ui.csv", "ShowUIPopup", "PASSED")
        else
            HPR.log("UI", "Timeout reached, no button clicked.")
            HPR.writeCsv(HPR.getExtensionDir() .. "output/ui.csv", "ShowUIPopup", "FAILED")
        end
    end)
end

function onTick(delta)

end

function onExit()

end