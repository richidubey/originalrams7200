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

#include "RAMS7200HWMapper.hxx"
#include "Transformations/RAMS7200StringTrans.hxx"
#include "Transformations/RAMS7200Int16Trans.hxx"
#include "Transformations/RAMS7200Int32Trans.hxx"
#include "Transformations/RAMS7200FloatTrans.hxx"
#include "Transformations/RAMS7200BoolTrans.hxx"
#include "Transformations/RAMS7200Uint8Trans.hxx"
#include "RAMS7200HWService.hxx"

#include <algorithm>
#include "Common/Logger.hxx"
#include "Common/Constants.hxx"
#include "Common/Utils.hxx"
#include <PVSSMacros.hxx>     // DEBUG macros


int RAMS7200HWMapper::usePriorTransformation(PeriphAddr *confPtr) {
  switch ((uint32_t)confPtr->getTransformationType()) {
    case TransUndefinedType:
      Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__, "Undefined transformation" + CharString(confPtr->getTransformationType()) +", For address: "+ confPtr->getName());
      return -1;
    case RAMS7200DrvBoolTransType:
      Common::Logger::globalInfo(Common::Logger::L3,"Bool transformation");
      confPtr->setTransform(new Transformations::RAMS7200BoolTrans);
      return 0;
    case RAMS7200DrvUint8TransType:
      Common::Logger::globalInfo(Common::Logger::L3,"Uint8 transformation");
      confPtr->setTransform(new Transformations::RAMS7200Uint8Trans);
      return 0;
    case RAMS7200DrvInt32TransType:
      Common::Logger::globalInfo(Common::Logger::L3,"Int32 transformation");
      confPtr->setTransform(new Transformations::RAMS7200Int32Trans);
      return 0;
    case RAMS7200DrvInt16TransType:
      Common::Logger::globalInfo(Common::Logger::L3,"Int16 transformation");
      confPtr->setTransform(new Transformations::RAMS7200Int16Trans);
      return 0;
    case RAMS7200DrvFloatTransType:
      Common::Logger::globalInfo(Common::Logger::L3,"Float transformation");
      confPtr->setTransform(new Transformations::RAMS7200FloatTrans);
      return 0;
    case RAMS7200DrvStringTransType:
      Common::Logger::globalInfo(Common::Logger::L3,"String transformation");
      confPtr->setTransform(new Transformations::RAMS7200StringTrans);
      return 0;
    default:
      Common::Logger::globalError("RAMS7200HWMapper::addDpPa", CharString("Illegal transformation type ") + CharString((int) confPtr->getTransformationType()));
      return -1;
  }
}

int  RAMS7200HWMapper::useDerivedTransformation(PeriphAddr *confPtr, std::string recvdAddress) {
   if(RAMS7200LibFacade::RAMS7200AddressGetAmount(recvdAddress) > 1) {
      Common::Logger::globalInfo(Common::Logger::L3,"String transformation");
      confPtr->setTransform(new Transformations::RAMS7200StringTrans);
      return 1;
    } else {
      switch(RAMS7200LibFacade::RAMS7200AddressGetWordLen(recvdAddress)) {
        case S7WLBit: 
            Common::Logger::globalInfo(Common::Logger::L3,"Bool transformation");
            confPtr->setTransform(new Transformations::RAMS7200BoolTrans);
            return 0;
        case S7WLByte:
            Common::Logger::globalInfo(Common::Logger::L3,"Uint8 transformation");
            confPtr->setTransform(new Transformations::RAMS7200Uint8Trans);
            return 0;
        case S7WLWord:
            Common::Logger::globalInfo(Common::Logger::L3,"Int16 transformation");
            confPtr->setTransform(new Transformations::RAMS7200Int16Trans);
            return 0;
        case S7WLReal:  
            Common::Logger::globalInfo(Common::Logger::L3,"Float transformation");
            confPtr->setTransform(new Transformations::RAMS7200FloatTrans);
            return 0;
        default :
            Common::Logger::globalError("RAMS7200HWMapper::addDpPa",CharString("Illegal (Unexpected) address : ") +  CharString(confPtr->getName()));
            return -1;
      }
    }
}
//--------------------------------------------------------------------------------
// We get new configs here. Create a new HW-Object on arrival and insert it.

PVSSboolean RAMS7200HWMapper::addDpPa(DpIdentifier &dpId, PeriphAddr *confPtr)
{
  // We don't use Subindices here, so its simple.
  // Otherwise we had to look if we already have a HWObject and adapt its length.

  Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "With Direction: " + CharString(confPtr->getDirection()) + ", addDpPa called for " + confPtr->getName().c_str());

  // tell the config how we will transform data to/from the device
  // by installing a Transformation object into the PeriphAddr
  // In this template, the Transformation type was set via the
  // configuration panel (it is already set in the PeriphAddr)

  // TODO this really depends on your protocol and is therefore just an example
  // in this example we use the ones from Pbus, as those can be selected
  // with the SIM driver parametrization panel

  std::vector<std::string> spltDol = Common::Utils::split((confPtr->getName()).c_str());

  if(spltDol.size() == 1 && !usePriorTransformation(confPtr)) {
      return HWMapper::addDpPa(dpId, confPtr);
  } else {
    std::string recvdAddress(spltDol[1]);

    if(RAMS7200LibFacade::RAMS7200AddressIsValid(recvdAddress) && !useDerivedTransformation(confPtr, recvdAddress)) {
      return HWMapper::addDpPa(dpId, confPtr);
    } else if(recvdAddress[0] == '_' && !!usePriorTransformation(confPtr)) { //Special Addresses
      return HWMapper::addDpPa(dpId, confPtr);
    } else {
      Common::Logger::globalError("RAMS7200HWMapper::addDpPa",CharString("Illegal (Unexpected) address : ") +  CharString(confPtr->getName()));
      return HWMapper::addDpPa(dpId, confPtr);
    }
  }

  // First add the config, then the HW-Object
  if ( !HWMapper::addDpPa(dpId, confPtr) )  // FAILED !! 
  {
    Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__, "Failed in adding DP Para to HW Mapper object for address : " + CharString(confPtr->getName()));
      return PVSS_FALSE;
  }

  std::vector<std::string> addressOptions = Common::Utils::split(confPtr->getName().c_str());

  if(spltDol.size() > 1 && addressCounter.find(addressOptions[0] + addressOptions[1]) == addressCounter.end()) {
    //Common::Logger::globalInfo(Common::Logger::L3, CharString("Inserting counter value 1 for hardware object with address: ") + (addressOptions[0] + addressOptions[1]).c_str());
    addressCounter.insert(std::pair<std::string, int>(addressOptions[0] + addressOptions[1], 1));
  } else if (spltDol.size() > 1){
    Common::Logger::globalInfo(Common::Logger::L3, CharString("Increasing counter value for hardware object with address: ") + (addressOptions[0] + addressOptions[1]).c_str());
    addressCounter[addressOptions[0] + addressOptions[1]]++; 
  }

  if(RAMS7200Addresses.count(addressOptions[0])){
      for(auto it = RAMS7200Addresses[addressOptions[0]].begin(); it!= RAMS7200Addresses[addressOptions[0]].end(); it++ ) {
        if(it->first == addressOptions[1]) {  
          Common::Logger::globalInfo(Common::Logger::L3, CharString("Increased counter for duplicate hardware address: ") + confPtr->getName().c_str());
          return PVSS_TRUE;
        }
      }
  }

  HWObject *hwObj = new HWObject;
  // Set Address and Subindex
  Common::Logger::globalInfo(Common::Logger::L3, "New Object", "name:" + confPtr->getName());
  hwObj->setConnectionId(confPtr->getConnectionId());
  hwObj->setAddress(confPtr->getName());       // Resolve the HW-Address, too

  // Set the data type.
  hwObj->setType(confPtr->getTransform()->isA());

  // Set the len needed for data from _all_ subindices of this PVSS-Address.
  // Because we will deal with subix 0 only this is the Transformation::itemSize
  hwObj->setDlen(confPtr->getTransform()->itemSize());
  //TODO - number of elements?
  // Add it to the list
  addHWObject(hwObj);

  if(confPtr->getDirection() == DIRECTION_IN || confPtr->getDirection() == DIRECTION_INOUT)
  {
    if (addressOptions.size() == 3) // IP + VAR + POLLTIME
    {
      if(addressOptions[0].compare("VERSION"))
        addAddress(addressOptions[0], addressOptions[1], addressOptions[2]);
    }
  }

  return PVSS_TRUE;
}

//--------------------------------------------------------------------------------

PVSSboolean RAMS7200HWMapper::clrDpPa(DpIdentifier &dpId, PeriphAddr *confPtr)
{
  Common::Logger::globalInfo(Common::Logger::L3, "clrDpPa called for" + confPtr->getName());

  std::vector<std::string> addressOptions = Common::Utils::split(confPtr->getName().c_str());

  if(addressOptions.size() > 1 && addressCounter.find(addressOptions[0] + addressOptions[1]) != addressCounter.end()) {
    addressCounter[addressOptions[0] + addressOptions[1]]--;
  } else if(addressOptions.size() > 1){
    Common::Logger::globalWarning(__PRETTY_FUNCTION__, "Tried to delete an untracked address");
    return PVSS_FALSE;
  }

  if(addressOptions.size() > 1 && addressCounter[addressOptions[0] + addressOptions[1]]) {
    Common::Logger::globalInfo(Common::Logger::L3, __PRETTY_FUNCTION__, "Decreased HW address counter for" + confPtr->getName());
    return HWMapper::clrDpPa(dpId, confPtr);
  }

  // Find our HWObject via a template`
  HWObject adrObj;
  adrObj.setAddress(confPtr->getName());

  // Lookup HW-Object via the Name, not via the HW-Address
  // The class type isn't important here
  HWObject *hwObj = findHWAddr(&adrObj);

  if(hwObj == NULL) {
    Common::Logger::globalWarning(__PRETTY_FUNCTION__, "Error in getting HW Address");
    return PVSS_FALSE;
  }

  if(confPtr->getDirection() == DIRECTION_IN || confPtr->getDirection() == DIRECTION_INOUT)
  {
      if (addressOptions.size() == 3) // IP + VAR + POLLTIME
      {
        removeAddress(addressOptions[0], addressOptions[1], addressOptions[2]);
      }
  }

  if ( hwObj ) {
    // Object exists, remove it from the list and delete it.
    clrHWObject(hwObj);
    delete hwObj;
  }

  if(addressOptions.size() > 1) {
    Common::Logger::globalInfo(Common::Logger::L3, __PRETTY_FUNCTION__, "Deleted entry in HW address counter for address : " + confPtr->getName());
    addressCounter.erase(addressCounter.find(addressOptions[0] + addressOptions[1]));
  }
  // Call function of base class to remove config
  return HWMapper::clrDpPa(dpId, confPtr);
}

void RAMS7200HWMapper::addAddress(const std::string &ip, const std::string &var, const std::string &pollTime)
{
    if(RAMS7200IPs.find(ip) == RAMS7200IPs.end())
    {
        RAMS7200IPs.insert(ip);
        isIPrunning.insert(std::pair<std::string, bool>(ip, true));
        Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Received var from a new IP Address : " + CharString(ip.c_str()));
        RAMS7200Addresses.erase(ip);
        RAMS7200Addresses.insert(std::pair<std::string, std::vector<std::pair<std::string, int>>>(ip, std::vector<std::pair<std::string, int>>()));
    }

    if(RAMS7200Addresses.count(ip)){
      if(std::find(RAMS7200Addresses[ip].begin(), RAMS7200Addresses[ip].end(), make_pair(var,  std::stoi(pollTime))) == RAMS7200Addresses[ip].end())
      {
        RAMS7200Addresses[ip].push_back(make_pair(var, std::stoi(pollTime)));
        Common::Logger::globalInfo(Common::Logger::L2, "Added to RAMS7200AddressList", var.c_str());
      }
    }
}

void RAMS7200HWMapper::removeAddress(const std::string &ip, const std::string &var, const std::string &pollTime)
{
  if(RAMS7200Addresses.count(ip)) {
    
    if(std::find(RAMS7200Addresses[ip].begin(), RAMS7200Addresses[ip].end(), make_pair(var,  std::stoi(pollTime))) != RAMS7200Addresses[ip].end()) {
        RAMS7200Addresses[ip].erase(std::find(RAMS7200Addresses[ip].begin(), RAMS7200Addresses[ip].end(), make_pair(var,  std::stoi(pollTime))));
        Common::Logger::globalInfo(Common::Logger::L3, __PRETTY_FUNCTION__,  CharString("Erased address: ") + var.c_str() + CharString("With polling time: ") + pollTime.c_str() + CharString(" On IP: ")+ CharString(ip.c_str()));
    }

    if(RAMS7200Addresses[ip].size() == 0) {
      RAMS7200IPs.erase(ip);
      RAMS7200Addresses.erase(ip);
      Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,  "All Addresses deleted from the IP : " + CharString(ip.c_str()));

      while(isIPrunning[ip])
      {
          Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__, " Sleeping for 1 second since the Polling is still running for the IP : " + CharString(ip.c_str()));
          std::this_thread::sleep_for(std::chrono::seconds(1));
      }
      Common::Logger::globalInfo(Common::Logger::L1, __PRETTY_FUNCTION__,"Ready to register new device as Lambda Thread exited from the polling loop for the IP : " + CharString(ip.c_str()));
    }
  }
}

bool RAMS7200HWMapper::checkIPExist(std::string ip) {
  return RAMS7200IPs.count(ip);
}