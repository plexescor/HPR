-- =============================================================================
-- Test Suite Component: Net I/O Server
-- =============================================================================
-- Starts the embedded HTTP server on port 18888 on its own extension thread:
--
--   HPR.startServer_E(port, handler) Starts an embedded HTTP server on the given
--                                    port and registers a Lua request handler.
--
-- Expected output (written to output/netio.csv):
--   StartServer,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "netio" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "Net I/O Server"
HPR.authorName = "Plexescor"

local testPort = 18888

function init()
    HPR.sleep_E(1000) -- Stagger startup to prevent CSV write collisions
    local started = false
    local attempts = 0
    while not started and attempts < 5 do
        started = HPR.startServer_E(testPort, function(req)
            HPR.log_E("NetIOServer", "Server received request: " .. (req.method or "") .. " " .. (req.path or ""))
            
            if req.method == "GET" then
                return { status = 200, body = "HELLO_GET" }
            elseif req.method == "POST" then
                return { status = 201, body = "HELLO_POST:" .. (req.body or "") }
            elseif req.method == "PUT" then
                return { status = 200, body = "HELLO_PUT:" .. (req.body or "") }
            elseif req.method == "DELETE" then
                return { status = 200, body = "HELLO_DELETE" }
            end
            return { status = 404, body = "NOT_FOUND" }
        end)
        if not started then
            attempts = attempts + 1
            HPR.sleep_E(300)
        end
    end

    if started then
        local success = HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/netio.csv", "StartServer", "PASSED")
        HPR.log_E("NetIOServer", "writeCsv_E PASSED success: " .. tostring(success))
    else
        local success = HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/netio.csv", "StartServer", "FAILED")
        HPR.log_E("NetIOServer", "writeCsv_E FAILED success: " .. tostring(success))
    end

    return 1000
end

function onTick(delta)

end

function onExit()

end
