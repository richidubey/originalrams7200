#include "RAMS7200Panel.hxx"
#include "Common/Logger.hxx"
#include "RAMS7200Resources.hxx"
#include "Common/Constants.hxx"
#include "RAMS7200Encryption.hxx"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>

RAMS7200Panel::RAMS7200Panel(RAMS7200MS& ms, queueToDPCallback cb)
    : ms(ms), _queueToDPCB(cb)
{
     Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Initialized RAMS7200Panel with TP IP: " + CharString(ms._tp_ip.c_str()));
}

void RAMS7200Panel::writeTouchConnErrDPE(bool val) {
    touch_panel_conn_error = val;
    
    Common::Logger::globalInfo(Common::Logger::L1,"FSThread: Touch panel connection erorr status for Panel IP : ", ms._tp_ip.c_str(), std::to_string(touch_panel_conn_error).c_str());
    auto pdata = new char[sizeof(bool)];
    memcpy(pdata, &touch_panel_conn_error , sizeof(bool));
    this->_queueToDPCB(ms._ip_combo + "$_touchConError", sizeof(bool), reinterpret_cast<char*>(pdata));
}

void RAMS7200Panel::FileSharingTask(int port) { // TODO: review this in depth
    
    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Start of FS thread, Requested Touch Panel IP is", ms._tp_ip.c_str());

    int iRetSend, iRetRecv;
    FILE *fpUser;
    int count;
    const char *ip = ms._tp_ip.c_str();

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
    server_addr.sin_addr.s_addr = inet_addr(ms._tp_ip.c_str());

    //Set timeout for 2 minutes on receive operations
    struct timeval tv;                                                              

    tv.tv_sec = 120;
    tv.tv_usec = 0;  

    int connect_try_count;

    connect_try_count = 0;
    
    //Always ready and trying to connect	
    while(ms._run) {
        while(RAMS7200Resources::getDisableCommands()){
            // If the Server is Passive (for redundant systems)
           sleep_for(std::chrono::seconds(1));
        };

        writeTouchConnErrDPE(true);


        Common::Logger::globalInfo(Common::Logger::L2, "FSThread: Connecting to touch panel ip:port", ms._tp_ip.c_str(), std::to_string(port).c_str());

        socket_desc = socket(AF_INET, SOCK_STREAM, 0);

         if(socket_desc == -1) {
            Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "FSThread: Error establishing socket for TP IP: ", ip);
            //TODO: Raise Alarm
            return;
        }
    
        Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "FSThread: Socket created to try to connect to IP: \n", ip);	

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
                            Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"FSThread : Non blocking connect failed for TP IP: ", ip);
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
                            Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"FSThread : Waiting upto 10 seconds to connect to IP", ip);
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
                            } while ( rc == -1 && errno == EINTR && ms._run.load());
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
                Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"FSThread: Could not connect in 10 seconds to IP\n", ip);
            else
                Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"FSThread: Error in nonblocking connect call or in getting current time for IP\n", ip);
            connect_try_count++;
            close(socket_desc);

            if(connect_try_count > 3) {
                //Tried three times consecutively. 
                Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"FSThread: Trying Again in 10 seconds to connect to TP IP: ",ip);
                //Raise alarm with higher severity. Sleep for 10 seconds.
                sleep_for(std::chrono::seconds(10));
            } else {
                Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "FSThread: Trying Again in 4 seconds to connect to TP IP: ", ip);
                //TODO: Raise Alarm
                sleep_for(std::chrono::seconds(4));
            }

            continue;
        }

        if( !nonblock ) {
            //Could not set the socket as non blocking. Continue with blocking connect
            if( connect(socket_desc, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0 ) {
                Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "FSThread: Error in connecting (blocking) to Touch Panel for IP: \n", ip);
                connect_try_count++;
                close(socket_desc);

                if(connect_try_count > 3) {
                    //Tried three times consecutively. 
                    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Trying Again in 10 seconds to TP IP: ", ip);
                    //Raise alarm with higher severity. Sleep for 10 seconds.
                    sleep_for( std::chrono::seconds(10));
                } else {
                    Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Trying Again in 4 seconds to TP IP: ", ip);
                    //TODO: Raise Alarm
                    sleep_for( std::chrono::seconds(4));
                }
                
                continue;
            }
        }

        // Restore original O_NONBLOCK state
        if( nonblock && ( fcntl(socket_desc ,F_SETFL,socket_desc_flagbf) < 0 ) ) {
            //Close connection and connect again after some time
             Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Error in making socket blocking again for TP IP", ip);
            connect_try_count++;
            close(socket_desc);

            if(connect_try_count > 3) {
                //Tried three times consecutively. 
                Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__, "Trying Again in 10 seconds for TP IP", ip);
                //Raise alarm with higher severity. Sleep for 10 seconds.
                sleep_for( std::chrono::seconds(10));
            } else {
                Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__, "Trying Again in 4 seconds for TP IP", ip);
                //TODO: Raise Alarm
                sleep_for( std::chrono::seconds(4));
            }
            
            continue;
        }

        // Success
        
        setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
        
        Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__, "FSThread: Connected to Touch Panel on IP: ", ip);

        connect_try_count = 0;

        while(ms._run) { //Connected to client - keep waiting for either User message or Logfile message
            int bufsize = 1024;
            char buffer[bufsize], subbuffer[12], lastMsg[bufsize];

            writeTouchConnErrDPE(false);


            if(RAMS7200Resources::getDisableCommands()) {
                //Driver in passive mode. (for redundant systems)
                close(socket_desc);
                break;
            }

            Common::Logger::globalInfo(Common::Logger::L2, __PRETTY_FUNCTION__, "FSThread: Waiting upto 2 minutes to receive number for handshake for TP IP", ip);
            memset(buffer, 0, sizeof(buffer));

            if( recv(socket_desc, buffer, bufsize, 0) <= 0 ) {
                Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__, "FSThread: Error in receiving number for handshake from Touch Panel so disconnecting from TP IP", ip);
                close(socket_desc);
                break;
            }
        
            int rand_rcv = atoi(buffer);
            Common::Logger::globalInfo(Common::Logger::L2, __PRETTY_FUNCTION__,"FSThread: Received number from client", std::to_string(rand_rcv).c_str());

            sprintf(buffer, "%d",rand_rcv+1);
        
            Common::Logger::globalInfo(Common::Logger::L2, __PRETTY_FUNCTION__,"FSThread: Sending received number + 1 to client for handshake", buffer);
            
            if( send(socket_desc, buffer, strlen(buffer), 0) < 0 ) {
                Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Sending of rand + 1 for connection initiation failed hence Closing connection for TP IP", ip);
                close(socket_desc);
                break;
            }

            Common::Logger::globalInfo(Common::Logger::L2, "FSThread: Number Sent");

            Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"FSThread: Waiting upto 2 minutes to receive message for treatment for TP IP: ", ip);
            memset(buffer, 0, sizeof(buffer));
            //Receive information about treatment 
            if( recv(socket_desc, buffer, bufsize, 0) <= 0 ) {
                Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Error in receiving message for treatment from Touch Panel so disconnecting from TP IP: ", ip);
                close(socket_desc);
                break;
            }

            Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Received message: " + CharString(buffer) + "from TP IP: ", ip);

            if( strcmp(buffer, "User") == 0 ) {
                Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__, "Accomodating User File Synchronization Treatment for TP IP:", ip);

                fpUser = fopen( (Common::Constants::getUserFilePath()).c_str(), "r");
                
                Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__, "User File Location is:" + CharString((Common::Constants::getUserFilePath()).c_str()) + "For TP IP: ", ip);

                if(fpUser == NULL) {
                    Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Error in opening User File to send User data for TP IP : ", ip);
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
                    Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Error in setting up the DES keys for TP IP: ",ip);
                    close(socket_desc);
                    break;
                }

                char temp[10];

                while(fgets(buffer, sizeof(buffer), fpUser)) {
                    //Common::Logger::globalInfo(Common::Logger::L2, "Line read:\n %s",buffer);

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
                                Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Error in sending encrypted data to TP IP: ", ip);
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
                Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Sending final marker ##DRV_ACK## for User File to TP IP", ip);
                iRetSend = send(socket_desc, buffer, strlen(buffer), 0);
                
                if(iRetSend <= 0) {
                    //Error in sending file content. Abort this Connection
                    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Error in sending final marker for UserFile to TP IP: ",ip);
                    close(socket_desc);
                    fclose(fpUser);
                    break;
                }

                Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Succesfully sent User File to TP IP: ",ip);
                fclose(fpUser);
            /////////////////////////////-----------------------------------------------------------////////////////////////
            } else if( strcmp(buffer, "LogFile") == 0 ) {

                switch_to_event = false;

                sprintf(buffer, ack_drv);
                    
                iRetSend = send(socket_desc, buffer, strlen(buffer), 0); 		

                if(iRetSend <= 0) {
                    //Error in sending final marker for Log File. Abort this Connection
                    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Error in sending marker for LogFile Message to TP IP: ", ip);
                    close(socket_desc);
                    sock_err = true;
                    break;
                }
                    
                Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Sent confirmation marker of LogFile Message to TP IP: ",ip);

                Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Accomodating LogFile Treatment of TP IP: ", ip);	

                char nFile[75];

                while(1) { //Keep receiving name of files
                    
                    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Waiting to receive filename from TP IP: ", ip);

                    memset(buffer, 0, sizeof(buffer));
                    iRetRecv = recv(socket_desc, buffer, bufsize, 0);

                    if(	iRetRecv <= 0 ) {
                        Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Error in receiving name of file from touchpanel so Disconnecting from TP IP: ",ip);
                        Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Return code of recv was " + CharString(std::to_string(iRetRecv).c_str()) + " from TP IP: ", ip);
                        Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Error number is" + CharString(std::to_string(errno).c_str())+" for TP IP: ", ip);
                        close(socket_desc);
                        break;
                    }

                    if(strcmp(buffer, "Event") == 0) { //Start Receiving Event files
                        switch_to_event = true;

                        sprintf(buffer, ack_drv);
                        iRetSend = send(socket_desc, buffer, strlen(buffer), 0); 		

                        if(iRetSend <= 0) {
                            //Error in sending final marker for Log File. Abort this Connection
                            Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Error in sending marker for LogFile Message to TP IP:", ip);
                            close(socket_desc);
                            sock_err = true;
                            break;
                        }
                        
                        Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Receiving event files from now from TP IP: ",ip);
                        continue;
                    }

                    if(strlen(buffer) >= strlen(ack_pnl))
                        memcpy( subbuffer, &buffer[strlen(buffer) - strlen(ack_pnl)], strlen(ack_pnl));

                    subbuffer[strlen(ack_pnl)] = '\0';
                    
                    
                    if( strcmp(ack_pnl, subbuffer) == 0 ) {
                        Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "LogFile treatment successfully finished for TP IP:", ip);
                        break;
                    }

                    Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,CharString("From TP IP: ") + ip + CharString("Received Full File Name: "), buffer);

                    if(switch_to_event)
                        strcpy(nFile, (Common::Constants::getEventFilePath()).c_str());
                    else
                        strcpy(nFile, (Common::Constants::getMeasFilePath()).c_str());
                    
                    memcpy(&(buffer[strlen(buffer) - 3]), "dat", 3); //Replace .log extension with .dat extension
                    strcat(nFile, buffer);
                
                    //fpLog = fopen(nFile, "w");
                    std::ofstream file(nFile); 
                    Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Received file location: " + CharString(nFile) + " From TP IP: ",ip);


                    if(!file.is_open()) {
                        Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Error in creating new file for file reception from Touch Panel for TP IP", ip);
                        close(socket_desc);
                        sock_err = true;
                        break;
                    }

                    sprintf(buffer, ack_drv);
                    
                    iRetSend = send(socket_desc, buffer, strlen(buffer), 0); 		

                    if(iRetSend <= 0) {
                        //Error in sending final marker for Log File. Abort this Connection
                        Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Error in sending marker for File Name reception for TP IP:", ip);
                        close(socket_desc);
                        //fclose(fpLog);
                        file.close();
                        sock_err = true;
                        break;
                    }
                    
                    Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Sent confirmation marker of file name reception: ##DRV_ACK## to TP IP:",ip);

                    Common::Logger::globalInfo(Common::Logger::L2, "File content:\n\n");
                    
                    subbuffer[0] = '0';
                    count = 0;

                    sock_err = false;
                    
                    while( strcmp(ack_pnl, subbuffer) != 0 ) {
                        count++;
                        Common::Logger::globalInfo(Common::Logger::L2, "Inside loop\n");
                        Common::Logger::globalInfo(Common::Logger::L2, "Count is "+ CharString(count) + "and sizeof buffer is "+ (std::to_string(sizeof(buffer))).c_str());	
                        //Common::Logger::globalInfo(Common::Logger::L1, "After memset of buffer to 0\n");
                        std::memset(buffer, 0, sizeof(buffer));

                        Common::Logger::globalInfo(Common::Logger::L2, "Before receive on the socket\n");
                        if( recv(socket_desc, buffer, bufsize - 1 , 0) <= 0) { //Keep space for 1 termination char
                            Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Error in socket connection with TP IP: ",ip);
                            close(socket_desc);
                            sock_err = true;
                            file.close();
                            break;
                        }
                        
                        //Common::Logger::globalInfo(Common::Logger::L1, "Packet number "<<count<<" is : "<<buffer;
                        
                        //Common::Logger::globalInfo(Common::Logger::L1, "Before next packet print\n");
                        Common::Logger::globalInfo(Common::Logger::L2, "Packet number ", std::to_string(count).c_str());
                        Common::Logger::globalInfo(Common::Logger::L2, " is : ", buffer);

                        Common::Logger::globalInfo(Common::Logger::L2, "strlen of buffer is :", std::to_string(strlen(buffer)).c_str());

                        //Common::Logger::globalInfo(Common::Logger::L1, "strlen is "<<strlen(buffer));

                        if(strlen(buffer) >= strlen(ack_pnl))
                            memcpy( subbuffer, &buffer[strlen(buffer) - strlen(ack_pnl)], strlen(ack_pnl));
                        else {
                            if(strlen(lastMsg) >= (strlen(ack_pnl) - strlen(buffer))) {
                                Common::Logger::globalInfo(Common::Logger::L2, "lastmsg is \n: ", lastMsg);
                                memcpy(subbuffer, &lastMsg[strlen(lastMsg) - (strlen(ack_pnl) - strlen(buffer))], strlen(ack_pnl) - strlen(buffer));
                                strcpy(&subbuffer[strlen(ack_pnl) - strlen(buffer)], buffer);
                            }
                        }

                        strcpy(lastMsg, buffer);
                        buffer[strlen(buffer)] = '\0';
                        lastMsg[strlen(buffer)] = '\0';
                        subbuffer[strlen(ack_pnl)] = '\0';

                        Common::Logger::globalInfo(Common::Logger::L2, "After packet print\n");
                        
                        Common::Logger::globalInfo(Common::Logger::L2, "Subbuffer is ",subbuffer);
                        if( strcmp("##PNL_ACK##", subbuffer) != 0 ) {
                            //fprintf( fpLog, "%s", buffer);
                            file<<buffer;	
                            Common::Logger::globalInfo(Common::Logger::L2, "Written to file\n");
                        } else {
                            if(strlen(buffer) >= strlen(ack_pnl)) {                           
                                buffer[strlen(buffer) - strlen(ack_pnl)] = '\0';
                                file<<buffer;	
                            } else {
                                std::ifstream rFile(nFile); 
                                std::stringstream dupBuffer;
                                dupBuffer << rFile.rdbuf();

                                std::string contents = dupBuffer.str();

                                rFile.close();
                                
                                for(unsigned int i=0; i <(strlen(ack_pnl) - strlen(buffer)); i++)
                                    contents.pop_back();
                                
                                file.seekp(0);
                                file<<contents;
                                Common::Logger::globalInfo(Common::Logger::L2, CharString("Did not write this msg to file and deleted the last ") + (std::to_string((strlen(ack_pnl) - strlen(buffer)))).c_str() + CharString(" characters from the file\n"));
                            }

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

                    Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"File reading completed for TP IP", ip);
                
                    sprintf(buffer, "##DRV_ACK##\n\n");
                    
                    iRetSend = send(socket_desc, buffer, strlen(buffer), 0); 		

                    if(iRetSend <= 0) {
                        //Error in sending final marker for Log File. Abort this Connection
                        Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Error in sending final marker for Log File to TP IP:", ip);
                        close(socket_desc);
                        file.close();
                        //fclose(fpLog);
                        break;
                    }
                    
                    Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__, "Sent confirmation marker of file receipt: ##DRV_ACK## to TP IP: ", ip);
                    
                    file.close();

                    //fclose(fpLog);
                } //While loop - keep receiving log file names
            }

            if(sock_err) 
                break;
        } //Inner while(1) loop - waiting for hanshake initiation from the touch panel

        close(socket_desc);      	

    } //Outermost while(1) loop - always trying to connect.
}
