-- =============================================================================
-- Test Suite: Live Data
-- =============================================================================
-- Verifies Live Data Getter APIs
--
--   HPR.getLiveTimeLogPerApp()
--   HPR.getLiveTimeLogPerTab()
--   HPR.getLiveTimeLogPerProject()
--
-- Expected output (written to output/livedata.csv):
--   GetLiveTimeLogApp,PASSED
--   GetLiveTimeLogTab,PASSED
--   GetLiveTimeLogProject,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "livedata" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "Live Data Tester"
HPR.authorName = "Plexescor"

function init()
    local log_App = HPR.getLiveTimeLogPerApp()
    local log_Tab = HPR.getLiveTimeLogPerTab()
    local log_Project = HPR.getLiveTimeLogPerProject()

    if next(log_App) ~= nil then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/livedata.csv", "GetLiveTimeLogApp", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/livedata.csv", "GetLiveTimeLogApp", "FAILED")
    end

    if next(log_Tab) ~= nil then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/livedata.csv", "GetLiveTimeLogTab", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/livedata.csv", "GetLiveTimeLogTab", "FAILED")
    end

    if next(log_Project) ~= nil then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/livedata.csv", "GetLiveTimeLogProject", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/livedata.csv", "GetLiveTimeLogProject", "FAILED")
    end
end

function onTick()

end

function onExit()

end