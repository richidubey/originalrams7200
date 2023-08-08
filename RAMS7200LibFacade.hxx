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

#ifndef RAMS7200LIBFACADE_HXX
#define RAMS7200LIBFACADE_HXX

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
 * @brief The RAMS7200LibFacade class is a facade and encompasses all the consumer interaction with snap7
 */

class RAMS7200LibFacade
{
public:
    /**
     * @brief RAMS7200LibFacade constructor
     * @param ip : the ip
     * @param consumeCallbackConsumer : a callback that will be called for eached polled message
     * */
    RAMS7200LibFacade(const std::string& ip, const std::string& tp_ip, consumeCallbackConsumer, errorCallbackConsumer);
    void Disconnect();

    RAMS7200LibFacade(const RAMS7200LibFacade&) = delete;
    RAMS7200LibFacade& operator=(const RAMS7200LibFacade&) = delete;

    bool isInitialized(){return _initialized;}
    void Poll(std::vector<std::pair<std::string, int>>&, std::chrono::time_point<std::chrono::steady_clock> loopStartTime);
    void write(std::vector<std::pair<std::string, void * >>);
    void clearLastWriteTimeList();
    void Connect();
    void Reconnect();


    TS7DataItem RAMS7200Read(std::string RAMS7200Address, void* val);
    // TS7DataItem* RAMS7200LibFacade::RAMS7200Read2(std::string RAMS7200Address1, void* val1, std::string RAMS7200Address2, void* val2);
    void RAMS7200ReadN(std::vector<std::string> validVars, int N);
    void RAMS7200ReadMaxN(std::vector <std::string> validVars, int N, int pdu_size, int VAR_OH, int MSG_OH);
    void RAMS7200ReadWriteMaxN(std::vector <std::pair<std::string, void *>> validVars, uint N, int PDU_SZ, int VAR_OH, int MSG_OH, int rorw);
    TS7DataItem RAMS7200Write(std::string RAMS7200Address, void* val);
    static int getByteSizeFromAddress(std::string RAMS7200Address);
    std::map<std::string, std::chrono::time_point<std::chrono::steady_clock> > lastWritePerAddress;
    void RAMS7200MarkDeviceConnectionError(std::string, bool);
    static TS7DataItem RAMS7200TS7DataItemFromAddress(std::string RAMS7200Address);

    void markForNextRead(std::vector<std::pair<std::string, void *>> addresses, std::chrono::time_point<std::chrono::steady_clock> loopFirstStartTime);
    
    static bool RAMS7200AddressIsValid(std::string RAMS7200Address);
    static int RAMS7200AddressGetWordLen(std::string RAMS7200Address);
    static int RAMS7200AddressGetAmount(std::string RAMS7200Address);
    
    void startFileSharingThread(char* touchPanelIP);
    void FileSharingTask(char* ip, int port);

    bool FSThreadRunning = false;
    bool stopCurrentFSThread = false;
    std::condition_variable CV_SwitchFSThread;
    std::mutex mutex_;
    int readFailures = 0; //allowed since C++11
    char* touch_panel_ip;

    bool touch_panel_conn_error = true; //Not connected initially
    std::map <std::string, TS7DataItem> VarItems;

private:
    //std::unique_ptr<Consumer> _consumer;
    std::string _ip; 
    std::string _tp_ip; 

    consumeCallbackConsumer _consumeCB;
    errorCallbackConsumer _errorCB;
    bool _initialized{false};
    TS7Client *_client;
    static int RAMS7200AddressGetStart(std::string RAMS7200Address);
    static int RAMS7200AddressGetArea(std::string RAMS7200Address);
    static int RAMS7200AddressGetBit(std::string RAMS7200Address);
    static int RAMS7200DataSizeByte(int WordLength);
    static void RAMS7200DisplayTS7DataItem(PS7DataItem item);
    TS7DataItem initializeIfMissVar(std::string);
    
    
};

#endif //RAMS7200LIBFACADE_HXX

