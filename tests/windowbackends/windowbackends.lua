-- =============================================================================
-- Test Suite: Window Backends
-- =============================================================================
-- Verifies custom window backend registration and window queries:
--
--   HPR.getCurrentWindow_E()   Returns active window process name.
--   HPR.getCurrentTitle_E()    Returns active window title bar text.
--   HPR.registerBackend_E(...) Registers a custom Lua window backend.
--
-- Expected output (written to output/windowbackends.csv):
--   GetCurrentWindow,PASSED
--   GetCurrentTitle,PASSED
--   RegisterBackend,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "windowbackends" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "WindowBackend"
HPR.authorName = "Plexescor"

function init()
    HPR.sleep_E(1000) -- wait till our friend starts up
end --1second tick

function onTick(delta)
    local name = HPR.getCurrentWindow_E()
    local title = HPR.getCurrentTitle_E()

    if (name ~= nil and title ~= nil) then
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/windowbackends.csv", "GetCurrentWindow", "PASSED")
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/windowbackends.csv", "GetCurrentTitle", "PASSED")

        if (name == "custom_name" and title == "custom_title") then
            HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/windowbackends.csv", "RegisterBackend", "PASSED")
        else
            HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/windowbackends.csv", "RegisterBackend", "FAILED")
        end
    else
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/windowbackends.csv", "GetCurrentWindow", "FAILED")
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/windowbackends.csv", "GetCurrentTitle", "FAILED")
    end
end

function onExit()

end