/******************************************************************************
 *
 *  $Id: AKMD_APIs.h 600 2012-04-16 05:04:18Z yamada.rj $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2004 Asahi Kasei Microdevices Corporation, Japan
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
#pragma once

#include "AKCommon.h"
#include "AKCompass.h"

//Gionee liujiang 2013-05-27 add for android4.2 start
#define LOGE ALOGE
#define LOGI ALOGI
#define LOGD ALOGD
#define LOGD_IF ALOGD_IF
//Gionee liujiang 2013-05-27 add for android4.2 end
#define AKMD_VALUE_CHECK
//#define AKMD_TEST_OUTPUT
#define AKMD_ST2_16BIT	0x10
#define AKMD_ST2_14BIT	0x00

#if defined(__cplusplus)
extern "C" {
#endif

int		AKMD_Init(int maxFormNum, int16 regs[]);

void	AKMD_Release(void);

int		AKMD_Start(const char* path);

int		AKMD_Stop(const char* path);

int AKMD_GetData(
	const	int ix,
	const	int iy,
	const	int iz,
	const	int st2,
	const	int freq,
			float* ox,
			float* oy,
			float* oz,
			float* oxbias,
			float* oybias,
			float* ozbias,
			int* accuracy
);

void	AKMD_ResetDOE(void);

int		AKMD_ChangeFormation(int form);

#if defined(__cplusplus)
}
#endif


