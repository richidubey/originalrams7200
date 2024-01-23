/** © Copyright 2022 CERN
 *
 * This software is distributed under the terms of the
 * GNU Lesser General Public Licence version 3 (LGPL Version 3),
 * copied verbatim in the file “LICENSE”
 *
 * In applying this licence, CERN does not waive the privileges
 * and immunities granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 *
 * Author: Adrien Ledeul (HSE)
 *
 **/

#include <csignal>

#include "RAMS7200LibFacade.hxx"
#include "RAMS7200Resources.hxx"
#include "Common/Constants.hxx"
#include "Common/Logger.hxx"
#include <thread>
#include <algorithm>
#include <vector>
#include <sstream>
#include <algorithm>


RAMS7200LibFacade::RAMS7200LibFacade(RAMS7200MS& ms, queueToDPCallback cb)
    : ms(ms), _queueToDPCB(cb)
{
     Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Initialized LibFacade with PLC IP: "+ CharString(ms._ip.c_str()));
}


void RAMS7200LibFacade::EnsureConnection(bool reduSwitch) {

    if(reduSwitch) {
        RAMS7200MarkDeviceConnectionError(!_client->Connected());
    }
    if(_client->Connected() && ioFailures < 5){ // TODO: parameterize this : No, COnstant + Driver Start read
        return;
    } else {
        if (_wasConnected) {
            Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Connection lost with PLC IP: ", ms._ip.c_str());
            if(!RAMS7200Resources::getDisableCommands()) {
                RAMS7200MarkDeviceConnectionError(true);
            }
        }
        do {
            //Disconnect and try to connect again.
            Disconnect();
            Connect();

            if(!_client->Connected()) {
                Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Failure in re-connection. Trying again in 5 seconds for PLC IP:" + CharString(ms._ip.c_str()));
                sleep_for(std::chrono::seconds(5));
            }
        } while(ms._run && !_wasConnected);
        if(!RAMS7200Resources::getDisableCommands()) {
            RAMS7200MarkDeviceConnectionError(false);
        }
    }
}

void RAMS7200LibFacade::Connect()
{
    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Connecting to : Local TSAP Port : Remote TSAP Port'", (ms._ip + " : "+ std::to_string(Common::Constants::getLocalTsapPort()) + ":" + std::to_string(Common::Constants::getRemoteTsapPort())).c_str());

    _client.reset(new TS7Client());

    _client->SetConnectionParams(ms._ip.c_str(), Common::Constants::getLocalTsapPort(), Common::Constants::getRemoteTsapPort());
    if(_client->Connect() == 0) {
        _wasConnected = true;
    }
    if (!RAMS7200Resources::getDisableCommands()) {
        RAMS7200MarkDeviceConnectionError(!_client->Connected());
    }
}


void RAMS7200LibFacade::Disconnect()
{
    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Disconnecting from '", ms._ip.c_str());
    _client->Disconnect();
    _wasConnected = false;
    ioFailures = 0;
}


void RAMS7200LibFacade::Poll()
{
    if(ms.vars.empty()){
        Common::Logger::globalWarning(__PRETTY_FUNCTION__, "No addresses for PLC IP:", ms._ip.c_str());
        return;
    }
    auto pollStartTime = std::chrono::steady_clock::now();
    Common::Logger::globalInfo(Common::Logger::L3,__PRETTY_FUNCTION__, ms._ip.c_str());
    std::vector<dpItem> addressesToPoll;
    std::vector<TS7DataItem> items;
    const auto pollInterval = Common::Constants::getPollingInterval();
    {
        std::lock_guard<std::mutex> lock{ms._rwmutex};
        for(auto& var : ms.vars) {
            const auto fpollTime = var.second.pollTime > pollInterval ? var.second.pollTime : pollInterval;
            const auto tDiff =  std::chrono::duration_cast<std::chrono::seconds>(pollStartTime - var.second.lastPollTime).count();
            if(tDiff >= fpollTime) {
                var.second.lastPollTime = std::chrono::steady_clock::now();
                addressesToPoll.emplace_back(dpItem{
                    ms._ip_combo + "$" + var.second.varName + "$" + std::to_string(var.second.pollTime),
                    Common::S7Utils::GetByteSizeFromAddress(var.second.varName),
                });
                items.emplace_back(var.second._toDP);
                Common::S7Utils::TS7AllocateDataItemForAddress(items.back());
                var.second._toDP.pdata = nullptr;
            }
        }
    }
    if(!addressesToPoll.empty()) {
        RAMS7200ReadWriteMaxN(addressesToPoll, items, 19, PDU_SIZE, OVERHEAD_READ_VARIABLE, OVERHEAD_READ_MESSAGE, Common::S7Utils::Operation::READ);
    }
    else
    {
        Common::Logger::globalInfo(Common::Logger::L3, "No vars to poll at the moment");
    }

}

void RAMS7200LibFacade::WriteToPLC() {
    std::vector<dpItem> addresses;
    std::vector<TS7DataItem> items;
    {
        std::lock_guard<std::mutex> lock{ms._rwmutex};
        for(auto& var : ms.vars) {
            if(var.second._toPlc.pdata != nullptr){
                addresses.emplace_back(dpItem{
                    ms._ip_combo + "$" + var.second.varName + "$" + std::to_string(var.second.pollTime),
                    Common::S7Utils::GetByteSizeFromAddress(var.second.varName),
                });
                items.emplace_back(var.second._toPlc);
                var.second._toPlc.pdata = nullptr;
                // Make sure that the next poll will happen immediately
                var.second.lastPollTime -= std::chrono::seconds(std::max(var.second.pollTime, Common::Constants::getPollingInterval()));
            }
        }
    }
    if(!addresses.empty()){
        RAMS7200ReadWriteMaxN(addresses, items, 10, PDU_SIZE, OVERHEAD_WRITE_VARIABLE, OVERHEAD_WRITE_MESSAGE, Common::S7Utils::Operation::WRITE);
    }
    else
    {
        Common::Logger::globalInfo(Common::Logger::L3, "No vars to write at the moment");
    }
}


void RAMS7200LibFacade::RAMS7200MarkDeviceConnectionError(bool error_status){
    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, std::to_string(error_status).c_str(), CharString("PLC IP: ") + CharString(ms._ip_combo.c_str())) ;
    
    auto pdata = new char[sizeof(bool)];
    memcpy(pdata, &error_status , sizeof(bool));
    this->_queueToDPCB(ms._ip_combo + "._system$_Error",sizeof(bool), pdata);
}

void RAMS7200LibFacade::RAMS7200ReadWriteMaxN(std::vector<dpItem> dpItems, std::vector<TS7DataItem> items, const uint N, const int PDU_SZ, const int VAR_OH, const int MSG_OH, const Common::S7Utils::Operation rorw) {
    try{

        int retOpt;
        int curr_sum;
        uint last_index = 0;
        uint to_send = 0;
        while(last_index < items.size()) {
            to_send = 0;
            curr_sum = 0;
            
            for (auto it = items.begin() + last_index; it != items.end(); ++it) {
                const auto& var = *it;
                const auto dataSize = Common::S7Utils::DataSizeByte(var.WordLen) * var.Amount;
                const auto itemSize = dataSize + VAR_OH;
                if (curr_sum + itemSize < PDU_SZ - MSG_OH && to_send < N) {
                    curr_sum += itemSize;
                    ++to_send;
                } else {
                    break;
                }
            }

            if(to_send == 0) {
                //This means that the current variable has a mem size > PDU. Call with ReadArea because it can split the request automatically (PDU Independance)
                to_send += 1;
                const auto& last_item = items[last_index];
                curr_sum = ((Common::S7Utils::DataSizeByte(last_item.WordLen)) * last_item.Amount) + VAR_OH + MSG_OH;

                if(rorw == Common::S7Utils::Operation::READ)
                    retOpt = _client->ReadArea(last_item.Area, last_item.DBNumber, last_item.Start, last_item.Amount, last_item.WordLen, last_item.pdata);
                else
                    retOpt = _client->WriteArea(last_item.Area, last_item.DBNumber, last_item.Start, last_item.Amount, last_item.WordLen, last_item.pdata);

            } else {
                if(rorw == Common::S7Utils::Operation::READ)
                    retOpt = _client->ReadMultiVars(&(items[last_index]), to_send);
                else {
                    retOpt = _client->WriteMultiVars(&(items[last_index]), to_send);
                }
            }

            std::stringstream addresses;
            for(uint i = last_index; i < last_index + to_send; i++) {
                Common::Logger::globalInfo(Common::Logger::L4, dpItems[i].dpAddress.c_str(), Common::S7Utils::DisplayTS7DataItem(&items[i], rorw).c_str());
                if(rorw == Common::S7Utils::Operation::READ){
                    if(items[i].Result == 0){
                        this->_queueToDPCB(dpItems[i].dpAddress, dpItems[i].dpSize , reinterpret_cast<char*>(items[i].pdata));
                    }
                    else {
                        Common::Logger::globalWarning(__PRETTY_FUNCTION__, "Error in reading address: ", dpItems[i].dpAddress.c_str());
                    }
                }
                addresses << " " << dpItems[i].dpAddress;
            }

            std::stringstream ss;
            ss << ms._ip << (rorw == Common::S7Utils::Operation::READ ? "Read" : "Write");
            if( retOpt == 0) {
                ss << "OK for PLC IP:" << ms._ip << " with " << to_send << " items and PDU size of " << curr_sum + MSG_OH << " for addresses:" << addresses.str() ;
                Common::Logger::globalInfo(Common::Logger::L3, ss.str().c_str());
            }
            else {
                ++ioFailures;
                ss << "KO for PLC IP:" << ms._ip << " with " << to_send << " items and PDU size of " << curr_sum + MSG_OH << " for addresses:" << addresses.str() ;
                ss << " ioFailures: " << ioFailures;
                Common::Logger::globalWarning(ss.str().c_str());
            }
            last_index += to_send;
        }

    }
    catch(std::exception& e){
        Common::Logger::globalWarning(__PRETTY_FUNCTION__," Encountered Exception:", e.what());
    }
}