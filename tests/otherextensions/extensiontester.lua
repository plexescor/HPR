HPR.extensionName = "ExtensionTester"
HPR.authorName = "Plexescor"

local iteration = 0

local extension1 = "Helper_ExtensionAPI1"
local extension2 = "Helper_ExtensionAPI2"
local extensionAuthor_Common = "Plexescor"

function init()

    local findCount = 0
    HPR.sleep_E(2000)

    local extensions = HPR.getLoadedExtensions_E()
    for i, ext in ipairs(extensions) do
        if (ext.extensionName == extension1 and ext.authorName == extensionAuthor_Common) then
            findCount = findCount + 1
            HPR.log_E("ExtensionTester", "Found loaded extension: " .. ext.extensionName .. " by " .. ext.authorName)
            HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/extensiontester.csv", "GetLoadedExtensions", "PASSED")
        
        elseif (ext.extensionName == extension2 and ext.authorName == extensionAuthor_Common) then
            findCount = findCount + 1
            HPR.log_E("ExtensionTester", "Found loaded extension: " .. ext.extensionName .. " by " .. ext.authorName)
            HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/extensiontester.csv", "GetLoadedExtensions", "PASSED")
        
        end
    end

    if (findCount ~= 2) then
        HPR.log_E("ExtensionTester", "Did not find both expected extensions. Found: " .. findCount)
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/extensiontester.csv", "GetLoadedExtensions", "FAILED")
    end

    findCount = 0

    -- HPR.sleep_E(5000)
    HPR.unloadExtension_E(extensionAuthor_Common, extension1)
    HPR.unloadExtension_E(extensionAuthor_Common, extension2)

    extensions = HPR.getLoadedExtensions_E()

    for i, ext in ipairs(extensions) do
        if (ext.extensionName == extension1 and ext.authorName == extensionAuthor_Common) then
            findCount = findCount + 1

        elseif (ext.extensionName == extension2 and ext.authorName == extensionAuthor_Common) then
            findCount = findCount + 1

        end
    end

    if (findCount ~= 0) then
        HPR.log_E("ExtensionTester", "Found extensions that should have been unloaded. Found: " .. findCount)
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/extensiontester.csv", "UnloadExtension", "FAILED")
    else
        HPR.log_E("ExtensionTester", "Successfully unloaded both extensions.")
        HPR.writeCsv_E(HPR.getExtensionDir_E() .. "output/extensiontester.csv", "UnloadExtension", "PASSED")
    end
end

function onTick(delta)

end

function onExit()

end