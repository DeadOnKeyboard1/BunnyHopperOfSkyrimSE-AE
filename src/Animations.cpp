#include "Animations.h"

#include <cctype>
#include <string>
#include <string_view>


Anim HashAnimation(const char* a_str, const std::uint32_t a_len)
{
	if (!a_str || a_len == 0) {
		return Anim(hash_64_fnv1a("", 0));
	}
	return Anim(hash_64_fnv1a(a_str, a_len));
}


Anim HashAnimation(const std::string_view a_str)
{
	if (a_str.empty()) {
		return HashAnimation("", 0);
	}

	std::string str;
	str.reserve(a_str.size());
	for (const char ch : a_str) {
		str += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	}
	return HashAnimation(str.c_str(), static_cast<std::uint32_t>(str.length()));
}


Anim HashAnimation(const RE::BSFixedString& a_str)
{
	if (a_str.empty()) {
		return HashAnimation("", 0);
	}
	return HashAnimation(static_cast<std::string_view>(a_str));
}
