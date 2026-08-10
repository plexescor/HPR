-- =============================================================================
-- Test Suite: UI
-- =============================================================================
-- Verifies HPR's UI popup extension API:
--
--   HPR.showUiPopup(text, leftBtnText, rightBtnText, callback)
--                             Shows a modal popup dialog. Callback receives:
--                               0  = left button clicked
--                               1  = right button clicked
--                              -1  = auto-dismissed after 7500ms timeout
--
-- Expected output (written to output/ui.csv):
--   ShowUIPopup,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "ui" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "UI"
HPR.authorName = "Plexescor"
HPR.versionSupport = { "0.9.7", "0.9.8" }

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