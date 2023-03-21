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

#ifndef S7200LIBFACADE_HXX
#define S7200LIBFACADE_HXX

#define OPERATION_READ 0
#define OPERATION_WRITE 1
#define OVERHEAD_READ_MESSAGE 13
#define OVERHEAD_READ_VARIABLE 5
#define OVERHEAD_WRITE_MESSAGE 12
#define OVERHEAD_WRITE_VARIABLE 16
#define PDU_SIZE 240

#include <string>
#include <chrono>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <map>
#include <functional>
#include <unordered_set>
#include <condition_variable>
#include <mutex>
#include "snap7.h"

using consumeCallbackConsumer = std::function<void(const std::string& ip, const std::string& var, const std::string& pollTime, char* payload)>;
using errorCallbackConsumer = std::function<void(const std::string& ip, int error,  const std::string& reason)>;

/**
 * @brief The S7200LibFacade class is a facade and encompasses all the consumer interaction with snap7
 */

class S7200LibFacade
{
public:
    /**
     * @brief S7200LibFacade constructor
     * @param ip : the ip
     * @param consumeCallbackConsumer : a callback that will be called for eached polled message
     * */
    S7200LibFacade(const std::string& ip, consumeCallbackConsumer, errorCallbackConsumer);
    void Disconnect();

    S7200LibFacade(const S7200LibFacade&) = delete;
    S7200LibFacade& operator=(const S7200LibFacade&) = delete;

    bool isInitialized(){return _initialized;}
    void Poll(std::vector<std::pair<std::string, int>>&, std::chrono::time_point<std::chrono::steady_clock> loopStartTime);
    void write(std::vector<std::pair<std::string, void * >>);
    void clearLastWriteTimeList();
    void Reconnect();


    TS7DataItem S7200Read(std::string S7200Address, void* val);
    // TS7DataItem* S7200LibFacade::S7200Read2(std::string S7200Address1, void* val1, std::string S7200Address2, void* val2);
    void S7200ReadN(std::vector<std::string> validVars, int N);
    void S7200ReadMaxN(std::vector <std::string> validVars, int N, int pdu_size, int VAR_OH, int MSG_OH);
    void S7200ReadWriteMaxN(std::vector <std::pair<std::string, void *>> validVars, uint N, int PDU_SZ, int VAR_OH, int MSG_OH, int rorw);
    TS7DataItem S7200Write(std::string S7200Address, void* val);
    static int getByteSizeFromAddress(std::string S7200Address);
    std::map<std::string, std::chrono::time_point<std::chrono::steady_clock> > lastWritePerAddress;
    static TS7DataItem S7200TS7DataItemFromAddress(std::string S7200Address);
    
    static bool S7200AddressIsValid(std::string S7200Address);
    void startFileSharingThread(char* touchPanelIP);
    void FileSharingTask(char* ip, int port);

    bool FSThreadRunning = false;
    bool stopCurrentFSThread = false;
    std::condition_variable CV_SwitchFSThread;
    std::mutex mutex_;
    int readFailures = 0; //allowed since C++11
    char* touch_panel_ip;

    bool touch_panel_conn_error = true; //Not connected initially

private:
    //std::unique_ptr<Consumer> _consumer;
    std::string _ip;

    consumeCallbackConsumer _consumeCB;
    errorCallbackConsumer _errorCB;
    bool _initialized{false};
    TS7Client *_client;
    static int S7200AddressGetWordLen(std::string S7200Address);
    static int S7200AddressGetStart(std::string S7200Address);
    static int S7200AddressGetArea(std::string S7200Address);
    static int S7200AddressGetAmount(std::string S7200Address);
    static int S7200AddressGetBit(std::string S7200Address);
    static int S7200DataSizeByte(int WordLength);
    static void S7200DisplayTS7DataItem(PS7DataItem item);
    
};

#endif //S7200LIBFACADE_HXX

