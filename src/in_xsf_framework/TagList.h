/*
 * xSF Tag List
 * By Naram Qashat (CyberBotX) [cyberbotx@cyberbotx.com]
 *
 * Storage of tags from PSF-style files, specifications found at
 * http://wiki.neillcorlett.com/PSFTagFormat
 */

#pragma once

#include <map>
#include <string>
#include <vector>
//#include "eqstr.h"
#include "ltstr.h"

class TagList
{
public:
	typedef std::map<std::string, std::string, lt_str> Tags;
	typedef std::vector<std::string> TagsList;
private:
	//static eq_str eqstr;

	Tags tags;
	TagsList tagsOrder;

	TagsList::const_iterator GetTagOrder(const std::string &name) const;
public:
	TagList() : tags(), tagsOrder() { }
	const TagsList &GetKeys() const;
	TagsList GetTags() const;
	bool Exists(const std::string &name) const;
	std::string operator[](const std::string &name) const;
	std::string &operator[](const std::string &name);
	void Remove(const std::string &name);
	void Clear();
};
