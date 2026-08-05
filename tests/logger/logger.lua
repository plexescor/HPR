-- =============================================================================
-- Test Suite: Logger Utilities
-- =============================================================================
-- Verifies HPR's native logging capabilities:
--
--   HPR.log(category, message)    Logs a message with a custom category prefix.
--
-- Expected output (written to output/logger.csv):
--   LogTest,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "logger" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "Logger"
HPR.authorName = "Plexescor"

function init()
    HPR.log(HPR.extensionName, "test_log")

    local timeMs = HPR.getTime_MS()
    local dateStr = HPR.convertToDate_DDMMYY(timeMs)
    local day = string.sub(dateStr, 1, 2)
    local month = string.sub(dateStr, 4, 5)
    local year = "20" .. string.sub(dateStr, 7, 8)
    local logDate = year .. "-" .. month .. "-" .. day

    local result = ""
    if string.find(HPR.getOsName() or "", "Windows", 1, true) then
        local appdata = HPR.runSystemCommand("echo %APPDATA%")
        appdata = appdata:gsub("[\r\n]", "")
        result = HPR.runSystemCommand("type \"" .. appdata .. "\\HPR\\HPR_Config\\logs\\" .. logDate .. ".log\"")
    else
        result = HPR.runSystemCommand("cat ~/.config/HPR/logs/" .. logDate .. ".log")
    end

    if string.find(result, "test_log") then
        HPR.log(HPR.extensionName, result)
        HPR.writeCsv(HPR.getExtensionDir() .. "output/logger.csv", "LogTest", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/logger.csv", "LogTest", "FAILED")
    end
end
