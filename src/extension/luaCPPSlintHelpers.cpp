#include "extensionManager.hpp"
#include "logger.hpp"

namespace LuaCPPSlint
{
	CppValue luaToCpp_Internal(const sol::object &obj, std::unordered_set<const void *> &visited)
	{
		CppValue v;
		if (obj.is<std::string>())
		{
			v.type = CppValue::Type::String;
			v.str_val = obj.as<std::string>();
		}
		else if (obj.is<double>())
		{
			v.type = CppValue::Type::Double;
			v.double_val = obj.as<double>();
		}
		else if (obj.is<bool>())
		{
			v.type = CppValue::Type::Bool;
			v.bool_val = obj.as<bool>();
		}
		else if (obj.is<sol::table>())
		{
			sol::table tab = obj.as<sol::table>();
			const void *tabPtr = tab.pointer();
			if (visited.find(tabPtr) != visited.end())
			{
				std::string extName = "Unknown Extension";
				sol::state_view lua(obj.lua_state());
				if (lua["HPR"].valid() && lua["HPR"]["extensionName"].valid())
				{
					extName = lua["HPR"]["extensionName"].get<std::string>();
				}
				std::string warningMsg = "\n[HPR SECURITY ERROR] Active Lua extension [" + extName +
										"] attempted to trigger a stack overflow / crash "
										"by pushing a cyclic self-referential table to the UI "
										"(circular "
										"reference detected at address " +
										std::to_string(reinterpret_cast<uintptr_t>(tabPtr)) +
										"). Returning empty object to prevent SegFault.\n";
				std::cerr << warningMsg;
				Logger::log(warningMsg);
				return v;
			}
			visited.insert(tabPtr);

			bool is_array = false;
			if (tab[1].valid())
			{
				is_array = true;
			}
			else
			{
				// Default empty tables to Array so they clear list models in Slint
				bool has_keys = false;
				tab.for_each([&](sol::object key, sol::object value) { has_keys = true; });
				if (!has_keys)
				{
					is_array = true;
				}
			}

			if (is_array)
			{
				v.type = CppValue::Type::Array;
				for (size_t i = 1;; ++i)
				{
					sol::object val = tab[i];
					if (!val.valid() || val.is<sol::nil_t>())
					{
						break;
					}
					v.array_val.push_back(luaToCpp_Internal(val, visited));
				}
			}
			else
			{
				v.type = CppValue::Type::Struct;
				tab.for_each(
					[&](sol::object key, sol::object value)
					{
						if (key.is<std::string>())
						{
							v.struct_val[key.as<std::string>()] = luaToCpp_Internal(value, visited);
						}
					});
			}
			visited.erase(tabPtr);
		}
		return v;
	}

	CppValue luaToCpp(const sol::object &obj)
	{
		std::unordered_set<const void *> visited;
		return luaToCpp_Internal(obj, visited);
	}

	CppValue slintToCpp(const slint::interpreter::Value &val)
	{
		CppValue v;
		if (auto s = val.to_string())
		{
			v.type = CppValue::Type::String;
			v.str_val = std::string(*s);
		}
		else if (auto n = val.to_number())
		{
			v.type = CppValue::Type::Double;
			v.double_val = *n;
		}
		else if (auto b = val.to_bool())
		{
			v.type = CppValue::Type::Bool;
			v.bool_val = *b;
		}
		else if (auto arr = val.to_array())
		{
			v.type = CppValue::Type::Array;
			for (size_t i = 0; i < arr->size(); ++i)
			{
				v.array_val.push_back(slintToCpp((*arr)[i]));
			}
		}
		else if (auto strct = val.to_struct())
		{
			v.type = CppValue::Type::Struct;
			for (const auto &[name, fieldVal] : *strct)
			{
				v.struct_val[std::string(name)] = slintToCpp(fieldVal);
			}
		}
		return v;
	}

	slint::interpreter::Value cppToSlint(const CppValue &val)
	{
		if (val.type == CppValue::Type::String)
		{
			return slint::interpreter::Value(slint::SharedString(val.str_val));
		}
		else if (val.type == CppValue::Type::Double)
		{
			return slint::interpreter::Value(val.double_val);
		}
		else if (val.type == CppValue::Type::Bool)
		{
			return slint::interpreter::Value(val.bool_val);
		}
		else if (val.type == CppValue::Type::Array)
		{
			auto model = std::make_shared<slint::VectorModel<slint::interpreter::Value>>();
			for (const auto &item : val.array_val)
			{
				model->push_back(cppToSlint(item));
			}
			return slint::interpreter::Value(std::shared_ptr<slint::Model<slint::interpreter::Value>>(model));
		}
		else if (val.type == CppValue::Type::Struct)
		{
			slint::interpreter::Struct s;
			for (const auto &[k, v] : val.struct_val)
			{
				s.set_field(k, cppToSlint(v));
			}
			return slint::interpreter::Value(s);
		}
		return slint::interpreter::Value();
	}

	sol::object cppToLua(sol::state &lua, const CppValue &val)
	{
		if (val.type == CppValue::Type::String)
		{
			return sol::make_object(lua, val.str_val);
		}
		else if (val.type == CppValue::Type::Double)
		{
			return sol::make_object(lua, val.double_val);
		}
		else if (val.type == CppValue::Type::Bool)
		{
			return sol::make_object(lua, val.bool_val);
		}
		else if (val.type == CppValue::Type::Array)
		{
			sol::table tab = lua.create_table();
			for (size_t i = 0; i < val.array_val.size(); ++i)
			{
				tab[i + 1] = cppToLua(lua, val.array_val[i]);
			}
			return tab;
		}
		else if (val.type == CppValue::Type::Struct)
		{
			sol::table tab = lua.create_table();
			for (const auto &[k, v] : val.struct_val)
			{
				tab[k] = cppToLua(lua, v);
			}
			return tab;
		}
		return sol::nil;
	}
} // namespace
