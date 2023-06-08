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

#include <cstring>

#include "RAMS7200FloatTrans.hxx"

#include "Transformations/RAMS7200Int16Trans.hxx"

#include "RAMS7200HWMapper.hxx"

#include "Common/Logger.hxx"

#include <cmath>

#include <FloatVar.hxx>

namespace Transformations{

TransformationType RAMS7200FloatTrans::isA() const {
    return (TransformationType) RAMS7200DrvFloatTransType;
}

TransformationType RAMS7200FloatTrans::isA(TransformationType type) const {
	if (type == isA())
		return type;
	else
		return Transformation::isA(type);
}

Transformation *RAMS7200FloatTrans::clone() const {
	return new RAMS7200FloatTrans;
}

int RAMS7200FloatTrans::itemSize() const {
	return size;
}

VariableType RAMS7200FloatTrans::getVariableType() const {
	return FLOAT_VAR;
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


PVSSboolean RAMS7200FloatTrans::toPeriph(PVSSchar *buffer, PVSSuint len, const Variable &var, const PVSSuint subix) const {

	if(var.isA() != FLOAT_VAR){
		ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
				ErrClass::ERR_PARAM, // Wrong parametrization
				ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
				"RAMS7200FloatTrans", "toPeriph", // File and function name
				"Wrong variable type" // Unfortunately we don't know which DP
				);

		return PVSS_FALSE;
	}

	Common::Logger::globalInfo(Common::Logger::L2,"RAMS7200FloatTrans::toPeriph : Float var received in transformation toPeriph, val is: ", std::to_string(((float)(reinterpret_cast<const FloatVar &>(var)).getValue())).c_str());
	reinterpret_cast<float *>(buffer)[subix] = ReverseFloat((float)(reinterpret_cast<const FloatVar &>(var)).getValue());

	return PVSS_TRUE;
}

VariablePtr RAMS7200FloatTrans::toVar(const PVSSchar *buffer, const PVSSuint dlen, const PVSSuint subix) const {

	if(buffer == NULL ){
		ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
				ErrClass::ERR_PARAM, // Wrong parametrization
				ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
				"RAMS7200FloatTrans", "toVar", // File and function name
				"Null buffer pointer" + CharString(dlen) // Unfortunately we don't know which DP
				);
		return NULL;
	}

	else if(dlen == sizeof(int16_t)) {
		Common::Logger::globalInfo(Common::Logger::L2,__PRETTY_FUNCTION__, "Float var returned with Dlen 2, treating it as int16");
		return new IntegerVar(__bswap_16((int16_t)*reinterpret_cast<const int16_t*>(buffer + (subix * size))));
	}
	else if(dlen%size > 0){
		ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
				ErrClass::ERR_PARAM, // Wrong parametrization
				ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
				"RAMS7200FloatTrans", "toVar", // File and function name
				"Dlen mod size is gt. 0: " + CharString(dlen) // Unfortunately we don't know which DP
				);
		return NULL;
	}

	else if(dlen < size*(subix+1)){
		ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
				ErrClass::ERR_PARAM, // Wrong parametrization
				ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
				"RAMS7200FloatTrans", "toVar", // File and function name
				"Dlen less than size mult. subix + 1: " + CharString(dlen) // Unfortunately we don't know which DP
				);
		return NULL;
	}

	else if(buffer == NULL || dlen%size > 0 || dlen < size*(subix+1)){
		ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
				ErrClass::ERR_PARAM, // Wrong parametrization
				ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
				"RAMS7200FloatTrans", "toVar", // File and function name
				"Null buffer pointer or wrong length: " + CharString(dlen) // Unfortunately we don't know which DP
				);
		return NULL;
	}

	return new FloatVar(ReverseFloat(reinterpret_cast<const float*>(buffer)[subix]));
}

}//namespace
