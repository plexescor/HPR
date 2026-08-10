-- =============================================================================
-- Test Suite: CSV I/O
-- =============================================================================
-- Verifies HPR's CSV reading and writing extension APIs:
--
--   HPR.writeCsv(path, key, value) Writes or updates a key-value row.
--   HPR.readCsv(path, key)         Reads a single key's value from a CSV.
--   HPR.deleteCsv(path, key)       Deletes a row from a CSV.
--   HPR.getExtensionDir()          Returns the extension's directory path.
--   HPR.getExtensionPath()         Returns the extension's full path.
--
-- Expected output (written to output/csvio.csv):
--   WriteCSV,PASSED
--   ReadCSV,PASSED
--   DeleteCSV,PASSED
--   GetExtensionDir,PASSED
--   GetExtensionPath,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "csvio" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "CSV I/O"
HPR.authorName = "Plexescor"

local expectedDir = "csvio"
function init()

    local dir = HPR.getExtensionDir()

    if dir ~= (expectedDir .. "/") and dir ~= (expectedDir .. "\\") then
        HPR.log("CSV I/O", "getExtensionDir() returned unexpected value: " .. dir)
        HPR.writeCsv(HPR.getExtensionDir() .. "output/csvio.csv", "GetExtensionDir", "FAILED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/csvio.csv", "GetExtensionDir", "PASSED")
    end

    local dir2 = HPR.getExtensionPath()

    if dir2 ~= (expectedDir .. "/") and dir2 ~= (expectedDir .. "\\") then
        HPR.log("CSV I/O", "getExtensionPath() returned unexpected value: " .. dir2)
        HPR.writeCsv(HPR.getExtensionDir() .. "output/csvio.csv", "GetExtensionPath", "FAILED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/csvio.csv", "GetExtensionPath", "PASSED")
    end

    HPR.writeCsv(HPR.getExtensionDir() .. "output/csvio.csv", "WriteCSV", "PASSED")
    HPR.writeCsv(HPR.getExtensionDir() .. "output/csvio.csv", "ReadCSV", "PENDING")

    HPR.writeCsv(HPR.getExtensionDir() .. "output/test.csv", "test", "yes")
    return 500
end

function onTick(delta)
    local val = HPR.readCsv(HPR.getExtensionDir() .. "output/csvio.csv", "ReadCSV")
    if val == "PENDING" then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/csvio.csv", "ReadCSV", "PASSED")
    end
end

function onExit()

    local result = HPR.deleteCsv(HPR.getExtensionDir() .. "output/test.csv")
    if result then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/csvio.csv", "DeleteCSV", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/csvio.csv", "DeleteCSV", "FAILED")
    end
end