#ifndef CONFSTORE_H
#define CONFSTORE_H

#include <fstream>
#include "json.hpp"
using json = nlohmann::json;

#include <string>

#ifndef CONFFILE
#define CONFFILE "conf.json"
#endif

class ConfStore
{
public:
    ConfStore(std::string file);

    void loadSettings(std::string file);
    void saveSettings(std::string file);

    json config;
private:
    std::string filename;
};

#endif // CONFSTORE_H
