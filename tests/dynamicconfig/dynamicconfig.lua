-- =============================================================================
-- Test Suite: Dynamic API Configuration
-- =============================================================================
-- Verifies dynamic updates of suffix, prefix, and compatibility configurations:
--
--   setLegacyAPISuffix(bool)     Toggles legacy _E suffix.
--   setUseHPRTablePrefix(bool)   Toggles global vs HPR. namespaces.
--   setVersionSupport(table)     Sets compatible HPR versions dynamically.
--
-- Expected output (written to output/dynamicconfig.csv):
--   GlobalAPI,PASSED
--   LegacySuffix,PASSED
--   VersionSupport,PASSED
--
-- How to run:
--   python tests/main.py  ->  select "dynamicconfig" suite, then close HPR.
-- =============================================================================

HPR.extensionName = "Dynamic Config"
HPR.authorName = "Plexescor"
HPR.versionSupport = { "0.9.8" }

function init()
    -- test useHPRTablePrefix (default true)
    local testSuccess = true

    -- verify that prefix functions exist initially under HPR, and not globally
    if HPR.writeCsv == nil then testSuccess = false end
    if writeCsv ~= nil then testSuccess = false end

    -- toggle useHPRTablePrefix to false (global binding direction)
    HPR.setUseHPRTablePrefix(false)
    if writeCsv == nil then testSuccess = false end
    if HPR.writeCsv == nil then testSuccess = false end

    -- toggle back to true (prefix-only direction)
    setUseHPRTablePrefix(true)
    if writeCsv ~= nil then testSuccess = false end
    if HPR.writeCsv == nil then testSuccess = false end

    -- set it to false for the remainder of the test
    HPR.setUseHPRTablePrefix(false)

    if testSuccess then
        writeCsv(getExtensionDir() .. "output/dynamicconfig.csv", "GlobalAPI", "PASSED")
    else
        writeCsv(getExtensionDir() .. "output/dynamicconfig.csv", "GlobalAPI", "FAILED")
    end

    -- test LegacyAPISuffix (defaults to false)
    local suffixSuccess = true

    -- verify default state: writeCsv exists, writeCsv_E does not
    if writeCsv == nil then suffixSuccess = false end
    if writeCsv_E ~= nil then suffixSuccess = false end

    -- toggle legacy suffix to true
    setLegacyAPISuffix(true)
    if writeCsv ~= nil then suffixSuccess = false end
    if writeCsv_E == nil then suffixSuccess = false end

    -- toggle legacy suffix back to false
    setLegacyAPISuffix_E(false)
    if writeCsv == nil then suffixSuccess = false end
    if writeCsv_E ~= nil then suffixSuccess = false end

    if suffixSuccess then
        writeCsv(getExtensionDir() .. "output/dynamicconfig.csv", "LegacySuffix", "PASSED")
    else
        writeCsv(getExtensionDir() .. "output/dynamicconfig.csv", "LegacySuffix", "FAILED")
    end

    -- test VersionSupport compatibility (no suffix)
    local versionSuccess = true

    print("initial compatibility: " .. tostring(amICompatible()))
    if amICompatible() == false then versionSuccess = false end

    -- set incompatible version list
    setVersionSupport({"9.9.9", "8.8.8"})
    print("after incompatible versionSupport: " .. tostring(amICompatible()))
    if amICompatible() == true then versionSuccess = false end

    -- set compatible version list (include current version)
    local ver = getHPRVersion()
    print("detected HPR version: " .. ver)
    setVersionSupport({ver, "9.9.9"})
    print("after compatible versionSupport: " .. tostring(amICompatible()))
    if amICompatible() == false then versionSuccess = false end

    -- toggle legacy suffix to true to test version support with suffix
    setLegacyAPISuffix(true)

    -- set incompatible version list with suffix
    setVersionSupport_E({"9.9.9", "8.8.8"})
    print("with suffix after incompatible: " .. tostring(amICompatible_E()))
    if amICompatible_E() == true then versionSuccess = false end

    -- set compatible version list with suffix
    setVersionSupport_E({ver, "9.9.9"})
    print("with suffix after compatible: " .. tostring(amICompatible_E()))
    if amICompatible_E() == false then versionSuccess = false end

    -- toggle legacy suffix back to false to write results using default writeCsv
    setLegacyAPISuffix_E(false)

    if versionSuccess then
        writeCsv(getExtensionDir() .. "output/dynamicconfig.csv", "VersionSupport", "PASSED")
    else
        writeCsv(getExtensionDir() .. "output/dynamicconfig.csv", "VersionSupport", "FAILED")
    end

    return 500
end
