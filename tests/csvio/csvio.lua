-- =============================================================================
-- Test Suite: CSV I/O
-- =============================================================================
-- Verifies HPR's CSV reading and writing extension APIs:
--
--   writeCsv(path, key, value) Writes or updates a key-value row.
--   readCsv(path, key)         Reads a single key's value from a CSV.
--   deleteCsv(path, key)       Deletes a row from a CSV.
--   getExtensionDir()          Returns the extension's directory path.
--   getExtensionPath()         Returns the extension's full path.
--
-- Expected output (written to output/csvio.csv):
--   WriteCSV,PASSED
--   ReadCSV,PASSED
--   DeleteCSV,PASSED
--   GetExtensionDir,PASSED
--   GetExtensionPath,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "csvio" suite, then close 
-- =============================================================================

extensionName = "CSV I/O"
authorName = "Plexescor"
HPR.useHPRTablePrefix = false

local expectedDir = "csvio"
function init()

    local dir = getExtensionDir()

    if dir ~= (expectedDir .. "/") and dir ~= (expectedDir .. "\\") then
        log("CSV I/O", "getExtensionDir() returned unexpected value: " .. dir)
        writeCsv(getExtensionDir() .. "output/csvio.csv", "GetExtensionDir", "FAILED")
    else
        writeCsv(getExtensionDir() .. "output/csvio.csv", "GetExtensionDir", "PASSED")
    end

    local dir2 = getExtensionPath()

    if dir2 ~= (expectedDir .. "/") and dir2 ~= (expectedDir .. "\\") then
        log("CSV I/O", "getExtensionPath() returned unexpected value: " .. dir2)
        writeCsv(getExtensionDir() .. "output/csvio.csv", "GetExtensionPath", "FAILED")
    else
        writeCsv(getExtensionDir() .. "output/csvio.csv", "GetExtensionPath", "PASSED")
    end

    writeCsv(getExtensionDir() .. "output/csvio.csv", "WriteCSV", "PASSED")
    writeCsv(getExtensionDir() .. "output/csvio.csv", "ReadCSV", "PENDING")

    writeCsv(getExtensionDir() .. "output/test.csv", "test", "yes")
    return 500
end

function onTick(delta)
    local val = readCsv(getExtensionDir() .. "output/csvio.csv", "ReadCSV")
    if val == "PENDING" then
        writeCsv(getExtensionDir() .. "output/csvio.csv", "ReadCSV", "PASSED")
    end
end

function onExit()

    local result = deleteCsv(getExtensionDir() .. "output/test.csv")
    if result then
        writeCsv(getExtensionDir() .. "output/csvio.csv", "DeleteCSV", "PASSED")
    else
        writeCsv(getExtensionDir() .. "output/csvio.csv", "DeleteCSV", "FAILED")
    end
end