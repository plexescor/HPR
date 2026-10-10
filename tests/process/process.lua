-- =============================================================================
-- Test Suite: Process
-- =============================================================================
-- Verifies HPR's Processes API
--
--   HPR.runSystemCommand(command)     Runs a command using piping and returns the result
--   HPR.getPid(name)                  Returns AppId for an app
--   HPR.killPid(pid)                  Kills an app using its PID
--
-- Expected output (written to output/process.csv):
--   RunSystemCommand,PASSED
--   GetPID,PASSED
--   KillPID,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "process" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "Process Tester"
HPR.authorName = "Plexescor"

function init()
    HPR.sleep(3000)
    -- We write the test result at the start, so when HPR is killed, the value remains and onExit() doesnt run
    -- If on exit runs, then HPR didnt crash, thus killPid also didnt work
    HPR.writeCsv(HPR.getExtensionDir() .. "output/process.csv", "KillPID", "PASSED")

    local echoResult = HPR.runSystemCommand("echo \"Hello\"")

    if echoResult:find("Hello", 1, true) then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/process.csv", "RunSystemCommand", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/process.csv", "RunSystemCommand", "FAILED")
    end

    local HPRPid = HPR.getPid("HPR")

    if HPRPid ~= nil and HPRPid ~= "" then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/process.csv", "GetPID", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/process.csv", "GetPID", "FAILED")
    end

    -- Sleep for 15 seconds so other extensions have time to finish
    -- TODO: increase this in the future
    HPRPid = HPR.getPid("HPR")
    print("PID is " .. HPRPid)
    HPR.sleep(15000)
    print("Sleep complete")

    HPR.killPid(HPRPid)
end

function onExit()
    HPR.writeCsv(HPR.getExtensionDir() .. "output/process.csv", "KillPID", "FAILED")
end