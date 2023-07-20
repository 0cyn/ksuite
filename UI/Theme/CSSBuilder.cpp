//
// Created by serket on 7/18/23.
//

#include "CSSBuilder.h"

CSSBuilder::CSSItem* CSSBuilder::CSSItem::AddSelector(std::string selector)
{
	selectors.insert(selector);
	return this;
}

CSSBuilder::CSSItem* CSSBuilder::CSSItem::AddSelectors(std::vector<std::string> _selectors)
{
	for (const auto& s : _selectors)
		selectors.insert(s);
	return this;
}

CSSBuilder::CSSItem* CSSBuilder::CSSItem::AddRule(std::string key, std::string value)
{
	rules[key] = value;
	return this;
}

CSSBuilder::CSSItem* CSSBuilder::CSSItem::CopyRules(CSSBuilder::CSSItem* item)
{
	for (const auto& [k, v] : item->rules)
	{
		rules[k] = v;
	}
	return this;
}

void CSSBuilder::CSSItem::Generate(std::string& text) const
{
	size_t i = 0;
	for (const auto& s : selectors)
	{
		if (i >= 1)
			text += ", ";
		text += s;
		i++;
	}
	text += "{";
	for (const auto& [k, v] : rules)
	{
		text.append(k);
		text.append(":");
		text.append(v);
		text.append(";");
	}
	text += "}";
}

CSSBuilder::CSSBuilder()
{
}

std::string CSSBuilder::Generate()
{
	std::string text;
	for (const auto& item : items)
	{
		item->Generate(text);
	}
	return text;
}

CSSBuilder::CSSItem* CSSBuilder::CreateNewItem()
{
	auto item = new CSSItem();
	items.push_back(item);
	return item;
}
