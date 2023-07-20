//
// Created by serket on 7/18/23.
//

#include <vector>
#include <string>
#include <unordered_map>
#include <set>

#ifndef KSUITE_CSSBUILDER_H
#define KSUITE_CSSBUILDER_H


class CSSBuilder {
public:
	struct CSSItem {
		std::set<std::string> selectors {};
		std::unordered_map<std::string, std::string> rules {};
		CSSItem* AddSelector(std::string selector);
		CSSItem* AddSelectors(std::vector<std::string> selectors);
		CSSItem* AddRule(std::string key, std::string value);
		CSSItem* CopyRules(CSSItem* item);

		void Generate(std::string& text) const;
	};

private:

	std::vector<CSSItem*> items;

public:
	CSSBuilder();
	~CSSBuilder()
	{
		items.clear();
	}
	std::string Generate();

	CSSItem* CreateNewItem();
};


#endif //KSUITE_CSSBUILDER_H
