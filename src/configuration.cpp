#include "configuration.h"
#include <fstream>
#include <iostream>


namespace VWServer
{

Configuration* Configuration::singleton = NULL;

Configuration::Configuration()
{
    fileConfigSaved = "../config.txt";

    readConfig();
}

Configuration* Configuration::getInstance() {
    if (NULL == singleton)
        singleton = new Configuration();
    return singleton;
}

void Configuration::readConfig()
{
	if (fileConfigSaved.empty())
		return;

    std::ifstream file;
    file.open(fileConfigSaved.c_str());

	if (!file.good())
		return;

    std::string configData;
    while (std::getline(file, configData)) {
    	std::size_t pos = configData.find_first_of('=');
    	if (std::string::npos == pos)
            continue;
    	std::string key = configData.substr(0, pos);
        key.erase(0, key.find_first_not_of(" "));
        key.erase(key.find_last_not_of(" ") + 1);

        std::string value = configData.substr(pos + 1);
        value.erase(0, value.find_first_not_of(" "));
        value.erase(value.find_last_not_of(" ") + 1);

        if ('#' != key[0]) {
            mConfigAttribute.insert(std::pair<std::string, std::string>(key, value));
        }
    }
	file.close();
}

std::string Configuration::find(std::string key)
{
    std::map<std::string, std::string>::iterator it = mConfigAttribute.find(key);
    if (it != mConfigAttribute.end()){
        return it->second;
    } else {
        return "";
    }
}

}

int main()
{}