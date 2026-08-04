-- =============================================================================
-- Test Suite Component: Net I/O JSON & ISO 8601 Utilities
-- =============================================================================
-- Verifies JSON serialization/parsing and ISO 8601 date parsing APIs:
--
--   HPR.parseISO8601_E(str)          Parses an ISO 8601 datetime string and
--                                    returns Unix timestamp in milliseconds.
--
--   HPR.parseJSON_E(jsonStr, path?)  Parses a raw JSON string into a Lua table
--                                    or plucks a value via dot-separated fieldPath.
--
--   HPR.toJSON_E(luaTable)           Serializes a Lua table into a minified
--                                    JSON string representation.
--
-- Expected output (written to output/netio.csv):
--   ParseISO8601,PASSED
--   ParseJSON,PASSED
--   ToJSON,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "netio" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "Net I/O JSON & ISO"
HPR.authorName = "Plexescor"

function init()
    -- 1. Test HPR.parseISO8601_E
    -- "2024-05-21T14:30:00.500Z" -> 1716301800500 ms
    local isoStr = "2024-05-21T14:30:00.500Z"
    local ms = HPR.parseISO8601_E(isoStr)
    if ms == 1716301800500 then
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/netio.csv", "ParseISO8601", "PASSED")
    else
        HPR.log_E("NetIOJsonIso", "parseISO8601_E failed, got: " .. tostring(ms))
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/netio.csv", "ParseISO8601", "FAILED")
    end

    -- 2. Test HPR.parseJSON_E (table parsing & nested dot-path extraction)
    local rawJson = '{"name":"firefox","active":true,"data":{"url":"https://example.com","port":8080}}'
    local tbl = HPR.parseJSON_E(rawJson)
    local urlVal = HPR.parseJSON_E(rawJson, "data.url")
    
    if type(tbl) == "table" and tbl.name == "firefox" and tbl.active == true and urlVal == "https://example.com" then
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/netio.csv", "ParseJSON", "PASSED")
    else
        HPR.log_E("NetIOJsonIso", "parseJSON_E failed")
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/netio.csv", "ParseJSON", "FAILED")
    end

    -- 3. Test HPR.toJSON_E (serializing Lua table to JSON string)
    local inputTable = { name = "firefox", active = true }
    local jsonStr = HPR.toJSON_E(inputTable)
    if type(jsonStr) == "string" and string.find(jsonStr, '"name":"firefox"', 1, true) and string.find(jsonStr, '"active":true', 1, true) then
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/netio.csv", "ToJSON", "PASSED")
    else
        HPR.log_E("NetIOJsonIso", "toJSON_E failed, got: " .. tostring(jsonStr))
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/netio.csv", "ToJSON", "FAILED")
    end

    return 1000
end

function onTick(delta)

end

function onExit()

end
