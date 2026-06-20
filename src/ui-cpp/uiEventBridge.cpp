#include "uiEventBridge.hpp"
#include "appEvents.hpp"
#include "appState.hpp"
#include "extensionManager.hpp"
#include "HPRInterpreter.hpp"
#include "limitsManager.hpp"
#include "telemetryManager.hpp"
#include "netUtilities.hpp"
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

// Slint stuff
#include "app-window.h"
#include <slint.h>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace {
    const std::string FIREBASE_HOST = "humanpatternrecorder-default-rtdb.firebaseio.com";

    std::string jsonEscape(const std::string& input) {
        std::string output = "";
        for (char c : input) {
            if (c == '"') output += "\\\"";
            else if (c == '\\') output += "\\\\";
            else if (c == '\b') output += "\\b";
            else if (c == '\f') output += "\\f";
            else if (c == '\n') output += "\\n";
            else if (c == '\r') output += "\\r";
            else if (c == '\t') output += "\\t";
            else if (static_cast<unsigned char>(c) < 32) {
                std::ostringstream ss;
                ss << "\\u" << std::setfill('0') << std::setw(4) << std::hex << (int)c;
                output += ss.str();
            } else {
                output += c;
            }
        }
        return output;
    }
}

//CONSTRUCTOR FOR **COMPILED** UI
UiEventBridge::UiEventBridge(slint::ComponentHandle<MainWindow>& ui,  ExtensionManager* extMgr, HPRInterpreter* interpreter)
{
    if (extMgr)
        this->extManager = extMgr;

    if (interpreter)
        this->interpreter = interpreter;


    //Connect to event hub
    init();

    ui->on_loadHistoricalData_Singular([](slint::SharedString dateFromUi) 
    {
        //convert to cpp string
        std::string requestedDate = std::string(dateFromUi);
        
        //Emit the signal that we need to load data
        EventHub::emit(
            Event::LOAD_DATABASE_SINGULAR, 
            DatabaseDate_Singular{requestedDate}
        );
    });

    ui->on_loadHistoricalData_Number([](int days, slint::SharedString mode)
    {
        std::string modeStr = std::string(mode);
        //Emit the signal that we need to load data
        EventHub::emit(
            Event::LOAD_DATABASE_NUMBER, 
            DatabaseDate_Number{days, modeStr}
        );
    });

    ui->on_loadHistoricalData_Range([](slint::SharedString dateFrom, slint::SharedString dateTo, slint::SharedString mode)
    {
        std::string dateFromStr = std::string(dateFrom);
        std::string dateToStr = std::string(dateTo);
        std::string modeStr = std::string(mode);
        //Emit the signal that we need to load data
        EventHub::emit(
            Event::LOAD_DATABASE_RANGE, 
            DatabaseDate_Range{dateFromStr, dateToStr, modeStr}
        );
    });

    ui->on_loadLiveData([this]() 
    {
        //No need for event hub
        this->showLiveData();
    });

    ui->on_tabViewClicked([this]() 
    {
        this->tabViewClicked();
    });

    ui->on_siteViewClicked([this]() 
    {
        this->siteViewClicked();
    });

    ui->on_rawViewClicked([this]() 
    {
        this->rawViewClicked();
    });

    ui->on_filterViewClicked([this]() 
    {
        this->filterViewClicked();
    });

    if (extManager)
    {
        ui->on_refreshExtensions([this]() 
        {
            std::thread([this]() {
                extManager->refresh();
            }).detach();
        });
        ui->on_disableExtension([this](slint::SharedString author, slint::SharedString name) 
        {
            std::thread([this, auth = std::string(author), nm = std::string(name)]() {
                extManager->unloadExtension(auth, nm);
            }).detach();
        });
        ui->on_reloadExtension([this](slint::SharedString author, slint::SharedString name) 
        {
            std::thread([this, auth = std::string(author), nm = std::string(name)]() {
                extManager->reloadExtension(auth, nm);
            }).detach();
        });
    }

    //no need to do shit in compiled mode

    // if (interpreter)
    // {
    //     ui->on_reloadUi([this]()
    //     {   
    //         slint::invoke_from_event_loop([this]() 
    //         {
    //             this->interpreter->reload();
    //         });
    //     });
    // }

    ui->on_setLimit([](slint::SharedString appName, int minutes) 
    {
        LimitsManager::setLimit(std::string(appName), minutes);
    });

    ui->on_setGoal([](slint::SharedString appName, int minutes) 
    {
        LimitsManager::setGoal(std::string(appName), minutes);
    });

    ui->on_setConfig([](slint::SharedString paramName, slint::SharedString value) 
    {
        std::string param = std::string(paramName);
        std::string val   = std::string(value);
        AppState::configManager.setConfig(param, val);
        if (param == "anonymous-telemetry" && val == "true") {
            std::thread([]() { TelemetryManager::checkAndSend(); }).detach();
        }
    });

    ui->on_loadInsights([this]()
    {
        int days = AppState::configManager.getConfig("pattern-days", 7);
        std::thread([days]() {
            EventHub::emit(Event::LOAD_PATTERNS_DATA, PatternDataRequest{days});
        }).detach();
    });


    ui->on_openKofi([this]()

    {
        //Open webbrowser
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", "https://ko-fi.com/plexescor", nullptr, nullptr, SW_SHOWNORMAL);
        #else
            int lol = system("xdg-open https://ko-fi.com/plexescor &");
        #endif
    });

    ui->on_openReleases([this]()
    {
        //Open webbrowser
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", "https://github.com/plexescor/HPR/releases", nullptr, nullptr, SW_SHOWNORMAL);
        #else
            int lol = system("xdg-open https://github.com/plexescor/HPR/releases &");
        #endif
    });

    ui->on_openIssues([this]()
    {
        //Open webbrowser
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", "https://github.com/plexescor/HPR/issues", nullptr, nullptr, SW_SHOWNORMAL);
        #else
            int lol = system("xdg-open https://github.com/plexescor/HPR/issues &");
        #endif
    });
    ui->on_start_drag([]() {
        #ifdef _WIN32
            HWND hwnd = FindWindowW(nullptr, L"HPR");
            if (hwnd) {
                ReleaseCapture();
                SendMessage(hwnd, WM_SYSCOMMAND, 0xF012, 0);
                PostMessage(hwnd, WM_LBUTTONUP, 0, 0);
            }
        #endif
    });

    slint::ComponentWeakHandle<MainWindow> weak_ui(ui);
    ui->on_minimize_window([weak_ui]() {
        if (auto handle = weak_ui.lock()) {
            (*handle)->hide();
        }
    });

    ui->on_close_window([weak_ui]() {
        if (auto handle = weak_ui.lock()) {
            (*handle)->hide();
        }
    });

    ui->on_miscKeyPressed_S([this](slint::SharedString key) {
        EventHub::emit("MISC_KEY_PRESSED", CppValue(CppValue::Type::String, std::string(key)));
    });

    ui->on_miscKeyReleased_S([this](slint::SharedString key) {
        EventHub::emit("MISC_KEY_RELEASED", CppValue(CppValue::Type::String, std::string(key)));
    });

    // ui->on_themeSelected([this](slint::SharedString themeName)
    // {
    //     themeName = std::string(themeName);
    //     std::string path;
    //     path = AppState::themeManager.getPathByName(opt_name.value());
    //     this->interpreter->reload();
    // });

    // ui->on_themeApply([this](slint::SharedString themeName)
    // {
    //     themeName = std::string(themeName);
    //     std::string path;
    //     path = AppState::themeManager.getPathByName(opt_name.value());
    //     this->interpreter->reload();
    // });

    ui->on_openUrl([](slint::SharedString url) {
        std::string urlStr = std::string(url);
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", urlStr.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        #else
            std::string cmd = "xdg-open " + urlStr + " &";
            int idc = system(cmd.c_str());
        #endif
    });

    ui->on_sendFeedback([weak_ui](slint::SharedString email, slint::SharedString message) {
        std::thread([weak_ui, emailStr = std::string(email), msgStr = std::string(message)]() {
            auto now = std::chrono::system_clock::now();
            uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

#ifdef _WIN32
            std::string osStr = "Windows";
            std::string compStr = "";
#else
            std::string osStr = "Linux";
            const char* xdg = std::getenv("XDG_CURRENT_DESKTOP");
            std::string compStr = xdg ? xdg : "Unknown";
#endif

            std::string body = "{\"email\":\"" + jsonEscape(emailStr) + 
                               "\",\"message\":\"" + jsonEscape(msgStr) + 
                               "\",\"timestamp\":" + std::to_string(ts) + 
                               ",\"version\":\"" + jsonEscape(AppState::APP_VERSION) + 
                               "\",\"os\":\"" + jsonEscape(osStr) + "\"";
            if (!compStr.empty()) {
                body += ",\"compositor\":\"" + jsonEscape(compStr) + "\"";
            }
            body += "}";

            std::map<std::string, std::string> headers = {
                {"Content-Type", "application/json"}
            };

            auto response = NativeNet::httpPost(FIREBASE_HOST, "/feedback.json", body, true, headers);
            bool success = response.second >= 200 && response.second < 300;
            std::string errMsg = success ? "" : "HTTP error " + std::to_string(response.second);

            slint::invoke_from_event_loop([weak_ui, success, errMsg]() {
                if (auto handle = weak_ui.lock()) {
                    (*handle)->set_is_sending_feedback(false);
                    (*handle)->set_feedbackSuccess(success);
                    (*handle)->set_feedbackErrorMsg(slint::SharedString(errMsg));
                    (*handle)->set_showFeedbackResultPrompt(true);
                    if (success) {
                        (*handle)->set_feedbackMessage("");
                    }
                }
            });
        }).detach();
    });
}

//CONSTRUCTOR FOR **INTERPRETED** UI
UiEventBridge::UiEventBridge(
    slint::ComponentHandle<slint::interpreter::ComponentInstance>& ui,  ExtensionManager* extMgr, HPRInterpreter* interpreter)
{
    if (extMgr)
        this->extManager = extMgr;

    if (interpreter)
        this->interpreter = interpreter;

    //Connect to event hub
    init();

    ui->set_callback("loadHistoricalData_Singular",
        [](auto args) -> slint::interpreter::Value 
        {
            // args is std::span<const slint::interpreter::Value>
            if (args.size() > 0) 
            {
                // Safely convert the interpreter value to an optional SharedString
                auto opt_str = args[0].to_string();
                
                if (opt_str.has_value()) 
                {
                    // Dereference the optional to get the SharedString, then cast to std::string
                    std::string requestedDate = std::string(opt_str.value());

                    EventHub::emit(
                        Event::LOAD_DATABASE_SINGULAR,
                        DatabaseDate_Singular{requestedDate}
                    );
                }
            }
            return slint::interpreter::Value(); // void return
        });

    ui->set_callback("loadHistoricalData_Number", [](auto args) -> slint::interpreter::Value 
    {
        if (args.size() > 1) 
        {
            auto opt_days = args[0].to_number();
            auto opt_mode = args[1].to_string();
            if (opt_days.has_value() && opt_mode.has_value()) 
            {
                int days = static_cast<int>(opt_days.value());
                std::string modeStr = std::string(opt_mode.value());

                EventHub::emit(
                    Event::LOAD_DATABASE_NUMBER,
                    DatabaseDate_Number{days, modeStr}
                );
            }
        }
        return slint::interpreter::Value(); // void return
    });

    ui->set_callback("loadHistoricalData_Range", [](auto args) -> slint::interpreter::Value 
    {
        if (args.size() > 2) 
        {
            auto opt_dateFrom = args[0].to_string();
            auto opt_dateTo = args[1].to_string();
            auto opt_mode = args[2].to_string();
            if (opt_dateFrom.has_value() && opt_dateTo.has_value() && opt_mode.has_value()) 
            {
                std::string dateFromStr = std::string(opt_dateFrom.value());
                std::string dateToStr = std::string(opt_dateTo.value());
                std::string modeStr = std::string(opt_mode.value());

                EventHub::emit(
                    Event::LOAD_DATABASE_RANGE,
                    DatabaseDate_Range{dateFromStr, dateToStr, modeStr}
                );
            }
        }
        return slint::interpreter::Value(); // void return
    });

    ui->set_callback("loadLiveData", [this](auto args) -> slint::interpreter::Value
    {
        //No need for event hub
        this->showLiveData();
        return slint::interpreter::Value(); // void return
    });

    ui->set_callback("tabViewClicked", [this](auto args) -> slint::interpreter::Value
    {
        //No need for event hub
        this->tabViewClicked();
        return slint::interpreter::Value(); // void return
    });

    ui->set_callback("siteViewClicked", [this](auto args) -> slint::interpreter::Value
    {
        //No need for event hub
        this->siteViewClicked();
        return slint::interpreter::Value(); // void return
    });

    ui->set_callback("rawViewClicked", [this](auto args) -> slint::interpreter::Value
    {
        //No need for event hub
        this->rawViewClicked();
        return slint::interpreter::Value(); // void return
    });

    ui->set_callback("filterViewClicked", [this](auto args) -> slint::interpreter::Value
    {
        //No need for event hub
        this->filterViewClicked();
        return slint::interpreter::Value(); // void return
    });

    if (interpreter)
    {
        ui->set_callback("reloadUi", [this](auto args) -> slint::interpreter::Value
        {   
            slint::invoke_from_event_loop([this]() 
            {
                std::string savedTheme = AppState::configManager.getConfig("custom-theme", std::string("default"));
                std::string path = "";
                if (savedTheme != "default" && AppState::themeManager.availableThemes_Bare.contains(savedTheme))
                {
                    path = AppState::themeManager.availableThemes_Bare[savedTheme];
                }
                this->interpreter->reload(path);
            });
            return slint::interpreter::Value(); // void return
        });
    }

    if (extManager)
    {
        ui->set_callback("refreshExtensions", [this](auto args) -> slint::interpreter::Value
        {
            std::thread([this]() {
                extManager->refresh();
            }).detach();
            return slint::interpreter::Value();
        });

        ui->set_callback("disableExtension", [this](auto args) -> slint::interpreter::Value
        {
            if (args.size() > 1)
            {
                auto opt_author = args[0].to_string();
                auto opt_name = args[1].to_string();
                if (opt_author.has_value() && opt_name.has_value())
                {
                    std::thread([this, auth = std::string(opt_author.value()), nm = std::string(opt_name.value())]() {
                        extManager->unloadExtension(auth, nm);
                    }).detach();
                }
            }
            return slint::interpreter::Value();
        });

        ui->set_callback("reloadExtension", [this](auto args) -> slint::interpreter::Value
        {
            if (args.size() > 1)
            {
                auto opt_author = args[0].to_string();
                auto opt_name = args[1].to_string();
                if (opt_author.has_value() && opt_name.has_value())
                {
                    std::thread([this, auth = std::string(opt_author.value()), nm = std::string(opt_name.value())]() {
                        extManager->reloadExtension(auth, nm);
                    }).detach();
                }
            }
            return slint::interpreter::Value();
        });
    }

    ui->set_callback("setLimit", [](auto args) -> slint::interpreter::Value 
    {
        if (args.size() > 1) 
        {
            auto opt_name = args[0].to_string();
            auto opt_mins = args[1].to_number();
            if (opt_name.has_value() && opt_mins.has_value()) 
            {
                LimitsManager::setLimit(std::string(opt_name.value()), (int)opt_mins.value());
            }
        }
        return slint::interpreter::Value();
    });

    ui->set_callback("setGoal", [](auto args) -> slint::interpreter::Value 
    {
        if (args.size() > 1) 
        {
            auto opt_name = args[0].to_string();
            auto opt_mins = args[1].to_number();
            if (opt_name.has_value() && opt_mins.has_value()) 
            {
                LimitsManager::setGoal(std::string(opt_name.value()), (int)opt_mins.value());
            }
        }
        return slint::interpreter::Value();
    });

    ui->set_callback("setConfig", [](auto args) -> slint::interpreter::Value 
    {
        if (args.size() > 1) 
        {
            auto opt_name = args[0].to_string();
            auto opt_val = args[1].to_string();
            if (opt_name.has_value() && opt_val.has_value()) 
            {
                std::string param = std::string(opt_name.value());
                std::string val   = std::string(opt_val.value());
                AppState::configManager.setConfig(param, val);
                if (param == "anonymous-telemetry" && val == "true") {
                    std::thread([]() { TelemetryManager::checkAndSend(); }).detach();
                }
            }
        }
        return slint::interpreter::Value();
    });

    ui->set_callback("loadInsights", [this](auto args) -> slint::interpreter::Value
    {
        int days = AppState::configManager.getConfig("pattern-days", 7);
        std::thread([days]() {
            EventHub::emit(Event::LOAD_PATTERNS_DATA, PatternDataRequest{days});
        }).detach();
        return slint::interpreter::Value();
    });

    ui->set_callback("refreshThemes", [this](auto args) -> slint::interpreter::Value 
    {
        AppState::themeManager.reload();
        return slint::interpreter::Value();
    });

    ui->set_callback("themeSelected", [this](auto args) -> slint::interpreter::Value 
    {
        if (args.size() > 0) 
        {
            auto opt_name = args[0].to_string();
            if (opt_name.has_value() && this->interpreter) 
            {
                std::string themeName = std::string(opt_name.value());
                if (auto mm = this->interpreter->getModelManager())
                {
                    mm->setSelectedTheme(themeName);
                }
            }
        }
        return slint::interpreter::Value();
    });

    ui->set_callback("themeApply", [this](auto args) -> slint::interpreter::Value 
    {
        if (args.size() > 0) 
        {
            auto opt_name = args[0].to_string();
            if (opt_name.has_value() && this->interpreter) 
            {
                std::string themeName = std::string(opt_name.value());
                std::string themePath = "";
                for (const auto& [key, path] : AppState::themeManager.availableThemes)
                {
                    if (key.first == themeName)
                    {
                        themePath = path;
                        break;
                    }
                }
                if (!themePath.empty())
                {
                    this->interpreter->reload(themePath);
                    AppState::configManager.setConfig("custom-theme", themeName);
                }
                else if (themeName == "default")
                {
                    this->interpreter->reload("");
                    AppState::configManager.setConfig("custom-theme", std::string("default"));
                }
            }
        }
        return slint::interpreter::Value();
    });

    ui->set_callback("openKofi", [this](auto args) -> slint::interpreter::Value

    {
        //open browser
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", "https://ko-fi.com/plexescor", nullptr, nullptr, SW_SHOWNORMAL);
        #else
            int idc = system("xdg-open https://ko-fi.com/plexescor &");
        #endif
        return slint::interpreter::Value(); // void return
    });

    ui->set_callback("openReleases", [this](auto args) -> slint::interpreter::Value
    {
        //open browser
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", "https://github.com/plexescor/HPR/releases", nullptr, nullptr, SW_SHOWNORMAL);
        #else
            int idc = system("xdg-open https://github.com/plexescor/HPR/releases &");
        #endif
        return slint::interpreter::Value(); // void return
    });

    ui->set_callback("openIssues", [this](auto args) -> slint::interpreter::Value
    {
        //open browser
        #ifdef _WIN32
            ShellExecuteA(nullptr, "open", "https://github.com/plexescor/HPR/issues", nullptr, nullptr, SW_SHOWNORMAL);
        #else
            int idc = system("xdg-open https://github.com/plexescor/HPR/issues &");
        #endif
        return slint::interpreter::Value(); // void return
    });
    ui->set_callback("start_drag", [](auto args) -> slint::interpreter::Value {
        #ifdef _WIN32
            HWND hwnd = FindWindowW(nullptr, L"HPR");
            if (hwnd) {
                ReleaseCapture();
                SendMessage(hwnd, WM_SYSCOMMAND, 0xF012, 0);
                PostMessage(hwnd, WM_LBUTTONUP, 0, 0);
            }
        #endif
        return slint::interpreter::Value();
    });

    slint::ComponentWeakHandle<slint::interpreter::ComponentInstance> weak_ui(ui);
    ui->set_callback("minimize_window", [weak_ui](auto args) -> slint::interpreter::Value {
        if (auto handle = weak_ui.lock()) {
            (*handle)->hide();
        }
        return slint::interpreter::Value();
    });

    ui->set_callback("close_window", [weak_ui](auto args) -> slint::interpreter::Value {
        if (auto handle = weak_ui.lock()) {
            (*handle)->hide();
        }
        return slint::interpreter::Value();
    });

    ui->set_callback("miscKeyPressed_S", [](auto args) -> slint::interpreter::Value {
        if (args.size() > 0) {
            auto opt_key = args[0].to_string();
            if (opt_key.has_value()) {
                EventHub::emit("MISC_KEY_PRESSED", CppValue(CppValue::Type::String, std::string(opt_key.value())));
            }
        }
        return slint::interpreter::Value();
    });

    ui->set_callback("miscKeyReleased_S", [](auto args) -> slint::interpreter::Value {
        if (args.size() > 0) {
            auto opt_key = args[0].to_string();
            if (opt_key.has_value()) {
                EventHub::emit("MISC_KEY_RELEASED", CppValue(CppValue::Type::String, std::string(opt_key.value())));
            }
        }
        return slint::interpreter::Value();
    });

    ui->set_callback("openUrl", [](auto args) -> slint::interpreter::Value {
        if (args.size() > 0) {
            auto opt_url = args[0].to_string();
            if (opt_url.has_value()) {
                std::string urlStr = std::string(opt_url.value());
                #ifdef _WIN32
                    ShellExecuteA(nullptr, "open", urlStr.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                #else
                    std::string cmd = "xdg-open " + urlStr + " &";
                    int idc = system(cmd.c_str());
                #endif
            }
        }
        return slint::interpreter::Value();
    });

    ui->set_callback("sendFeedback", [weak_ui](auto args) -> slint::interpreter::Value {
        if (args.size() > 1) {
            auto opt_email = args[0].to_string();
            auto opt_msg = args[1].to_string();
            if (opt_email.has_value() && opt_msg.has_value()) {
                std::string email = std::string(*opt_email);
                std::string msg = std::string(*opt_msg);

                std::thread([weak_ui, email, msg]() {
                    auto now = std::chrono::system_clock::now();
                    uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

#ifdef _WIN32
                    std::string osStr = "Windows";
                    std::string compStr = "";
#else
                    std::string osStr = "Linux";
                    const char* xdg = std::getenv("XDG_CURRENT_DESKTOP");
                    std::string compStr = xdg ? xdg : "Unknown";
#endif

                    std::string body = "{\"email\":\"" + jsonEscape(email) + 
                                       "\",\"message\":\"" + jsonEscape(msg) + 
                                       "\",\"timestamp\":" + std::to_string(ts) + 
                                       ",\"version\":\"" + jsonEscape(AppState::APP_VERSION) + 
                                       "\",\"os\":\"" + jsonEscape(osStr) + "\"";
                    if (!compStr.empty()) {
                        body += ",\"compositor\":\"" + jsonEscape(compStr) + "\"";
                    }
                    body += "}";

                    std::map<std::string, std::string> headers = {
                        {"Content-Type", "application/json"}
                    };

                    auto response = NativeNet::httpPost(FIREBASE_HOST, "/feedback.json", body, true, headers);
                    bool success = response.second >= 200 && response.second < 300;
                    std::string errMsg = success ? "" : "HTTP status " + std::to_string(response.second);

                    slint::invoke_from_event_loop([weak_ui, success, errMsg]() {
                        if (auto handle = weak_ui.lock()) {
                            (*handle)->set_property("is-sending-feedback", slint::interpreter::Value(false));
                            (*handle)->set_property("feedbackSuccess", slint::interpreter::Value(success));
                            (*handle)->set_property("feedbackErrorMsg", slint::interpreter::Value(slint::SharedString(errMsg)));
                            (*handle)->set_property("showFeedbackResultPrompt", slint::interpreter::Value(true));
                            if (success) {
                                (*handle)->set_property("feedbackMessage", slint::interpreter::Value(slint::SharedString("")));
                            }
                        }
                    });
                }).detach();
            }
        }
        return slint::interpreter::Value();
    });
}


UiEventBridge::~UiEventBridge()
{
    EventHub::disconnect(Event::HISTORY_LOADED_SINGULAR, loadDbSingularId);
    EventHub::disconnect(Event::HISTORY_LOADED_NUMBER, loadDbNumberId);
    EventHub::disconnect(Event::HISTORY_LOADED_RANGE, loadDbRangeId);
}

void UiEventBridge::init() {
    // -----------------------Connecting to the Event Hub ----------------------------------------
    loadDbSingularId = EventHub::connect(Event::HISTORY_LOADED_SINGULAR, [this](EventData data)
    {
        this->showHistoricalDataSingular();
    });

    loadDbNumberId = EventHub::connect(Event::HISTORY_LOADED_NUMBER, [this](EventData data)
    {
        this->showHistoricalDataNumber();
    });

    loadDbRangeId = EventHub::connect(Event::HISTORY_LOADED_RANGE, [this](EventData data)
    {
        this->showHistoricalDataRange();
    });
}

void UiEventBridge::showHistoricalDataSingular()
{
    //Make the current app state historical
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.currentView = AppState::CurrentView::HISTORICAL_SINGULAR;
}

void UiEventBridge::showHistoricalDataNumber()
{
    //Make the current app state historical
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.currentView = AppState::CurrentView::HISTORICAL_NUMBER;
}

void UiEventBridge::showHistoricalDataRange()
{
    //Make the current app state historical
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.currentView = AppState::CurrentView::HISTORICAL_RANGE;
}

void UiEventBridge::showLiveData()
{
    EventHub::emit(Event::LOAD_LIVE_DATA); // Tell everyone we need live data, so they can prepare it before we switch the view
    //Make the current app state live
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.currentView = AppState::CurrentView::LIVE;
}

void UiEventBridge::tabViewClicked()
{
    //Make the current app state live
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.useTabView = true;
}

void UiEventBridge::siteViewClicked()
{
    //Make the current app state live
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.useTabView = false;
}

void UiEventBridge::filterViewClicked()
{
    //Make the current app state live
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.isRawProjectView = false;
}

void UiEventBridge::rawViewClicked()
{
    //Make the current app state live
    std::lock_guard<std::mutex> lock(AppState::stateMutex);
    AppState::state.isRawProjectView = true;
}