/******************************************************************************
 *
 * $Id: AKCommon.h 370 2011-10-12 08:00:39Z yamada.rj $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2009 Asahi Kasei Microdevices Corporation, Japan
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
#ifndef AKMD_INC_AKCOMMON_H
#define AKMD_INC_AKCOMMON_H

#ifdef WIN32
#include "Android.h"

#else
#include <stdio.h>
#include <string.h>    //memset
#include <math.h>      //pow
#include <unistd.h>
#include <stdarg.h>    //va_list
#include <cutils/log.h>

#include <hardware/hardware.h>
#include <hardware/sensors.h>
#endif

/*** Constant definition ******************************************************/
#undef LOG_TAG
#define LOG_TAG "AKMD2"
//#define LOGD(format, ...)  printf(format, ##__VA_ARGS__);

#define DBG_LEVEL0   0x0001	// Critical
#define DBG_LEVEL1   0x0002	// Notice
#define DBG_LEVEL2   0x0003	// Information
#define DBG_LEVEL3   0x0004	// Debug
#define DBGFLAG      DBG_LEVEL2


/*
 * Debug print macro.
 */
#ifndef DBGPRINT
#define DBGPRINT(level, format, ...) \
    ((((level) != 0) && ((level) <= DBGFLAG))  \
     ? (fprintf(stdout, (format), ##__VA_ARGS__)) \
     : (void)0)
/*
#define DBGPRINT(level, format, ...)                                  \
    LOGV_IF(((level) != 0) && ((level) <= DBGFLAG), (format), ##__VA_ARGS__)
*/

#endif

/***** Log output ********************************************/  
#ifdef AKM_LOG_ENABLE  
#define AKM_LOG(format, ...)	LOGD((format), ##__VA_ARGS__)  
#else  
#define AKM_LOG(format, ...)  
#endif


#endif //AKMD_INC_AKCOMMON_H

