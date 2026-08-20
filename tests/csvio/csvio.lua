-- =============================================================================
-- Test Suite: CSV I/O
-- =============================================================================
-- Verifies HPR's CSV reading and writing extension APIs:
--
--   HPR.writeCsv(path, key, value)        Writes or updates a key-value row.
--   HPR.writeCsv(path, key, v1, v2, ...)  Writes or updates a multi-value row.
--   HPR.readCsv(path, key)               Reads a single key's value or table.
--   HPR.readCsv(path)                    Reads entire file as a table.
--   HPR.deleteCsv(path)                  Deletes a CSV file.
--   HPR.getExtensionDir()                Returns the extension's directory path.
--   HPR.getExtensionPath()               Returns the extension's full path.
--
-- Expected output (written to output/csvio.csv):
--   WriteCSV,PASSED
--   ReadCSV,PASSED
--   DeleteCSV,PASSED
--   GetExtensionDir,PASSED
--   GetExtensionPath,PASSED
--   WriteCSV_MultiValue,PASSED
--   ReadCSV_MultiValue_Key,PASSED
--   ReadCSV_SingleValue_BackwardCompat,PASSED
--   ReadCSV_MultiValue_FullFile,PASSED
--   WriteCSV_UpdateRow,PASSED
--   ReadCSV_KeyNotFound,PASSED
--   WriteCSV_NumericValue,PASSED
--   ReadCSV_NumericValue,PASSED
--   WriteCSV_BoolValue,PASSED
--   ReadCSV_BoolValue,PASSED
--   WriteCSV_MixedTypes,PASSED
--   ReadCSV_MixedTypes,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "csvio" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "CSV I/O"
HPR.authorName = "Plexescor"
HPR.versionSupport = { "0.9.7", "0.9.8" }

local expectedDir = "csvio"
local dir = nil
local out = nil

function init()
    dir = HPR.getExtensionDir()
    out = dir .. "output/csvio.csv"

    -- GetExtensionDir
    if dir ~= (expectedDir .. "/") and dir ~= (expectedDir .. "\\") then
        HPR.log("CSV I/O", "getExtensionDir() returned unexpected value: " .. dir)
        HPR.writeCsv(out, "GetExtensionDir", "FAILED")
    else
        HPR.writeCsv(out, "GetExtensionDir", "PASSED")
    end

    -- GetExtensionPath
    local dir2 = HPR.getExtensionPath()
    if dir2 ~= (expectedDir .. "/") and dir2 ~= (expectedDir .. "\\") then
        HPR.log("CSV I/O", "getExtensionPath() returned unexpected value: " .. dir2)
        HPR.writeCsv(out, "GetExtensionPath", "FAILED")
    else
        HPR.writeCsv(out, "GetExtensionPath", "PASSED")
    end

    -- WriteCSV
    HPR.writeCsv(out, "WriteCSV", "PASSED")

    -- ReadCSV (resolved in onTick via PENDING sentinel)
    HPR.writeCsv(out, "ReadCSV", "PENDING")

    -- test.csv is used by the DeleteCSV test in onExit
    HPR.writeCsv(dir .. "output/test.csv", "test", "yes")

    -- WriteCSV_MultiValue
    local ok = HPR.writeCsv(dir .. "output/multivalue_test.csv", "player", "Alice", 99, true)
    if ok then
        HPR.writeCsv(out, "WriteCSV_MultiValue", "PASSED")
    else
        HPR.writeCsv(out, "WriteCSV_MultiValue", "FAILED")
    end

    -- ReadCSV_MultiValue_Key
    local row = HPR.readCsv(dir .. "output/multivalue_test.csv", "player")
    if type(row) == "table" and row[1] == "Alice" and row[2] == 99 and row[3] == true then
        HPR.writeCsv(out, "ReadCSV_MultiValue_Key", "PASSED")
    else
        HPR.writeCsv(out, "ReadCSV_MultiValue_Key", "FAILED")
    end

    -- ReadCSV_SingleValue_BackwardCompat: single-value rows must still return a scalar
    HPR.writeCsv(dir .. "output/multivalue_test.csv", "score", 42)
    local sv = HPR.readCsv(dir .. "output/multivalue_test.csv", "score")
    if type(sv) == "number" and sv == 42 then
        HPR.writeCsv(out, "ReadCSV_SingleValue_BackwardCompat", "PASSED")
    else
        HPR.writeCsv(out, "ReadCSV_SingleValue_BackwardCompat", "FAILED")
    end

    -- ReadCSV_MultiValue_FullFile: full-file read returns scalars for old rows, tables for new
    local all = HPR.readCsv(dir .. "output/multivalue_test.csv")
    if type(all) == "table"
        and type(all["player"]) == "table"
        and all["player"][1] == "Alice"
        and all["player"][2] == 99
        and all["player"][3] == true
        and type(all["score"]) == "number"
        and all["score"] == 42
    then
        HPR.writeCsv(out, "ReadCSV_MultiValue_FullFile", "PASSED")
    else
        HPR.writeCsv(out, "ReadCSV_MultiValue_FullFile", "FAILED")
    end

    -- WriteCSV_UpdateRow: writing the same key twice must replace in-place
    HPR.writeCsv(dir .. "output/update_test.csv", "item", "old_val", 0)
    HPR.writeCsv(dir .. "output/update_test.csv", "item", "new_val", 1, false)
    local updRow = HPR.readCsv(dir .. "output/update_test.csv", "item")
    if type(updRow) == "table"
        and updRow[1] == "new_val"
        and updRow[2] == 1
        and updRow[3] == false
    then
        HPR.writeCsv(out, "WriteCSV_UpdateRow", "PASSED")
    else
        HPR.writeCsv(out, "WriteCSV_UpdateRow", "FAILED")
    end

    -- ReadCSV_KeyNotFound: missing key must return empty string
    local missing = HPR.readCsv(dir .. "output/multivalue_test.csv", "no_such_key")
    if missing == "" then
        HPR.writeCsv(out, "ReadCSV_KeyNotFound", "PASSED")
    else
        HPR.writeCsv(out, "ReadCSV_KeyNotFound", "FAILED")
    end

    -- WriteCSV_NumericValue / ReadCSV_NumericValue
    HPR.writeCsv(dir .. "output/numeric_test.csv", "pi", 3.14)
    local pi = HPR.readCsv(dir .. "output/numeric_test.csv", "pi")
    if type(pi) == "number" and math.abs(pi - 3.14) < 0.0001 then
        HPR.writeCsv(out, "WriteCSV_NumericValue", "PASSED")
        HPR.writeCsv(out, "ReadCSV_NumericValue", "PASSED")
    else
        HPR.writeCsv(out, "WriteCSV_NumericValue", "FAILED")
        HPR.writeCsv(out, "ReadCSV_NumericValue", "FAILED")
    end

    -- WriteCSV_BoolValue / ReadCSV_BoolValue
    HPR.writeCsv(dir .. "output/bool_test.csv", "flag", true)
    local bv = HPR.readCsv(dir .. "output/bool_test.csv", "flag")
    if bv == true then
        HPR.writeCsv(out, "WriteCSV_BoolValue", "PASSED")
        HPR.writeCsv(out, "ReadCSV_BoolValue", "PASSED")
    else
        HPR.writeCsv(out, "WriteCSV_BoolValue", "FAILED")
        HPR.writeCsv(out, "ReadCSV_BoolValue", "FAILED")
    end

    -- WriteCSV_MixedTypes / ReadCSV_MixedTypes
    HPR.writeCsv(dir .. "output/mixed_test.csv", "entry", "hello", 7, false, "world")
    local mr = HPR.readCsv(dir .. "output/mixed_test.csv", "entry")
    if type(mr) == "table"
        and mr[1] == "hello"
        and mr[2] == 7
        and mr[3] == false
        and mr[4] == "world"
    then
        HPR.writeCsv(out, "WriteCSV_MixedTypes", "PASSED")
        HPR.writeCsv(out, "ReadCSV_MixedTypes", "PASSED")
    else
        HPR.writeCsv(out, "WriteCSV_MixedTypes", "FAILED")
        HPR.writeCsv(out, "ReadCSV_MixedTypes", "FAILED")
    end

    return 500
end

function onTick(delta)
    local val = HPR.readCsv(out, "ReadCSV")
    if val == "PENDING" then
        HPR.writeCsv(out, "ReadCSV", "PASSED")
    end
end

function onExit()
    -- DeleteCSV (original test)
    local result = HPR.deleteCsv(dir .. "output/test.csv")
    if result then
        HPR.writeCsv(out, "DeleteCSV", "PASSED")
    else
        HPR.writeCsv(out, "DeleteCSV", "FAILED")
    end

    -- Clean up scratch files so the test runner doesn't pick them up
    HPR.deleteCsv(dir .. "output/multivalue_test.csv")
    HPR.deleteCsv(dir .. "output/update_test.csv")
    HPR.deleteCsv(dir .. "output/numeric_test.csv")
    HPR.deleteCsv(dir .. "output/bool_test.csv")
    HPR.deleteCsv(dir .. "output/mixed_test.csv")
end