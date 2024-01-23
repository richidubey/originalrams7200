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


#ifndef RAMS7200HWSERVICE_H_
#define RAMS7200HWSERVICE_H_

#include <HWService.hxx>
#include "RAMS7200MS.hxx"
#include "RAMS7200LibFacade.hxx"
#include "RAMS7200Panel.hxx"
#include "Common/Logger.hxx"

#include <memory>
#include <queue>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <tuple>

using toDPTriple = std::tuple<CharString, uint16_t, char*>;

class RAMS7200HWService : public HWService
{
  public:
    RAMS7200HWService();
    virtual PVSSboolean initialize(int argc, char *argv[]);
    virtual PVSSboolean start();
    virtual void stop();
    virtual void workProc();
    virtual PVSSboolean writeData(HWObject *objPtr);
    int CheckIP(std::string);

private:
    void queueToDP(const std::string&, uint16_t, char*);
    void handleNewMS(RAMS7200MS&);

    queueToDPCallback  _queueToDPCB{[this](const std::string& dp_address, uint16_t length, char* payload){this->queueToDP(dp_address, length, payload);}};
    std::function<void(RAMS7200MS&)> _newMSCB{[this](RAMS7200MS& ms){this->handleNewMS(ms);}};

    //Common
    void insertInDataToDp(CharString&& address, uint16_t length, char* value);
    std::mutex _toDPmutex;
    std::queue<toDPTriple> _toDPqueue;

    enum
    {
       ADDRESS_OPTIONS_IP_COMBO = 0,
       ADDRESS_OPTIONS_VAR,
       ADDRESS_OPTIONS_POLLTIME,
       ADDRESS_OPTIONS_SIZE
    } ADDRESS_OPTIONS;

    std::vector<std::thread> _plcThreads;
    std::vector<std::thread> _panelThreads;
};


void handleSegfault(int signal_code);

#endif
