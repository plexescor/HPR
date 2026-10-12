#include "extensionManager.hpp"
#include "databaseManager.hpp"
#include "timeUtils.hpp"
#include "appState.hpp"
#include "logger.hpp"

#include <string>
#include <filesystem>
#include <vector>
#include <sqlite3.h>

// Runs INSERT, UPDATE, DELETE, CREATE TABLE, etc.
void DatabaseManager::executeSQL(const std::string &sql, const std::vector<std::string> &params)
{
	if (AppState::extManager)
	{
		std::vector<CppValue> cppParams;
		for (const auto &p : params)
		{
			cppParams.push_back(CppValue(CppValue::Type::String, p));
		}
		CppValue paramsVal(CppValue::Type::Array);
		paramsVal.array_val = cppParams;

		auto res =
			AppState::extManager->dispatchOverride("dbExecute", {CppValue(CppValue::Type::String, sql), paramsVal});
		if (res.has_value())
		{
			return;
		}
	}
	std::lock_guard<std::mutex> lock(pendingQueriesMutex);
	pendingQueries.push_back({sql, params});
}

ParallelQueryResult DatabaseManager::dbQueryNumber(int days, const std::string &mode, const std::string &sql,
												   const std::vector<std::string> &params)
{
	ParallelQueryResult out;
	std::mutex localMutex; // local to this call only
	std::vector<std::map<std::string, std::string>> rawRows;

	uint64_t timeStampMs =
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
			.count();
	std::string activeFileName = this->fileName;

	std::filesystem::path path;
#ifdef _WIN32
	path = std::getenv("APPDATA");
	path /= std::filesystem::path("HPR/HPR_DB/");
#else
	const char *home = std::getenv("HOME");
	if (!home)
		throw std::runtime_error("HOME env var not set");
	path = home;
	path /= std::filesystem::path(".local/share/HPR/HPR_DB/");
#endif

	std::vector<std::string> fileNames;
	fileNames.reserve(days);
	for (int i = 0; i < days; ++i)
	{
		fileNames.push_back(convertToDate_DDMMYY(timeStampMs));
		timeStampMs = (timeStampMs >= 86400000) ? (timeStampMs - 86400000) : 0;
	}

	std::vector<std::thread> workers;
	workers.reserve(days);

	for (int i = 0; i < days; ++i)
	{
		workers.emplace_back(
			[this, i, &fileNames, &path, activeFileName, &sql, &params, &localMutex, &rawRows]()
			{
				try
				{
					std::vector<std::map<std::string, std::string>> dayRows;

					if (fileNames[i] + ".db" == activeFileName)
					{
						std::lock_guard<std::mutex> lock(dbQueryMutex);
						if (db)
						{
							sqlite3 *rawDb = db->connection().get();
							sqlite3_stmt *stmt = nullptr;
							if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
							{
								for (size_t p = 0; p < params.size(); ++p)
									sqlite3_bind_text(stmt, p + 1, params[p].c_str(), -1, SQLITE_TRANSIENT);
								int colCount = sqlite3_column_count(stmt);
								while (sqlite3_step(stmt) == SQLITE_ROW)
								{
									std::map<std::string, std::string> row;
									for (int c = 0; c < colCount; ++c)
									{
										const char *colName = sqlite3_column_name(stmt, c);
										const unsigned char *colVal = sqlite3_column_text(stmt, c);
										row[colName] = colVal ? reinterpret_cast<const char *>(colVal) : "";
									}
									dayRows.push_back(row);
								}
								sqlite3_finalize(stmt);
							}
						}
					}
					else
					{
						std::filesystem::path fullPath =
							path / extractMMYY_from_DDMMYY(fileNames[i]) / (fileNames[i] + ".db");
						if (!std::filesystem::exists(fullPath))
						{
							std::cerr << "Historical file not found: " << fullPath << std::endl;
							Logger::log("[HPR] Historical file not found: " + fullPath.string());
							return;
						}

						sqlite::database dayDb(fullPath.string());
						sqlite3 *rawDb = dayDb.connection().get();
						sqlite3_stmt *stmt = nullptr;
						if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
						{
							for (size_t p = 0; p < params.size(); ++p)
								sqlite3_bind_text(stmt, p + 1, params[p].c_str(), -1, SQLITE_TRANSIENT);
							int colCount = sqlite3_column_count(stmt);
							while (sqlite3_step(stmt) == SQLITE_ROW)
							{
								std::map<std::string, std::string> row;
								for (int c = 0; c < colCount; ++c)
								{
									const char *colName = sqlite3_column_name(stmt, c);
									const unsigned char *colVal = sqlite3_column_text(stmt, c);
									row[colName] = colVal ? reinterpret_cast<const char *>(colVal) : "";
								}
								dayRows.push_back(row);
							}
							sqlite3_finalize(stmt);
						}
						else
						{
							std::cerr << "[DB QUERY PREPARE ERROR] " << sqlite3_errmsg(rawDb) << " | SQL: " << sql
									  << std::endl;
							Logger::log("[DB QUERY PREPARE ERROR] " + std::string(sqlite3_errmsg(rawDb)) +
										" | SQL: " + sql);
						}
					}

					std::lock_guard<std::mutex> lock(localMutex);
					rawRows.insert(rawRows.end(), dayRows.begin(), dayRows.end());
				}
				catch (const std::exception &e)
				{
					std::cerr << "[dbQueryNumber worker] " << e.what() << std::endl;
					Logger::log("[dbQueryNumber worker] " + std::string(e.what()));
				}
			});
	}

	for (auto &w : workers)
		if (w.joinable())
			w.join();

	out.rows = mergeQueryResults(rawRows, mode);
	out.ok = true;
	return out;
}

ParallelQueryResult DatabaseManager::dbQueryRange(std::string dateFrom, std::string dateTo, const std::string &mode,
												  const std::string &sql, const std::vector<std::string> &params)
{
	ParallelQueryResult out;

	uint64_t timeStampFrom = parseDate_DDMMYY(dateFrom);
	uint64_t timeStampTo = parseDate_DDMMYY(dateTo);

	if (timeStampFrom == 0 || timeStampTo == 0)
	{
		std::cerr << "Invalid date format! Dates should be in DDMMYY format." << std::endl;
		Logger::log("[HPR] Invalid date format! Dates should be in DDMMYY format.");
		return out; // out.ok stays false
	}

	// Auto-swap if dateFrom is chronologically after dateTo
	if (timeStampFrom > timeStampTo)
	{
		std::swap(timeStampFrom, timeStampTo);
	}

	int days = static_cast<int>((timeStampTo - timeStampFrom) / 86400000) + 1; // +1 to include both endpoints

	std::string activeFileName = this->fileName;
	std::mutex localMutex; // local to this call only — never touches AppState
	std::vector<std::map<std::string, std::string>> rawRows;

	std::filesystem::path path;
#ifdef _WIN32
	path = std::getenv("APPDATA");
	path /= std::filesystem::path("HPR/HPR_DB/");
#else
	const char *home = std::getenv("HOME");
	if (!home)
		throw std::runtime_error("HOME env var not set");
	path = home;
	path /= std::filesystem::path(".local/share/HPR/HPR_DB/");
#endif

	// Walk backward from timeStampTo, same seed convention as loadDb_Range
	std::vector<std::string> fileNames;
	fileNames.reserve(days);
	uint64_t walkTs = timeStampTo;
	for (int i = 0; i < days; ++i)
	{
		fileNames.push_back(convertToDate_DDMMYY(walkTs));
		walkTs = (walkTs >= 86400000) ? (walkTs - 86400000) : 0;
	}

	std::vector<std::thread> workers;
	workers.reserve(days);

	for (int i = 0; i < days; ++i)
	{
		workers.emplace_back(
			[this, i, &fileNames, &path, activeFileName, &sql, &params, &localMutex, &rawRows]()
			{
				try
				{
					std::vector<std::map<std::string, std::string>> dayRows;

					if (fileNames[i] + ".db" == activeFileName)
					{
						// Today ,query the live connection under dbQueryMutex
						std::lock_guard<std::mutex> lock(dbQueryMutex);
						if (db)
						{
							sqlite3 *rawDb = db->connection().get();
							sqlite3_stmt *stmt = nullptr;
							if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
							{
								for (size_t p = 0; p < params.size(); ++p)
									sqlite3_bind_text(stmt, p + 1, params[p].c_str(), -1, SQLITE_TRANSIENT);

								int colCount = sqlite3_column_count(stmt);
								while (sqlite3_step(stmt) == SQLITE_ROW)
								{
									std::map<std::string, std::string> row;
									for (int c = 0; c < colCount; ++c)
									{
										const char *colName = sqlite3_column_name(stmt, c);
										const unsigned char *colVal = sqlite3_column_text(stmt, c);
										row[colName] = colVal ? reinterpret_cast<const char *>(colVal) : "";
									}
									dayRows.push_back(row);
								}
								sqlite3_finalize(stmt);
							}
							else
							{
								std::cerr << "[DB QUERY PREPARE ERROR] " << sqlite3_errmsg(rawDb) << " | SQL: " << sql
										  << std::endl;
								Logger::log("[DB QUERY PREPARE ERROR] " + std::string(sqlite3_errmsg(rawDb)) +
											" | SQL: " + sql);
							}
						}
					}
					else
					{
						// Past day — open the on-disk daily db file directly
						std::filesystem::path fullPath =
							path / extractMMYY_from_DDMMYY(fileNames[i]) / (fileNames[i] + ".db");

						if (!std::filesystem::exists(fullPath))
						{
							std::cerr << "Historical file not found: " << fullPath << std::endl;
							Logger::log("[HPR] Historical file not found: " + fullPath.string());
							return;
						}

						sqlite::database dayDb(fullPath.string());
						sqlite3 *rawDb = dayDb.connection().get();
						sqlite3_stmt *stmt = nullptr;
						if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
						{
							for (size_t p = 0; p < params.size(); ++p)
								sqlite3_bind_text(stmt, p + 1, params[p].c_str(), -1, SQLITE_TRANSIENT);

							int colCount = sqlite3_column_count(stmt);
							while (sqlite3_step(stmt) == SQLITE_ROW)
							{
								std::map<std::string, std::string> row;
								for (int c = 0; c < colCount; ++c)
								{
									const char *colName = sqlite3_column_name(stmt, c);
									const unsigned char *colVal = sqlite3_column_text(stmt, c);
									row[colName] = colVal ? reinterpret_cast<const char *>(colVal) : "";
								}
								dayRows.push_back(row);
							}
							sqlite3_finalize(stmt);
						}
						else
						{
							std::cerr << "[DB QUERY PATH ERROR] " << sqlite3_errmsg(rawDb) << " | SQL: " << sql
									  << std::endl;
							Logger::log("[DB QUERY PATH ERROR] " + std::string(sqlite3_errmsg(rawDb)) +
										" | SQL: " + sql);
						}
					}

					std::lock_guard<std::mutex> lock(localMutex);
					rawRows.insert(rawRows.end(), dayRows.begin(), dayRows.end());
				}
				catch (const std::exception &e)
				{
					std::cerr << "[dbQueryRange worker] " << e.what() << std::endl;
					Logger::log("[dbQueryRange worker] " + std::string(e.what()));
				}
			});
	}

	for (auto &w : workers)
	{
		if (w.joinable())
			w.join();
	}

	out.rows = mergeQueryResults(rawRows, mode);
	out.ok = true;
	return out;
}

std::vector<std::map<std::string, std::string>>
DatabaseManager::mergeQueryResults(const std::vector<std::map<std::string, std::string>> &rawRows,
								   const std::string &mode)
{
	if (mode == "raw" || rawRows.empty())
		return rawRows;

	// group by every non-numeric column; sum/average every numeric column
	std::map<std::vector<std::pair<std::string, std::string>>, std::map<std::string, double>> sums;
	std::map<std::vector<std::pair<std::string, std::string>>, std::map<std::string, int>> counts;
	std::map<std::vector<std::pair<std::string, std::string>>, std::map<std::string, std::string>> keyDisplay;

	for (const auto &row : rawRows)
	{
		std::vector<std::pair<std::string, std::string>> key;
		std::map<std::string, double> numericFields;
		for (const auto &[col, val] : row)
		{
			char *end = nullptr;
			double d = std::strtod(val.c_str(), &end);
			bool isNumeric = (end != val.c_str() && *end == '\0');
			if (isNumeric)
				numericFields[col] = d;
			else
				key.push_back({col, val});
		}
		for (const auto &[col, d] : numericFields)
		{
			sums[key][col] += d;
			counts[key][col] += 1;
		}
		keyDisplay[key] = row; // keep last row's string form for non-numeric columns
	}

	std::vector<std::map<std::string, std::string>> merged;
	for (const auto &[key, fields] : sums)
	{
		std::map<std::string, std::string> outRow = keyDisplay[key];
		for (const auto &[col, total] : fields)
		{
			double v = (mode == "average") ? (total / counts[key][col]) : total;
			if (v == std::floor(v))
			{
				outRow[col] = std::to_string(static_cast<int64_t>(v));
			}
			else
			{
				outRow[col] = std::to_string(v);
			}
		}
		merged.push_back(outRow);
	}
	return merged;
}

// Runs SELECT and returns dynamic columns and rows to Lua
std::vector<std::map<std::string, std::string>> DatabaseManager::querySQL(const std::string &sql,
																		  const std::vector<std::string> &params)
{
	if (AppState::extManager)
	{
		std::vector<CppValue> cppParams;
		for (const auto &p : params)
		{
			cppParams.push_back(CppValue(CppValue::Type::String, p));
		}
		CppValue paramsVal(CppValue::Type::Array);
		paramsVal.array_val = cppParams;

		auto res =
			AppState::extManager->dispatchOverride("dbQuery", {CppValue(CppValue::Type::String, sql), paramsVal});
		if (res.has_value())
		{
			std::vector<std::map<std::string, std::string>> results;
			if (res->type == CppValue::Type::Array)
			{
				for (const auto &rowVal : res->array_val)
				{
					if (rowVal.type == CppValue::Type::Struct)
					{
						std::map<std::string, std::string> row;
						for (const auto &[k, v] : rowVal.struct_val)
						{
							if (v.type == CppValue::Type::String)
							{
								row[k] = v.str_val;
							}
							else if (v.type == CppValue::Type::Double)
							{
								row[k] = std::to_string(v.double_val);
							}
							else if (v.type == CppValue::Type::Bool)
							{
								row[k] = v.bool_val ? "true" : "false";
							}
						}
						results.push_back(row);
					}
				}
			}
			return results;
		}
	}
	std::lock_guard<std::mutex> lock(dbQueryMutex);
	std::vector<std::map<std::string, std::string>> results;
	if (!db)
		return results;
	// Get the raw sqlite3 connection handle from sqlite_modern_cpp
	sqlite3 *rawDb = db->connection().get();
	sqlite3_stmt *stmt = nullptr;
	if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
	{
		std::cerr << "[DB QUERY PREPARE ERROR] " << sqlite3_errmsg(rawDb) << " | SQL: " << sql << std::endl;
		Logger::log("[DB QUERY PREPARE ERROR] " + std::string(sqlite3_errmsg(rawDb)) + " | SQL: " + sql);
		return results;
	}

	// Bind parameters dynamically
	for (size_t i = 0; i < params.size(); i++)
	{
		sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
	}

	int colCount = sqlite3_column_count(stmt);
	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		std::map<std::string, std::string> row;
		for (int i = 0; i < colCount; i++)
		{
			const char *colName = sqlite3_column_name(stmt, i);
			const unsigned char *colVal = sqlite3_column_text(stmt, i);

			row[colName] = colVal ? reinterpret_cast<const char *>(colVal) : "";
		}
		results.push_back(row);
	}
	sqlite3_finalize(stmt);
	return results;
}

std::vector<std::map<std::string, std::string>> DatabaseManager::querySQL_Path(const std::string &dbPath,
																			   const std::string &sql,
																			   const std::vector<std::string> &params)
{
	if (AppState::extManager)
	{
		std::vector<CppValue> cppParams;
		for (const auto &p : params)
		{
			cppParams.push_back(CppValue(CppValue::Type::String, p));
		}
		CppValue paramsVal(CppValue::Type::Array);
		paramsVal.array_val = cppParams;

		auto res =
			AppState::extManager->dispatchOverride("dbQueryPath", {CppValue(CppValue::Type::String, dbPath),
																   CppValue(CppValue::Type::String, sql), paramsVal});
		if (res.has_value())
		{
			std::vector<std::map<std::string, std::string>> results;
			if (res->type == CppValue::Type::Array)
			{
				for (const auto &rowVal : res->array_val)
				{
					if (rowVal.type == CppValue::Type::Struct)
					{
						std::map<std::string, std::string> row;
						for (const auto &[k, v] : rowVal.struct_val)
						{
							if (v.type == CppValue::Type::String)
							{
								row[k] = v.str_val;
							}
							else if (v.type == CppValue::Type::Double)
							{
								row[k] = std::to_string(v.double_val);
							}
							else if (v.type == CppValue::Type::Bool)
							{
								row[k] = v.bool_val ? "true" : "false";
							}
						}
						results.push_back(row);
					}
				}
			}
			return results;
		}
	}
	std::vector<std::map<std::string, std::string>> results;
	try
	{
		sqlite::database targetDb(dbPath);
		sqlite3 *rawDb = targetDb.connection().get();
		sqlite3_stmt *stmt = nullptr;
		if (sqlite3_prepare_v2(rawDb, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
		{
			std::cerr << "[DB QUERY PATH ERROR] " << sqlite3_errmsg(rawDb) << " | SQL: " << sql << std::endl;
			Logger::log("[DB QUERY PATH ERROR] " + std::string(sqlite3_errmsg(rawDb)) + " | SQL: " + sql);
			return results;
		}

		// Bind parameters dynamically
		for (size_t i = 0; i < params.size(); i++)
		{
			sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
		}

		int colCount = sqlite3_column_count(stmt);
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			std::map<std::string, std::string> row;
			for (int i = 0; i < colCount; i++)
			{
				const char *colName = sqlite3_column_name(stmt, i);
				const unsigned char *colVal = sqlite3_column_text(stmt, i);
				row[colName] = colVal ? reinterpret_cast<const char *>(colVal) : "";
			}
			results.push_back(row);
		}
		sqlite3_finalize(stmt);
	}
	catch (const std::exception &e)
	{
		std::cerr << "[DB QUERY PATH EXCEPTION] " << e.what() << " | SQL: " << sql << std::endl;
		Logger::log("[DB QUERY PATH EXCEPTION] " + std::string(e.what()) + " | SQL: " + sql);
	}
	return results;
}