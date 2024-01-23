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

#ifndef CONSTANTS_HXX_
#define CONSTANTS_HXX_

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <map>
#include <Common/Utils.hxx>
#include <Common/Logger.hxx>

namespace Common{

    /*!
    * \class Constants
    * \brief Class containing constant values used in driver
    */
    class Constants{
    public:

        static void setDrvName(std::string lp);
        static std::string& getDrvName();

        static std::string& getDrvVersion();

        // called in driver init to set the driver number dynamically
        static void setDrvNo(uint32_t no);
        // subsequentally called when writing buffers etc.
        static uint32_t getDrvNo();

        static void setLocalTsapPort(uint32_t port);
        static const uint32_t& getLocalTsapPort();

        static void setRemoteTsapPort(uint32_t port);
        static const uint32_t& getRemoteTsapPort();

        static void setPollingInterval(uint32_t pollingInterval);
        static const uint32_t& getPollingInterval();
        
        static void setUserFilePath(std::string);
        static std::string& getUserFilePath();

        static void setMeasFilePath(std::string);
        static std::string& getMeasFilePath();
        
        static void setEventFilePath(std::string);
        static std::string& getEventFilePath();

        static const std::map<std::string,std::function<void(const char *)>>& GetParseMap();

        static uint32_t getMsCopyPort();

    private:
        static std::string drv_name;
        static std::string drv_version;
        static std::string MEASUREMENT_PATH;
        static std::string EVENT_PATH;
        static std::string USERFILE_PATH;
        static uint32_t DRV_NO;   // WinCC OA manager number
        static uint32_t TSAP_PORT_LOCAL;
        static uint32_t TSAP_PORT_REMOTE;
        static uint32_t POLLING_INTERVAL;
        static uint32_t MSCOPY_PORT;

        static std::map<std::string, std::function<void(const char *)>> parse_map;
    };

    inline const std::map<std::string,std::function<void(const char *)>>& Constants::GetParseMap()
    {
        return parse_map;
    }

    inline void Constants::setDrvName(std::string dname){
        drv_name = dname;
    }

    inline std::string& Constants::getDrvName(){
        return drv_name;
    }

    inline std::string& Constants::getDrvVersion(){
        return drv_version;
    }

    inline void Constants::setDrvNo(uint32_t no){
        DRV_NO = no;
    }

    inline uint32_t Constants::getDrvNo(){
        return DRV_NO;
    }

    inline void Constants::setLocalTsapPort(uint32_t port){
        Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__,"Setting TSAP_PORT_LOCAL=" + CharString(port));
        TSAP_PORT_LOCAL = port;
    }

    inline const uint32_t& Constants::getLocalTsapPort(){
        return TSAP_PORT_LOCAL;
    }

    inline void Constants::setRemoteTsapPort(uint32_t port){
        Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__,"Setting TSAP_PORT_REMOTE=" + CharString(port));
        TSAP_PORT_REMOTE = port;
    }

    inline const uint32_t& Constants::getRemoteTsapPort(){
        return TSAP_PORT_REMOTE;
    }

    inline void Constants::setPollingInterval(uint32_t pollingInterval)
    {
        Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__,"Setting POLLING_INTERVAL=" + CharString(pollingInterval));
        POLLING_INTERVAL = pollingInterval;
    }

    inline const uint32_t& Constants::getPollingInterval()
    {
        return POLLING_INTERVAL;
    }

    inline void Constants::setUserFilePath(std::string userFilePath) 
    { 
        Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__,"Setting USERFILE_PATH=", userFilePath.c_str());
        USERFILE_PATH = userFilePath;
    }
    
    inline std::string& Constants::getUserFilePath() {
        return USERFILE_PATH;
    }


    inline std::string& Constants::getMeasFilePath() {
        return MEASUREMENT_PATH;
    }

    inline void Constants::setMeasFilePath(std::string measFilePath) 
    {
        Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__,"Setting MEASUREMENT_PATH=", measFilePath.c_str());
        MEASUREMENT_PATH = measFilePath;

    }

    inline std::string& Constants::getEventFilePath() {
        return EVENT_PATH;
    }

    inline void Constants::setEventFilePath(std::string eventFilePath) 
    {
        Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__,"Setting EVENT_PATH=", eventFilePath.c_str());
        EVENT_PATH = eventFilePath;
    }

    inline uint32_t Constants::getMsCopyPort() {
        return MSCOPY_PORT;
    }


}//namespace
#endif /* CONSTANTS_HXX_ */
