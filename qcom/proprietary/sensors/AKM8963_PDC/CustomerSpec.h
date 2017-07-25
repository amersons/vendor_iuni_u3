/******************************************************************************
 *
 *  $Id: CustomerSpec.h 1048 2013-07-18 07:56:35Z yamada.rj $
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
#ifndef AKMD_INC_CUSTOMERSPEC_H
#define AKMD_INC_CUSTOMERSPEC_H

/*******************************************************************************
 User defines parameters.
 ******************************************************************************/

/* Certification information */
#define CSPEC_CI_LICENSER	"ASAHIKASEI"
#define CSPEC_CI_LICENSEE	"GIONEE_63_R1073"

/*
  Parameters for Average
   The number of magnetic data block to be averaged.
   NBaveh*(*nh) must be 1, 2, 4, 8 or 16.
 */
#define CSPEC_HNAVE		8

/* The number of formation */
#define CSPEC_NUM_FORMATION		1

/* the counter of Suspend */
#define CSPEC_CNTSUSPEND_SNG	8

/* PDC file */
//Gionee liujiang modify for akm8963 start
//#define CSPEC_PDC_FILE	"/system/vendor/etc/pdc.txt"
#define CSPEC_PDC_FILE	"/data/misc/sensors/pdc.txt"
//Gionee liujiang modify for akm8963 end
#endif //AKMD_INC_CUSTOMERSPEC_H

