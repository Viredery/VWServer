#pragma once

#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include <string>
#include <map>

namespace VWServer
{

class Configuration
{
public:
	~Configuration(){};
    static Configuration* getInstance();
    std::string find(std::string key);

private:
    Configuration();
    Configuration(const Configuration&);
    void readConfig();

    std::string fileConfigSaved;
    std::map<std::string, std::string> mConfigAttribute;
    static Configuration* singleton;
};


}  // namespace HTC

#endif