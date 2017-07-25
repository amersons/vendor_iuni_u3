/******************************************************************************
 *
 * $Id: Measure.c 383 2011-11-21 08:14:27Z yamada.rj $
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
#ifdef WIN32
#include "AK8963Driver.h"
#endif
#include "Measure.h"
#include "FileIO.h"
//Gionee liujiang 2013-05-27 add for android4.2 start
#include "AKMD_APIs.h"
//Gionee liujiang 2013-05-27 add for android4.2 end
/* deg x (pi/180.0) */
#define DEG2RAD(deg)		((deg) * 0.017453292)

static FORM_CLASS* g_form = NULL;

/*!
 This function open formation status device.
 @return Return 0 on success. Negative value on fail.
 */
static int16 openForm(void)
{
	if (g_form != NULL) {
		if (g_form->open != NULL) {
			return g_form->open();
		}
	}
	// If function is not set, return success.
	return 0;
}

/*!
 This function close formation status device.
 @return None.
 */
static void closeForm(void)
{
	if (g_form != NULL) {
		if (g_form->close != NULL) {
			g_form->close();
		}
	}
}

/*!
 This function check formation status
 @return The index of formation.
 */
static int16 checkForm(void)
{
	if (g_form != NULL) {
		if (g_form->check != NULL) {
			return g_form->check();
		}
	}
	// If function is not set, return default value.
	return 0;
}

/*!
 This function registers the callback function.
 @param[in]
 */
void RegisterFormClass(FORM_CLASS* pt)
{
	g_form = pt;
}

/*!
 Initialize #AK8963PRMS structure. At first, 0 is set to all parameters.
 After that, some parameters, which should not be 0, are set to specific
 value. Some of initial values can be customized by editing the file
 \c "CustomerSpec.h".
 @param[out] prms A pointer to #AK8963PRMS structure.
 */
void InitAK8963PRMS(AK8963PRMS* prms)
{
	// Set 0 to the AK8963PRMS structure.
	memset(prms, 0, sizeof(AK8963PRMS));

	// Sensitivity
	prms->m_hs.u.x = AKSC_HSENSE_TARGET;
	prms->m_hs.u.y = AKSC_HSENSE_TARGET;
	prms->m_hs.u.z = AKSC_HSENSE_TARGET;

	// HDOE
	prms->m_hdst = AKSC_HDST_UNSOLVED;

	// Number of average
	prms->m_hnave = CSPEC_HNAVE;
}

/*!
 Fill #AK8963PRMS structure with default value.
 @param[out] prms A pointer to #AK8963PRMS structure.
 */
void SetDefaultPRMS(AK8963PRMS* prms)
{
	int16 i;
	// Set parameter to HDST, HO, HREF
	for (i = 0; i < CSPEC_NUM_FORMATION; i++) {
		prms->HSUC_HDST[i] = AKSC_HDST_UNSOLVED;
		prms->HSUC_HO[i].u.x = 0;
		prms->HSUC_HO[i].u.y = 0;
		prms->HSUC_HO[i].u.z = 0;
		prms->HFLUCV_HREF[i].u.x = 0;
		prms->HFLUCV_HREF[i].u.y = 0;
		prms->HFLUCV_HREF[i].u.z = 0;
		prms->HSUC_HBASE[i].u.x = 0;
		prms->HSUC_HBASE[i].u.y = 0;
		prms->HSUC_HBASE[i].u.z = 0;
	}
}

/*!
 Set initial values to registers of AK8963. Then initialize algorithm
 parameters.
 @return If parameters are read successfully, the return value is
 #AKRET_PROC_SUCCEED. Otherwise the return value is #AKRET_PROC_FAIL. No
 error code is reserved to show which operation has failed.
 @param[in,out] prms A pointer to a #AK8963PRMS structure.
 */
int16 InitAK8963_Measure(AK8963PRMS* prms)
{
#if 0
	// Reset device.
	if (AKD_ResetAK8963() != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}
#endif

	prms->m_form = checkForm();

	// Restore the value when succeeding in estimating of HOffset.
	prms->m_ho   = prms->HSUC_HO[prms->m_form];
	prms->m_ho32.u.x = (int32)prms->HSUC_HO[prms->m_form].u.x;
	prms->m_ho32.u.y = (int32)prms->HSUC_HO[prms->m_form].u.y;
	prms->m_ho32.u.z = (int32)prms->HSUC_HO[prms->m_form].u.z;

	prms->m_hdst = prms->HSUC_HDST[prms->m_form];
	prms->m_hbase = prms->HSUC_HBASE[prms->m_form];

	// Initialize the decompose parameters
	AKSC_InitDecomp8963(prms->m_hdata);

	// Initialize HDOE parameters
	AKSC_InitHDOEProcPrmsS3(
							&prms->m_hdoev,
							1,
							&prms->m_ho,
							prms->m_hdst
							);

	AKSC_InitHFlucCheck(
						&(prms->m_hflucv),
						&(prms->HFLUCV_HREF[prms->m_form]),
						HFLUCV_TH
						);

	// Reset counter
	prms->m_cntSuspend = 0;
	prms->m_callcnt = 0;

	return AKRET_PROC_SUCCEED;
}


/*!
 Execute "Onboard Function Test" (NOT includes "START" and "END" command).
 @retval 1 The test is passed successfully.
 @retval -1 The test is failed.
 @retval 0 The test is aborted by kind of system error.
 @param[in] prms A pointer to a #AK8963PRMS structure.
 */
#if 0
int16 FctShipmntTestProcess_Body(AK8963PRMS* prms)
{
	int16   pf_total;  //p/f flag for this subtest
	BYTE    i2cData[16];
	int16   hdata[3];
	int16   asax;
	int16   asay;
	int16   asaz;

	//***********************************************
	//  Reset Test Result
	//***********************************************
	pf_total = 1;

	//***********************************************
	//  Step1
	//***********************************************

	// Reset device.
	if (AKD_ResetAK8963() != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// When the serial interface is SPI,
	// write "00011011" to I2CDIS register(to disable I2C,).
	if (CSPEC_SPI_USE == 1) {
		i2cData[0] = 0x1B;
		if (AKD_TxData(AK8963_REG_I2CDIS, i2cData, 1) != AKD_SUCCESS) {
			AKMERROR;
			return 0;
		}
	}

	// Read values from WIA to ASTC.
	if (AKD_RxData(AK8963_REG_WIA, i2cData, 13) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// TEST
	TEST_DATA(TLIMIT_NO_RST_WIA,   TLIMIT_TN_RST_WIA,   (int16)i2cData[0],  TLIMIT_LO_RST_WIA,   TLIMIT_HI_RST_WIA,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_INFO,  TLIMIT_TN_RST_INFO,  (int16)i2cData[1],  TLIMIT_LO_RST_INFO,  TLIMIT_HI_RST_INFO,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_ST1,   TLIMIT_TN_RST_ST1,   (int16)i2cData[2],  TLIMIT_LO_RST_ST1,   TLIMIT_HI_RST_ST1,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HXL,   TLIMIT_TN_RST_HXL,   (int16)i2cData[3],  TLIMIT_LO_RST_HXL,   TLIMIT_HI_RST_HXL,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HXH,   TLIMIT_TN_RST_HXH,   (int16)i2cData[4],  TLIMIT_LO_RST_HXH,   TLIMIT_HI_RST_HXH,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HYL,   TLIMIT_TN_RST_HYL,   (int16)i2cData[5],  TLIMIT_LO_RST_HYL,   TLIMIT_HI_RST_HYL,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HYH,   TLIMIT_TN_RST_HYH,   (int16)i2cData[6],  TLIMIT_LO_RST_HYH,   TLIMIT_HI_RST_HYH,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HZL,   TLIMIT_TN_RST_HZL,   (int16)i2cData[7],  TLIMIT_LO_RST_HZL,   TLIMIT_HI_RST_HZL,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HZH,   TLIMIT_TN_RST_HZH,   (int16)i2cData[8],  TLIMIT_LO_RST_HZH,   TLIMIT_HI_RST_HZH,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_ST2,   TLIMIT_TN_RST_ST2,   (int16)i2cData[9],  TLIMIT_LO_RST_ST2,   TLIMIT_HI_RST_ST2,   &pf_total);
	TEST_DATA(TLIMIT_NO_RST_CNTL1, TLIMIT_TN_RST_CNTL1, (int16)i2cData[10], TLIMIT_LO_RST_CNTL1, TLIMIT_HI_RST_CNTL1, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_CNTL2, TLIMIT_TN_RST_CNTL2, (int16)i2cData[11], TLIMIT_LO_RST_CNTL2, TLIMIT_HI_RST_CNTL2, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_ASTC,  TLIMIT_TN_RST_ASTC,  (int16)i2cData[12], TLIMIT_LO_RST_ASTC,  TLIMIT_HI_RST_ASTC,  &pf_total);

	// Read values from I2CDIS.
	if (AKD_RxData(AK8963_REG_I2CDIS, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	if (CSPEC_SPI_USE == 1) {
		TEST_DATA(TLIMIT_NO_RST_I2CDIS, TLIMIT_TN_RST_I2CDIS, (int16)i2cData[0], TLIMIT_LO_RST_I2CDIS_USESPI, TLIMIT_HI_RST_I2CDIS_USESPI, &pf_total);
	}else{
		TEST_DATA(TLIMIT_NO_RST_I2CDIS, TLIMIT_TN_RST_I2CDIS, (int16)i2cData[0], TLIMIT_LO_RST_I2CDIS_USEI2C, TLIMIT_HI_RST_I2CDIS_USEI2C, &pf_total);
	}

	// Set to FUSE ROM access mode
	if (AKD_SetMode(AK8963_MODE_FUSE_ACCESS) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Read values from ASAX to ASAZ
	if (AKD_RxData(AK8963_FUSE_ASAX, i2cData, 3) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	asax = (int16)i2cData[0];
	asay = (int16)i2cData[1];
	asaz = (int16)i2cData[2];

	// TEST
	TEST_DATA(TLIMIT_NO_ASAX, TLIMIT_TN_ASAX, asax, TLIMIT_LO_ASAX, TLIMIT_HI_ASAX, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAY, TLIMIT_TN_ASAY, asay, TLIMIT_LO_ASAY, TLIMIT_HI_ASAY, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAZ, TLIMIT_TN_ASAZ, asaz, TLIMIT_LO_ASAZ, TLIMIT_HI_ASAZ, &pf_total);

	// Read values. CNTL
	if (AKD_RxData(AK8963_REG_CNTL1, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Set to PowerDown mode
	if (AKD_SetMode(AK8963_MODE_POWERDOWN) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// TEST
	TEST_DATA(TLIMIT_NO_WR_CNTL1, TLIMIT_TN_WR_CNTL1, (int16)i2cData[0], TLIMIT_LO_WR_CNTL1, TLIMIT_HI_WR_CNTL1, &pf_total);

	//***********************************************
	//  Step2
	//***********************************************

	// Set to SNG measurement pattern (Set CNTL register)
	if (AKD_SetMode(AK8963_MODE_SNG_MEASURE | (prms->m_outbit << 4)) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Wait for DRDY pin changes to HIGH.
	// Get measurement data from AK8963
	// ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + ST2
	// = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 = 8 bytes
	if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	hdata[0] = (int16)((((uint16)(i2cData[2]))<<8)+(uint16)(i2cData[1]));
	hdata[1] = (int16)((((uint16)(i2cData[4]))<<8)+(uint16)(i2cData[3]));
	hdata[2] = (int16)((((uint16)(i2cData[6]))<<8)+(uint16)(i2cData[5]));

	if((i2cData[7] & 0x10) == 0){ // 14bit mode
		hdata[0] <<= 2;
		hdata[1] <<= 2;
		hdata[2] <<= 2;
	}	

	// TEST
	TEST_DATA(TLIMIT_NO_SNG_ST1, TLIMIT_TN_SNG_ST1, (int16)i2cData[0], TLIMIT_LO_SNG_ST1, TLIMIT_HI_SNG_ST1, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HX, TLIMIT_TN_SNG_HX, hdata[0], TLIMIT_LO_SNG_HX, TLIMIT_HI_SNG_HX, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HY, TLIMIT_TN_SNG_HY, hdata[1], TLIMIT_LO_SNG_HY, TLIMIT_HI_SNG_HY, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HZ, TLIMIT_TN_SNG_HZ, hdata[2], TLIMIT_LO_SNG_HZ, TLIMIT_HI_SNG_HZ, &pf_total);
	if((i2cData[7] & 0x10) == 0){ // 14bit mode
		TEST_DATA(
			TLIMIT_NO_SNG_ST2,
			TLIMIT_TN_SNG_ST2,
			(int16)i2cData[7],
			TLIMIT_LO_SNG_ST2_14BIT,
			TLIMIT_HI_SNG_ST2_14BIT,
			&pf_total
			);
	} else { // 16bit mode
		TEST_DATA(
			TLIMIT_NO_SNG_ST2,
			TLIMIT_TN_SNG_ST2,
			(int16)i2cData[7],
			TLIMIT_LO_SNG_ST2_16BIT,
			TLIMIT_HI_SNG_ST2_16BIT,
			&pf_total
			);
	}

	// Generate magnetic field for self-test (Set ASTC register)
	i2cData[0] = 0x40;
	if (AKD_TxData(AK8963_REG_ASTC, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Set to Self-test mode (Set CNTL register)
	if (AKD_SetMode(AK8963_MODE_SELF_TEST | (prms->m_outbit << 4)) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// Wait for DRDY pin changes to HIGH.
	// Get measurement data from AK8963
	// ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + ST2
	// = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 = 8Byte
	if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	// TEST
	TEST_DATA(TLIMIT_NO_SLF_ST1, TLIMIT_TN_SLF_ST1, (int16)i2cData[0], TLIMIT_LO_SLF_ST1, TLIMIT_HI_SLF_ST1, &pf_total);

	hdata[0] = (int16)((((uint16)(i2cData[2]))<<8)+(uint16)(i2cData[1]));
	hdata[1] = (int16)((((uint16)(i2cData[4]))<<8)+(uint16)(i2cData[3]));
	hdata[2] = (int16)((((uint16)(i2cData[6]))<<8)+(uint16)(i2cData[5]));

	if((i2cData[7] & 0x10) == 0){ // 14bit mode
		hdata[0] <<= 2;
		hdata[1] <<= 2;
		hdata[2] <<= 2;
	}

	// TEST
	TEST_DATA(
			  TLIMIT_NO_SLF_RVHX,
			  TLIMIT_TN_SLF_RVHX,
			  (hdata[0])*((asax - 128)*0.5f/128.0f + 1),
			  TLIMIT_LO_SLF_RVHX,
			  TLIMIT_HI_SLF_RVHX,
			  &pf_total
			  );

	TEST_DATA(
			  TLIMIT_NO_SLF_RVHY,
			  TLIMIT_TN_SLF_RVHY,
			  (hdata[1])*((asay - 128)*0.5f/128.0f + 1),
			  TLIMIT_LO_SLF_RVHY,
			  TLIMIT_HI_SLF_RVHY,
			  &pf_total
			  );

	TEST_DATA(
			  TLIMIT_NO_SLF_RVHZ,
			  TLIMIT_TN_SLF_RVHZ,
			  (hdata[2])*((asaz - 128)*0.5f/128.0f + 1),
			  TLIMIT_LO_SLF_RVHZ,
			  TLIMIT_HI_SLF_RVHZ,
			  &pf_total
			  );

	// TEST
	if((i2cData[7] & 0x10) == 0){ // 14bit mode
		TEST_DATA(
			TLIMIT_NO_SLF_ST2,
			TLIMIT_TN_SLF_ST2,
			(int16)i2cData[7],
			TLIMIT_LO_SLF_ST2_14BIT,
			TLIMIT_HI_SLF_ST2_14BIT,
			&pf_total
			);
	}else{ // 16bit mode
		TEST_DATA(
			TLIMIT_NO_SLF_ST2,
			TLIMIT_TN_SLF_ST2,
			(int16)i2cData[7],
			TLIMIT_LO_SLF_ST2_16BIT,
			TLIMIT_HI_SLF_ST2_16BIT,
			&pf_total
			);
	}

	// Set to Normal mode for self-test.
	i2cData[0] = 0x00;
	if (AKD_TxData(AK8963_REG_ASTC, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}

	return pf_total;
}
#endif


/*!
 SmartCompass main calculation routine. This function will be processed
 when INT pin event is occurred.
 @retval AKRET_
 @param[in] bData An array of register values which holds,
 ST1, HXL, HXH, HYL, HYH, HZL, HZH and ST2 value respectively.
 @param[in,out] prms A pointer to a #AK8963PRMS structure.
 @param[in] curForm The index of hardware position which represents the
 index when this function is called.
 @param[in] hDecimator HDOE will execute once while this function is called
 this number of times.
 */
int16 GetMagneticVector(
	const int16	bData[],
	AK8963PRMS*	prms,
	const int16	curForm,
	const int16	hDecimator)
{
	const int16vec hrefZero = {{0, 0, 0}};
	int16vec	have;
	int16		dor, derr, hofl, cb;
	int32vec	preHbase;
	int16		overflow;
	int16		hfluc;
	int16		hdSucc;
	int16		aksc_ret;
	int16		ret;

	have.u.x = 0;
	have.u.y = 0;
	have.u.z = 0;
	dor = 0;
	derr = 0;
	hofl = 0;
	cb = 0;
	preHbase = prms->m_hbase;
	overflow = 0;
	ret = AKRET_PROC_SUCCEED;

	// Subtract the formation suspend counter
	if (prms->m_cntSuspend > 0) {
		prms->m_cntSuspend--;

		// Check the suspend counter
		if (prms->m_cntSuspend == 0) {
			// Restore the value when succeeding in estimating of HOffset.
			prms->m_ho   = prms->HSUC_HO[prms->m_form];
			prms->m_ho32.u.x = (int32)prms->HSUC_HO[prms->m_form].u.x;
			prms->m_ho32.u.y = (int32)prms->HSUC_HO[prms->m_form].u.y;
			prms->m_ho32.u.z = (int32)prms->HSUC_HO[prms->m_form].u.z;

			prms->m_hdst = prms->HSUC_HDST[prms->m_form];
			prms->m_hbase = prms->HSUC_HBASE[prms->m_form];

			// Initialize the decompose parameters
			AKSC_InitDecomp8963(prms->m_hdata);

			// Initialize HDOE parameters
			AKSC_InitHDOEProcPrmsS3(
				&prms->m_hdoev,
				1,
				&prms->m_ho,
				prms->m_hdst
			);

			// Initialize HFlucCheck parameters
			AKSC_InitHFlucCheck(
				&(prms->m_hflucv),
				&(prms->HFLUCV_HREF[prms->m_form]),
				HFLUCV_TH
			);
		}
	}

	// Decompose one block data into each Magnetic sensor's data
	aksc_ret = AKSC_Decomp8963(
					bData,
					prms->m_hnave,
					&prms->m_asa,
					prms->m_hdata,
					&prms->m_hbase,
					&prms->m_hn,
					&have,
					&dor,
					&derr,
					&hofl,
					&cb
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
		AKMDUMP("AKSC_Decomp8963 failed.\n"
				"  ST1=0x%02X, ST2=0x%02X\n"
				"  XYZ(HEX)=%02X,%02X,%02X,%02X,%02X,%02X\n"
				"  asa(dec)=%d,%d,%d\n"
				"  hbase(dec)=%ld,%ld,%ld\n",
				bData[0], bData[7],
				bData[1], bData[2], bData[3], bData[4], bData[5], bData[6],
				prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z,
				prms->m_hbase.u.x, prms->m_hbase.u.y, prms->m_hbase.u.z);
		return AKRET_PROC_FAIL;
	}

	// Check the formation change
	if (prms->m_form != curForm) {
		prms->m_form = curForm;
		prms->m_cntSuspend = CSPEC_CNTSUSPEND_SNG;
		prms->m_callcnt = 0;
		ret |= AKRET_FORMATION_CHANGED;
		return ret;
	}

	// Check derr
	if (derr == 1) {
		ret |= AKRET_DATA_READERROR;
		return ret;
	}

	// Check hofl
	if (hofl == 1) {
		if (prms->m_cntSuspend <= 0) {
			// Set a HDOE level as "HDST_UNSOLVED"
			AKSC_SetHDOELevel(
							  &prms->m_hdoev,
							  &prms->m_ho,
							  AKSC_HDST_UNSOLVED,
							  1
							  );
			prms->m_hdst = AKSC_HDST_UNSOLVED;
		}
		ret |= AKRET_DATA_OVERFLOW;
		return ret;
	}

	// Check hbase
	if (cb == 1) {
		// Translate HOffset
		AKSC_TransByHbase(
			&(preHbase),
			&(prms->m_hbase),
			&(prms->m_ho),
			&(prms->m_ho32),
			&overflow
		);
		if (overflow == 1) {
			ret |= AKRET_OFFSET_OVERFLOW;
		}

		// Set hflucv.href to 0
		AKSC_InitHFlucCheck(
			&(prms->m_hflucv),
			&(hrefZero),
			HFLUCV_TH
		);

		if (prms->m_cntSuspend <= 0) {
			AKSC_SetHDOELevel(
				&prms->m_hdoev,
				&prms->m_ho,
				AKSC_HDST_UNSOLVED,
				1
			);
			prms->m_hdst = AKSC_HDST_UNSOLVED;
		}

		ret |= AKRET_HBASE_CHANGED;
		return ret;
	}

	if (prms->m_cntSuspend <= 0) {
		// Detect a fluctuation of magnetic field.
		hfluc = AKSC_HFlucCheck(&(prms->m_hflucv), &(prms->m_hdata[0]));

		if (hfluc == 1) {
			// Set a HDOE level as "HDST_UNSOLVED"
			AKSC_SetHDOELevel(
				&prms->m_hdoev,
				&prms->m_ho,
				AKSC_HDST_UNSOLVED,
				1
			);
			prms->m_hdst = AKSC_HDST_UNSOLVED;
			ret |= AKRET_HFLUC_OCCURRED;
			return ret;
		}
		else {
			prms->m_callcnt--;
			if (prms->m_callcnt <= 0) {
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

				if (hdSucc == AKSC_CERTIFICATION_DENIED) {
					AKMERROR;
					return AKRET_PROC_FAIL;
				}
				if (hdSucc > 0) {
					prms->HSUC_HO[prms->m_form] = prms->m_ho;
					prms->m_ho32.u.x = (int32)prms->m_ho.u.x;
					prms->m_ho32.u.y = (int32)prms->m_ho.u.y;
					prms->m_ho32.u.z = (int32)prms->m_ho.u.z;

					prms->HSUC_HDST[prms->m_form] = prms->m_hdst;
					prms->HFLUCV_HREF[prms->m_form] = prms->m_hflucv.href;
					prms->HSUC_HBASE[prms->m_form] = prms->m_hbase;
				}

				//Set decimator counter
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
		AKMDUMP("AKSC_VNorm failed.\n"
				"  have=%6d,%6d,%6d  ho=%6d,%6d,%6d  hs=%6d,%6d,%6d\n",
				have.u.x, have.u.y, have.u.z,
				prms->m_ho.u.x, prms->m_ho.u.y, prms->m_ho.u.z,
				prms->m_hs.u.x, prms->m_hs.u.y, prms->m_hs.u.z);
		ret |= AKRET_VNORM_ERROR;
		return ret;
	}

#if 0
	//Convert layout from sensor to Android by using PAT number.
	// Magnetometer
	ConvertCoordinate(prms->m_layout, &prms->m_hvec);
#endif

	return AKRET_PROC_SUCCEED;
}

