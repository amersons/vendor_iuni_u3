/******************************************************************************
 *
 * $Id: CustomerSpec.h 383 2011-11-21 08:14:27Z yamada.rj $
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
#ifndef AKMD_INC_CUSTOMERSPEC_H
#define AKMD_INC_CUSTOMERSPEC_H

/*******************************************************************************
 User defines parameters.
 ******************************************************************************/

// Certification information
#define CSPEC_CI_AK_DEVICE	8963
#define CSPEC_CI_LICENSER	"ASAHIKASEI"
#define CSPEC_CI_LICENSEE	"GIONEE_63_R601"

// Parameters for Average
//	The number of magnetic data block to be averaged.
//	 NBaveh*(*nh) must be 1, 2, 4, 8 or 16.
#define CSPEC_HNAVE		8

// The number of formation
#define CSPEC_NUM_FORMATION		1

// the counter of Suspend
#define CSPEC_CNTSUSPEND_SNG	8

//// Parameters for FctShipmntTest
////  1 : USE SPI
////  0 : NOT USE SPI(I2C)
//#define CSPEC_SPI_USE			0

// Setting file
#define CSPEC_SETTING_FILE	"/data/misc/akmd_set.txt"

#endif //AKMD_INC_CUSTOMERSPEC_H

