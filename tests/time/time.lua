-- =============================================================================
-- Test Suite: Time Utilities
-- =============================================================================
-- Verifies HPR's native Sleep, time conversion, formatting, and date parsing APIs:
--
--   HPR.formatTime_HHMMSS(ms)        Formats duration in ms -> "Xh Ym Zs".
--   HPR.convertToDate_DDMMYY(ms)    Converts ms timestamp -> "DD-MM-YY".
--   HPR.convertToDate_MMYY(ms)      Converts ms timestamp -> "MM-YY".
--   HPR.convertToTime_HHMMSS_12(ms) Converts ms timestamp -> 12h time ("hh:mm:ss am/pm").
--   HPR.parseDate_DDMMYY(str)       Parses "DD-MM-YY" -> Unix ms timestamp.
--   HPR.parseDate_MMYY(str)         Parses "MM-YY" -> Unix ms timestamp (1st of month).
--   HPR.extractMMYY_from_DDMMYY(str)Extracts "MM-YY" substring from "DD-MM-YY".
--   HPR.sleep(ms)                     Sleeps for specified ms duration.
--
--   TIMEZONE NOTE:
--   Date & time formatting functions use the system's local timezone (e.g. IST UTC+5:30).
--   Assertions use pattern-matching and type checking to ensure tests pass reliably
--   across all timezones without hardcoding UTC-specific date/time strings.
--
-- Expected output (written to output/time.csv):
--   FormatTime_HHMMSS,PASSED
--   ConvertToDate_DDMMYY,PASSED
--   ConvertToDate_MMYY,PASSED
--   ConvertToTime_HHMMSS_12,PASSED
--   ParseDate_DDMMYY,PASSED
--   ParseDate_MMYY,PASSED
--   ExtractMMYY_from_DDMMYY,PASSED
--   Sleep,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "time" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "Time"
HPR.authorName = "Plexescor"

function init()
    local testTime = 1718012345678 -- Mon Jun 10 2024

    local formattedTime = HPR.formatTime_HHMMSS(testTime)
    local date = HPR.convertToDate_DDMMYY(testTime)
    local dateShort = HPR.convertToDate_MMYY(testTime)
    local time12 = HPR.convertToTime_HHMMSS_12(testTime)

    local timeStamp = HPR.parseDate_DDMMYY(date)
    local timeStampShort = HPR.parseDate_MMYY(dateShort)
    local extracted = HPR.extractMMYY_from_DDMMYY(date)

    -- 1. formatTime_HHMMSS returns duration string e.g. "477225h 39m 5s"
    if type(formattedTime) == "string" and string.find(formattedTime, "s", 1, true) then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "FormatTime_HHMMSS", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "FormatTime_HHMMSS", "FAILED")
    end

    -- 2. convertToDate_DDMMYY returns "DD-MM-YY"
    if type(date) == "string" and string.match(date, "^%d%d%-%d%d%-%d%d$") then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "ConvertToDate_DDMMYY", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "ConvertToDate_DDMMYY", "FAILED")
    end

    -- 3. convertToDate_MMYY returns "MM-YY"
    if type(dateShort) == "string" and string.match(dateShort, "^%d%d%-%d%d$") then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "ConvertToDate_MMYY", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "ConvertToDate_MMYY", "FAILED")
    end

    -- 4. convertToTime_HHMMSS_12 returns lowercase 12h time e.g. "03:39:05 pm"
    if type(time12) == "string" and string.match(time12, "^%d%d:%d%d:%d%d %a%a$") then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "ConvertToTime_HHMMSS_12", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "ConvertToTime_HHMMSS_12", "FAILED")
    end

    -- 5. parseDate_DDMMYY returns non-zero timestamp ms
    if type(timeStamp) == "number" and timeStamp > 0 then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "ParseDate_DDMMYY", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "ParseDate_DDMMYY", "FAILED")
    end

    -- 6. parseDate_MMYY returns non-zero timestamp ms
    if type(timeStampShort) == "number" and timeStampShort > 0 then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "ParseDate_MMYY", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "ParseDate_MMYY", "FAILED")
    end

    -- 7. extractMMYY_from_DDMMYY extracts MM-YY from DD-MM-YY string
    if type(extracted) == "string" and extracted == string.sub(date, 4) then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "ExtractMMYY_from_DDMMYY", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "ExtractMMYY_from_DDMMYY", "FAILED")
    end


    -- Sleep test
    local sleepStart = HPR.getTime_MS()
    HPR.sleep(767)
    local sleepEnd = HPR.getTime_MS()
    
    -- HPR.log(HPR.extensionName, "Sleep test: expected ~767ms, got " .. (sleepEnd - sleepStart) .. "ms")
    -- Windows jitters a bit, so allow 200ms margin of error
    if math.abs((sleepEnd - sleepStart) - 767) < 200 then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "Sleep", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "Sleep", "FAILED")
    end


    --GetTimeMS test

    local timeMs = HPR.getTime_MS()
    local dateStr = HPR.convertToDate_DDMMYY(timeMs)
    local day = string.sub(dateStr, 1, 2)
    local month = string.sub(dateStr, 4, 5)
    local year = "20" .. string.sub(dateStr, 7, 8)
    local logDate = year .. "-" .. month .. "-" .. day

    --What we will do check if log file for today exists and has content. 
    --If so, we can assume getTime_MS working correctly.
    local result = ""
    if string.find(HPR.getOsName() or "", "Windows", 1, true) then
        local appdata = HPR.runSystemCommand("echo %APPDATA%")
        appdata = appdata:gsub("[\r\n]", "")
        result = HPR.runSystemCommand("type \"" .. appdata .. "\\HPR\\HPR_Config\\logs\\" .. logDate .. ".log\"")
    else
        result = HPR.runSystemCommand("cat ~/.config/HPR/logs/" .. logDate .. ".log")
    end

    if string.len(result) > 7 then
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "GetTime_MS", "PASSED")
    else
        HPR.writeCsv(HPR.getExtensionDir() .. "output/time.csv", "GetTime_MS", "FAILED")
    end
end

function onTick(delta)

end

function onExit()

end