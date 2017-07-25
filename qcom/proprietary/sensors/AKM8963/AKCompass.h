/******************************************************************************
 *
 * $Id: AKCompass.h 383 2011-11-21 08:14:27Z yamada.rj $
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
#ifndef AKMD_INC_AKCOMPASS_H
#define AKMD_INC_AKCOMPASS_H

#include "AKCommon.h"
#include "CustomerSpec.h"

//**************************************
// Include files for AK8963  library.
//**************************************
#include "libAK8963/AK8963.h"
#include "libAK8963/AKConfigure.h"
#include "libAK8963/AKMDevice.h"
#include "libAK8963/AKCertification.h"
#include "libAK8963/AKDirection6D.h"
#include "libAK8963/AKHDOE.h"
#include "libAK8963/AKHFlucCheck.h"
#include "libAK8963/AKManualCal.h"
#include "libAK8963/AKVersion.h"

/*** Constant definition ******************************************************/
#define AKMD_ERROR		-1
#define AKMD_SUCCESS	 0

#define	HFLUCV_TH		2500

/*** Type declaration *********************************************************/

/*! A parameter structure which is needed for HDOE and Direction calculation. */
typedef struct _AK8963PRMS{

	// Variables for magnetic sensor.
	int16vec	m_ho;
	int16vec	HSUC_HO[CSPEC_NUM_FORMATION];
	int32vec	m_ho32;
	int16vec	m_hs;
	int16vec	HFLUCV_HREF[CSPEC_NUM_FORMATION];
	AKSC_HFLUCVAR	m_hflucv;

	// Variables for Decomp8963.
	int16vec	m_hdata[AKSC_HDATA_SIZE];
	int16		m_hnave;
	int16		m_hn;		// Number of acquired data
	int16vec	m_hvec;		// Averaged value
	int16vec	m_asa;

	// Variables for HDOE.
	AKSC_HDOEVAR	m_hdoev;
	AKSC_HDST	m_hdst;
	AKSC_HDST	HSUC_HDST[CSPEC_NUM_FORMATION];

	// Variables for formation change
	int16		m_form;
	int16		m_cntSuspend;

	// Variables for decimation.
	int16		m_callcnt;

    //// Variables for outbit.
	//int16		m_outbit;

	// Ceritication
	uint8		m_licenser[AKSC_CI_MAX_CHARSIZE+1];	//end with '\0'
	uint8		m_licensee[AKSC_CI_MAX_CHARSIZE+1];	//end with '\0'
	int16		m_key[AKSC_CI_MAX_KEYSIZE];

	// base
	int32vec	m_hbase;
	int32vec	HSUC_HBASE[CSPEC_NUM_FORMATION];
    
    // For Qualcomm
	int16			m_maxFormNum;

} AK8963PRMS;


/*** Global variables *********************************************************/

/*** Prototype of function ****************************************************/

#endif //AKMD_INC_AKCOMPASS_H

