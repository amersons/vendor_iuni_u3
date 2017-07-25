/******************************************************************************
 *
 *  $Id: Measure.c 370 2011-10-12 08:00:39Z yamada.rj $
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
#ifdef WIN32
#include "AK8975Driver.h"
#endif
#include "Measure.h"


/*!
 Initialize #AK8975PRMS structure. At first, 0 is set to all parameters. 
 After that, some parameters, which should not be 0, are set to specific
 value. Some of initial values can be customized by editing the file
 \c "CustomerSpec.h".
 @param[out] prms A pointer to #AK8975PRMS structure.
 */
void InitAK8975PRMS(AK8975PRMS* prms)
{
	// Sensitivity
	prms->m_hs.u.x = AKSC_HSENSE_TARGET;
	prms->m_hs.u.y = AKSC_HSENSE_TARGET;
	prms->m_hs.u.z = AKSC_HSENSE_TARGET;

	// HDOE
	prms->m_hdst = AKSC_HDST_UNSOLVED;

	// m_hdata is initialized with AKSC_InitDecomp8975
	prms->m_hnave = CSPEC_HNAVE;
}

/*!
 Fill #AK8975PRMS structure with default value.
 @param[out] prms A pointer to #AK8975PRMS structure.
 */
void SetDefaultPRMS(AK8975PRMS* prms)
{
	int i;
	// Set parameter to HDST, HO, HREF
	for(i=0; i<prms->m_maxFormNum; i++){
		prms->p_HSUC_HDST[i] = AKSC_HDST_UNSOLVED;
		prms->p_HSUC_HO[i].u.x = 0;
		prms->p_HSUC_HO[i].u.y = 0;
		prms->p_HSUC_HO[i].u.z = 0;
		prms->p_HFLUCV_HREF[i].u.x = 0;
		prms->p_HFLUCV_HREF[i].u.y = 0;
		prms->p_HFLUCV_HREF[i].u.z = 0;
	}
}

/*!
 Set initial values to registers of AK8975. Then initialize algorithm 
 parameters.
 @return If parameters are read successfully, the return value is 
 #AKRET_PROC_SUCCEED. Otherwise the return value is #AKRET_PROC_FAIL. No 
 error code is reserved to show which operation has failed.
 @param[in,out] prms A pointer to a #AK8975PRMS structure.
 */
int16 InitAK8975_Measure(AK8975PRMS* prms)
{
	// Get current formation
	prms->m_form = getFormation();

	// Restore the value when succeeding in estimating of HOffset. 
	prms->m_ho   = prms->p_HSUC_HO[prms->m_form];
	prms->m_hdst = prms->p_HSUC_HDST[prms->m_form];

	// Initialize the decompose parameters
	AKSC_InitDecomp8975(prms->m_hdata);
	
	// Initialize HDOE parameters
	AKSC_InitHDOEProcPrmsS3(
							&prms->m_hdoev,
							1,
							&prms->m_ho,
							prms->m_hdst
							);
	
	AKSC_InitHFlucCheck(
						&(prms->m_hflucv),
						&(prms->p_HFLUCV_HREF[prms->m_form]),
						HFLUCV_TH
						);

	// Reset counter
	prms->m_cntSuspend = 0;
	prms->m_callcnt = 0;
	
	return AKRET_PROC_SUCCEED;
}

/*!
 Set initial values to registers of AK8975. Then initialize algorithm 
 parameters.
 @return Currently, this function always return #AKRET_PROC_SUCCEED. 
 @param[in,out] prms A pointer to a #AK8975PRMS structure.
 */
int16 SwitchFormation(AK8975PRMS* prms)
{
	// Restore the value when succeeding in estimating of HOffset. 
	prms->m_ho   = prms->p_HSUC_HO[prms->m_form]; 
	prms->m_hdst = prms->p_HSUC_HDST[prms->m_form];
	
	// Initialize the decompose parameters
	AKSC_InitDecomp8975(prms->m_hdata);
	
	// Initialize HDOE parameters
	AKSC_InitHDOEProcPrmsS3(
							&prms->m_hdoev,
							1,
							&prms->m_ho,
							prms->m_hdst
							);

	AKSC_InitHFlucCheck(
						&(prms->m_hflucv),
						&(prms->p_HFLUCV_HREF[prms->m_form]),
						HFLUCV_TH
						);
	
	// Reset counter
	prms->m_callcnt = 0;
	
	return AKRET_PROC_SUCCEED;
}

int16 MeasuringEventProcess(
							const int16	bData[],
							AK8975PRMS*	prms,
							const int16	curForm,
							const int16	hDecimator,
							const int16	cntSuspend
							)
{
	int16vec have;
	int16    dor, derr, hofl;
	int16    isOF;
	int16    aksc_ret;
	int16    hdSucc;
	
	dor = 0;
	derr = 0;
	hofl = 0;
	
	// Decompose one block data into each Magnetic sensor's data
	aksc_ret = AKSC_Decomp8975(
							   bData,
							   prms->m_hnave,
							   &prms->m_asa,
							   prms->m_hdata,
							   &prms->m_hn,
							   &have,
							   &dor,
							   &derr,
							   &hofl
							   );

	AKM_LOG("%s: ST1, HXH&HXL, HYH&HYL, HZH&HZL, ST2,"
			" hdata[0].u.x, hdata[0].u.y, hdata[0].u.z,"
			" asax, asay, asaz ="
			" %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
			__FUNCTION__,
			bData[0],
			(int16)(((uint16)bData[2])<<8|bData[1]),
			(int16)(((uint16)bData[4])<<8|bData[3]),
			(int16)(((uint16)bData[6])<<8|bData[5]), bData[7],
			prms->m_hdata[0].u.x, prms->m_hdata[0].u.y, prms->m_hdata[0].u.z,
			prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z);

	if (aksc_ret == 0) {
		DBGPRINT(DBG_LEVEL1, 
				 "AKSC_Decomp8975 failed. asa(dec)=%d,%d,%d hn=%d\n", 
				 prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z, prms->m_hn);
		return AKRET_PROC_FAIL;
	}
	
	// Check the formation change
	if(prms->m_form != curForm){
		// Set counter
		prms->m_cntSuspend = cntSuspend;
		
		prms->m_form = curForm;
		
		return AKRET_FORMATION_CHANGED;
	}
	
	if(derr == 1){
		return AKRET_DATA_READERROR;
	}
	
	if(prms->m_cntSuspend > 0){
		prms->m_cntSuspend--;
	}
	else {
		// Detect a fluctuation of magnetic field.
		isOF = AKSC_HFlucCheck(&(prms->m_hflucv), &(prms->m_hdata[0]));
		
		if(hofl == 1){
			// Set a HDOE level as "HDST_UNSOLVED" 
			AKSC_SetHDOELevel(
							  &prms->m_hdoev,
							  &prms->m_ho,
							  AKSC_HDST_UNSOLVED,
							  1
							  );
			prms->m_hdst = AKSC_HDST_UNSOLVED;
			return AKRET_DATA_OVERFLOW;
		}
		else if(isOF == 1){
			// Set a HDOE level as "HDST_UNSOLVED" 
			AKSC_SetHDOELevel(
							  &prms->m_hdoev,
							  &prms->m_ho,
							  AKSC_HDST_UNSOLVED,
							  1
							  );
			prms->m_hdst = AKSC_HDST_UNSOLVED;
			return AKRET_HFLUC_OCCURRED;
		}
		else {
			prms->m_callcnt--;
			if(prms->m_callcnt <= 0){
				//Calculate Magnetic sensor's offset by DOE
				hdSucc = AKSC_HDOEProcessS3(
											prms->m_licenser,
											prms->m_licensee,
											prms->m_key,
											&prms->m_hdoev,
											prms->m_hdata,
											prms->m_hn,
											&prms->m_ho,
											&prms->m_hdst
											);
				
				if(hdSucc > 0){
					prms->p_HSUC_HO[prms->m_form] = prms->m_ho;
					prms->p_HSUC_HDST[prms->m_form] = prms->m_hdst;
					prms->p_HFLUCV_HREF[prms->m_form] = prms->m_hflucv.href;
				}
				
				prms->m_callcnt = hDecimator;
			}
		}
	}
	
	// Subtract offset and normalize magnetic field vector.
	aksc_ret = AKSC_VNorm(
						  &have,
						  &prms->m_ho,
						  &prms->m_hs,
						  AKSC_HSENSE_TARGET,
						  &prms->m_hvec
						  );
	if (aksc_ret == 0) {
		DBGPRINT(DBG_LEVEL1, "AKSC_VNorm failed.\n");
		return AKRET_PROC_FAIL;
	}

	return AKRET_PROC_SUCCEED;
}

