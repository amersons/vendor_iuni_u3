/******************************************************************************
 *
 *  $Id: AKMD_APIs.c 600 2012-04-16 05:04:18Z yamada.rj $
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
#include "AKMD_APIs.h"
#include "FileIO.h"
#include "Measure.h"

#define AKMD_PRMS_INIT		1
#define AKMD_PRMS_NOT_INIT	0

#define AKMD_MAG_MINVAL		-3219128	/* 4900 uT */
#define AKMD_MAG_MAXVAL		3219128		/* -4900 uT */

// Parameters
// Global variables from AKCommon.h
int g_opmode;		/*!< 0:Daemon mode, 1:Console mode. */
int g_dbgzone;		/*!< Debug zone. */

// 0: NOT initialized,  1:Initialized
static int16		g_init = 0;
// 0: Initial value.
// The maximum value will be specified when AKMD_Init() is called.
static int16		g_form = 0;
// Parameters for calculation.
static AK8963PRMS	g_prms;

/******************************************************************************/
/*	STATIC Function                                                           */
/******************************************************************************/
/*! Get current formation.
 @return Form factor number.
 */
int16 getFormation(void)
{
	return g_form;
}

/******************************************************************************/
/*! Set license key for SmartCompass cetification. 
 @return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
 @regs[in] Register values which are obtained from AK8963 device.
 @prms[out] The member variable of AK8963PRMS structure (m_key, m_licenser and 
 m_licensee) will be filled with license information.
 */
static int SetLicenseKey(
	const	int16 regs[],
			AK8963PRMS*	prms
)
{
	// Set keywords for SmartCompassLibrary certification
	prms->m_key[0] = CSPEC_CI_AK_DEVICE;
	prms->m_key[1] = regs[0];
	prms->m_key[2] = regs[1];
	prms->m_key[3] = regs[2];
	prms->m_key[4] = regs[3];
	strlcpy(prms->m_licenser, CSPEC_CI_LICENSER, AKSC_CI_MAX_CHARSIZE);
	prms->m_licenser[AKSC_CI_MAX_CHARSIZE] = '\0';
	strlcpy(prms->m_licensee, CSPEC_CI_LICENSEE, AKSC_CI_MAX_CHARSIZE);
	prms->m_licensee[AKSC_CI_MAX_CHARSIZE] = '\0';

	return AKMD_SUCCESS;
}


/******************************************************************************/
/*	PUBLIC Function                                                           */
/******************************************************************************/
/*! Initialize AK8963PRMS structure. This function must be called before 
application uses any APIs in this file.  If AK8963PRMS are already initialized,
this function discards all existing data.  When APIs are not used anymore, 
AKMD_Release function must be called at the end.  When this function succeeds,
form is set to 0.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in] maxFormNum The maximum number of form of this device. This 
number should be from 1 to CSPEC_NUM_FORMATION.
\param[in] regs Specify pointer to a uint8_t array.  The array should be filled
with the values which are acquired from AK8963 device.
*/
int  AKMD_Init(int maxFormNum, int16 regs[])
{
	LOGD_IF(g_init, "AK8963PRMS is already initialized.");

#ifdef AKMD_VALUE_CHECK
	// Check the range of arguments
	if((maxFormNum <= 0)||(CSPEC_NUM_FORMATION < maxFormNum)){
		LOGE("AKMD_Init: Invalid formFactorNumber.");
		return AKMD_ERROR;
	}
	if(regs == NULL){
		LOGE("AKMD_Init: regs can't be NULL.");
		return AKMD_ERROR;
	}
#endif
	// Put dummy value
	regs[0] = 72;
	/* Don't adjust ASA since it is done in the mag driver */
	regs[1] = 128; /* 243; */
	regs[2] = 128; /* 243; */
	regs[3] = 128; /* 254; */

	// Clear allocated memory at first.
	AKMD_Release();

	// Clear all data
	memset(&g_prms, 0, sizeof(AK8963PRMS));
	// Copy current value
	g_prms.m_maxFormNum = maxFormNum;
#if 0
	// Allocate
	g_prms.p_HSUC_HO = (int16vec*)malloc(sizeof(int16vec) * maxFormNum);
	if(g_prms.p_HSUC_HO == NULL){
		goto AKMD_ALLOC_FAIL_1;
	}
	g_prms.p_HFLUCV_HREF = (int16vec*)malloc(sizeof(int16vec) * maxFormNum);
	if(g_prms.p_HFLUCV_HREF == NULL){
		goto AKMD_ALLOC_FAIL_2;
	}
	g_prms.p_HSUC_HDST = (AKSC_HDST*)malloc(sizeof(AKSC_HDST) * maxFormNum);
	if(g_prms.p_HSUC_HDST == NULL){
		goto AKMD_ALLOC_FAIL_3;
	}
#endif
	// Initialize AK8963PRMS structure.
	InitAK8963PRMS(&g_prms);

	// Additional settings
	g_prms.m_asa.u.x = regs[1];
	g_prms.m_asa.u.y = regs[2];
	g_prms.m_asa.u.z = regs[3];

	// Initialize the certification key.
	if(SetLicenseKey(regs, &g_prms) == AKMD_ERROR){
		goto AKMD_SETLICENSE_FAIL;
	}

	// Set flag
	g_init = AKMD_PRMS_INIT;

	return AKMD_SUCCESS;

AKMD_SETLICENSE_FAIL:
#if 0
	free(g_prms.p_HSUC_HDST);
AKMD_ALLOC_FAIL_3:
	free(g_prms.p_HFLUCV_HREF);
AKMD_ALLOC_FAIL_2:
	free(g_prms.p_HSUC_HO);
AKMD_ALLOC_FAIL_1:
#endif
	LOGE("AKMD_Init: Alloc failed.");
	return AKMD_ERROR;
}

/******************************************************************************/
/*! Release allocated memory. This function must be called at the end of using
APIs.
\return None
*/
void  AKMD_Release(void)
{
	if(g_init != AKMD_PRMS_NOT_INIT){
#if 0
		// Free
		free(g_prms.p_HSUC_HO);
		free(g_prms.p_HFLUCV_HREF);
		free(g_prms.p_HSUC_HDST);
#endif
		// Put invalid number for safety.
		g_prms.m_maxFormNum = 0;
		// Set flag
		g_init = AKMD_PRMS_NOT_INIT;
		LOGI("AKMD_Release: Memory free.");
	}
}

/******************************************************************************/
/*! Load parameters from a file and initialize SmartCompass library. This 
function must be called when a sequential measurement starts.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in] path The path to a setting file to be read out. The path name should 
be terminated with NULL.
*/
int  AKMD_Start(const char* path)
{
#ifdef AKMD_VALUE_CHECK
	if(g_init == AKMD_PRMS_NOT_INIT){
		LOGE("AKMD_Start: PRMS are not initialized.");
		return AKMD_ERROR;
	}
	if(path == NULL){
		LOGE("AKMD_Start: path can't be NULL.");
		return AKMD_ERROR;
	}
#endif
	// Load parameters
	if(LoadParameters(path, &g_prms) == 0){
		LOGI("AKMD_Start: Setting file cannot be read.");
		// If parameters can't be read from file, set default value.
		SetDefaultPRMS(&g_prms);
	}
	// Init SmartCompass library functions.
	if(InitAK8963_Measure(&g_prms) != AKRET_PROC_SUCCEED){
		LOGE("AKMD_Start: Start error.");
		return AKMD_ERROR;
	}
	return AKMD_SUCCESS;
}

/******************************************************************************/
/*! Save parameters to a file. This function must be called when a sequential 
measurement ends.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in] path The path to a setting file to be written. The path name should 
be terminated with NULL.
*/
int  AKMD_Stop(const char* path)
{
#ifdef AKMD_VALUE_CHECK
	if(g_init == AKMD_PRMS_NOT_INIT){
		LOGE("AKMD_Start: PRMS are not initialized.");
		return AKMD_ERROR;
	}
	if(path == NULL){
		LOGE("AKMD_Start: path can't be NULL.");
		return AKMD_ERROR;
	}
#endif
	// Save parameters
	if(SaveParameters(path, &g_prms) == 0){
		LOGE("AKMD_Stop: Setting file cannot be written.");
		return AKMD_ERROR;
	}
	return AKMD_SUCCESS;
}

/******************************************************************************/
/*! It is used to calculate offset from the sensor value. This function convert
from X,Y,Z data format to SmartCompass "bData" format.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in] hx X-axis value Gaus in Q16 format
\param[in] hy Y-axis value Gaus
\param[in] hz Z-axis value Gaus
\param[in] st2 Specify the ST2 register value.
\param[in] freq Measurement interval in Hz.
*/

/* From Sensor1 to AK8963 */
#define CONVERT_TO_AK8963	(0.010172526)	/* 100 / 65536 / 0.15 */
/* From AK8963 to Android */
#define CONVERT_TO_ANDROID	(0.06) 			/*  */
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
)
{
	int16	ret;
	int16	regx, regy, regz;
	int16	bData[AKSC_BDATA_SIZE];
#ifdef AKMD_VALUE_CHECK
	if(g_init == AKMD_PRMS_NOT_INIT){
		LOGE("AKMD_SaveMag: PRMS are not initialized.");
		return AKMD_ERROR;
	}
	if((ix < AKMD_MAG_MINVAL) || (AKMD_MAG_MAXVAL < ix)){
		LOGE("AKMD_SaveMag: Invalid ix value (%d).", ix);
		return AKMD_ERROR;
	}
	if((iy < AKMD_MAG_MINVAL) || (AKMD_MAG_MAXVAL < iy)){
		LOGE("AKMD_SaveMag: Invalid iy value (%d).", iy);
		return AKMD_ERROR;
	}
	if((iz < AKMD_MAG_MINVAL) || (AKMD_MAG_MAXVAL < iz)){
		LOGE("AKMD_SaveMag: Invalid iz value (%d).", iz);
		return AKMD_ERROR;
	}
#endif
	// Convert from QCT unit to AK8963 register value.
	regx = (int16)(ix * CONVERT_TO_AK8963);
	regy = (int16)(iy * CONVERT_TO_AK8963);
	regz = (int16)(iz * CONVERT_TO_AK8963);

	LOGD("REG DATA:%d,%d,%d", regx, regy, regz);

	// Inverse decomp, i.e. compose
	bData[0] = (int16)(1);
	bData[1] = (int16)(regx & 0xFF);
	bData[2] = (int16)((regx >> 8) & 0xFF);
	bData[3] = (int16)(regy & 0xFF);
	bData[4] = (int16)((regy >> 8) & 0xFF);
	bData[5] = (int16)(regz & 0xFF);
	bData[6] = (int16)((regz >> 8) & 0xFF);
	//bData[7] = (int16)(st2 & 0xFF);
	bData[7] = (int16)(AKMD_ST2_16BIT & 0xFF); /* Fixed 'ST2' for AK8963 */

	//LOGD("BDATA:%02X%02X%02X%02X%02X%02X%02X%02X",
	//	bData[0],bData[1],bData[2],bData[3],bData[4],bData[5],bData[6],bData[7]);

	// TODO: decimator
	// Convert from sensor units to SmartCompass units.
	ret = GetMagneticVector(
		bData,
		&g_prms,
		getFormation(),
		(int16)(freq/10)
	);

	// Check the return value
	if (ret == AKRET_PROC_SUCCEED) {
		; // Do nothing
	} else if (ret == AKRET_FORMATION_CHANGED) {
		AKMDEBUG(DBG_LEVEL3, "Formation changed.\n");
		return AKMD_ERROR;
	} else if (ret == AKRET_DATA_READERROR) {
		AKMDEBUG(DBG_LEVEL3, "Data read error occurred.\n");
		return AKMD_ERROR;
	} else if (ret == AKRET_DATA_OVERFLOW) {
		AKMDEBUG(DBG_LEVEL3, "Data overflow occurred.\n");
		return AKMD_ERROR;
	} else if (ret == AKRET_OFFSET_OVERFLOW) {
		AKMDEBUG(DBG_LEVEL3, "Offset overflow occurred.\n");
		return AKMD_ERROR;
	} else if (ret == AKRET_HBASE_CHANGED) {
		AKMDEBUG(DBG_LEVEL3, "Huge magnetic force.\n");
		return AKMD_ERROR;
	} else if (ret == AKRET_HFLUC_OCCURRED) {
		AKMDEBUG(DBG_LEVEL3, "A fluctuation of magnetic field.\n");
		return AKMD_ERROR;
	} else if (ret == AKRET_VNORM_ERROR) {
		AKMDEBUG(DBG_LEVEL3, "Data normalizeation was failed.\n");
		return AKMD_ERROR;
	} else {
		LOGE("GetMagneticVector has failed (0x%04X).\n", ret);
		return AKMD_ERROR;
	}

	// Convert from SmartCompass to Android
	*ox = g_prms.m_hvec.u.x * CONVERT_TO_ANDROID;
	*oy = g_prms.m_hvec.u.y * CONVERT_TO_ANDROID;
	*oz = g_prms.m_hvec.u.z * CONVERT_TO_ANDROID;

	//Bias offset is in uTesla
	*oxbias = (g_prms.m_ho.u.x + g_prms.m_hbase.u.x) * CONVERT_TO_ANDROID;
	*oybias = (g_prms.m_ho.u.y + g_prms.m_hbase.u.y) * CONVERT_TO_ANDROID;
	*ozbias = (g_prms.m_ho.u.z + g_prms.m_hbase.u.z) * CONVERT_TO_ANDROID;
	
	*accuracy = (int)g_prms.m_hdst;

	return AKMD_SUCCESS;
}


/******************************************************************************/
/*! It resets HDOE
\return None
*/
void  AKMD_ResetDOE(void)
{
	AKSC_SetHDOELevel(&g_prms.m_hdoev, &g_prms.m_ho, AKSC_HDST_UNSOLVED, 1);
}

/******************************************************************************/
/*! It is used to change the calibration values in according to the current 
form.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in] form The number of forum to be used from next API 
call. This number should be larger than 0 and less than maxFormNum which is 
specified in AKMD_Init function.
*/
int  AKMD_ChangeFormation(int form)
{
#ifdef AKMD_VALUE_CHECK
	if(g_init == AKMD_PRMS_NOT_INIT){
		LOGE("AKMD_ChangeFormFactor: PRMS are not initialized.");
		return AKMD_ERROR;
	}
	if((form < 0) || (g_prms.m_maxFormNum <= form)){
		LOGE("AKMD_ChangeFormFactor: Invalid formFactorNumber.");
		return AKMD_ERROR;
	}
#endif
	g_form = form;

	return AKMD_SUCCESS;
}

