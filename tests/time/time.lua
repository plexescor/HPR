-- =============================================================================
-- Test Suite: Time Utilities
-- =============================================================================
-- Verifies HPR's native time conversion, formatting, and date parsing APIs:
--
--   HPR.formatTime_HHMMSS_E(ms)        Formats duration in ms -> "Xh Ym Zs".
--   HPR.convertToDate_DDMMYY_E(ms)    Converts ms timestamp -> "DD-MM-YY".
--   HPR.convertToDate_MMYY_E(ms)      Converts ms timestamp -> "MM-YY".
--   HPR.convertToTime_HHMMSS_12_E(ms) Converts ms timestamp -> 12h time ("hh:mm:ss am/pm").
--   HPR.parseDate_DDMMYY_E(str)       Parses "DD-MM-YY" -> Unix ms timestamp.
--   HPR.parseDate_MMYY_E(str)         Parses "MM-YY" -> Unix ms timestamp (1st of month).
--   HPR.extractMMYY_from_DDMMYY_E(str)Extracts "MM-YY" substring from "DD-MM-YY".
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
--
-- How to run:
--   python tests/main.py  ->  select "time" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "Time"
HPR.authorName = "Plexescor"

function init()
    local testTime = 1718012345678 -- Mon Jun 10 2024

    local formattedTime = HPR.formatTime_HHMMSS_E(testTime)
    local date = HPR.convertToDate_DDMMYY_E(testTime)
    local dateShort = HPR.convertToDate_MMYY_E(testTime)
    local time12 = HPR.convertToTime_HHMMSS_12_E(testTime)

    local timeStamp = HPR.parseDate_DDMMYY_E(date)
    local timeStampShort = HPR.parseDate_MMYY_E(dateShort)
    local extracted = HPR.extractMMYY_from_DDMMYY_E(date)

    -- 1. formatTime_HHMMSS_E returns duration string e.g. "477225h 39m 5s"
    if type(formattedTime) == "string" and string.find(formattedTime, "s", 1, true) then
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "FormatTime_HHMMSS", "PASSED")
    else
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "FormatTime_HHMMSS", "FAILED")
    end

    -- 2. convertToDate_DDMMYY_E returns "DD-MM-YY"
    if type(date) == "string" and string.match(date, "^%d%d%-%d%d%-%d%d$") then
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "ConvertToDate_DDMMYY", "PASSED")
    else
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "ConvertToDate_DDMMYY", "FAILED")
    end

    -- 3. convertToDate_MMYY_E returns "MM-YY"
    if type(dateShort) == "string" and string.match(dateShort, "^%d%d%-%d%d$") then
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "ConvertToDate_MMYY", "PASSED")
    else
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "ConvertToDate_MMYY", "FAILED")
    end

    -- 4. convertToTime_HHMMSS_12_E returns lowercase 12h time e.g. "03:39:05 pm"
    if type(time12) == "string" and string.match(time12, "^%d%d:%d%d:%d%d %a%a$") then
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "ConvertToTime_HHMMSS_12", "PASSED")
    else
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "ConvertToTime_HHMMSS_12", "FAILED")
    end

    -- 5. parseDate_DDMMYY_E returns non-zero timestamp ms
    if type(timeStamp) == "number" and timeStamp > 0 then
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "ParseDate_DDMMYY", "PASSED")
    else
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "ParseDate_DDMMYY", "FAILED")
    end

    -- 6. parseDate_MMYY_E returns non-zero timestamp ms
    if type(timeStampShort) == "number" and timeStampShort > 0 then
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "ParseDate_MMYY", "PASSED")
    else
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "ParseDate_MMYY", "FAILED")
    end

    -- 7. extractMMYY_from_DDMMYY_E extracts MM-YY from DD-MM-YY string
    if type(extracted) == "string" and extracted == string.sub(date, 4) then
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "ExtractMMYY_from_DDMMYY", "PASSED")
    else
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/time.csv", "ExtractMMYY_from_DDMMYY", "FAILED")
    end
end

function onTick(delta)

end

function onExit()

end