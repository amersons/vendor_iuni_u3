/******************************************************************************
 *
 *  $Id: CustomerSpec.h 336 2011-09-12 07:21:51Z yamada.rj $
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
#ifndef INC_CUSTOMERSPEC_H
#define INC_CUSTOMERSPEC_H

//***************************
// User defines parameters.
//***************************

// Certification information
#define CSPEC_CI_AK_DEVICE	8975
#define CSPEC_CI_LICENSER	"ASAHIKASEI"
#define CSPEC_CI_LICENSEE	"GIONEE_75_R378"

// Parameters for Average
//	The number of magnetic data block to be averaged.
//	 NBaveh*(*nh) must be 1, 2, 4, 8 or 16.
#define CSPEC_HNAVE				8

// The maximum number of formation
#define CSPEC_NUM_FORMATION		1

// the Suspend time in millisecond
#define CSPEC_CNTSUSPEND_TIME	1000

#endif //INC_CUSTOMERSPEC_H

