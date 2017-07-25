/******************************************************************************
 *
 *  $Id: AKCompass.h 336 2011-09-12 07:21:51Z yamada.rj $
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
#ifndef INC_AKCOMPASS_H
#define INC_AKCOMPASS_H

#include "AKCommon.h"
#include "CustomerSpec.h"

//**************************************
// Include files for AK8975  library.
//**************************************
#include "libAK8975/AK8975.h"
#include "libAK8975/AKConfigure.h"
#include "libAK8975/AKMDevice.h"
#include "libAK8975/AKCertification.h"
#include "libAK8975/AKDirection6D.h"
#include "libAK8975/AKHDOE.h"
#include "libAK8975/AKHFlucCheck.h"
#include "libAK8975/AKManualCal.h"
#include "libAK8975/AKVersion.h"

//========================= Constant definition ==============================//
#define AKMD_ERROR		-1
#define AKMD_SUCCESS	 0

//   parameter for theta filter
#define	THETAFILTER_SCALE	4128
#define HFLUCV_TH	2500		//*5 format

//========================= Type declaration  ================================//

// A parameter structure.
typedef struct _AK8975PRMS{
	// Variables for magnetic sensor.
	int16vec		m_ho;
	//int16vec*		p_HSUC_HO;
	int16vec		p_HSUC_HO[CSPEC_NUM_FORMATION];
	int16vec		m_hs;
	//int16vec*		p_HFLUCV_HREF;
	int16vec		p_HFLUCV_HREF[CSPEC_NUM_FORMATION];
	AKSC_HFLUCVAR	m_hflucv;

	// Variables for Decomp8975.
	int16vec		m_hdata[AKSC_HDATA_SIZE];
	int16			m_hn;
	int16			m_hnave;
	int16vec		m_hvec;
	int16vec		m_asa;

	// Variables for HDOE.
	AKSC_HDOEVAR	m_hdoev;
	AKSC_HDST		m_hdst;
	//AKSC_HDST*		p_HSUC_HDST;
	AKSC_HDST		p_HSUC_HDST[CSPEC_NUM_FORMATION];

	// Variables for formation change
	int16			m_form;

	int16			m_maxFormNum;
	int16			m_cntSuspend;
	int16			m_callcnt;

	// Ceritication
	uint8			m_licenser[AKSC_CI_MAX_CHARSIZE+1];	//end with '\0'
	uint8			m_licensee[AKSC_CI_MAX_CHARSIZE+1];	//end with '\0'
	int16			m_key[AKSC_CI_MAX_KEYSIZE];

} AK8975PRMS;

#endif //INC_AKCOMPASS_H

