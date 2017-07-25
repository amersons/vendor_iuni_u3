/******************************************************************************
 *
 *  $Id: AKCompass.h 897 2012-12-13 02:53:06Z yamada.rj $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2004 Asahi Kasei Microdevices Corporation, Japan
 * All Rights Reserved.
 *
 * This software program is the proprietary program of Asahi Kasei Microdevices
 * Corporation("AKM") licensed to authorized Licensee under the respective
 * agreement between the Licensee and AKM only for use with AKM's electronic
 * compass IC.
 *
 * THIS SOFTWARE IS PROVIDED TO YOU "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABLITY, FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT OF
 * THIRD PARTY RIGHTS, AND WE SHALL NOT BE LIABLE FOR ANY LOSSES AND DAMAGES
 * WHICH MAY OCCUR THROUGH USE OF THIS SOFTWARE.
 *
 * -- End Asahi Kasei Microdevices Copyright Notice --
 *
 ******************************************************************************/
#ifndef AKMD_INC_AKCOMPASS_H
#define AKMD_INC_AKCOMPASS_H

#include "AKCommon.h"
#include "CustomerSpec.h"

//**************************************
// Include files for SmartCompass library.
//**************************************
#include "libSmartCompass/AKCertification.h"
#include "libSmartCompass/AKConfigure.h"
#include "libSmartCompass/AKDecomp.h"
#include "libSmartCompass/AKMDevice.h"
#include "libSmartCompass/AKDirection6D.h"
#include "libSmartCompass/AKHDOE.h"
#include "libSmartCompass/AKHFlucCheck.h"
#include "libSmartCompass/AKManualCal.h"
#include "libSmartCompass/AKVersion.h"


/*** Constant definition ******************************************************/
#define	THETAFILTER_SCALE	4128
#define	HFLUCV_TH			2500
#define PDC_SIZE			27

/*** Type declaration *********************************************************/

/*! A parameter structure which is needed for HDOE and Direction calculation. */
typedef struct _AKSCPRMS{

	// Variables for magnetic sensor.
	int16vec	m_ho;
	int16vec	HSUC_HO[CSPEC_NUM_FORMATION];
	int32vec	m_ho32;
	int16vec	m_hs;
	int16vec	HFLUCV_HREF[CSPEC_NUM_FORMATION];
	AKSC_HFLUCVAR	m_hflucv;

	// Variables for DecompS3.
	int16		m_hnave;
	int16vec	m_asa;
	uint8		m_pdc[PDC_SIZE];
	uint8*		m_pdcptr;
	int16vec	m_hdata[AKSC_HDATA_SIZE];
	int32vec	m_hbase;
	int16		m_hn;

	// Variables for HDOE.
	AKSC_HDOEVAR	m_hdoev;
	AKSC_HDST	m_hdst;
	AKSC_HDST	HSUC_HDST[CSPEC_NUM_FORMATION];

	// Variables for base
	int32vec	HSUC_HBASE[CSPEC_NUM_FORMATION];

	// Variables for vnorm
	int16vec	m_hvec;

	// Variables for formation change
	int16		m_form;
	int16		m_cntSuspend;
	int16		m_callcnt;

	// Ceritication
	uint8		m_licenser[AKSC_CI_MAX_CHARSIZE+1];	//end with '\0'
	uint8		m_licensee[AKSC_CI_MAX_CHARSIZE+1];	//end with '\0'
	int16		m_key[AKSC_CI_MAX_KEYSIZE];

	// Special variables for SAM
	int16		m_curForm;
    
} AKSCPRMS;


/*** Global variables *********************************************************/

/*** Prototype of function ****************************************************/

#endif //AKMD_INC_AKCOMPASS_H

