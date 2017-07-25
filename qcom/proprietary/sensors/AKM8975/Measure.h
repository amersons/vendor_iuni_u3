/******************************************************************************
 *
 *  $Id: Measure.h 336 2011-09-12 07:21:51Z yamada.rj $
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
#ifndef AKMD_INC_MEASURE_H
#define AKMD_INC_MEASURE_H

// Include files for AK8975 library.
#include "AKCompass.h"

/*** Constant definition ******************************************************/
#define AKRET_PROC_FAIL         0x00	/*!< The process failes. */
#define AKRET_PROC_SUCCEED      0x01	/*!< The process has been successfully done. */
#define AKRET_FORMATION_CHANGED 0x02	/*!< The formation is changed */
#define AKRET_HFLUC_OCCURRED    0x03	/*!< A magnetic field fluctuation occurred. */
#define AKRET_DATA_OVERFLOW     0x04	/*!< Data overflow occurred. */
#define AKRET_DATA_READERROR    0x05	/*!< Data read error occurred. */

/*** Type declaration *********************************************************/

/*** Global variables *********************************************************/

/*** Prototype of function ****************************************************/
extern int16 getFormation(void);

void InitAK8975PRMS(
	AK8975PRMS*	prms	/*!< [out] Pointer to parameters of AK8975 */
);

void SetDefaultPRMS(
	AK8975PRMS*	prms	/*!< [in,out] Pointer to parameters of AK8975 */
);

int16 InitAK8975_Measure(
	AK8975PRMS*	prms	/*!< [in,out] Pointer to parameters of AK8975 */
);

int16 SwitchFormation(
	AK8975PRMS*	prms	/*!< [in,out] Pointer to parameters of AK8975 */
);

int16 MeasuringEventProcess(
	const	int16 bData[],
			AK8975PRMS* prms,
	const	int16 curForm,
	const	int16 hDecimator,
	const	int16 cntSuspend
);

#endif

