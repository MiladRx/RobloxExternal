#pragma once

#include <string>
#include <vector>

namespace Cheat {
namespace Config {

    std::string Directory();
    std::vector<std::string> List();

    bool Save(const std::string& name);
    bool Load(const std::string& name);
    bool Remove(const std::string& name);

    // last-used config name stamped to configs\last.txt on Save/Load
    std::string LastName();
    bool LoadLast();

}
}
