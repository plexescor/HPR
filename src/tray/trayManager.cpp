#include "trayManager.hpp"
#include "appState.hpp"
#include "logger.hpp"
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include <psapi.h>
#include <windows.h>
#endif

#ifdef __linux__
#include "linuxUtilities.hpp"
#include "windowUtilities.hpp"
#include <dbus/dbus.h>
#include <unistd.h> // getpid()
#endif

TrayManager::TrayManager()
{
#ifdef _WIN32
	currentPlatform = "Windows";
#endif
#ifdef __linux__
	std::string currentPlatformCommand = "echo $XDG_CURRENT_DESKTOP";
	currentPlatform = runSystemCommand(currentPlatformCommand);
#endif
}

TrayManager::~TrayManager()
{
#ifdef _WIN32
	if (trayHwnd)
		PostMessage(trayHwnd, WM_QUIT, 0, 0);
#endif

	running = false;

	// join first, then unref — unreffing while the thread still uses the
	// connection would be a race
	if (trayThread.joinable())
		trayThread.join();

#ifdef __linux__
	if (dbusConn)
	{
		dbus_connection_close(dbusConn);
		dbus_connection_unref(dbusConn);
		dbusConn = nullptr;
	}
#endif
}

void TrayManager::run()
{
#ifdef _WIN32
	trayThread = std::thread(&TrayManager::trayManager_LoopWindows, this);
#endif
#ifdef __linux__
	trayThread = std::thread(&TrayManager::trayManager_LoopLinux, this);
#endif
}

#ifdef _WIN32

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_QUIT 1001
#define ID_TRAY_SHOW 1002

static TrayManager *g_trayInstance = nullptr;
WNDPROC TrayManager::g_originalWndProc = nullptr;

LRESULT CALLBACK TrayManager::trayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_TRAYICON:
		if (lParam == WM_RBUTTONUP)
		{
			if (g_trayInstance)
				g_trayInstance->showContextMenu(hwnd);
		}
		return 0;

	case WM_COMMAND:
		if (LOWORD(wParam) == ID_TRAY_QUIT)
		{
			if (g_trayInstance && g_trayInstance->onQuit)
				g_trayInstance->onQuit();

			PostQuitMessage(0);
		}
		else if (LOWORD(wParam) == ID_TRAY_SHOW)
		{
			if (g_trayInstance && g_trayInstance->onShow)
				g_trayInstance->onShow();
		}
		return 0;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
}

LRESULT CALLBACK TrayManager::hprSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_SIZE && wParam == SIZE_MINIMIZED)
	{
		if (g_trayInstance && g_trayInstance->onHide)
			g_trayInstance->onHide();
		return 0;
	}

	return CallWindowProcW(g_originalWndProc, hwnd, msg, wParam, lParam);
}

void TrayManager::createTrayIcon()
{
	WNDCLASSW wc = {};
	wc.lpfnWndProc = trayWndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = L"HPRTrayClass";
	RegisterClassW(&wc);

	trayHwnd =
		CreateWindowW(L"HPRTrayClass", L"HPR Tray", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);

	HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));

	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = trayHwnd;
	nid.uID = 1;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;
	nid.hIcon = hIcon;
	wcscpy_s(nid.szTip, L"HPR - Human Pattern Recorder");

	Shell_NotifyIconW(NIM_ADD, &nid);
}

void TrayManager::destroyTrayIcon()
{
	Shell_NotifyIconW(NIM_DELETE, &nid);
	if (trayHwnd)
	{
		DestroyWindow(trayHwnd);
		trayHwnd = nullptr;
	}
}

void TrayManager::showContextMenu(HWND hwnd)
{
	HMENU menu = CreatePopupMenu();
	AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW, L"Show HPR");
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING, ID_TRAY_QUIT, L"Quit");

	POINT pt;
	GetCursorPos(&pt);

	SetForegroundWindow(hwnd);
	TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
	DestroyMenu(menu);
}

void TrayManager::trayManager_LoopWindows()
{
	g_trayInstance = this;
	createTrayIcon();

	// Wait for HPR window to exist then subclass it
	HWND hprHwnd = nullptr;
	while (running && !hprHwnd)
	{
		hprHwnd = FindWindowW(nullptr, L"HPR");
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	// Store original WndProc and replace with ours
	if (hprHwnd)
	{
		g_originalWndProc = (WNDPROC)SetWindowLongPtrW(hprHwnd, GWLP_WNDPROC, (LONG_PTR)hprSubclassProc);
	}

	MSG msg;
	while (running && GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Restore original WndProc on exit
	if (hprHwnd && g_originalWndProc)
	{
		SetWindowLongPtrW(hprHwnd, GWLP_WNDPROC, (LONG_PTR)g_originalWndProc);
	}

	destroyTrayIcon();
	g_trayInstance = nullptr;
}

#endif

#ifdef __linux__

// panels call Introspect first, if you don't reply to this they just silently
// drop your item
static const char *SNI_INTROSPECTION_XML = "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection "
										   "1.0//EN\""
										   "  \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">"
										   "<node>"
										   "  <interface name=\"org.kde.StatusNotifierItem\">"
										   "    <property name=\"Category\"      type=\"s\"            "
										   "access=\"read\"/>"
										   "    <property name=\"Id\"            type=\"s\"            "
										   "access=\"read\"/>"
										   "    <property name=\"Title\"         type=\"s\"            "
										   "access=\"read\"/>"
										   "    <property name=\"Status\"        type=\"s\"            "
										   "access=\"read\"/>"
										   "    <property name=\"IconName\"      type=\"s\"            "
										   "access=\"read\"/>"
										   "    <property name=\"IconThemePath\" type=\"s\"            "
										   "access=\"read\"/>"
										   "    <property name=\"Menu\"          type=\"o\"            "
										   "access=\"read\"/>"
										   "    <property name=\"ToolTip\"       type=\"(sa(iiay)ss)\" "
										   "access=\"read\"/>"
										   "    <method name=\"Activate\"><arg type=\"i\" direction=\"in\"/><arg "
										   "type=\"i\" direction=\"in\"/></method>"
										   "    <method name=\"ContextMenu\"><arg type=\"i\" direction=\"in\"/><arg "
										   "type=\"i\" direction=\"in\"/></method>"
										   "    <method name=\"SecondaryActivate\"><arg type=\"i\" "
										   "direction=\"in\"/><arg type=\"i\" direction=\"in\"/></method>"
										   "    <method name=\"ProvideXdgActivationToken\"><arg type=\"s\" "
										   "direction=\"in\"/></method>"
										   "  </interface>"
										   "  <interface name=\"com.canonical.dbusmenu\">"
										   "    <method name=\"GetLayout\">"
										   "      <arg name=\"parentId\"       type=\"i\"          direction=\"in\"/>"
										   "      <arg name=\"recursionDepth\" type=\"i\"          direction=\"in\"/>"
										   "      <arg name=\"propertyNames\"  type=\"as\"         direction=\"in\"/>"
										   "      <arg name=\"revision\"       type=\"u\"          direction=\"out\"/>"
										   "      <arg name=\"layout\"         type=\"(ia{sv}av)\" direction=\"out\"/>"
										   "    </method>"
										   "    <method name=\"AboutToShow\">"
										   "      <arg name=\"id\"             type=\"i\" direction=\"in\"/>"
										   "      <arg name=\"needsUpdate\"    type=\"b\" direction=\"out\"/>"
										   "    </method>"
										   "    <method name=\"Event\">"
										   "      <arg name=\"id\"             type=\"i\" direction=\"in\"/>"
										   "      <arg name=\"eventId\"        type=\"s\" direction=\"in\"/>"
										   "      <arg name=\"data\"           type=\"v\" direction=\"in\"/>"
										   "      <arg name=\"timestamp\"      type=\"u\" direction=\"in\"/>"
										   "    </method>"
										   "    <signal name=\"LayoutUpdated\">"
										   "      <arg name=\"revision\"       type=\"u\"/>"
										   "      <arg name=\"parent\"         type=\"i\"/>"
										   "    </signal>"
										   "  </interface>"
										   "  <interface name=\"org.freedesktop.DBus.Properties\">"
										   "    <method name=\"Get\">"
										   "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>"
										   "      <arg name=\"property_name\"  type=\"s\" direction=\"in\"/>"
										   "      <arg name=\"value\"          type=\"v\" direction=\"out\"/>"
										   "    </method>"
										   "    <method name=\"GetAll\">"
										   "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>"
										   "      <arg name=\"props\"          type=\"a{sv}\" direction=\"out\"/>"
										   "    </method>"
										   "  </interface>"
										   "  <interface name=\"org.freedesktop.DBus.Introspectable\">"
										   "    <method name=\"Introspect\"><arg type=\"s\" "
										   "direction=\"out\"/></method>"
										   "  </interface>"
										   "</node>";

// appends a key-value string pair into a dbus dict iterator, used for GetAll
static void appendStringProp(DBusMessageIter *arr, const char *key, const char *val)
{
	DBusMessageIter entry, variant;
	dbus_message_iter_open_container(arr, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &val);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(arr, &entry);
}

// builds and sends a ToolTip reply — type is (sa(iiay)ss): icon_name,
// icon_data[], title, description panels like waybar show the title+description
// on hover
static void replyWithToolTip(DBusConnection *conn, DBusMessage *msg)
{
	DBusMessage *reply = dbus_message_new_method_return(msg);
	DBusMessageIter replyIter, variant, ttStruct, iconArr;

	dbus_message_iter_init_append(reply, &replyIter);
	dbus_message_iter_open_container(&replyIter, DBUS_TYPE_VARIANT, "(sa(iiay)ss)", &variant);
	dbus_message_iter_open_container(&variant, DBUS_TYPE_STRUCT, nullptr, &ttStruct);

	const char *ttIconName = "";
	dbus_message_iter_append_basic(&ttStruct, DBUS_TYPE_STRING, &ttIconName);

	// icon data array, leave empty
	dbus_message_iter_open_container(&ttStruct, DBUS_TYPE_ARRAY, "(iiay)", &iconArr);
	dbus_message_iter_close_container(&ttStruct, &iconArr);

	const char *ttTitle = "HPR - Human Pattern Recorder\nLeft/Right click: Open "
						  "HPR  |  Middle click: Quit";
	const char *ttBody = "Left/Right click: Open HPR  |  Middle click: Quit";
	dbus_message_iter_append_basic(&ttStruct, DBUS_TYPE_STRING, &ttTitle);
	dbus_message_iter_append_basic(&ttStruct, DBUS_TYPE_STRING, &ttBody);

	dbus_message_iter_close_container(&variant, &ttStruct);
	dbus_message_iter_close_container(&replyIter, &variant);

	dbus_connection_send(conn, reply, nullptr);
	dbus_connection_flush(conn);
	dbus_message_unref(reply);
}

// Helper: send RegisterStatusNotifierItem to the watcher.
// Called once at startup and again any time the watcher (re)appears on the bus
// (e.g. on GNOME when the AppIndicator extension finishes loading).
static void registerWithSNIWatcher(DBusConnection *conn, const std::string &serviceName)
{
	DBusMessage *msg = dbus_message_new_method_call("org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
													"org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem");
	if (msg)
	{
		const char *svcCStr = serviceName.c_str();
		dbus_message_append_args(msg, DBUS_TYPE_STRING, &svcCStr, DBUS_TYPE_INVALID);
		dbus_connection_send(conn, msg, nullptr);
		dbus_connection_flush(conn);
		dbus_message_unref(msg);
	}
}

void TrayManager::trayManager_LoopLinux()
{
	const bool isGnome = (currentPlatform.find("GNOME") != std::string::npos);

	DBusError err;
	dbus_error_init(&err);

	dbusConn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err))
	{
		std::cerr << "[TrayManager] dbus session connect failed: " << err.message << "\n";
		Logger::log("[TrayManager] dbus session connect failed: " + std::string(err.message));
		dbus_error_free(&err);
		return;
	}
	if (!dbusConn)
	{
		std::cerr << "[TrayManager] could not connect to session bus\n";
		Logger::log("[TrayManager] could not connect to session bus");
		return;
	}

	// service name has to follow org.kde.StatusNotifierItem-<PID>-<instance>,
	// thats just the convention
	std::string serviceName = "org.kde.StatusNotifierItem-" + std::to_string(getpid()) + "-1";

	int ret = dbus_bus_request_name(dbusConn, serviceName.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
	if (dbus_error_is_set(&err))
	{
		std::cerr << "[TrayManager] request_name failed: " << err.message << "\n";
		Logger::log("[TrayManager] request_name failed: " + std::string(err.message));
		dbus_error_free(&err);
		dbus_connection_unref(dbusConn);
		dbusConn = nullptr;
		return;
	}
	if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
	{
		std::cerr << "[TrayManager] could not become primary owner of SNI bus name\n";
		Logger::log("[TrayManager] could not become primary owner of SNI bus name");
		dbus_connection_unref(dbusConn);
		dbusConn = nullptr;
		return;
	}

	// Subscribe to NameOwnerChanged for org.kde.StatusNotifierWatcher so we
	// know when GNOME's AppIndicator extension (or any SNI host) comes online
	// after we have already started.  Without this the registration call below
	// is silently dropped when the watcher isn't up yet.
	dbus_bus_add_match(dbusConn,
					   "type='signal'"
					   ",sender='org.freedesktop.DBus'"
					   ",interface='org.freedesktop.DBus'"
					   ",member='NameOwnerChanged'"
					   ",arg0='org.kde.StatusNotifierWatcher'",
					   &err);
	if (dbus_error_is_set(&err))
	{
		dbus_error_free(&err);
	}
	dbus_connection_flush(dbusConn);

	// tell the watcher we exist — this is what makes the icon show up in
	// waybar/kde/cinnamon. On GNOME the watcher may not be up yet; the
	// NameOwnerChanged handler below will retry.
	registerWithSNIWatcher(dbusConn, serviceName);

	auto lastRegisterAttempt = std::chrono::steady_clock::now();
	bool registered = false;
	while (running)
	{
		// poll for 100ms so we can check running without blocking forever
		if (!dbus_connection_read_write(dbusConn, 100))
			break;

		// Retry registration every 5 seconds, but only if watcher is actually
		// on the bus (avoids duplicate icons on Cinnamon where watcher races
		// app at boot)
		auto now = std::chrono::steady_clock::now();
		if (!registered && std::chrono::duration_cast<std::chrono::seconds>(now - lastRegisterAttempt).count() >= 5)
		{
			DBusError checkErr;
			dbus_error_init(&checkErr);
			DBusMessage *checkMsg = dbus_message_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
																 "org.freedesktop.DBus", "GetNameOwner");
			const char *watcherName = "org.kde.StatusNotifierWatcher";
			dbus_message_append_args(checkMsg, DBUS_TYPE_STRING, &watcherName, DBUS_TYPE_INVALID);
			DBusMessage *checkReply = dbus_connection_send_with_reply_and_block(dbusConn, checkMsg, 200, &checkErr);
			dbus_message_unref(checkMsg);
			bool watcherUp = (checkReply != nullptr && !dbus_error_is_set(&checkErr));
			if (checkReply)
				dbus_message_unref(checkReply);
			dbus_error_free(&checkErr);

			if (watcherUp)
				registerWithSNIWatcher(dbusConn, serviceName);
			lastRegisterAttempt = now;
		}

		DBusMessage *msg = dbus_connection_pop_message(dbusConn);
		if (!msg)
			continue;

		const char *iface = dbus_message_get_interface(msg);
		const char *member = dbus_message_get_member(msg);

		if (!iface || !member)
		{
			dbus_message_unref(msg);
			continue;
		}

		if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Introspectable", "Introspect"))
		{
			DBusMessage *reply = dbus_message_new_method_return(msg);
			dbus_message_append_args(reply, DBUS_TYPE_STRING, &SNI_INTROSPECTION_XML, DBUS_TYPE_INVALID);
			dbus_connection_send(dbusConn, reply, nullptr);
			dbus_connection_flush(dbusConn);
			dbus_message_unref(reply);
		}

		else if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Get"))
		{
			const char *propName = nullptr;
			const char *ifaceName = nullptr;
			dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &ifaceName, DBUS_TYPE_STRING, &propName,
								  DBUS_TYPE_INVALID);

			// ToolTip is a struct type, handle it separately from the simple
			// string props
			if (propName && std::string(propName) == "ToolTip")
			{
				replyWithToolTip(dbusConn, msg);
			}
			// Menu is an object path (type 'o'), not a string — handle
			// separately. Only advertise on GNOME — other DEs use
			// Activate/ContextMenu for LMB/RMB.
			else if (propName && std::string(propName) == "Menu" && currentPlatform.find("GNOME") != std::string::npos)
			{
				const char *menuPath = "/";
				DBusMessage *reply = dbus_message_new_method_return(msg);
				DBusMessageIter replyIter, variant;
				dbus_message_iter_init_append(reply, &replyIter);
				dbus_message_iter_open_container(&replyIter, DBUS_TYPE_VARIANT, "o", &variant);
				dbus_message_iter_append_basic(&variant, DBUS_TYPE_OBJECT_PATH, &menuPath);
				dbus_message_iter_close_container(&replyIter, &variant);
				dbus_connection_send(dbusConn, reply, nullptr);
				dbus_connection_flush(dbusConn);
				dbus_message_unref(reply);
			}
			else
			{
				const std::string &themePath = LinuxInitialiser::getIconThemePath();
				const char *value = nullptr;
				if (propName && std::string(propName) == "Category")
					value = "ApplicationStatus";
				else if (propName && std::string(propName) == "Id")
					value = "HPR";
				else if (propName && std::string(propName) == "Title")
					value = "HPR - Human Pattern Recorder\nLeft/Right click: "
							"Open HPR  | "
							" Middle click: Quit";
				else if (propName && std::string(propName) == "Status")
					value = "Active";
				else if (propName && std::string(propName) == "IconName")
					value = themePath.empty() ? "application-x-executable" : "hpr";

				else if (propName && std::string(propName) == "IconThemePath")
					value = themePath.c_str();

				if (value)
				{
					DBusMessage *reply = dbus_message_new_method_return(msg);
					DBusMessageIter replyIter, variant;
					dbus_message_iter_init_append(reply, &replyIter);
					dbus_message_iter_open_container(&replyIter, DBUS_TYPE_VARIANT, "s", &variant);
					dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &value);
					dbus_message_iter_close_container(&replyIter, &variant);
					dbus_connection_send(dbusConn, reply, nullptr);
					dbus_connection_flush(dbusConn);
					dbus_message_unref(reply);
				}
				else
				{
					// Unknown property: send a proper error so the caller
					// doesn't hang/timeout
					DBusMessage *errReply = dbus_message_new_error(msg, "org.freedesktop.DBus.Error.UnknownProperty",
																   propName ? propName : "unknown");
					dbus_connection_send(dbusConn, errReply, nullptr);
					dbus_connection_flush(dbusConn);
					dbus_message_unref(errReply);
				}
			}
		}

		else if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "GetAll"))
		{
			DBusMessage *reply = dbus_message_new_method_return(msg);
			DBusMessageIter replyIter, arr;
			dbus_message_iter_init_append(reply, &replyIter);
			dbus_message_iter_open_container(&replyIter, DBUS_TYPE_ARRAY, "{sv}", &arr);

			appendStringProp(&arr, "Category", "ApplicationStatus");
			appendStringProp(&arr, "Id", "HPR");
			appendStringProp(&arr, "Title",
							 "HPR - Human Pattern Recorder\nLeft/Right click: Open "
							 "HPR  |  Middle click: Quit");
			appendStringProp(&arr, "Status", "Active");

			if (isGnome)
			{
				const std::string &themePath = LinuxInitialiser::getIconThemePath();
				const char *iconName = themePath.empty() ? "application-x-executable" : "hpr";
				const char *iconThemePath = themePath.c_str();

				appendStringProp(&arr, "IconName", iconName);
				if (!themePath.empty())
					appendStringProp(&arr, "IconThemePath", iconThemePath);

				// Menu property — tells GNOME to use our dbusmenu on RMB
				DBusMessageIter entry, variant;
				const char *menuKey = "Menu";
				const char *menuPath = "/";
				dbus_message_iter_open_container(&arr, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
				dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &menuKey);
				dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "o", &variant);
				dbus_message_iter_append_basic(&variant, DBUS_TYPE_OBJECT_PATH, &menuPath);
				dbus_message_iter_close_container(&entry, &variant);
				dbus_message_iter_close_container(&arr, &entry);
			}
			else
			{
				const std::string &themePath = LinuxInitialiser::getIconThemePath();

				appendStringProp(&arr, "IconName", themePath.empty() ? "application-x-executable" : "hpr");

				if (!themePath.empty())
					appendStringProp(&arr, "IconThemePath", themePath.c_str());
			}

			dbus_message_iter_close_container(&replyIter, &arr);
			dbus_connection_send(dbusConn, reply, nullptr);
			dbus_connection_flush(dbusConn);
			dbus_message_unref(reply);
		}

		else if (isGnome && dbus_message_is_method_call(msg, "com.canonical.dbusmenu", "GetLayout"))
		{
			DBusMessage *reply = dbus_message_new_method_return(msg);
			DBusMessageIter it, root, rootProps, children;
			dbus_message_iter_init_append(reply, &it);

			dbus_uint32_t rev = 1;
			dbus_message_iter_append_basic(&it, DBUS_TYPE_UINT32, &rev);

			dbus_message_iter_open_container(&it, DBUS_TYPE_STRUCT, nullptr, &root);
			dbus_int32_t rootId = 0;
			dbus_message_iter_append_basic(&root, DBUS_TYPE_INT32, &rootId);
			dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY, "{sv}", &rootProps);
			dbus_message_iter_close_container(&root, &rootProps);

			dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY, "v", &children);

			auto appendItem = [&](dbus_int32_t id, const char *label)
			{
				DBusMessageIter vari, item, props, gc, entry, val;
				dbus_message_iter_open_container(&children, DBUS_TYPE_VARIANT, "(ia{sv}av)", &vari);
				dbus_message_iter_open_container(&vari, DBUS_TYPE_STRUCT, nullptr, &item);
				dbus_message_iter_append_basic(&item, DBUS_TYPE_INT32, &id);

				dbus_message_iter_open_container(&item, DBUS_TYPE_ARRAY, "{sv}", &props);
				{
					const char *k = "label";
					dbus_message_iter_open_container(&props, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
					dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &k);
					dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &val);
					dbus_message_iter_append_basic(&val, DBUS_TYPE_STRING, &label);
					dbus_message_iter_close_container(&entry, &val);
					dbus_message_iter_close_container(&props, &entry);
				}
				{
					const char *k = "enabled";
					dbus_bool_t v = TRUE;
					dbus_message_iter_open_container(&props, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
					dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &k);
					dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &val);
					dbus_message_iter_append_basic(&val, DBUS_TYPE_BOOLEAN, &v);
					dbus_message_iter_close_container(&entry, &val);
					dbus_message_iter_close_container(&props, &entry);
				}
				{
					const char *k = "visible";
					dbus_bool_t v = TRUE;
					dbus_message_iter_open_container(&props, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
					dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &k);
					dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &val);
					dbus_message_iter_append_basic(&val, DBUS_TYPE_BOOLEAN, &v);
					dbus_message_iter_close_container(&entry, &val);
					dbus_message_iter_close_container(&props, &entry);
				}
				dbus_message_iter_close_container(&item, &props);
				dbus_message_iter_open_container(&item, DBUS_TYPE_ARRAY, "v", &gc);
				dbus_message_iter_close_container(&item, &gc);
				dbus_message_iter_close_container(&vari, &item);
				dbus_message_iter_close_container(&children, &vari);
			};

			appendItem(1, "Show HPR");
			appendItem(2, "Quit");

			dbus_message_iter_close_container(&root, &children);
			dbus_message_iter_close_container(&it, &root);

			dbus_connection_send(dbusConn, reply, nullptr);
			dbus_connection_flush(dbusConn);
			dbus_message_unref(reply);
		}

		else if (isGnome && dbus_message_is_method_call(msg, "com.canonical.dbusmenu", "Event"))
		{
			dbus_int32_t itemId = 0;
			DBusMessageIter eIter;
			dbus_message_iter_init(msg, &eIter);
			dbus_message_iter_get_basic(&eIter, &itemId);

			if (itemId == 1)
			{
				if (onShow)
					onShow();
			}
			if (itemId == 2)
			{
				if (onQuit)
					onQuit();
			}

			DBusMessage *reply = dbus_message_new_method_return(msg);
			dbus_connection_send(dbusConn, reply, nullptr);
			dbus_connection_flush(dbusConn);
			dbus_message_unref(reply);
		}

		else if (isGnome && dbus_message_is_method_call(msg, "com.canonical.dbusmenu", "AboutToShow"))
		{
			DBusMessage *reply = dbus_message_new_method_return(msg);
			dbus_bool_t needsUpdate = FALSE;
			dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &needsUpdate, DBUS_TYPE_INVALID);
			dbus_connection_send(dbusConn, reply, nullptr);
			dbus_connection_flush(dbusConn);
			dbus_message_unref(reply);
		}

		else if (isGnome && dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem", "ProvideXdgActivationToken"))
		{
			const char *token = nullptr;
			dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &token, DBUS_TYPE_INVALID);
			if (token && token[0] != '\0')
				setenv("XDG_ACTIVATION_TOKEN", token, 1);
			dbus_error_free(&err);
			DBusMessage *reply = dbus_message_new_method_return(msg);
			dbus_connection_send(dbusConn, reply, nullptr);
			dbus_connection_flush(dbusConn);
			dbus_message_unref(reply);
		}

		// left click = show the window
		else if (dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem", "Activate"))
		{
			registered = true;
			if (onShow)
				onShow();

			DBusMessage *reply = dbus_message_new_method_return(msg);
			dbus_connection_send(dbusConn, reply, nullptr);
			dbus_connection_flush(dbusConn);
			dbus_message_unref(reply);
		}

		// waybar sends ContextMenu for ALL clicks (left and right), Activate is
		// never fired
		else if (dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem", "ContextMenu"))
		{
			registered = true;
			if (onShow)
				onShow();

			DBusMessage *reply = dbus_message_new_method_return(msg);
			dbus_connection_send(dbusConn, reply, nullptr);
			dbus_connection_flush(dbusConn);
			dbus_message_unref(reply);
		}

		// middle click = quit
		else if (dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem", "SecondaryActivate"))
		{
			registered = true;
			if (onQuit)
				onQuit();

			DBusMessage *reply = dbus_message_new_method_return(msg);
			dbus_connection_send(dbusConn, reply, nullptr);
			dbus_connection_flush(dbusConn);
			dbus_message_unref(reply);
		}

		// re-try on every shit
		else if (dbus_message_is_signal(msg, "org.freedesktop.DBus", "NameOwnerChanged"))
		{
			const char *name = nullptr;
			const char *oldOwner = nullptr;
			const char *newOwner = nullptr;
			dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING, &oldOwner, DBUS_TYPE_STRING,
								  &newOwner, DBUS_TYPE_INVALID);
			dbus_error_free(&err);

			if (name && std::string(name) == "org.kde.StatusNotifierWatcher" && newOwner && newOwner[0] != '\0')
			{
				registered = false; // watcher restarted, re-register
				registerWithSNIWatcher(dbusConn, serviceName);
			}
		}

		dbus_message_unref(msg);
	}
}

#endif
