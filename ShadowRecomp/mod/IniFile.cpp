/**
 * INI file parser.
 */

#include <string>
#include <locale>
#include <codecvt>
#include <mod/IniFile.h>




#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <list>
#include <stdexcept>

using std::transform;
using std::string;
using std::unordered_map;
using std::wstring;

#ifndef _WIN32
static std::string wstring_to_utf8(const std::wstring& wstr)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
	return conv.to_bytes(wstr);
}
#endif

inline bool iequals(const std::string& a, const std::string& b)
{
	return std::equal(a.begin(), a.end(),
		b.begin(), b.end(),
		[](char a, char b) {
			return std::tolower(a) == std::tolower(b);
		});
}

/** IniGroup **/

/**
 * Check if the INI group has the specified key.
 * @param key Key.
 * @return True if the key exists; false if not.
 */
bool IniGroup::hasKey(const string& key) const
{
	return (m_data.find(key) != m_data.end());
}

/**
 * Check if the INI group has the specified key with a non-empty value.
 * @param key Key.
 * @return True if the key exists; false if not or value is empty.
 */
bool IniGroup::hasKeyNonEmpty(const string& key) const
{
	auto iter = m_data.find(key);
	if (iter == m_data.end())
		return false;

	return !iter->second.empty();
}

const unordered_map<string, string>* IniGroup::data() const
{
	return &m_data;
}

/**
 * Get a string value from the INI group.
 * @param key Key.
 * @param def Default value.
 * @return String value.
 */
string IniGroup::getString(const string& key, const string& def) const
{
	auto iter = m_data.find(key);
	return (iter != m_data.end() ? iter->second : def);
}


/**
 * Get a boolean value from the INI group.
 * @param key Key.
 * @param def Default value.
 * @return Boolean value.
 */
bool IniGroup::getBool(const string& key, bool def) const
{
	auto iter = m_data.find(key);
	if (iter == m_data.end())
		return def;

	return iequals(iter->second, "true") ||
		iter->second == "1";
}

/**
 * Get an integer value from the INI group.
 * @param key Key.
 * @param radix Radix.
 * @param def Default value.
 * @return Integer value.
 */
int IniGroup::getIntRadix(const string& key, int radix, int def) const
{
	auto iter = m_data.find(key);
	if (iter == m_data.end())
		return def;

	try
	{
		return (int)std::stoll(iter->second, nullptr, radix);
	}
	catch (std::invalid_argument const& ex)
	{
		return def;
	}
	catch (std::out_of_range const& ex)
	{
		return def;
	}
}

/**
 * Get an integer value from the INI group.
 * @param key Key.
 * @param def Default value.
 * @return Integer value.
 */
int IniGroup::getInt(const string& key, int def) const
{
	return getIntRadix(key, 10, def);
}

/**
 * Get a floating-point value from the INI group.
 * @param key Key.
 * @param def Default value.
 * @return Floating-point value.
 */
float IniGroup::getFloat(const string& key, float def) const
{
	auto iter = m_data.find(key);
	if (iter == m_data.end())
		return def;

	return std::stof(iter->second);
}

/**
* Set a string value in the INI group.
* @param key Key.
* @param val Value.
*/
void IniGroup::setString(const string& key, const string& val)
{
	m_data[key] = val;
}


/**
* Set a boolean value in the INI group.
* @param key Key.
* @param val Value.
*/
void IniGroup::setBool(const string& key, bool val)
{
	string str;
	if (val)
		str = "True";
	else
		str = "False";
	m_data[key] = str;
}

/**
* Set an integer value in the INI group.
* @param key Key.
* @param radix Radix.
* @param val Value.
*/
void IniGroup::setIntRadix(const string& key, int radix, int val)
{
	std::ostringstream ss;

	switch (radix)
	{
	case 8:  
		ss << std::oct << val; break;
	case 16: 
		ss << std::hex << val; break;
	default: 
		ss << val; break;
	}


}

/**
* Set an integer value in the INI group.
* @param key Key.
* @param val Value.
*/
void IniGroup::setInt(const string& key, int val)
{
	m_data[key] = std::to_string(val);
}

/**
* Set a floating-point value in the INI group.
* @param key Key.
* @param val Value.
*/
void IniGroup::setFloat(const string& key, float val)
{
	m_data[key] = std::to_string(val);
}

/**
* Remove a key from the INI group.
* @param key Key.
* @return True if key was found.
*/
bool IniGroup::removeKey(const string& key)
{
	if (hasKey(key))
	{
		m_data.erase(key);
		return true;
	}
	return false;
}

IniGroup::iterator IniGroup::begin()
{
	return m_data.begin();
}

IniGroup::const_iterator IniGroup::cbegin() const
{
	return m_data.cbegin();
}

IniGroup::iterator IniGroup::end()
{
	return m_data.end();
}

IniGroup::const_iterator IniGroup::cend() const
{
	return m_data.cend();
}

/** IniFile **/

IniFile::IniFile(const string& filename)
{
	std::ifstream f(filename);
	if (f)
		load(f);

}

IniFile::IniFile(const wstring& filename)
{
#ifdef _WIN32
	std::ifstream f(filename);
#else
	std::ifstream f(wstring_to_utf8(filename));
#endif
	if (f)
		load(f);
}

IniFile::IniFile(const char* filename)
{
	std::ifstream f(filename);

	if (f)
		load(f);
}

IniFile::IniFile(const wchar_t* filename)
{
#ifdef _WIN32
	std::ifstream f(filename);
#else
	std::ifstream f(wstring_to_utf8(filename));
#endif
	if (f)
		load(f);
}

IniFile::IniFile(std::istream& f)
{
	load(f);
}

IniFile::~IniFile()
{
	clear();
}

/**
 * Get an INI group.
 * @param section Section.
 * @return INI group, or nullptr if not found.
 */
IniGroup* IniFile::getGroup(const string& section)
{
	auto iter = m_groups.find(section);
	return (iter != m_groups.end() ? iter->second.get() : nullptr);
}

/**
* Get an INI group.
* @param section Section.
* @return INI group, or nullptr if not found.
*/
const IniGroup* IniFile::getGroup(const string& section) const
{
	auto iter = m_groups.find(section);
	return (iter != m_groups.end() ? iter->second.get() : nullptr);
}

/**
* Create an INI group, or retrieve an existing group.
* @param section Section.
* @return INI group.
*/
IniGroup* IniFile::createGroup(const string& section)
{
	auto iter = m_groups.find(section);
	if (iter != m_groups.end())
		return iter->second.get();

	auto group = std::make_unique<IniGroup>();
	IniGroup* ptr = group.get(); // keep raw pointer for return

	m_groups.emplace(section, std::move(group));
	return ptr;
}

/**
 * Check if the INI file has the specified group.
 * @param section Section.
 * @return True if the section exists; false if not.
 */
bool IniFile::hasGroup(const string& section) const
{
	return (m_groups.find(section) != m_groups.end());
}

/**
 * Check if the INI file has the specified key.
 * @param section Section.
 * @param key Key.
 * @return True if the key exists; false if not.
 */
bool IniFile::hasKey(const string& section, const string& key) const
{
	auto iter = m_groups.find(section);
	if (iter == m_groups.end())
		return false;

	return iter->second->hasKey(key);
}

/**
 * Check if the INI file has the specified key with a non-empty value.
 * @param section Section.
 * @param key Key.
 * @return True if the key exists; false if not or value is empty.
 */
bool IniFile::hasKeyNonEmpty(const string& section, const string& key) const
{
	auto iter = m_groups.find(section);
	if (iter == m_groups.end())
		return false;

	return iter->second->hasKeyNonEmpty(key);
}

/**
 * Get a string value from the INI file.
 * @param section Section.
 * @param key Key.
 * @param def Default value.
 * @return String value.
 */
string IniFile::getString(const string& section, const string& key, const string& def) const
{
	const IniGroup* group = getGroup(section);
	if (!group)
		return def;
	return group->getString(key, def);
}


/**
 * Get a boolean value from the INI file.
 * @param section Section.
 * @param key Key.
 * @param def Default value.
 * @return Boolean value.
 */
bool IniFile::getBool(const string& section, const string& key, bool def) const
{
	const IniGroup* group = getGroup(section);
	if (!group)
		return def;
	return group->getBool(key, def);
}

/**
 * Get an integer value from the INI file.
 * @param section Section.
 * @param key Key.
 * @param radix Radix.
 * @param def Default value.
 * @return Integer value.
 */
int IniFile::getIntRadix(const string& section, const string& key, int radix, int def) const
{
	const IniGroup* group = getGroup(section);
	if (!group)
		return def;
	return group->getIntRadix(key, radix, def);
}

/**
 * Get an integer value from the INI file.
 * @param section Section.
 * @param key Key.
 * @param def Default value.
 * @return Integer value.
 */
int IniFile::getInt(const string& section, const string& key, int def) const
{
	const IniGroup* group = getGroup(section);
	if (!group)
		return def;
	return group->getInt(key, def);
}

/**
 * Get a floating-point value from the INI file.
 * @param section Section.
 * @param key Key.
 * @param def Default value.
 * @return Floating-point value.
 */
float IniFile::getFloat(const string& section, const string& key, float def) const
{
	const IniGroup* group = getGroup(section);
	if (!group)
		return def;
	return group->getFloat(key, def);
}

/**
* Set a boolean value in the INI file.
* @param section Section.
* @param key Key.
* @param val Value.
*/
void IniFile::setBool(const string& section, const string& key, bool val)
{
	createGroup(section)->setBool(key, val);
}

/**
* Set an integer value in the INI file.
* @param section Section.
* @param key Key.
* @param radix Radix.
* @param val Value.
*/
void IniFile::setIntRadix(const string& section, const string& key, int radix, int val)
{
	createGroup(section)->setIntRadix(key, radix, val);
}

/**
* Set an integer value in the INI file.
* @param section Section.
* @param key Key.
* @param val Value.
*/
void IniFile::setInt(const string& section, const string& key, int val)
{
	createGroup(section)->setInt(key, val);
}

/**
* Set a floating-point value in the INI file.
* @param section Section.
* @param key Key.
* @param val Value.
*/
void IniFile::setFloat(const string& section, const string& key, float val)
{
	createGroup(section)->setFloat(key, val);
}

/**
* Remove a section from the INI file.
* @param group Section.
* @return True if section was found.
*/
bool IniFile::removeGroup(const string& group)
{

	return false;
}

/**
* Remove a key from the INI file.
* @param section Section.
* @param key Key.
* @return True if key was found.
*/
bool IniFile::removeKey(const string& section, const string& key)
{

	IniGroup* group = getGroup(section);
	if (group)
		return group->removeKey(key);
	return false;
}

/**
* Save an INI file.
* @param filename Name of file to save to.
*/
void IniFile::save(const string& filename) const
{
	std::ofstream f(filename);
	if (f)
		save(f);
}

/**
* Save an INI file.
* @param filename Name of file to save to.
*/
void IniFile::save(const wstring& filename) const
{
#ifdef _WIN32
	std::ofstream f(filename);
#else
	std::ofstream f(wstring_to_utf8(filename));
#endif
	if (f)
		save(f);
}

/**
* Save an INI file.
* @param std::istream. (File is not closed after processing.)
*/
void IniFile::save(std::ofstream& f) const
{
	std::list<std::pair<string, IniGroup*>> list;

	for (auto it = cbegin(); it != cend(); ++it)
	{
		if (it->first.empty())
		{
			list.emplace_front(it->first, it->second.get());
		}
		else
		{
			list.emplace_back(it->first, it->second.get());
		}
	}

	for (const auto& gr : list)
	{
		if (!gr.first.empty())
		{
			f << "[" << escape(gr.first, true, false) << "]\n";
		}

		for (const auto& kv : *gr.second)
		{
			f << escape(kv.first, false, true)
				<< "="
				<< escape(kv.second, false, false)
				<< "\n";
		}
	}
}



/**
 * Load an INI file.
 * Internal function; called from the constructor.
 * @param f FILE pointer. (File is not closed after processing.)
 */
void IniFile::load(std::istream& f)
{
	auto group = std::make_unique<IniGroup>();
	IniGroup* curGroup = group.get();
	m_groups[""] = std::move(group);

	std::string line;
	while (std::getline(f, line))
	{
		if (line.empty())
			continue;

		if (line.back() == '\r')
			line.pop_back();

		bool startswithbracket = false;
		int firstequals = -1;
		int endbracket = -1;

		std::string sb;
		sb.reserve(line.size());

		for (size_t c = 0; c < line.size(); c++)
		{
			switch (line[c])
			{
			case '\\':
				if (c + 1 >= line.size())
					break;

				c++;
				switch (line[c])
				{
				case 'n': sb += '\n'; break;
				case 'r': sb += '\r'; break;
				default: sb += line[c]; break;
				}
				break;

			case '=':
				if (firstequals == -1)
					firstequals = (int)sb.length();
				sb += '=';
				break;

			case '[':
				if (c == 0)
					startswithbracket = true;
				sb += '[';
				break;

			case ']':
				endbracket = (int)sb.length();
				sb += ']';
				break;

			case ';':
				c = line.size();
				break;

			default:
				sb += line[c];
				break;
			}
		}

		if (startswithbracket && endbracket != -1)
		{
			std::string section = sb.substr(1, endbracket - 1);
			auto& group = m_groups[section];

			if (!group)
				group = std::make_unique<IniGroup>();

			curGroup = group.get();
		}
		else if (!sb.empty())
		{
			std::string key, value;

			if (firstequals > -1)
			{
				key = sb.substr(0, firstequals);
				value = sb.substr(firstequals + 1);
			}
			else
			{
				key = sb;
			}

			curGroup->m_data[key] = value;
		}
	}
}

/**
 * Clear the loaded INI file.
 */
void IniFile::clear()
{
	m_groups.clear();

}

string IniFile::escape(const string& str, bool sec, bool key)
{
	string result(str);

	for (size_t i = 0; i < result.size(); i++)
	{
		switch (result[i])
		{
		case '=':
			if (key)
				result.insert(i++, "\\");
			break;
		case '[':
			if (key && i == 0)
				result.insert(i++, "\\");
			break;
		case ']':
			if (sec)
				result.insert(i++, "\\");
			break;
		case '\\':
		case ';':
			result.insert(i++, "\\");
			break;
		case '\n':
			result.insert(i++, "\\");
			result[i] = 'n';
			break;
		case '\r':
			result.insert(i++, "\\");
			result[i] = 'r';
			break;

		default:
			break;
		}
	}

	return result;
}
