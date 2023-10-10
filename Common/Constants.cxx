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

#include "Constants.hxx"
#include "Logger.hxx"
#include "Utils.hxx"
#include "config.h"
#include <cstring>

namespace Common {

    std::string Constants::drv_name = "RAMS7200";
    uint32_t Constants::DRV_NO = 0;                         // Read from PVSS on driver startup
    uint32_t Constants::TSAP_PORT_LOCAL = 0;                // Read from PVSS on driver startup from config file
    uint32_t Constants::TSAP_PORT_REMOTE = 0;               // Read from PVSS on driver startupconfig file
    size_t Constants::POLLING_INTERVAL = 1;                 // Read from PVSS on driver startupconfig file
    std::string Constants::drv_version = PROJECT_VER;
    std::string MEASUREMENT_PATH = "/opt/ramdev/PVSS_projects/REMUS_TEST/data/mes/in/";
    std::string EVENT_PATH = "/opt/ramdev/PVSS_projects/REMUS_TEST/data/mes/in/";
    std::string USERFILE_PATH = "/opt/ramdev/PVSS_projects/REMUS_TEST/data/usr/in/User.dat";

    // The map can be used to map a callback to a HwObject address
    std::map<std::string, std::function<void(const char*)>> Constants::parse_map =
    {
        {   "_DEBUGLVL",
            [](const char* data)
            {
                int16_t retVal;
                memcpy(&retVal, data, sizeof(int16_t));
                std::reverse(reinterpret_cast<char*>(&retVal), reinterpret_cast<char*>(&retVal) + sizeof(int16_t));

                Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "setLogLvl :",std::to_string(retVal).c_str());
                Common::Logger::setLogLvl(retVal);
            }
        }
        
        /*
        ,
        {   "DEBOUNCINGTHREADINTERVAL",
            [](const char* data)
            {
							Common::Logger::globalInfo(Common::Logger::L1, "setDebouncingThreadInterval:", CharString(data));
              Common::Constants::setDebouncingThreadInterval((int)std::atoi(data));
            }
        },
        {   "MAXPOLLRECORDS",
            [](const char* data)
            {
							Common::Logger::globalInfo(Common::Logger::L1, "setConsumerMaxPollRecords:", CharString(data));
              Common::Constants::setConsumerMaxPollRecords((size_t)std::atoi(data));
            }
        }
        */

    };
}
