#include "RAMS7200MS.hxx"
#include "Common/S7Utils.hxx"
#include "Common/Logger.hxx"
#include <algorithm>

RAMS7200MS::RAMS7200MS(std::string dp_address) :
 _ip_combo(dp_address),
 _ip(_ip_combo.substr(0, _ip_combo.find(";"))),
 _tp_ip(_ip_combo == _ip ? "" : _ip_combo.substr(_ip_combo.find(";") + 1, _ip_combo.size() - 1))
{}

void RAMS7200MS::addVar(std::string varName, int pollTime)
{
    std::lock_guard<std::mutex> lock{_rwmutex};
    auto var = RAMS7200MSVar(varName, pollTime, Common::S7Utils::TS7DataItemFromAddress(varName, false));
    vars.emplace(varName, std::move(var));
}

void RAMS7200MS::removeVar(std::string varName)
{
    std::lock_guard<std::mutex> lock{_rwmutex};
    auto it = vars.find(varName);
    if(it != vars.end()) {
        vars.erase(it);
    }
}

void RAMS7200MS::queuePLCItem(const std::string& varName, void* item)
{
    try
    {
        std::lock_guard<std::mutex> lock{_rwmutex};
        vars.at(varName)._toPlc.pdata = item;
    }
    catch(const std::out_of_range& e)
    {
        Common::Logger::globalWarning(__PRETTY_FUNCTION__, "Undefined address", e.what());
    }
}
