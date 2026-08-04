-- =============================================================================
-- Test Suite: CSV I/O
-- =============================================================================
-- Verifies HPR's CSV reading and writing extension APIs:
--
--   HPR.writeCsv_E(path, key, value) Writes or updates a key-value row.
--   HPR.readCsv_E(path, key)         Reads a single key's value from a CSV.
--
-- Expected output (written to output/csvio.csv):
--   WriteCSV,PASSED
--   ReadCSV,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "csvio" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "CSV I/O"
HPR.authorName = "Plexescor"

function init()
    HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/csvio.csv", "WriteCSV", "PASSED")
    HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/csvio.csv", "ReadCSV", "PENDING")
    return 500
end

function onTick(delta)
    local val = HPR.readCsv_E(HPR.getExtensionDir_E() .. "output/csvio.csv", "ReadCSV")
    if val == "PENDING" then
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/csvio.csv", "ReadCSV", "PASSED")
    end
end

function onExit()

end