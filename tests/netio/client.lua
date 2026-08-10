-- =============================================================================
-- Test Suite Component: Net I/O Client
-- =============================================================================
-- Sends HTTP GET/POST/PUT/DELETE requests to the Net I/O Server on port 18888:
--
--   HPR.httpGet(host, path, ...)    Sends an HTTP GET request to target server.
--   HPR.httpPost(host, path, body) Sends an HTTP POST request with body.
--   HPR.httpPut(host, path, body)  Sends an HTTP PUT request with body.
--   HPR.httpDelete(host, path, ...) Sends an HTTP DELETE request.
--
-- Expected output (written to output/netio.csv):
--   HTTPGet,PASSED
--   HTTPPost,PASSED
--   HTTPPut,PASSED
--   HTTPDelete,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "netio" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "Net I/O Client"
HPR.authorName = "Plexescor"
HPR.versionSupport = { "0.9.7", "0.9.8" }

local testPort = 18888

function init()
    HPR.sleep(2500) -- Wait for server.lua to bind port 18888 and write its CSV entry

    local host = "127.0.0.1:" .. testPort

    -- 1. Test HTTP GET
    local bodyGet, statusGet = HPR.httpGet(host, "/test", false)
    if statusGet == 200 and bodyGet == "HELLO_GET" then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/netio.csv", "HTTPGet", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/netio.csv", "HTTPGet", "FAILED")
    end

    -- 2. Test HTTP POST
    local bodyPost, statusPost = HPR.httpPost(host, "/test", "DATA", false)
    if statusPost == 201 and string.find(bodyPost or "", "HELLO_POST:DATA", 1, true) then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/netio.csv", "HTTPPost", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/netio.csv", "HTTPPost", "FAILED")
    end

    -- 3. Test HTTP PUT
    local bodyPut, statusPut = HPR.httpPut(host, "/test", "UPDATE", false)
    if statusPut == 200 and string.find(bodyPut or "", "HELLO_PUT:UPDATE", 1, true) then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/netio.csv", "HTTPPut", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/netio.csv", "HTTPPut", "FAILED")
    end

    -- 4. Test HTTP DELETE
    local bodyDel, statusDel = HPR.httpDelete(host, "/test", false)
    if statusDel == 200 and bodyDel == "HELLO_DELETE" then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/netio.csv", "HTTPDelete", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/netio.csv", "HTTPDelete", "FAILED")
    end

    return 1000
end

function onTick(delta)

end

function onExit()

end
