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
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <thread>
#include "RAMS7200Encryption.hxx"
#include <algorithm>
#include <vector>


RAMS7200LibFacade::RAMS7200LibFacade(const std::string& ip, consumeCallbackConsumer cb, errorCallbackConsumer erc = nullptr)
    : _ip(ip), _consumeCB(cb), _errorCB(erc)
{
     Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Initialized LibFacade with IP: ", _ip.c_str());
}

void RAMS7200LibFacade::Connect()
{
    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Connecting to : Local TSAP Port : Remote TSAP Port'", (_ip + " : "+ std::to_string(Common::Constants::getLocalTsapPort()) + ":" + std::to_string(Common::Constants::getRemoteTsapPort())).c_str());

    try{
        _client = new TS7Client();

        _client->SetConnectionParams(_ip.c_str(), Common::Constants::getLocalTsapPort(), Common::Constants::getRemoteTsapPort());
        int res = _client->Connect();


        if (res==0) {
            Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Connected to '", _ip.c_str());
            //printf("  PDU Requested  : %d bytes\n",Client->PDURequested());
            //printf("  PDU Negotiated : %d bytes\n",Client->PDULength());
            _initialized = true;
        }
    }
    catch(std::exception& e)
    {
        Common::Logger::globalWarning("Snap7: Unable to initialize connection!", e.what());
    }
}

void RAMS7200LibFacade::Reconnect()
{
 Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Reconnecting to : Local TSAP Port : Remote TSAP Port'", (_ip + " : "+ std::to_string(Common::Constants::getLocalTsapPort()) + ":" + std::to_string(Common::Constants::getRemoteTsapPort())).c_str());

    try{
        _client = new TS7Client();

        _client->SetConnectionParams(_ip.c_str(), Common::Constants::getLocalTsapPort(), Common::Constants::getRemoteTsapPort());
        int res = _client->Connect();


        if (res==0) {
            Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Connected to '", _ip.c_str());
            //printf("  PDU Requested  : %d bytes\n",Client->PDURequested());
            //printf("  PDU Negotiated : %d bytes\n",Client->PDULength());
            _initialized = true;
        }
    }
    catch(std::exception& e)
    {
        Common::Logger::globalWarning("Snap7: Unable to initialize connection!", e.what());
    }
}

void RAMS7200LibFacade::Disconnect()
{
    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Disconnecting from '", _ip.c_str());

    try{
        int res = _client->Disconnect();


        if (res==0) {
            Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Disconnected successfully from '", _ip.c_str());
            _initialized = false;
        }
    }
    catch(std::exception& e)
    {
        Common::Logger::globalWarning("Snap7: Unable to disconnect!", e.what());
    }
}

void RAMS7200LibFacade::clearLastWriteTimeList() {
    lastWritePerAddress.clear();
    //clear() destroys all the elements in the map
}

void RAMS7200LibFacade::Poll(std::vector<std::pair<std::string, int>>& vars, std::chrono::time_point<std::chrono::steady_clock> loopStartTime)
{
    std::vector<std::pair<std::string, void *>> addresses;

    for (uint i = 0 ; i < vars.size() ; i++) {
        if(RAMS7200AddressIsValid(vars[i].first)){
            if(lastWritePerAddress.count(vars[i].first) == 0) {
                lastWritePerAddress.insert(std::pair<std::string, std::chrono::time_point<std::chrono::steady_clock>>(vars[i].first, loopStartTime));
                Common::Logger::globalInfo(Common::Logger::L3,"Added to lastWritePerAddress queue: address", vars[i].first.c_str());
                addresses.push_back(std::pair<std::string, void *>(vars[i].first, (void *)&vars[i].second));
            } else{
                int fpollTime;
                int fpollingInterval = std::chrono::seconds(Common::Constants::getPollingInterval()).count() > 0 ? std::chrono::seconds(Common::Constants::getPollingInterval()).count() : 2;  

                if(fpollingInterval < vars[i].second) {
                    fpollTime = vars[i].second;
                } else {
                    fpollTime = fpollingInterval;
                    //Common::Logger::globalInfo(Common::Logger::L1,"Using default polling Interval for address", vars[i].first.c_str());
                    //Common::Logger::globalInfo(Common::Logger::L1,"Default polling Interval: ", std::to_string(fpollingInterval).c_str());
                }

                std::chrono::duration<double> tDiff = loopStartTime - lastWritePerAddress[vars[i].first];
                if((int)tDiff.count() >= fpollTime) {
                    lastWritePerAddress[vars[i].first] = loopStartTime;
                    addresses.push_back(std::pair<std::string, void *>(vars[i].first, (void *)&vars[i].second));
                }
            }
        }
    }

    if(addresses.size() == 0) {
        Common::Logger::globalInfo(Common::Logger::L2, "Valid vars size is 0, did not call read");
        return;
    }
    
                       
    // for(uint i = 0; i < addresses.size() ; i++) {
    //     int a;
    //     std::memcpy(&a, addresses[i].second, sizeof(int));    
    //     Common::Logger::globalInfo(Common::Logger::L1,("Address: "+ addresses[i].first + "Polling Time" + std::to_string(a)).c_str());
    // }
    
    RAMS7200ReadWriteMaxN(addresses, 19, PDU_SIZE, OVERHEAD_READ_VARIABLE, OVERHEAD_READ_MESSAGE, OPERATION_READ);
}

void RAMS7200LibFacade::write(std::vector<std::pair<std::string, void *>> addresses) {
    RAMS7200ReadWriteMaxN(addresses, 12, PDU_SIZE, OVERHEAD_WRITE_VARIABLE, OVERHEAD_WRITE_MESSAGE, OPERATION_WRITE);

    for(uint i = 0; i < addresses.size(); i++) {
        delete[] (char *)addresses[i].second;
    }
}


int RAMS7200LibFacade::RAMS7200AddressGetWordLen(std::string RAMS7200Address)
{
    if(RAMS7200Address.length() < 2){
        return -1; //invalid
    }
    else if(std::tolower((char) RAMS7200Address.at(1)) == 'b'){
        return S7WLByte;
    }
    else if(std::tolower((char) RAMS7200Address.at(1)) == 'w'){
        return S7WLWord;
    }
    else if(std::tolower((char) RAMS7200Address.at(1)) == 'd'){
        return S7WLReal; //e.g. VD124 GLB.CAL.GANA1
    }
    else{  //e.g.: V255.3
        return S7WLBit;
    }
    return 0; //dummy
}

int RAMS7200LibFacade::RAMS7200AddressGetStart(std::string RAMS7200Address)
{
    if(RAMS7200Address.length() < 2){
        return -1; //invalid
    }
    else if(std::tolower((char) RAMS7200Address.at(1)) == 'b' || std::tolower((char) RAMS7200Address.at(1)) == 'w' || std::tolower((char) RAMS7200Address.at(1)) == 'd'){ //Addesses like XX9999
        if(RAMS7200Address.find_first_of('.')  == std::string::npos){
            return (int) std::stoi(RAMS7200Address.substr(2)); //e.g.:VB2978
        }
        else{
            return (int) std::stoi(RAMS7200Address.substr(2, RAMS7200Address.find_first_of('.')-1)); //e.g.:VB2978.20
        }
    }
    else{ //Addesses like X9999
        if(RAMS7200Address.find_first_of('.')  == std::string::npos){
            return -1; //invalid
        }
        else{
            return (int) std::stoi(RAMS7200Address.substr(1, RAMS7200Address.find_first_of('.')-1)); //e.g.: V255.3
        }
    }
  return 0; //dummy
}

int RAMS7200LibFacade::RAMS7200AddressGetArea(std::string RAMS7200Address)
{
    if(RAMS7200Address.length() < 2){
        return -1; //invalid
    }
    else if(std::tolower((char) RAMS7200Address.at(0)) == 'v'){ //Data Blocks
        return S7AreaDB;
    }
    else if(std::tolower((char) RAMS7200Address.at(0)) == 'i' || std::tolower((char) RAMS7200Address.at(0)) == 'e'){ //Inputs
        return S7AreaPE;
    }
    else if(std::tolower((char) RAMS7200Address.at(0)) == 'q' || std::tolower((char) RAMS7200Address.at(0)) == 'a'){ //Outputs
        return S7AreaPA;
    }
    else if(std::tolower((char) RAMS7200Address.at(0)) == 'm' || std::tolower((char) RAMS7200Address.at(0)) == 'f'){ //Flag memory
        return S7AreaMK;
    }
    else if(std::tolower((char) RAMS7200Address.at(0)) == 't'){ //Timers
        return S7AreaTM;
    }
    else if(std::tolower((char) RAMS7200Address.at(0)) == 'c' || std::tolower((char) RAMS7200Address.at(0)) == 'z'){ //Counters
        return S7AreaCT;
    }
    return -1; //invalid
}


bool RAMS7200LibFacade::RAMS7200AddressIsValid(std::string RAMS7200Address){
    return RAMS7200AddressGetArea(RAMS7200Address)!=-1 && 
    RAMS7200AddressGetWordLen(RAMS7200Address)!=-1 &&
    RAMS7200AddressGetAmount(RAMS7200Address)!=-1 &&
    RAMS7200AddressGetStart(RAMS7200Address)!=-1;
}

int RAMS7200LibFacade::RAMS7200AddressGetAmount(std::string RAMS7200Address)
{
    if(RAMS7200Address.length() < 2){
        return -1; //invalid
    }
    else if(std::tolower((char) RAMS7200Address.at(1)) == 'b'){ //VB can be words or strings if it contains a '.'
        if(RAMS7200Address.find_first_of('.') != std::string::npos){
            return (int) std::stoi(RAMS7200Address.substr(RAMS7200Address.find('.')+1));
        }
        else{
            return 1;
        }
    }
    else if(std::tolower((char) RAMS7200Address.at(1)) == 'w' || std::tolower((char) RAMS7200Address.at(1)) == 'd'){
        return 1;
    }
    else{ //bit addressing like V255.3
        return 1;
    }
    return 0; //dummy
}

int RAMS7200LibFacade::RAMS7200AddressGetBit(std::string RAMS7200Address)
{
    if(RAMS7200Address.length() < 2){
        return -1; //invalid
    }
    else if(RAMS7200AddressGetWordLen(RAMS7200Address) == S7WLBit && RAMS7200Address.find_first_of('.') != std::string::npos){
        return (int) std::stoi(RAMS7200Address.substr(RAMS7200Address.find('.')+1));
    }
    else{
        return 0; //N/A
    }
    return 0; //dummy
}

int RAMS7200LibFacade::RAMS7200DataSizeByte(int WordLength)
{
     switch (WordLength){
          case S7WLBit     : return 1;  // S7 sends 1 byte per bit
          case S7WLByte    : return 1;
          case S7WLWord    : return 2;
          case S7WLDWord   : return 4;
          case S7WLReal    : return 4;
          case S7WLCounter : return 2;
          case S7WLTimer   : return 2;
          default          : return 0;
     }
}

void RAMS7200LibFacade::RAMS7200DisplayTS7DataItem(PS7DataItem item)
{
    //hexdump(item->pdata  , sizeof(item->pdata));
    switch(item->WordLen){
        case S7WLByte:
            if(item->Amount>1){
                std::string strVal( reinterpret_cast<char const*>(item->pdata));
                //printf("-->read valus as string :'%s'\n", strVal.c_str());
            }
            else{
                uint8_t byteVal;
                std::memcpy(&byteVal, item->pdata  , sizeof(uint8_t));
                //printf("-->read value as byte : %d\n", byteVal);
            }
            break;
        case S7WLWord:
            uint16_t wordVal;
            std::memcpy(&wordVal, item->pdata  , sizeof(uint16_t));
            //printf("-->read value as word : %d\n", __bswap_16(wordVal));
            break;
        case S7WLReal:{
            float realVal;
            u_char f[] = { static_cast<byte*>(item->pdata)[3], static_cast<byte*>(item->pdata)[2], static_cast<byte*>(item->pdata)[1], static_cast<byte*>(item->pdata)[0]};
            std::memcpy(&realVal, f, sizeof(float));
            //printf("-->read value as real : %.3f\n", realVal);
            }
            break;
        case S7WLBit:
            uint8_t bitVal;
            std::memcpy(&bitVal, item->pdata  , sizeof(uint8_t));
            //printf("-->read value as bit : %d\n", bitVal);      
            break;
    }
}

TS7DataItem RAMS7200LibFacade::RAMS7200TS7DataItemFromAddress(std::string RAMS7200Address){
    TS7DataItem item;
    item.Area     = RAMS7200AddressGetArea(RAMS7200Address);
    item.WordLen  = RAMS7200AddressGetWordLen(RAMS7200Address);
    item.DBNumber = 1;
    item.Start    = item.WordLen == S7WLBit ? (RAMS7200AddressGetStart(RAMS7200Address)*8)+RAMS7200AddressGetBit(RAMS7200Address) : RAMS7200AddressGetStart(RAMS7200Address);
    item.Amount   = RAMS7200AddressGetAmount(RAMS7200Address);
    item.pdata   = new char[RAMS7200DataSizeByte(item.WordLen )*item.Amount];
    return item;
}

float ReverseFloat( const float inFloat )
{
   float retVal;
   char *floatToConvert = ( char* ) & inFloat;
   char *returnFloat = ( char* ) & retVal;

   // swap the bytes into a temporary buffer
   returnFloat[0] = floatToConvert[3];
   returnFloat[1] = floatToConvert[2];
   returnFloat[2] = floatToConvert[1];
   returnFloat[3] = floatToConvert[0];

   return retVal;
}

void RAMS7200LibFacade::RAMS7200ReadWriteMaxN(std::vector <std::pair<std::string, void *>> validVars, uint N, int PDU_SZ, int VAR_OH, int MSG_OH, int rorw) {
    try{
        uint last_index = 0;
        uint to_send = 0;

        TS7DataItem *item = new TS7DataItem[validVars.size()];

        for(uint i = 0; i < validVars.size(); i++) {
           // Common::Logger::globalInfo(Common::Logger::L1,"Getting item with address", validVars[i].first.c_str());
            item[i] = RAMS7200TS7DataItemFromAddress(validVars[i].first);
            
            if(rorw == 1) {
                int memSize = (RAMS7200DataSizeByte(item[i].WordLen )*item[i].Amount);

                if(RAMS7200DataSizeByte(item[i].WordLen) == 2) {
                    std::memcpy(item[i].pdata , validVars[i].second, sizeof(int16_t));
                } else if (RAMS7200DataSizeByte(item[i].WordLen) == 4) {
                    std::memcpy(item[i].pdata , validVars[i].second, sizeof(float));
                } else {
                    //case string
                    std::memcpy(item[i].pdata, validVars[i].second, memSize);
                }
            }
        }

        int retOpt;

        int curr_sum;

        last_index = 0;
        while(last_index < validVars.size()) {
            to_send = 0;
            curr_sum = 0;
            
            uint i;
            for(i = last_index; i < validVars.size(); i++) {
                
                if( curr_sum + (((RAMS7200DataSizeByte(item[i].WordLen)) * item[i].Amount) + VAR_OH) < ( PDU_SZ - MSG_OH ) ) {
                    to_send++;
                    curr_sum += ((RAMS7200DataSizeByte(item[i].WordLen)) * item[i].Amount) + VAR_OH;
                } else{
                    break;
                }

                if(to_send == N) { //Request upto N variables
                    break;
                }
            }

            if(to_send == 0) {
                //This means that the current variable has a mem size > PDU. Call with ReadArea 
                //printf("To read a single variable with index %d and total size %d\n", last_index, ((RAMS7200DataSizeByte(item[i].WordLen)) * item[i].Amount));
                to_send += 1;
                curr_sum = ((RAMS7200DataSizeByte(item[i].WordLen)) * item[i].Amount) + VAR_OH + MSG_OH;

                if(rorw == 0)
                    retOpt = _client->ReadArea(item[last_index].Area, item[last_index].DBNumber, item[last_index].Start, item[last_index].Amount, item[last_index].WordLen, item[last_index].pdata);
                else
                    retOpt = _client->WriteArea(item[last_index].Area, item[last_index].DBNumber, item[last_index].Start, item[last_index].Amount, item[last_index].WordLen, item[last_index].pdata);

            } else {
                //printf("To read %d variables, total requesting incl overheads: %d\n", to_send, curr_sum+MSG_OH);
                //printf("To read variables from range %d to %d \n", last_index, last_index + to_send);

                if(rorw == 0)
                    retOpt = _client->ReadMultiVars(&(item[last_index]), to_send);
                else {
                    retOpt = _client->WriteMultiVars(&(item[last_index]), to_send);
                }
            }

            if( retOpt == 0) {
                //printf("Read/Write OK. ");
                //printf("Read/Write %d items\n", to_send);

                if(rorw == 0) {
                    Common::Logger::globalInfo(Common::Logger::L3, "Read OK");
                
                    for(uint i = last_index; i < last_index + to_send; i++) {
                        int a;
                        std::memcpy(&a, validVars[i].second, sizeof(int));    
                        this->_consumeCB(_ip, validVars[i].first, std::to_string(a), reinterpret_cast<char*>(item[i].pdata));
                    }
                } else {
                    Common::Logger::globalInfo(Common::Logger::L1, "Write OK");
                }
            }
            else{
                if(rorw == 0) {
                    //printf("-->Read NOK!, Tried to read %d elements with total requesting size: %d .retOpt is %d\n", to_send, curr_sum + MSG_OH, retOpt);
                    Common::Logger::globalInfo(Common::Logger::L1, "-->Read NOK");
                    readFailures++;
                }
                else {
                    //printf("-->Write NOK!, Tried to write %d elements with total requesting size: %d .retOpt is %d\n", to_send, curr_sum + MSG_OH, retOpt);
                    Common::Logger::globalInfo(Common::Logger::L1, "-->Write NOK");
                }
            }
            
            last_index += to_send;
        }

    }
    catch(std::exception& e){
        printf("Exception in read function\n");
        Common::Logger::globalWarning(__PRETTY_FUNCTION__," Read invalid. Encountered Exception.");
        //Common::Logger::globalError(e.what());
    }
}


int RAMS7200LibFacade::getByteSizeFromAddress(std::string RAMS7200Address)
{
    TS7DataItem item = RAMS7200TS7DataItemFromAddress(RAMS7200Address);
    return (RAMS7200DataSizeByte(item.WordLen )*item.Amount);
}


void RAMS7200LibFacade::FileSharingTask(char* ip, int port) {
    
    FSThreadRunning = true;
    Common::Logger::globalInfo(Common::Logger::L1,"File Sharing Thread::Inside thread, Requested Touch Panel IP is", ip);

    int iRetSend, iRetRecv;
    FILE *fpUser;
    int count;
    TS7DataItem TouchPan_Conn_Stat_item;

    int socket_desc = -1;
    bool switch_to_event;

    char ack_drv[] = "##DRV_ACK##\n\n";
    char ack_pnl[] = "##PNL_ACK##";

    bool sock_err = false;
    struct sockaddr_in server_addr;
    
    //memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port); //host to network short
    //inet_aton(ip, &server_addr.sin_addr); //from dots and numbers to in_addr
    server_addr.sin_addr.s_addr = inet_addr(ip);

    //Set timeout for 2 minutes onr receive operations
    struct timeval tv;                                                              

    tv.tv_sec = 120;
    tv.tv_usec = 0;  

    int connect_try_count;

    connect_try_count = 0;
    
    //Always ready and trying to connect	
    while(1) {
        while(RAMS7200Resources::getDisableCommands()){
            // If the Server is Passive (for redundant systems)
            std::this_thread::sleep_for(std::chrono::seconds(1));

            {
                std::unique_lock<std::mutex> lck(mutex_);
                
                if(stopCurrentFSThread) {
                    Common::Logger::globalInfo(Common::Logger::L1, "File Sharing Thread::File Sharing Thread asked to stop. Exiting ... IP being served by this thread was : ", ip);
                    FSThreadRunning = false;
                    CV_SwitchFSThread.notify_one();
                    delete[] ip;
                    return;
                }
            }

        };

        Common::Logger::globalInfo(Common::Logger::L1, "FSThread: Writing true to DP for touch panel connection erorr status for Panel IP : ", ip);
        touch_panel_conn_error = true;
        TouchPan_Conn_Stat_item = RAMS7200TS7DataItemFromAddress("touchConnError");
        memcpy(TouchPan_Conn_Stat_item.pdata, &touch_panel_conn_error , sizeof(bool));

        this->_consumeCB(_ip, "touchConError", "", reinterpret_cast<char*>(TouchPan_Conn_Stat_item.pdata));

        {
            std::unique_lock<std::mutex> lck(mutex_);
            
            if(stopCurrentFSThread) {
                Common::Logger::globalInfo(Common::Logger::L1, "File Sharing Thread::File Sharing Thread asked to stop. Exiting ...  IP being served by this thread was : ", ip);
                FSThreadRunning = false;
                CV_SwitchFSThread.notify_one();
                close(socket_desc);
                delete[] ip;
                return;
            }
        }

        Common::Logger::globalInfo(Common::Logger::L2, "FSThread: Connecting to ip", ip);
        Common::Logger::globalInfo(Common::Logger::L2, "on port", std::to_string(port).c_str()); 

        socket_desc = socket(AF_INET, SOCK_STREAM, 0);

         if(socket_desc == -1) {
            Common::Logger::globalInfo(Common::Logger::L1, "File Sharing Thread::Error establishing socket");
            //TODO: Raise Alarm
            return;
        }
    
        Common::Logger::globalInfo(Common::Logger::L1, "File Sharing Thread::Socket created to try to connect to IP: \n", ip);	

        //Setting socket non blocking for the connect call.
        int socket_desc_flagbf;
        
        bool nonblock;
        int rc;

        nonblock = false;
        bool try_again, timed_out;

        try_again = false;
        timed_out = false;
        rc = 0;

        if( (socket_desc_flagbf = fcntl(socket_desc, F_GETFL, 0) >= 0 )) {
            
            if(fcntl(socket_desc, F_SETFL, socket_desc_flagbf | O_NONBLOCK) >= 0) {

                nonblock = true;

                // Start connecting (asynchronously)
                do {
                    if ( connect(socket_desc, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0 ) {
                        // Did connect return an error? If so, we'll try again after some time.
                        if ((errno != EWOULDBLOCK) && (errno != EINPROGRESS)) {
                            try_again = true;
                            Common::Logger::globalInfo(Common::Logger::L1, "FSThread : Non blocking connect failed");
                        }
                        // Otherwise, we'll wait for it to complete.
                        else {
                            // Set a deadline timestamp 'timeout' ms from now (needed b/c poll can be interrupted)
                            struct timespec now;
                            if( clock_gettime(CLOCK_MONOTONIC, &now) < 0 ) { 
                                try_again = true; 
                                break; 
                            }
                            struct timespec deadline = { .tv_sec = now.tv_sec + 10, .tv_nsec = now.tv_nsec }; //Set 10 second timeout
                            // Wait for the connection to complete.
                            Common::Logger::globalInfo(Common::Logger::L1, "FSThread : Waiting upto 10 seconds to connect to IP", ip);
                            do {
                                // Calculate how long until the deadline
                                if(clock_gettime(CLOCK_MONOTONIC, &now)<0) { 
                                    try_again = true; 
                                    break; 
                                }

                                int ms_until_deadline = (int)(  (deadline.tv_sec  - now.tv_sec)*1000l
                                                            + (deadline.tv_nsec - now.tv_nsec)/1000000l);
                                if(ms_until_deadline < 0) { 
                                    rc=0; 
                                    timed_out = true;
                                    try_again = true;
                                    break;
                                }
                                // Wait for connect to complete (or for the timeout deadline)
                                struct pollfd pfds[] = { { .fd = socket_desc, .events = POLLOUT } }; //Waiting for writing access on the socket descriptor
                                
                                rc = poll(pfds, 1, ms_until_deadline);
                                // If poll 'succeeded', make sure it *really* succeeded
                                if(rc > 0) {
                                    int error = 0; 
                                    
                                    socklen_t len = sizeof(error);
                                    
                                    int retval = getsockopt(socket_desc, SOL_SOCKET, SO_ERROR, &error, &len);
                                    
                                    if( retval == 0 ) 
                                        errno = error;
                                    
                                    if( error != 0) 
                                        rc = -1;
                                }
                            } while ( rc == -1 && errno == EINTR);
                            // Did poll timeout? If so, fail.
                            if(rc==0) {
                                timed_out = true;
                                try_again = true;
                            }

                            if(rc == -1 && !timed_out) {
                                try_again = true;
                            }
                        }
                    }
                } while(0);
            }
        }

        if(nonblock and try_again) {
            //Close connection and connect again after some time

            if(timed_out)
                Common::Logger::globalInfo(Common::Logger::L1, "FSThread: Could not connect in 10 seconds to IP\n", ip);
            else
                Common::Logger::globalInfo(Common::Logger::L1, "FSThread: Error in nonblocking connect call or in getting current time for IP\n", ip);
            connect_try_count++;
            close(socket_desc);

            if(connect_try_count > 3) {
                //Tried three times consecutively. 
                Common::Logger::globalInfo(Common::Logger::L1, "FSThread: Trying Again in 10 seconds\n");
                //Raise alarm with higher severity. Sleep for 10 seconds.
                std::this_thread::sleep_for(std::chrono::seconds(10));
            } else {
                Common::Logger::globalInfo(Common::Logger::L1, "FSThread: Trying Again in 4 seconds\n");
                //TODO: Raise Alarm
                std::this_thread::sleep_for(std::chrono::seconds(4));
            }

            continue;
        }

        if( !nonblock ) {
            //Could not set the socket as non blocking. Continue with blocking connect
            if( connect(socket_desc, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0 ) {
                Common::Logger::globalInfo(Common::Logger::L1, "FSThread: Error in connecting (blocking) to Touch Panel for IP: \n", ip);
                connect_try_count++;
                close(socket_desc);

                if(connect_try_count > 3) {
                    //Tried three times consecutively. 
                    Common::Logger::globalInfo(Common::Logger::L1, "Trying Again in 10 seconds\n");
                    //Raise alarm with higher severity. Sleep for 10 seconds.
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                } else {
                    Common::Logger::globalInfo(Common::Logger::L1, "Trying Again in 4 seconds\n");
                    //TODO: Raise Alarm
                    std::this_thread::sleep_for(std::chrono::seconds(4));
                }
                
                continue;
            }
        }

        // Restore original O_NONBLOCK state
        if( nonblock && ( fcntl(socket_desc ,F_SETFL,socket_desc_flagbf) < 0 ) ) {
            //Close connection and connect again after some time
             Common::Logger::globalInfo(Common::Logger::L1, "Error in making socket blocking again\n");
            connect_try_count++;
            close(socket_desc);

            if(connect_try_count > 3) {
                //Tried three times consecutively. 
                Common::Logger::globalInfo(Common::Logger::L1, "Trying Again in 10 seconds\n");
                //Raise alarm with higher severity. Sleep for 10 seconds.
                std::this_thread::sleep_for(std::chrono::seconds(10));
            } else {
                Common::Logger::globalInfo(Common::Logger::L1, "Trying Again in 4 seconds\n");
                //TODO: Raise Alarm
                std::this_thread::sleep_for(std::chrono::seconds(4));
            }
            
            continue;
        }

        // Success
        
        setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
        
        Common::Logger::globalInfo(Common::Logger::L1, "FSThread: Connected to Touch Panel on IP: ", ip);

        connect_try_count = 0;

        while(1) { //Connected to client - keep waiting for either User message or Logfile message
            int bufsize = 1024;
            char buffer[bufsize], subbuffer[12];

            Common::Logger::globalInfo(Common::Logger::L1, "FSThread: Writing false to DP for touch panel connection erorr status for TP IP: ", ip);
            touch_panel_conn_error = false;
            TouchPan_Conn_Stat_item = RAMS7200TS7DataItemFromAddress("touchConnError");
            memcpy(TouchPan_Conn_Stat_item.pdata, &touch_panel_conn_error , sizeof(bool));
            this->_consumeCB(_ip, "touchConError", "", reinterpret_cast<char*>(TouchPan_Conn_Stat_item.pdata));

             {
                std::unique_lock <std::mutex> lck(mutex_);
                
                if(stopCurrentFSThread) {
                    Common::Logger::globalInfo(Common::Logger::L1, "FSThread: File Sharing Thread asked to stop. Exiting ... was serving IP: ",ip);
                    
                    Common::Logger::globalInfo(Common::Logger::L1, "FSThread: Writing true to DP for touch panel connection erorr status");
                    touch_panel_conn_error = true;
                    TouchPan_Conn_Stat_item = RAMS7200TS7DataItemFromAddress("touchConnError");
                    memcpy(TouchPan_Conn_Stat_item.pdata, &touch_panel_conn_error , sizeof(bool));
                    this->_consumeCB(_ip, "touchConError", "", reinterpret_cast<char*>(TouchPan_Conn_Stat_item.pdata));

                    FSThreadRunning = false;
                    CV_SwitchFSThread.notify_one();
                    close(socket_desc);
                    delete[] ip;
                    return;
                }
            }

            if(RAMS7200Resources::getDisableCommands()) {
                //Driver in passive mode. (for redundant systems)
                close(socket_desc);
                break;
            }

            Common::Logger::globalInfo(Common::Logger::L2, "FSThread: Waiting upto 2 minutes to receive number for handshake \n");
            memset(buffer, 0, sizeof(buffer));

            if( recv(socket_desc, buffer, bufsize, 0) <= 0 ) {
                Common::Logger::globalInfo(Common::Logger::L1, "FSThread: Error in receiving number for handshake from Touch Panel. \nDisconnecting.\n");
                close(socket_desc);
                break;
            }
        
            int rand_rcv = atoi(buffer);
            Common::Logger::globalInfo(Common::Logger::L2, "FSThread: Received number from client", std::to_string(rand_rcv).c_str());

            sprintf(buffer, "%d",rand_rcv+1);
        
            Common::Logger::globalInfo(Common::Logger::L2, "FSThread: Sending received number + 1 to client for hanshake", buffer);
            
            if( send(socket_desc, buffer, strlen(buffer), 0) < 0 ) {
                Common::Logger::globalInfo(Common::Logger::L1, "Sending of rand + 1 for connection initiation failed\n");
                Common::Logger::globalInfo(Common::Logger::L1, "Closing connection\n");
                close(socket_desc);
                break;
            }

            Common::Logger::globalInfo(Common::Logger::L2, "FSThread: Number Sent");

            Common::Logger::globalInfo(Common::Logger::L1, "FSThread: Waiting upto 2 minutes to receive message for treatment\n");
            memset(buffer, 0, sizeof(buffer));
            //Receive information about treatment 
            if( recv(socket_desc, buffer, bufsize, 0) <= 0 ) {
                Common::Logger::globalInfo(Common::Logger::L1, "Error in receiving message for treatment from Touch Panel. \nDisconnecting.\n");
                close(socket_desc);
                break;
            }

            Common::Logger::globalInfo(Common::Logger::L1, "Received message: ", buffer);

            if( strcmp(buffer, "User") == 0 ) {
                Common::Logger::globalInfo(Common::Logger::L1, "Accomodating User File Synchronization Treatment\n");

                fpUser = fopen( (Common::Constants::getUserFilePath()).c_str(), "r");
                
                Common::Logger::globalInfo(Common::Logger::L1, "User File Location is:", (Common::Constants::getUserFilePath()).c_str());

                if(fpUser == NULL) {
                    Common::Logger::globalInfo(Common::Logger::L1, "Error in opening User File to send User data\n");
                    close(socket_desc);
                    break;
                }

                sock_err = false;

                unsigned char ct[8], key[8] = "123";
                symmetric_key skey;
                int err;
                char pt[9];

                /* schedule the key */
                if ((err = des_setup(key, /* the key we will use */
                                    8, /* key is 8 bytes (64-bits) long */
                                    0, /* 0 == use default # of rounds */
                                    &skey) /* where to put the scheduled key */
                                    ) != CRYPT_OK) {
                    Common::Logger::globalInfo(Common::Logger::L1, "Error in setting up the DES keys\n");
                    close(socket_desc);
                    break;
                }

                char temp[10];

                while(fgets(buffer, sizeof(buffer), fpUser)) {
                    Common::Logger::globalInfo(Common::Logger::L2, "Line read:\n %s",buffer);

                    for(unsigned int i=0; i<strlen(buffer); i+=8) {
                        memset(pt, 0, 8);
                        memset(ct, 0, 8);

                        if(strlen(buffer) - i > 8)
                            memcpy(pt, &buffer[i], 8); 
                        else 
                            memcpy(pt, &buffer[i], strlen(buffer) - i);

                        des_ecb_encrypt(reinterpret_cast<const unsigned char *>(pt), /* encrypt this 8-byte array */ct, /* store encrypted data here */ &skey); /* our previously scheduled key */

                        for(int i = 0; i<8; i ++) {
                            sprintf(temp, "%d\n", ct[i]);

                            iRetSend = send(socket_desc, temp, strlen(temp), 0);
                    
                            if(iRetSend <= 0) {
                                //Error in sending file content. Abort this Connection
                                Common::Logger::globalInfo(Common::Logger::L1, "Error in sending encrypted data.\n");
                                close(socket_desc);
                                fclose(fpUser);
                                sock_err = true;
                                break;
                            }
                        }
                        
                        if(sock_err) {
                            break; 
                        }
                        
                    }
                
                    if(sock_err) {
                        break; 
                    }
                }

                if(sock_err) {
                    break; 
                }

                sprintf(buffer, ack_drv);
                Common::Logger::globalInfo(Common::Logger::L1, "Sending final marker ##DRV_ACK## for User File\n");
                iRetSend = send(socket_desc, buffer, strlen(buffer), 0);
                
                if(iRetSend <= 0) {
                    //Error in sending file content. Abort this Connection
                    Common::Logger::globalInfo(Common::Logger::L1, "Error in sending final marker for UserFile.\n");
                    close(socket_desc);
                    fclose(fpUser);
                    break;
                }

                Common::Logger::globalInfo(Common::Logger::L1, "Succesfully sent User File\n");
                fclose(fpUser);
            /////////////////////////////-----------------------------------------------------------////////////////////////
            } else if( strcmp(buffer, "LogFile") == 0 ) {

                switch_to_event = false;

                sprintf(buffer, ack_drv);
                    
                iRetSend = send(socket_desc, buffer, strlen(buffer), 0); 		

                if(iRetSend <= 0) {
                    //Error in sending final marker for Log File. Abort this Connection
                    Common::Logger::globalInfo(Common::Logger::L1, "Error in sending marker for LogFile Message.\n");
                    close(socket_desc);
                    sock_err = true;
                    break;
                }
                    
                Common::Logger::globalInfo(Common::Logger::L1, "Sent confirmation marker of LogFile Message");

                Common::Logger::globalInfo(Common::Logger::L1, "Accomodating LogFile Treatment\n");	

                while(1) { //Keep receiving name of files
                    
                    Common::Logger::globalInfo(Common::Logger::L1, "Waiting to receive filename\n");

                    memset(buffer, 0, sizeof(buffer));
                    iRetRecv = recv(socket_desc, buffer, bufsize, 0);

                    if(	iRetRecv <= 0 ) {
                        Common::Logger::globalInfo(Common::Logger::L1, "Error in receiving name of file from touchpanel.\nDisconnecting.\n");
                        Common::Logger::globalInfo(Common::Logger::L1, "Return code of recv was ", std::to_string(iRetRecv).c_str());
                        Common::Logger::globalInfo(Common::Logger::L1, "Error number is", std::to_string(errno).c_str());
                        close(socket_desc);
                        break;
                    }

                    if(strcmp(buffer, "Event") == 0) { //Start Receiving Event files
                        switch_to_event = true;

                        sprintf(buffer, ack_drv);
                        iRetSend = send(socket_desc, buffer, strlen(buffer), 0); 		

                        if(iRetSend <= 0) {
                            //Error in sending final marker for Log File. Abort this Connection
                            Common::Logger::globalInfo(Common::Logger::L1, "Error in sending marker for LogFile Message.\n");
                            close(socket_desc);
                            sock_err = true;
                            break;
                        }
                        
                        Common::Logger::globalInfo(Common::Logger::L1, "Receiving event files from now\n");
                        continue;
                    }

                    if(strlen(buffer) >= strlen(ack_pnl))
                        memcpy( subbuffer, &buffer[strlen(buffer) - strlen(ack_pnl)], strlen(ack_pnl));

                    subbuffer[strlen(ack_pnl)] = '\0';
                    
                    
                    if( strcmp(ack_pnl, subbuffer) == 0 ) {
                        Common::Logger::globalInfo(Common::Logger::L1, "LogFile treatment successfully finished.\n");
                        break;
                    }

                    Common::Logger::globalInfo(Common::Logger::L1, "Received Full File Name: ", buffer);
                    

                    char *nFile = new char[75];

                    if(switch_to_event)
                        strcpy(nFile, (Common::Constants::getEventFilePath()).c_str());
                    else
                        strcpy(nFile, (Common::Constants::getMeasFilePath()).c_str());
                    
                    memcpy(&(buffer[strlen(buffer) - 3]), "dat", 3); //Replace .log extension with .dat extension
                    strcat(nFile, buffer);
                
                    //fpLog = fopen(nFile, "w");
                    std::ofstream file(nFile); 
                    Common::Logger::globalInfo(Common::Logger::L1, "Received file location: ", nFile);


                    if(!file.is_open()) {
                        Common::Logger::globalInfo(Common::Logger::L1, "Error in creating new file for file reception from Touch Panel.\n");
                        close(socket_desc);
                        sock_err = true;
                        break;
                    }

                    sprintf(buffer, ack_drv);
                    
                    iRetSend = send(socket_desc, buffer, strlen(buffer), 0); 		

                    if(iRetSend <= 0) {
                        //Error in sending final marker for Log File. Abort this Connection
                        Common::Logger::globalInfo(Common::Logger::L1, "Error in sending marker for File Name reception.\n");
                        close(socket_desc);
                        //fclose(fpLog);
                        file.close();
                        remove(nFile);
                        sock_err = true;
                        break;
                    }
                    
                    Common::Logger::globalInfo(Common::Logger::L1, "Sent confirmation marker of file name reception: ##DRV_ACK##");

                    Common::Logger::globalInfo(Common::Logger::L2, "File content:\n\n");
                    
                    subbuffer[0] = '0';
                    count = 0;

                    sock_err = false;
                    
                    while( strcmp(ack_pnl, subbuffer) != 0 ) {
                        count++;
                        Common::Logger::globalInfo(Common::Logger::L2, "Inside loop\n");
                        Common::Logger::globalInfo(Common::Logger::L2, "Count is "+ CharString(count) + "and sizeof buffer is "+ CharString(sizeof(buffer)));	
                        //Common::Logger::globalInfo(Common::Logger::L1, "After memset of buffer to 0\n");
                        std::memset(buffer, 0, sizeof(buffer));

                        Common::Logger::globalInfo(Common::Logger::L2, "Before receive on the socket\n");
                        if( recv(socket_desc, buffer, bufsize, 0) <= 0) {
                            
                            Common::Logger::globalInfo(Common::Logger::L1, "Error in socket connection.\n");
                            close(socket_desc);
                            sock_err = true;
                            file.close();
                            //fclose(fpLog); 
                            remove(nFile);
                            break;
                        }
                        
                        //Common::Logger::globalInfo(Common::Logger::L1, "Packet number "<<count<<" is : "<<buffer;
                        
                        //Common::Logger::globalInfo(Common::Logger::L1, "Before next packet print\n");
                        Common::Logger::globalInfo(Common::Logger::L2, "Packet number ", std::to_string(count).c_str());
                        Common::Logger::globalInfo(Common::Logger::L2, " is : ", buffer);

                        //Common::Logger::globalInfo(Common::Logger::L1, "strlen is "<<strlen(buffer));

                        if(strlen(buffer) >= strlen(ack_pnl))
                            memcpy( subbuffer, &buffer[strlen(buffer) - strlen(ack_pnl)], strlen(ack_pnl));

                        subbuffer[strlen(ack_pnl)] = '\0';

                        Common::Logger::globalInfo(Common::Logger::L2, "After packet print\n");
                        
                        Common::Logger::globalInfo(Common::Logger::L2, "Subbuffer is ",subbuffer);
                        if( strcmp("##PNL_ACK##", subbuffer) != 0 ) {
                            //fprintf( fpLog, "%s", buffer);
                            file<<buffer;	
                            Common::Logger::globalInfo(Common::Logger::L2, "Written to file\n");
                        } else {
                            buffer[strlen(buffer) - strlen(ack_pnl)] = '\0';
                            file<<buffer;	
                            //fprintf( fpLog, "%s", buffer);
                        } 		
                        Common::Logger::globalInfo(Common::Logger::L2, "After subbuffer comparison\n");
                        //Common::Logger::globalInfo(Common::Logger::L1, "Subbuffer is : "<<subbuffer));
                        /* 
                        if(count == 15) {
                            Common::Logger::globalInfo(Common::Logger::L1, "buffer[0] = "<<buffer[0]);
                            Common::Logger::globalInfo(Common::Logger::L1, "buffer[1] = "<<buffer[1]); 
                            Common::Logger::globalInfo(Common::Logger::L1, "buffer[2] = "<<buffer[2]); 
                            Common::Logger::globalInfo(Common::Logger::L1, "buffer[3] = "<<buffer[3]);
                            Common::Logger::globalInfo(Common::Logger::L1, "buffer[4] = "<<buffer[4]);
                            Common::Logger::globalInfo(Common::Logger::L1, "buffer[5] = "<<buffer[5]);
                            Common::Logger::globalInfo(Common::Logger::L1, "buffer[6] = "<<buffer[6]);
                            Common::Logger::globalInfo(Common::Logger::L1, "buffer[7] = "<<buffer[7]); 
                        }*/
                    }	
                    
                    if(sock_err) {
                        break;
                    }

                    Common::Logger::globalInfo(Common::Logger::L1, "File reading completed");
                
                    sprintf(buffer, "##DRV_ACK##\n\n");
                    
                    iRetSend = send(socket_desc, buffer, strlen(buffer), 0); 		

                    if(iRetSend <= 0) {
                        //Error in sending final marker for Log File. Abort this Connection
                        Common::Logger::globalInfo(Common::Logger::L1, "Error in sending final marker for Log File.\n");
                        close(socket_desc);
                        file.close();
                        //fclose(fpLog);
                        break;
                    }
                    
                    Common::Logger::globalInfo(Common::Logger::L1, "Sent confirmation marker of file receipt: ##DRV_ACK##");
                    
                    file.close();

                    {
                        std::unique_lock<std::mutex> lck(mutex_);
                        
                        if(stopCurrentFSThread) {
                            Common::Logger::globalInfo(Common::Logger::L1, "File Sharing Thread asked to stop. Exiting ...");

                            Common::Logger::globalInfo(Common::Logger::L1, "Writing true to DP for touch panel connection erorr status");
                            touch_panel_conn_error = true;
                            TouchPan_Conn_Stat_item = RAMS7200TS7DataItemFromAddress("touchConnError");
                            memcpy(TouchPan_Conn_Stat_item.pdata, &touch_panel_conn_error , sizeof(bool));
                            this->_consumeCB(_ip, "touchConError", "", reinterpret_cast<char*>(TouchPan_Conn_Stat_item.pdata));

                            FSThreadRunning = false;
                            CV_SwitchFSThread.notify_all();
                            close(socket_desc);
                            delete[] ip;
                            return;
                        }
                    }

                    //fclose(fpLog);
                } //While loop - keep receiving log file names
            }

            if(sock_err) 
                break;
        } //Inner while(1) loop - waiting for hanshake initiation from the touch panel

        close(socket_desc);      	

    } //Outermost while(1) loop - always trying to connect.

    FSThreadRunning = false; //Control should never reach here
}

void RAMS7200LibFacade::startFileSharingThread(char* touchPanelIP) {

    std::thread startFileSharingT ([&](){
        {      
            std::unique_lock<std::mutex> lck(mutex_);

            if(stopCurrentFSThread) {
                touch_panel_ip = touchPanelIP;
                Common::Logger::globalInfo(Common::Logger::L1,"startFileSharingThread: There is already a thread waiting for the current thread to exit.\nUpdating the final touch panel IP address to: ", touchPanelIP);
                return;
            }
            
            else if(FSThreadRunning == true) {
                Common::Logger::globalInfo(Common::Logger::L1,"startFileSharingThread: Stopping the currently running File Sharing Thread");
                stopCurrentFSThread = true;
                touch_panel_ip = touchPanelIP;
                CV_SwitchFSThread.wait(lck, [&]() {return !FSThreadRunning;});
                Common::Logger::globalInfo(Common::Logger::L1,"startFileSharingThread: Currently running File Sharing Thread Stopped");
                stopCurrentFSThread = false;
            } 
            else {
                touch_panel_ip = touchPanelIP;
                Common::Logger::globalInfo(Common::Logger::L1,"startFileSharingThread: No threads for Touch Panel are running. Starting a new thread.");
            }
        }

        std::thread FileSharingThread (&RAMS7200LibFacade::FileSharingTask, this, touch_panel_ip, 20248);    
        FileSharingThread.detach();
    });

    startFileSharingT.detach();
}