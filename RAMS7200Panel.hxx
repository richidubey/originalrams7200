#pragma once
#include <thread>
#include "RAMS7200MS.hxx"
#include "Common/Logger.hxx"

using queueToDPCallback = std::function<void(const std::string& dp_address, uint16_t length, char* payload)>;


class RAMS7200Panel{

public:
    /**
     * @brief RAMS7200Panel constructor
     * @param RAMS7200MS & : const reference to the MS object
     * @param queueToDPCallback : a callback that will be called after each poll
     * */
    RAMS7200Panel(RAMS7200MS& , queueToDPCallback);
    RAMS7200Panel(const RAMS7200Panel&) = delete;
    RAMS7200Panel& operator=(const RAMS7200Panel&) = delete;
    RAMS7200Panel(RAMS7200Panel&&) = delete;
    RAMS7200Panel& operator=(RAMS7200Panel&&) = delete;
    ~RAMS7200Panel() = default;

    template <typename T>
    void sleep_for(T duration)
    {
        std::unique_lock<std::mutex> lk(ms._threadMutex);
        ms._threadCv.wait_for(lk, duration, [&](){
           return !ms._run.load();     
        });
    }
    void FileSharingTask(int port);
    
private:
    void writeTouchConnErrDPE(bool);
    
    RAMS7200MS& ms;
    bool touch_panel_conn_error = true; //Not connected initially
    queueToDPCallback _queueToDPCB;
};