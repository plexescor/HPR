-- =============================================================================
-- Test Suite: Lifecycle Hooks
-- =============================================================================
-- Verifies that HPR correctly calls the three core extension lifecycle hooks:
--
--   init()       Called once when the extension is loaded. Returns the desired
--                tick interval in milliseconds.
--
--   onTick(delta) Called repeatedly by HPR at the interval returned by init().
--                 delta is the actual elapsed time in ms since the last tick.
--                 This test checks that delta stays within ±100ms of the
--                 expected interval (500ms).
--
--   onExit()     Called once when HPR is shutting down or the extension is
--                being unloaded.
--
--   onAction()   Called when user presses the "ACTION" button in the UI to invoke
--                extension specific functions
--
-- Expected output (written to output/lifecyclehooks.csv):
--   Init,PASSED
--   Tick,PASSED   (or FAILED if delta drifts beyond ±100ms)
--   Exit,PASSED
--   Action,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "lifecyclehooks" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "LifecycleHooks"
HPR.authorName = "Plexescor"

local iteration = 0
local expectedDelta = 500

function init()
    HPR.writeCsv(HPR.getExtensionDir() .. "output/lifecyclehooks.csv", "Init", "PASSED")
    return expectedDelta
end

function onTick(delta)
    if (iteration ~= 0) then
        if math.abs(delta - expectedDelta) <= 100 then
            HPR.writeCsv(HPR.getExtensionDir() .. "output/lifecyclehooks.csv", "Tick", "PASSED")
            -- HPR.log("Tick delta: " .. delta .. "ms, expected: " .. expectedDelta .. "ms")
        else
            HPR.writeCsv(HPR.getExtensionDir() .. "output/lifecyclehooks.csv", "Tick", "FAILED")
            -- HPR.log("Tick delta: " .. delta .. "ms, expected: " .. expectedDelta .. "ms")
        end
    end

    iteration = iteration + 1
end

function onExit()
    HPR.writeCsv(HPR.getExtensionDir() .. "output/lifecyclehooks.csv", "Exit", "PASSED")
end

function onAction()
    HPR.writeCsv(HPR.getExtensionDir() .. "output/lifecyclehooks.csv", "Action", "PASSED")
end