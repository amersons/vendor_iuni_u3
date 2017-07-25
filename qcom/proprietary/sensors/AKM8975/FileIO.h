/******************************************************************************
 *
 *  $Id: FileIO.h 336 2011-09-12 07:21:51Z yamada.rj $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2004 - 2011 Asahi Kasei Microdevices Corporation, Japan
 * All Rights Reserved.
 *
 * This software program is proprietary program of Asahi Kasei Microdevices 
 * Corporation("AKM") licensed to authorized Licensee under Software License 
 * Agreement (SLA) executed between the Licensee and AKM.
 * 
 * Use of the software by unauthorized third party, or use of the software 
 * beyond the scope of the SLA is strictly prohibited.
 *
 * -- End Asahi Kasei Microdevices Copyright Notice --
 *
 ******************************************************************************/
#ifndef AKMD_INC_FILEIO_H
#define AKMD_INC_FILEIO_H

// Include file for AK8975 library.
#include "AKCompass.h"

/*** Constant definition ******************************************************/
#define DELIMITER           " = "

/*** Type declaration *********************************************************/

/*** Global variables *********************************************************/

/*** Prototype of function ****************************************************/
int16 LoadParameters(
	const char* path,
	AK8975PRMS * prms
);

int16 LoadInt16(
	FILE * lpFile,
	const char *lpKeyName,
	int16 * val
);

int16 LoadInt16vec(
	FILE * lpFile, 
	const char *lpKeyName,
	int16vec * vec
);

int16 SaveParameters(
	const char* path,
	AK8975PRMS * prms
);

int16 SaveInt16(
	FILE * lpFile, 
	const char *lpKeyName,
	const int16 val
);

int16 SaveInt16vec(
	FILE * lpFile,
	const char *lpKeyName,
	const int16vec * vec
);

#endif //AKMD_INC_FILEIO_H

