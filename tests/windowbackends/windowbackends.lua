-- =============================================================================
-- Test Suite: Window Backends
-- =============================================================================
-- Verifies custom window backend registration and window queries:
--
--   HPR.getCurrentWindow()   Returns active window process name.
--   HPR.getCurrentTitle()    Returns active window title bar text.
--   HPR.registerBackend(...) Registers a custom Lua window backend.
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
HPR.versionSupport = { "0.9.7", "0.9.8" }

function init()
    HPR.sleep(1000) -- wait till our friend starts up
end --1second tick

function onTick(delta)
    local name = HPR.getCurrentWindow()
    local title = HPR.getCurrentTitle()

    if (name ~= nil and title ~= nil) then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/windowbackends.csv", "GetCurrentWindow", "PASSED")
        HPR.writeCsv(HPR.getExtensionDir() .. "output/windowbackends.csv", "GetCurrentTitle", "PASSED")

        if (name == "custom_name" and title == "custom_title") then
            HPR.writeCsv(HPR.getExtensionDir() .. "output/windowbackends.csv", "RegisterBackend", "PASSED")
        else
            HPR.writeCsv(HPR.getExtensionDir() .. "output/windowbackends.csv", "RegisterBackend", "FAILED")
        end
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/windowbackends.csv", "GetCurrentWindow", "FAILED")
        HPR.writeCsv(HPR.getExtensionDir() .. "output/windowbackends.csv", "GetCurrentTitle", "FAILED")
    end
end

function onExit()

end