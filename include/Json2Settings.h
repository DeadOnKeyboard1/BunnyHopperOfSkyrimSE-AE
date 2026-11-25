#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace Json2Settings {

namespace detail {

class ISetting {
public:
	virtual ~ISetting() = default;
	[[nodiscard]] virtual std::string_view Key() const noexcept = 0;
	[[nodiscard]] virtual bool Assign(const nlohmann::json& a_value, std::string& a_error) = 0;
	virtual void WriteTo(nlohmann::ordered_json& a_object) const = 0;
	virtual void ResetToDefault() noexcept = 0;
	[[nodiscard]] virtual bool IsResettable() const noexcept = 0;
	[[nodiscard]] virtual std::string Dump() const = 0;
};

inline auto Registry() -> std::vector<ISetting*>&
{
	static std::vector<ISetting*> entries;
	return entries;
}

[[nodiscard]] inline auto load_json_object(std::string_view a_fileName, nlohmann::ordered_json& a_outObject) -> bool
{
	std::ifstream in(std::filesystem::path(a_fileName), std::ios::binary);
	if (!in.is_open()) {
		return false;
	}

	std::ostringstream buffer;
	buffer << in.rdbuf();
	std::string raw = buffer.str();
	if (raw.starts_with("\xEF\xBB\xBF")) {
		raw.erase(0, 3);
	}
	const auto existing = nlohmann::ordered_json::parse(raw, nullptr, false, true);
	if (existing.is_discarded() || !existing.is_object()) {
		return false;
	}

	a_outObject = existing;
	return true;
}

[[nodiscard]] inline auto write_json_object(std::string_view a_fileName, const nlohmann::ordered_json& a_object)
	-> std::pair<std::string, bool>
{
	const std::filesystem::path filePath(a_fileName);
	std::error_code             ec;
	const auto                  parent = filePath.parent_path();
	if (!parent.empty()) {
		std::filesystem::create_directories(parent, ec);
		if (ec) {
			return { "Failed to create directory for settings file \"" + std::string(a_fileName) + "\": " + ec.message(), false };
		}
	}

	std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
	if (!out.is_open()) {
		return { "Failed to open settings file for writing \"" + std::string(a_fileName) + "\"", false };
	}

	out << a_object.dump(2) << '\n';
	out.flush();
	if (!out.good()) {
		return { "Failed while writing settings file \"" + std::string(a_fileName) + "\"", false };
	}

	return { "", true };
}

}  // namespace detail

template <class T>
class Setting final : public detail::ISetting {
public:
	static constexpr bool kIsAtomic = std::is_same_v<T, bool> || std::is_integral_v<T> || std::is_floating_point_v<T>;

	Setting(std::string a_key, T a_default, bool a_resettable = true) :
		_key(std::move(a_key)),
		_default(a_default),
		_value(a_default),
		_resettable(a_resettable)
	{
		detail::Registry().push_back(this);
	}

	Setting(const Setting&) = delete;
	Setting(Setting&&) = delete;
	Setting& operator=(const Setting&) = delete;
	Setting& operator=(Setting&&) = delete;

	[[nodiscard]] T Get() const noexcept
	{
		if constexpr (kIsAtomic) {
			return _value.load(std::memory_order_relaxed);
		} else {
			return _value;
		}
	}

	void Set(T a_value) noexcept(kIsAtomic)
	{
		if constexpr (kIsAtomic) {
			if constexpr (std::is_floating_point_v<T>) {
				if (!std::isfinite(a_value)) {
					return;
				}
			}
			_value.store(a_value, std::memory_order_relaxed);
		} else {
			_value = std::move(a_value);
		}
	}

	[[nodiscard]] constexpr const T& GetDefault() const noexcept { return _default; }

	void ResetToDefault() noexcept override
	{
		Set(_default);
	}

	[[nodiscard]] bool IsResettable() const noexcept override { return _resettable; }

	[[nodiscard]] T operator*() const noexcept { return Get(); }
	[[nodiscard]] operator T() const noexcept { return Get(); }
	[[nodiscard]] std::string_view Key() const noexcept override { return _key; }

	bool Assign(const nlohmann::json& a_value, std::string& a_error) override
	{
		if constexpr (std::is_same_v<T, bool>) {
			if (!a_value.is_boolean()) {
				a_error = "Setting \"" + _key + "\" expected boolean";
				return false;
			}
			Set(a_value.get<bool>());
			return true;
		} else if constexpr (std::is_integral_v<T>) {
			if (!a_value.is_number_integer()) {
				a_error = "Setting \"" + _key + "\" expected integer";
				return false;
			}
			Set(static_cast<T>(a_value.get<std::int64_t>()));
			return true;
		} else if constexpr (std::is_floating_point_v<T>) {
			if (!a_value.is_number()) {
				a_error = "Setting \"" + _key + "\" expected number";
				return false;
			}
			const auto num = static_cast<T>(a_value.get<double>());
			if (!std::isfinite(num)) {
				a_error = "Setting \"" + _key + "\" expected finite number";
				return false;
			}
			Set(num);
			return true;
		} else {
			if (!a_value.is_string()) {
				a_error = "Setting \"" + _key + "\" expected string";
				return false;
			}
			Set(a_value.get<std::string>());
			return true;
		}
	}

	void WriteTo(nlohmann::ordered_json& a_object) const override
	{
		a_object[_key] = Get();
	}

	[[nodiscard]] std::string Dump() const override
	{
		const auto val = Get();
		if constexpr (std::is_same_v<T, bool>) {
			return _key + " = " + (val ? "true" : "false");
		} else if constexpr (std::is_arithmetic_v<T>) {
			return _key + " = " + std::to_string(val);
		} else {
			return _key + " = \"" + val + "\"";
		}
	}

private:
	using StorageT = std::conditional_t<kIsAtomic, std::atomic<T>, T>;

	std::string _key;
	T           _default;
	StorageT    _value;
	bool        _resettable = true;
};

using bSetting = Setting<bool>;
using iSetting = Setting<std::int32_t>;
using fSetting = Setting<float>;
using sSetting = Setting<std::string>;

inline void reset_defaults() noexcept
{
	for (auto* setting : detail::Registry()) {
		if (setting && setting->IsResettable()) {
			setting->ResetToDefault();
		}
	}
}

[[nodiscard]] inline auto load_settings(std::string_view a_fileName, bool a_dumpParse = false)
	-> std::pair<std::string, bool>
{
	std::ifstream file(std::filesystem::path(a_fileName), std::ios::binary);
	if (!file.is_open()) {
		return { "Failed to open settings file \"" + std::string(a_fileName) + "\"", false };
	}

	std::ostringstream buffer;
	buffer << file.rdbuf();
	std::string raw = buffer.str();
	if (raw.starts_with("\xEF\xBB\xBF")) {
		raw.erase(0, 3);
	}
	const auto parsed = nlohmann::json::parse(raw, nullptr, false, true);
	if (parsed.is_discarded() || !parsed.is_object()) {
		return { "Failed to parse JSON object from \"" + std::string(a_fileName) + "\"", false };
	}

	std::ostringstream log;
	bool               ok = true;
	for (auto* setting : detail::Registry()) {
		if (!setting) {
			continue;
		}
		const auto it = parsed.find(std::string(setting->Key()));
		if (it != parsed.end()) {
			std::string error;
			if (!setting->Assign(*it, error)) {
				log << error << '\n';
				ok = false;
			}
		}
		if (a_dumpParse) {
			log << setting->Dump() << '\n';
		}
	}

	return { log.str(), ok };
}

[[nodiscard]] inline auto save_settings(std::string_view a_fileName)
	-> std::pair<std::string, bool>
{
	nlohmann::ordered_json root = nlohmann::ordered_json::object();
	(void)detail::load_json_object(a_fileName, root);

	for (const auto* setting : detail::Registry()) {
		if (setting) {
			setting->WriteTo(root);
		}
	}

	return detail::write_json_object(a_fileName, root);
}

[[nodiscard]] inline auto save_single_setting(std::string_view a_fileName, const detail::ISetting& a_setting)
	-> std::pair<std::string, bool>
{
	nlohmann::ordered_json root = nlohmann::ordered_json::object();
	if (!detail::load_json_object(a_fileName, root)) {
		for (const auto* entry : detail::Registry()) {
			if (entry) {
				entry->WriteTo(root);
			}
		}
	}
	a_setting.WriteTo(root);
	return detail::write_json_object(a_fileName, root);
}

}  // namespace Json2Settings
