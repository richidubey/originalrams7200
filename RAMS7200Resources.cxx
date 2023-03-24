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

// Our Resource class.
// This class will interpret the command line and read the config file

#include "RAMS7200Resources.hxx"
#include "Common/Logger.hxx"
#include "Common/Constants.hxx"
#include <ErrHdl.hxx>

<<<<<<< HEAD:RAMS7200Resources.cxx
const CharString RAMS7200Resources::SECTION_NAME = "ramS7200";
=======
const CharString RAMS7200Resources::SECTION_NAME = "rams7200";
>>>>>>> update-name-improv:RAMS7200Resources.cxx
const CharString RAMS7200Resources::TSAP_PORT_LOCAL = "localTSAP";
const CharString RAMS7200Resources::TSAP_PORT_REMOTE = "remoteTSAP";
const CharString RAMS7200Resources::POLLING_INTERVAL = "pollingInterval";
const CharString RAMS7200Resources::MEASUREMENT_PATH = "mesFile";
const CharString RAMS7200Resources::EVENT_PATH = "eventFile";
const CharString RAMS7200Resources::USERFILE_PATH = "userFile";

 std::string Common::Constants::MEASUREMENT_PATH;
 std::string Common::Constants::EVENT_PATH;
std::string Common::Constants::USERFILE_PATH;

//-------------------------------------------------------------------------------
// init is a wrapper around begin, readSection and end

void RAMS7200Resources::init(int &argc, char *argv[])
{
  // Prepass of commandline arguments, e.g. get the arguments needed to
  // find the config file.
  begin(argc, argv);

  // Read the config file
  while ( readSection() || generalSection() ) ;

  // Postpass of the commandline arguments, e.g. get the arguments that
  // will override entries in the config file
  end(argc, argv);
}

//-------------------------------------------------------------------------------

PVSSboolean RAMS7200Resources::readSection() {
	// Are we in our section ? This is true for "remus_drv" or "remus_drv_<num>"
	if (!isSection(SECTION_NAME))
		return PVSS_FALSE;

	// skip "[remus_drv_<num>]"
	getNextEntry();

	std::string tmpStr;

	try{
		// Now read the section until new section or end of file
		while ((cfgState != CFG_SECT_START) && (cfgState != CFG_EOF)) {
			// Test the keywords
			if (keyWord.startsWith(TSAP_PORT_LOCAL)) {
				cfgStream >> tmpStr;
				Common::Constants::setLocalTsapPort(strtol(tmpStr.c_str(), NULL, 16));
			}else if(keyWord.startsWith(TSAP_PORT_REMOTE)) {
				cfgStream >> tmpStr;
				Common::Constants::setRemoteTsapPort(strtol(tmpStr.c_str(), NULL, 16));
			}else if(keyWord.startsWith(POLLING_INTERVAL)) {
				cfgStream >> tmpStr;
				Common::Constants::setPollingInterval(atoi(tmpStr.c_str()));
      		}else if(keyWord.startsWith(MEASUREMENT_PATH)) {
				cfgStream >> tmpStr;
				Common::Constants::setMeasFilePath(tmpStr);
      		}else if(keyWord.startsWith(EVENT_PATH)) {
				cfgStream >> tmpStr;
				Common::Constants::setEventFilePath(tmpStr);
      		}else if(keyWord.startsWith(USERFILE_PATH)) {
				cfgStream >> tmpStr;
				Common::Constants::setUserFilePath(tmpStr);
      		}

			getNextEntry();
		}
	}catch(std::runtime_error e){
	 Common::Logger::globalError(e.what());
		return PVSS_FALSE;
	}

	// So the loop will stop at the end of the file
	return cfgState != CFG_EOF;
}


RAMS7200Resources& RAMS7200Resources::GetInstance()
{
    static RAMS7200Resources krs;
    return krs;
}

//-------------------------------------------------------------------------------
// Interface to internal Datapoints
// Get the number of names we need the DpId for

int RAMS7200Resources::getNumberOfDpNames()
{
  // TODO if you use internal DPs
  return 0;
}

//-------------------------------------------------------------------------------
