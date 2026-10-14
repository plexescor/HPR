-- =============================================================================
-- Test Suite: Live Data
-- =============================================================================
-- Verifies Live Data Getter APIs
--
--   HPR.getLiveTimeLogPerApp()
--   HPR.getLiveTimeLogPerTab()
--   HPR.getLiveTimeLogPerProject()
--   HPR.getProductivityScores()
--   HPR.getProductivityScores_Hourly()
--
-- Expected output (written to output/livedata.csv):
--   GetLiveTimeLogApp,PASSED
--   GetLiveTimeLogTab,PASSED
--   GetLiveTimeLogProject,PASSED
--   GetProductivityScores,PASSED
--   GetProductivityScores_Hourly,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "livedata" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "Live Data Tester"
HPR.authorName = "Plexescor"

function init()
    HPR.sleep(4000) -- give user time to go to insights page
    local log_App = HPR.getLiveTimeLogPerApp()
    local log_Tab = HPR.getLiveTimeLogPerTab()
    local log_Project = HPR.getLiveTimeLogPerProject()
    local scores      = HPR.getProductivityScores(false, false)
    local scores_Hourly = HPR.getProductivityScores_Hourly(true, false)

    for app, ms in pairs(log_App) do
        HPR.log(HPR.extensionName, "App: " .. app .. " = " .. tostring(ms) .. "ms")
    end

    for tab, ms in pairs(log_Tab) do
        HPR.log(HPR.extensionName, "Tab: " .. tab .. " = " .. tostring(ms) .. "ms")
    end

    for project, ms in pairs(log_Project) do
        HPR.log(HPR.extensionName, "Project: " .. project .. " = " .. tostring(ms) .. "ms")
    end

    for i, score in ipairs(scores) do
        HPR.log(HPR.extensionName, "Score[" .. i .. "] = " .. tostring(score))
    end

    for i, score in ipairs(scores_Hourly) do
        HPR.log(HPR.extensionName, "Hourly Score[" .. i .. "] = " .. tostring(score))
    end

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

    if scores ~= nil and #scores > 0 then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/livedata.csv", "GetProductivityScores", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/livedata.csv", "GetProductivityScores", "FAILED")
    end

    if scores_Hourly ~= nil and #scores_Hourly > 0 then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/livedata.csv", "GetProductivityScores_Hourly", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/livedata.csv", "GetProductivityScores_Hourly", "FAILED")
    end
end

function onTick()

end

function onExit()

end