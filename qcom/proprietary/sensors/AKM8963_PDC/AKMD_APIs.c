/******************************************************************************
 *
 *  $Id: AKMD_APIs.c 1038 2013-06-14 12:29:08Z yamada.rj $
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
#include "AKMD_APIs.h"

#include "AKCompass.h"
#include "FileIO.h"
#include "Measure.h"

#define AKMD_VALUE_CHECK

#if defined(AKMD_AK8975)
#define AKMD_MAG_MINVAL		-786432		/*  1200 uT (12 gauss in Q16) */
#define AKMD_MAG_MAXVAL		786432		/* -1200 uT (12 gauss in Q16) */
#define AKMD_BDATA_SIZE		8
#define AKMD_WIA			0x48
#define AKMD_ASA			0x80
#define AKMD_ST2			0x00
#define CONVERT_TO_AKM		(0.005086263f)	/* From Sensor1 to AKM (100 / 65536 / 0.3) */
#elif defined(AKMD_AK8963)
#define AKMD_MAG_MINVAL		-3219128	/*  4900 uT (49 gauss in Q16) */
#define AKMD_MAG_MAXVAL		3219128		/* -4900 uT (49 gauss in Q16) */
#define AKMD_BDATA_SIZE		8
#define AKMD_WIA			0x48
#define AKMD_ASA			0x80
#define AKMD_ST2			0x10
#define CONVERT_TO_AKM		(0.010172526f)	/* From Sensor1 to AKM (100 / 65536 / 0.15) */
#elif defined(AKMD_AK09912)
#define AKMD_MAG_MINVAL		-3219128	/*  4900 uT (49 gauss in Q16) */
#define AKMD_MAG_MAXVAL		3219128		/* -4900 uT (49 gauss in Q16) */
#define AKMD_BDATA_SIZE		9
#define AKMD_WIA			0x448
#define AKMD_ASA			0x80
#define AKMD_ST2			0x00
#define CONVERT_TO_AKM		(0.010172526f)	/* From Sensor1 to AKM (100 / 65536 / 0.15) */
#elif defined(AKMD_AK09911)
#define AKMD_MAG_MINVAL		-3219128	/*  4900 uT (49 gauss in Q16) */
#define AKMD_MAG_MAXVAL		3219128		/* -4900 uT (49 gauss in Q16) */
#define AKMD_BDATA_SIZE		9
#define AKMD_WIA			0x548
#define AKMD_ASA			0x80
#define AKMD_ST2			0x00
#define CONVERT_TO_AKM		(0.002543131f)	/* From Sensor1 to AKM (100 / 65536 / 0.6) */
#endif

/******************************************************************************/
/*	STATIC Function                                                           */
/******************************************************************************/
/*! Set license key for SmartCompass cetification.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in] regs Register values which are obtained from AKM device.
\param[out] prms The member variable of AKSCPRMS structure (m_key, m_licenser and
 m_licensee) will be filled with license information.
\param[in] licensee Magic phrase.
*/
static void SetLicenseKey(
	const	int16	regs[4],
			AKSCPRMS	*prms,
	const	char	*licensee
)
{
	// Set keywords for SmartCompassLibrary certification
	prms->m_key[0] = AKSC_GetVersion_Device();
	prms->m_key[1] = regs[0];
	prms->m_key[2] = regs[1];
	prms->m_key[3] = regs[2];
	prms->m_key[4] = regs[3];
	strlcpy(prms->m_licenser, CSPEC_CI_LICENSER, AKSC_CI_MAX_CHARSIZE);
	prms->m_licenser[AKSC_CI_MAX_CHARSIZE] = '\0';
	strlcpy(prms->m_licensee, licensee, AKSC_CI_MAX_CHARSIZE);
	prms->m_licensee[AKSC_CI_MAX_CHARSIZE] = '\0';
}


/******************************************************************************/
/*	PUBLIC Function                                                           */
/******************************************************************************/
/*! Get the size of state information. The size will be a multiple of 4.
\return The size of state information.
*/
long AKMD_SAM_GetSizeOfData(void)
{
	size_t sz = sizeof(AKSCPRMS);
	return (sz & (~0x3)) + 4;
}

/*! Initialize AKSCPRMS structure. This function must be called before
application uses any APIs in this file.  If AKSCPRMS are already initialized,
this function discards all existing data.  When APIs are not used anymore,
AKMD_Release function must be called at the end.  When this function succeeds,
form is set to 0.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in/out] mem A pointer to the AKSCPRMS structure.
*/
int AKMD_SAM_Init(void *mem)
{
	AKSCPRMS *prms;
	int16 regs[4];
#ifdef AKMD_VALUE_CHECK
	if(mem == NULL) {
		AKMERROR;
		return AKMD_ERROR;
	}
#endif
	prms = (AKSCPRMS *)mem;

	// Put temporary value
	regs[0] = AKMD_WIA;
	/* Don't adjust ASA since it is done in the ddf driver */
	regs[1] = AKMD_ASA;
	regs[2] = AKMD_ASA;
	regs[3] = AKMD_ASA;

	// Initialize AKSCPRMS structure.
	InitAKSCPRMS(prms);

	// Additional settings
	prms->m_asa.u.x = regs[1];
	prms->m_asa.u.y = regs[2];
	prms->m_asa.u.z = regs[3];

	// Initialize the certification key.
	SetLicenseKey(regs, prms, CSPEC_CI_LICENSEE);

	return AKMD_SUCCESS;
}

/******************************************************************************/
/*! This function must be called at the end of using
APIs.
\return None
\param[in/out] mem A pointer to the AKSCPRMS structure.
*/
void AKMD_SAM_Release(void *mem)
{
	AKSCPRMS *prms;
#ifdef AKMD_VALUE_CHECK
	if(mem == NULL){
		AKMERROR;
		return;
	}
#endif
	prms = (AKSCPRMS *)mem;
}

/******************************************************************************/
/*! Load parameters from a file and initialize SmartCompass library. This
function must be called when a sequential measurement starts.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in/out] mem A pointer to the AKSCPRMS structure.
\param[in] path The path to a setting file to be read out. The path name should
be terminated with NULL.
*/
int AKMD_SAM_Start(void *mem, const char *path)
{
	AKSCPRMS *prms;
#ifdef AKMD_VALUE_CHECK
	if(mem == NULL){
		AKMERROR;
		return AKMD_ERROR;
	}
	if(path == NULL){
		AKMERROR;
		return AKMD_ERROR;
	}
#endif
	prms = (AKSCPRMS *)mem;

	// Load parameters
	if(LoadParameters(path, prms) == 0){
		// If parameters can't be read from file, set default value.
		SetDefaultPRMS(prms);
	}
	// Load PDC
	LoadPDC(CSPEC_PDC_FILE, prms);

	// Init SmartCompass library functions.
	if(Init_Measure(prms) != AKRET_PROC_SUCCEED){
		AKMERROR;
		return AKMD_ERROR;
	}
	return AKMD_SUCCESS;
}

/******************************************************************************/
/*! Save parameters to a file. This function must be called when a sequential
measurement ends.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in/out] mem A pointer to the AKSCPRMS structure.
\param[in] path The path to a setting file to be written. The path name should
be terminated with NULL.
*/
int AKMD_SAM_Stop(void *mem, const char *path)
{
	AKSCPRMS *prms;
#ifdef AKMD_VALUE_CHECK
	if(mem == NULL){
		AKMERROR;
		return AKMD_ERROR;
	}
	if(path == NULL){
		AKMERROR;
		return AKMD_ERROR;
	}
#endif
	prms = (AKSCPRMS *)mem;

	// Save parameters
	if(SaveParameters(path, prms) == 0){
		AKMERROR;
		return AKMD_ERROR;
	}
	return AKMD_SUCCESS;
}

/******************************************************************************/

/* From AKM to Android */
#define CONVERT_TO_ANDROID	(0.06f) 			/*  */

/*! It is used to calculate offset from the sensor value. This function convert
from X,Y,Z data format to SmartCompass "bData" format.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in/out] mem A pointer to the AKSCPRMS structure.
\param[in] hx X-axis value Gauss in Q16 format
\param[in] hy Y-axis value Gauss in Q16 format
\param[in] hz Z-axis value Gauss in Q16 format
\param[in] st2 Specify the ST2 register value.
\param[in] freq Measurement interval in Hz.
\param[out] ox Offset subtracted X-axis agnetic value in AKSC format.
\param[out] oy Offset subtracted Y-axis magnetic value in AKSC format.
\param[out] oz Offset subtracted Z-axis magnetic value in AKSC format.
\param[out] oxbias X-axis offset value in AKSC format.
\param[out] oybias Y-axis offset value in AKSC format.
\param[out] ozbias Z-axis offset value in AKSC format.
\param[out] accuracy Accuracy of calculated offset value.
*/
int AKMD_SAM_GetData(
			void *mem,
	const	int ix,
	const	int iy,
	const	int iz,
	const	int st2,
	const	int freq,
			float *ox,
			float *oy,
			float *oz,
			float *oxbias,
			float *oybias,
			float *ozbias,
			int *accuracy
)
{
	AKSCPRMS *prms;
	int16	ret;
	int16	regx, regy, regz;
	int16	bData[AKMD_BDATA_SIZE];
#ifdef AKMD_VALUE_CHECK
	if (mem == NULL) {
		AKMERROR;
		return AKMD_ERROR;
	}
	if ((freq < 0) || (100 < freq)) {
		AKMERROR;
		return AKMD_ERROR;
	}
#endif
	if ((ix < AKMD_MAG_MINVAL) || (AKMD_MAG_MAXVAL < ix)) {
		AKMERROR;
		return AKMD_ERROR;
	}
	if ((iy < AKMD_MAG_MINVAL) || (AKMD_MAG_MAXVAL < iy)) {
		AKMERROR;
		return AKMD_ERROR;
	}
	if ((iz < AKMD_MAG_MINVAL) || (AKMD_MAG_MAXVAL < iz)) {
		AKMERROR;
		return AKMD_ERROR;
	}

	prms = (AKSCPRMS *)mem;

	// Convert from QCT unit to AKM register value.
	regx = (int16)(ix * CONVERT_TO_AKM);
	regy = (int16)(iy * CONVERT_TO_AKM);
	regz = (int16)(iz * CONVERT_TO_AKM);

	// Inverse decomp, i.e. compose
	bData[0] = (int16)(1);
	bData[1] = (int16)(regx & 0xFF);
	bData[2] = (int16)((regx >> 8) & 0xFF);
	bData[3] = (int16)(regy & 0xFF);
	bData[4] = (int16)((regy >> 8) & 0xFF);
	bData[5] = (int16)(regz & 0xFF);
	bData[6] = (int16)((regz >> 8) & 0xFF);
#if defined(AKMD_AK8975) | defined(AKMD_AK8963)
	bData[7] = (int16)(AKMD_ST2);
#elif defined(AKMD_AK09912) | defined(AKMD_AK09911)
	bData[7] = (int16)(0x80);
	bData[8] = (int16)(AKMD_ST2);
#endif

	//ALOGD("BDATA:%02X%02X%02X%02X%02X%02X%02X%02X%02X",
	//	bData[0],bData[1],bData[2],bData[3],bData[4],bData[5],bData[6],bData[7],bData[8]);
	//ALOGD("REG DATA:%d,%d,%d", regx, regy, regz);

	ret = GetMagneticVector(
		bData,
		prms,
		prms->m_curForm,
		(int16)(freq/10)
	);

	// Check the return value
	if (ret == AKRET_PROC_SUCCEED) {
		; // Do nothing
	} else if (ret == AKRET_FORMATION_CHANGED) {
		AKMERROR;
		return AKMD_ERROR;
	} else if (ret == AKRET_DATA_READERROR) {
		AKMERROR;
		return AKMD_ERROR;
	} else if (ret == AKRET_DATA_OVERFLOW) {
		AKMERROR;
		return AKMD_ERROR;
	} else if (ret == AKRET_OFFSET_OVERFLOW) {
		AKMERROR;
		return AKMD_ERROR;
	} else if (ret == AKRET_HBASE_CHANGED) {
		AKMERROR;
		return AKMD_ERROR;
	} else if (ret == AKRET_HFLUC_OCCURRED) {
		AKMERROR;
		return AKMD_ERROR;
	} else if (ret == AKRET_VNORM_ERROR) {
		AKMERROR;
		return AKMD_ERROR;
	} else {
		AKMERROR;
		return AKMD_ERROR;
	}

	// Convert from SmartCompass to Android
	*ox = prms->m_hvec.u.x * CONVERT_TO_ANDROID;
	*oy = prms->m_hvec.u.y * CONVERT_TO_ANDROID;
	*oz = prms->m_hvec.u.z * CONVERT_TO_ANDROID;

	//Bias offset is in uTesla
	*oxbias = (prms->m_ho.u.x + prms->m_hbase.u.x) * CONVERT_TO_ANDROID;
	*oybias = (prms->m_ho.u.y + prms->m_hbase.u.y) * CONVERT_TO_ANDROID;
	*ozbias = (prms->m_ho.u.z + prms->m_hbase.u.z) * CONVERT_TO_ANDROID;

	*accuracy = (int)prms->m_hdst;

	return AKMD_SUCCESS;
}


/******************************************************************************/
/*! It resets HDOE
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in/out] mem A pointer to the AKSCPRMS structure.
*/
int AKMD_SAM_ResetDOE(void *mem)
{
	AKSCPRMS *prms;
#ifdef AKMD_VALUE_CHECK
	if (mem == NULL) {
		AKMERROR;
		return AKMD_ERROR;
	}
#endif
	prms = (AKSCPRMS *)mem;

	AKSC_SetHDOELevel(&prms->m_hdoev, &prms->m_ho, AKSC_HDST_UNSOLVED, 1);
	return AKMD_SUCCESS;
}

/******************************************************************************/
/*! It is used to change the calibration values in according to the current
form.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in/out] mem A pointer to the AKSCPRMS structure.
\param[in] form The number of forum to be used from next API call. This number
should be larger than 0 and less than CSPEC_NUM_FORMATION.
*/
int AKMD_SAM_ChangeFormation(void *mem, int form)
{
	AKSCPRMS *prms;
#ifdef AKMD_VALUE_CHECK
	if (mem == NULL) {
		AKMERROR;
		return AKMD_ERROR;
	}
	if ((form < 0) || (CSPEC_NUM_FORMATION <= form)) {
		AKMERROR;
		return AKMD_ERROR;
	}
#endif
	prms = (AKSCPRMS *)mem;

	prms->m_curForm = (int16)form;
	return AKMD_SUCCESS;
}


/******************************************************************************/
/*! These functions are for compatibility.
*/

static AKSCPRMS g_prms;
//static int g_prms;
static void		*g_mem = (void *)(&g_prms);

int AKMD_Init(int maxFormNum, int16 regs[]){
	return AKMD_SAM_Init(g_mem);
}

void AKMD_Release(void){
	AKMD_SAM_Release(g_mem);
}

int AKMD_Start(const char* path){
	return AKMD_SAM_Start(g_mem, path);
}

int AKMD_Stop(const char* path){
	return AKMD_SAM_Stop(g_mem, path);
}

int AKMD_GetData(
    const   int ix, 
    const   int iy, 
    const   int iz, 
    const   int st2,
    const   int freq,
            float* ox, 
            float* oy, 
            float* oz, 
            float* oxbias,
            float* oybias,
            float* ozbias,
            int* accuracy
){
	return AKMD_SAM_GetData(
		g_mem,
		ix,
		iy,
		iz,
		st2,
		freq,
		ox,
		oy,
		oz,
		oxbias,
		oybias,
		ozbias,
		accuracy);
}

void AKMD_ResetDOE(void){
	AKMD_SAM_ResetDOE(g_mem);
}

int AKMD_ChangeFormation(int form){
	return AKMD_SAM_ChangeFormation(g_mem, form);
}
