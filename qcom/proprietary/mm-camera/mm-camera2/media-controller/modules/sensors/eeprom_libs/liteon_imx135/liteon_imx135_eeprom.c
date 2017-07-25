/*============================================================================

  Copyright (c) 2013 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/
#include <stdio.h>
#include <dlfcn.h>
#include <math.h>
#include <stdlib.h>
#include "chromatix.h"
#include "eeprom.h"
#include "sensor_common.h"

#define WB_OFFSET 0x10
#define AF_OFFSET 0x20
#define LSC_OFFSET 0x51

#define AWB_REG_SIZE 8
#define LSC_REG_SIZE 504

#define RG_RATIO_TYPICAL_VALUE 	551
#define BG_RATIO_TYPICAL_VALUE	592

#define ABS(x)            (((x) < 0) ? -(x) : (x))

struct otp_struct {
	uint8_t production_year;
	uint8_t production_month;
	uint8_t production_day;
  	uint8_t module_integrator_id;
  	uint8_t lens_id;
	uint8_t vcm_id;
	uint8_t driver_ic_id;
	uint8_t ois_fw_ver;
  
  	uint16_t rg_ratio;
  	uint16_t bg_ratio;
	uint16_t g_ave;
	uint16_t ob;
  	uint16_t gold_rg_ratio;
  	uint16_t gold_bg_ratio;
	uint16_t gold_g_ave;
	
  	uint16_t af_start_cur;
  	uint16_t af_infinite_cur;
  	uint16_t af_macro_cur;
} otp_data;

struct msm_camera_i2c_reg_array g_reg_array[AWB_REG_SIZE+LSC_REG_SIZE];
struct msm_camera_i2c_reg_setting g_reg_setting;

/** liteon_imx135_get_calibration_items:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Get calibration capabilities and mode items.
 *
 * This function executes in eeprom module context
 *
 * No return value.
 **/
void liteon_imx135_get_calibration_items(void *e_ctrl)
{
  sensor_eeprom_data_t *ectrl = (sensor_eeprom_data_t *)e_ctrl;
  eeprom_calib_items_t *e_items = &(ectrl->eeprom_data.items);
  e_items->is_insensor = TRUE;
  e_items->is_afc = FALSE; //TRUE;
  e_items->is_wbc = TRUE;
  e_items->is_lsc = FALSE; //TRUE;
  e_items->is_dpc = FALSE;
}


/** liteon_imx135_read_wbdata:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Read the data structure of white balance calibration
 *
 * This function executes in eeprom module context
 *
 * Return: int to indicate read white balance success or not.
 **/
static int liteon_imx135_read_wbdata(sensor_eeprom_data_t *e_ctrl)
{
	uint8_t hi_byte, low_byte;
//Gionee <zhuangxiaojian> <2014-08-12> modify for CR01350579 begin
#ifdef ORIGINAL_VERSION
#else
	if (!e_ctrl) {
		SERR("Invalid argument");
		return -EINVAL;
	}
#endif
//Gionee <zhuangxiaojian> <2014-08-12> modify for CR01350579 end
	otp_data.production_year 	= (uint8_t)(e_ctrl->eeprom_params.buffer[0]);
	otp_data.production_month = (uint8_t)(e_ctrl->eeprom_params.buffer[1]);
	otp_data.production_day 	= (uint8_t)(e_ctrl->eeprom_params.buffer[2]);
	otp_data.module_integrator_id = (uint8_t)(e_ctrl->eeprom_params.buffer[3]);
	otp_data.lens_id 			= (uint8_t)(e_ctrl->eeprom_params.buffer[4]);
	otp_data.vcm_id 			= (uint8_t)(e_ctrl->eeprom_params.buffer[5]);
	otp_data.driver_ic_id 	= (uint8_t)(e_ctrl->eeprom_params.buffer[6]);
	otp_data.ois_fw_ver		= (uint8_t)(e_ctrl->eeprom_params.buffer[7]);

	hi_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[WB_OFFSET+0]);
	low_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[WB_OFFSET+1]);
	otp_data.rg_ratio = (hi_byte << 8) + low_byte;
	hi_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[WB_OFFSET+2]);
	low_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[WB_OFFSET+3]);
	otp_data.bg_ratio = (hi_byte << 8) + low_byte;
	hi_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[WB_OFFSET+4]);
	low_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[WB_OFFSET+5]);
	otp_data.g_ave = (hi_byte << 8) + low_byte;
	otp_data.ob = (uint8_t)(e_ctrl->eeprom_params.buffer[WB_OFFSET+6]);
	hi_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[WB_OFFSET+7]);
	low_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[WB_OFFSET+8]);
	otp_data.gold_rg_ratio = (hi_byte << 8) + low_byte;
	hi_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[WB_OFFSET+9]);
	low_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[WB_OFFSET+10]);
	otp_data.gold_bg_ratio = (hi_byte << 8) + low_byte;
	hi_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[WB_OFFSET+11]);
	low_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[WB_OFFSET+12]);
	otp_data.gold_g_ave = (hi_byte << 8) + low_byte;

	//erroneous  OTP value
	if (otp_data.rg_ratio > otp_data.bg_ratio) {
		uint16_t temp;

		temp = otp_data.rg_ratio;
		otp_data.rg_ratio = otp_data.bg_ratio;
		otp_data.bg_ratio = temp;
	}

	SLOW("module_integrator_id %d, ois_fw_ver %d, rg_ratio %d, bg_ratio %d, g_ave %d \
		ob %d, gold_rg_ratio %d, gold_bg_ratio %d, gold_g_ave %d",
		otp_data.module_integrator_id,  	
		otp_data.ois_fw_ver,  	
		otp_data.rg_ratio,
		otp_data.bg_ratio,
		otp_data.g_ave,
		otp_data.ob,
		otp_data.gold_rg_ratio,
		otp_data.gold_bg_ratio,
		otp_data.gold_g_ave
  );

  return 0;
}

/** liteon_imx135_calc_otp:
 *
 * Calculate R/B target value for otp white balance data format
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 */
static void liteon_imx135_calc_otp(uint16_t r_ratio, uint16_t b_ratio,
  uint16_t *r_target, uint16_t *b_target, uint16_t r_offset, uint16_t b_offset)
{
  if ((b_offset * ABS(RG_RATIO_TYPICAL_VALUE - r_ratio))
    < (r_offset * ABS(BG_RATIO_TYPICAL_VALUE - b_ratio))) {
    if (b_ratio < BG_RATIO_TYPICAL_VALUE)
      *b_target = BG_RATIO_TYPICAL_VALUE - b_offset;
    else
      *b_target = BG_RATIO_TYPICAL_VALUE + b_offset;

    if (r_ratio < RG_RATIO_TYPICAL_VALUE) {
      *r_target = RG_RATIO_TYPICAL_VALUE
        - ABS(BG_RATIO_TYPICAL_VALUE - *b_target)
        * ABS(RG_RATIO_TYPICAL_VALUE - r_ratio)
        / ABS(BG_RATIO_TYPICAL_VALUE - b_ratio);
    } else {
      *r_target = RG_RATIO_TYPICAL_VALUE
        + ABS(BG_RATIO_TYPICAL_VALUE - *b_target)
        * ABS(RG_RATIO_TYPICAL_VALUE - r_ratio)
        / ABS(BG_RATIO_TYPICAL_VALUE - b_ratio);
    }
  } else {
    if (r_ratio < RG_RATIO_TYPICAL_VALUE)
      *r_target = RG_RATIO_TYPICAL_VALUE - r_offset;
    else
      *r_target = RG_RATIO_TYPICAL_VALUE + r_offset;

    if (b_ratio < BG_RATIO_TYPICAL_VALUE) {
      *b_target = BG_RATIO_TYPICAL_VALUE
        - ABS(RG_RATIO_TYPICAL_VALUE - *r_target)
        * ABS(BG_RATIO_TYPICAL_VALUE - b_ratio)
        / ABS(RG_RATIO_TYPICAL_VALUE - r_ratio);
    } else {
      *b_target = BG_RATIO_TYPICAL_VALUE
        + ABS(RG_RATIO_TYPICAL_VALUE - *r_target)
        * ABS(BG_RATIO_TYPICAL_VALUE - b_ratio)
        / ABS(RG_RATIO_TYPICAL_VALUE - r_ratio);
    }
  }
}

/** liteon_imx135_calc_gain:
 *
 * Calculate white balance gain
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 **/
static void liteon_imx135_calc_gain(uint16_t R_target,
  uint16_t B_target, uint16_t *R_gain, uint16_t *B_gain, uint16_t *G_gain)
{
  /* 0x100 = 1x gain */
  if (otp_data.bg_ratio < B_target) {
    if (otp_data.rg_ratio < R_target) {
      *G_gain = 0x100;
      *B_gain = 0x100 *
        B_target /
        otp_data.bg_ratio;
      *R_gain = 0x100 *
        R_target /
        otp_data.rg_ratio;
    } else {
      *R_gain = 0x100;
      *G_gain = 0x100 *
        otp_data.rg_ratio /
        R_target;
      *B_gain = 0x100 * otp_data.rg_ratio * B_target
        / (otp_data.bg_ratio * R_target);
    }
  } else {
    if (otp_data.rg_ratio < R_target) {
      *B_gain = 0x100;
      *G_gain = 0x100 *
        otp_data.bg_ratio /
        B_target;
      *R_gain = 0x100 * otp_data.bg_ratio * R_target
        / (otp_data.rg_ratio * B_target);
    } else {
      if (B_target * otp_data.rg_ratio < R_target * otp_data.bg_ratio) {
        *B_gain = 0x100;
        *G_gain = 0x100 *
          otp_data.bg_ratio /
          B_target;
        *R_gain = 0x100 * otp_data.bg_ratio * R_target
        / (otp_data.rg_ratio * B_target);
      } else {
        *R_gain = 0x100;
        *G_gain = 0x100 *
          otp_data.rg_ratio /
          R_target;
        *B_gain = 0x100 * otp_data.rg_ratio * B_target
        / (otp_data.bg_ratio * R_target);
      }
    }
  }
}


/** liteon_imx135_update_awb:
 *
 * Calculate and apply white balance calibration data
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 **/
static void liteon_imx135_update_awb()
{
	uint16_t R_gain, G_gain, B_gain;
	uint16_t R_offset_outside, B_offset_outside;
	uint16_t R_offset_inside, B_offset_inside;
	uint16_t R_target, B_target;

	R_offset_inside = RG_RATIO_TYPICAL_VALUE / 100;
	R_offset_outside = RG_RATIO_TYPICAL_VALUE * 3 / 100;
	B_offset_inside = BG_RATIO_TYPICAL_VALUE / 100;
	B_offset_outside = BG_RATIO_TYPICAL_VALUE * 3 / 100;

	if ((ABS(otp_data.rg_ratio - RG_RATIO_TYPICAL_VALUE)
		< R_offset_inside)
		&& (ABS(otp_data.bg_ratio - BG_RATIO_TYPICAL_VALUE)
		< B_offset_inside)) {
		R_gain = 0x100;
		G_gain = 0x100;
		B_gain = 0x100;
		SLOW("both rg and bg offset be within 1%%");
	} else {
		if ((ABS(otp_data.rg_ratio - RG_RATIO_TYPICAL_VALUE)
			  < R_offset_outside)
			  && (ABS(otp_data.bg_ratio - BG_RATIO_TYPICAL_VALUE)
			  < B_offset_outside)) {
			  liteon_imx135_calc_otp(otp_data.rg_ratio, otp_data.bg_ratio
			    , &R_target, &B_target,
			    R_offset_inside, B_offset_inside);
			  SLOW("both rg and bg offset be within 1%% to 3%%");
		} else {
			  liteon_imx135_calc_otp(otp_data.rg_ratio, otp_data.bg_ratio
			    , &R_target, &B_target,
			    R_offset_outside, B_offset_outside);
		}
		liteon_imx135_calc_gain(R_target,B_target,&R_gain,&B_gain,&G_gain);
	}

	if (R_gain > 0x100) {
		g_reg_array[g_reg_setting.size].reg_addr = 0x0210;
		g_reg_array[g_reg_setting.size].reg_data = R_gain >> 8;
		g_reg_setting.size++;
		g_reg_array[g_reg_setting.size].reg_addr = 0x0211;
		g_reg_array[g_reg_setting.size].reg_data = R_gain & 0xff;
		g_reg_setting.size++;
	}
	if (G_gain > 0x100) {
		g_reg_array[g_reg_setting.size].reg_addr = 0x020E;
		g_reg_array[g_reg_setting.size].reg_data = G_gain >> 8;
		g_reg_setting.size++;
		g_reg_array[g_reg_setting.size].reg_addr = 0x020F;
		g_reg_array[g_reg_setting.size].reg_data = G_gain & 0xff;
		g_reg_setting.size++;
		g_reg_array[g_reg_setting.size].reg_addr = 0x0214;
		g_reg_array[g_reg_setting.size].reg_data = G_gain >> 8;
		g_reg_setting.size++;
		g_reg_array[g_reg_setting.size].reg_addr = 0x0215;
		g_reg_array[g_reg_setting.size].reg_data = G_gain & 0xff;
		g_reg_setting.size++;
	}
	if (B_gain > 0x100) {
		g_reg_array[g_reg_setting.size].reg_addr = 0x0212;
		g_reg_array[g_reg_setting.size].reg_data = B_gain >> 8;
		g_reg_setting.size++;
		g_reg_array[g_reg_setting.size].reg_addr = 0x0213;
		g_reg_array[g_reg_setting.size].reg_data = B_gain & 0xff;
		g_reg_setting.size++;
	}

	SLOW("register gain R_gain 0x%x, G_gain 0x%x, B_gain 0x%x", R_gain, G_gain, B_gain);
	
	/* zero array size will cause camera crash, tanrifei, 20140102*/
	if (g_reg_setting.size ==0) {
		g_reg_array[g_reg_setting.size].reg_addr = 0x0210;
		g_reg_array[g_reg_setting.size].reg_data = 0x01;
		g_reg_setting.size++;
		g_reg_array[g_reg_setting.size].reg_addr = 0x0211;
		g_reg_array[g_reg_setting.size].reg_data = 0x00;
		g_reg_setting.size++;
	}
	/* add end */
}

/** liteon_imx135_format_wbdata:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format the data structure of white balance calibration
 *
 * This function executes in eeprom module context
 *
 * No return value.
 **/
static void liteon_imx135_format_wbdata(sensor_eeprom_data_t *e_ctrl)
{
  SLOW("Enter");
  int rc = 0;

  rc = liteon_imx135_read_wbdata(e_ctrl);
  if(rc < 0)
	SERR("read wbdata failed");
  liteon_imx135_update_awb();

  SLOW("Exit");
}

static int liteon_imx135_read_afdata(sensor_eeprom_data_t *e_ctrl)
{
	uint8_t hi_byte, low_byte;

	hi_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[AF_OFFSET+0]);
	low_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[AF_OFFSET+1]);
	otp_data.af_start_cur = (hi_byte << 8) + low_byte;
	hi_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[AF_OFFSET+2]);
	low_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[AF_OFFSET+3]);
	otp_data.af_infinite_cur = (hi_byte << 8) + low_byte;
	hi_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[AF_OFFSET+4]);
	low_byte = (uint8_t)(e_ctrl->eeprom_params.buffer[AF_OFFSET+5]);
	otp_data.af_macro_cur = (hi_byte << 8) + low_byte;
	
	SLOW("af_start_cur %d, af_infinite_cur %d, af_macro_cur %d",
		otp_data.af_start_cur,		
		otp_data.af_infinite_cur,	
		otp_data.af_macro_cur
  );

  return 0;
}


static void liteon_imx135_format_afdata(sensor_eeprom_data_t *e_ctrl)
{
  SLOW("Enter");
  int rc = 0;

  rc = liteon_imx135_read_afdata(e_ctrl);
  if(rc < 0) {
	SERR("read wbdata failed");
  } else {
	  e_ctrl->eeprom_data.afc.starting_dac = otp_data.af_start_cur;
	  e_ctrl->eeprom_data.afc.infinity_dac = otp_data.af_infinite_cur;   
	  e_ctrl->eeprom_data.afc.macro_dac = otp_data.af_macro_cur;
  }

  SLOW("Exit");
}


/** liteon_imx135_format_lensshading:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format the data structure of lens shading correction calibration
 *
 * This function executes in eeprom module context
 *
 * No return value.
 **/
void liteon_imx135_format_lensshading(sensor_eeprom_data_t *e_ctrl)
{
  int i = 0, j = 0;

  for (i=0; i<7; i++) {
	for (j=0; j<9; j++) {
		float gain = 0;

		gain = e_ctrl->eeprom_params.buffer[LSC_OFFSET+i*9*4+j*4] + 
			((float)e_ctrl->eeprom_params.buffer[LSC_OFFSET+i*9*4+j*4+1])/256;
		SLOW("Block No. %d, R gain %f", i*9+j, gain);
	}
  }

  for (i = 0; i < LSC_REG_SIZE; i++) {
    g_reg_array[g_reg_setting.size].reg_addr = 0x4800 + i;
    g_reg_array[g_reg_setting.size].reg_data =
      (uint8_t)(e_ctrl->eeprom_params.buffer[LSC_OFFSET + i]);
    g_reg_setting.size++;
  }
  SLOW("Exit");
}

/** liteon_imx135_format_calibration_data:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format all the data structure of calibration
 *
 * This function executes in eeprom module context and generate
 *   all the calibration registers setting of the sensor.
 *
 * No return value.
 **/
void liteon_imx135_format_calibration_data(void *e_ctrl) {
  SLOW("Enter");
  sensor_eeprom_data_t *ectrl = (sensor_eeprom_data_t *)e_ctrl;
  uint8_t *data = ectrl->eeprom_params.buffer;

  g_reg_setting.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
  g_reg_setting.data_type = MSM_CAMERA_I2C_BYTE_DATA;
  g_reg_setting.reg_setting = &g_reg_array[0];
  g_reg_setting.size = 0;
  g_reg_setting.delay = 0;
  liteon_imx135_format_wbdata(ectrl);
  liteon_imx135_format_afdata(ectrl);
  //liteon_imx135_format_lensshading(ectrl);

  SLOW("Exit");
}

/** liteon_imx135_get_raw_data:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *    @data: point to the destination msm_camera_i2c_reg_setting
 *
 * Get the all the calibration registers setting of the sensor
 *
 * This function executes in eeprom module context.
 *
 * No return value.
 **/
int32_t liteon_imx135_get_raw_data(void *e_ctrl, void *data) {
  if (e_ctrl && data)
    memcpy(data, &g_reg_setting, sizeof(g_reg_setting));
  else
    SERR("failed Null pointer");
  return 0;
}

void liteon_imx135_do_af_calibration(void *e_ctrl)
{
	SLOW("Enter");
	sensor_eeprom_data_t *ectrl = (sensor_eeprom_data_t *)e_ctrl;
	actuator_tuned_params_t *aftune =
	  &(ectrl->eeprom_afchroma.af_tune_ptr->actuator_tuned_params);

	if (!((ectrl->eeprom_data.afc.infinity_dac >= ectrl->eeprom_data.afc.starting_dac) &&
		(ectrl->eeprom_data.afc.macro_dac > ectrl->eeprom_data.afc.infinity_dac))) {
		SLOW("error AF OTP data");
		return;
	}

	aftune->region_params[0].code_per_step = ectrl->eeprom_data.afc.starting_dac /
	  (aftune->region_params[0].step_bound[0] -
	  aftune->region_params[0].step_bound[1]);

	#if 0
	aftune->region_params[1].code_per_step =
	  (ectrl->eeprom_data.afc.macro_dac + 100 - ectrl->eeprom_data.afc.starting_dac +
	  (aftune->region_params[1].step_bound[0] - aftune->region_params[1].step_bound[1] - 1)) /
	  (aftune->region_params[1].step_bound[0] -
	  aftune->region_params[1].step_bound[1]);
	#endif

	SLOW("region0, %d %d, code_per_step %d, region1 %d %d, code_per_step %d",
		aftune->region_params[0].step_bound[0],
	    aftune->region_params[0].step_bound[1],
	    aftune->region_params[0].code_per_step,
	    aftune->region_params[1].step_bound[0],
	    aftune->region_params[1].step_bound[1],
	    aftune->region_params[1].code_per_step);
	
	SLOW("Exit");
}

static eeprom_lib_func_t liteon_imx135_lib_func_ptr = {
  .get_calibration_items = liteon_imx135_get_calibration_items,
  .format_calibration_data = liteon_imx135_format_calibration_data,
  .do_af_calibration = liteon_imx135_do_af_calibration,
  .do_wbc_calibration = NULL,
  .do_lsc_calibration = NULL,
  .do_dpc_calibration = NULL,
  .get_dpc_calibration_info = NULL,
  .get_raw_data = liteon_imx135_get_raw_data,
};

/** liteon_imx135_eeprom_open_lib:
 *
 * Get the funtion pointer of this lib.
 *
 * This function executes in eeprom module context.
 *
 * return eeprom_lib_func_t point to the function pointer.
 **/
void* liteon_imx135_eeprom_open_lib(void) {
  return &liteon_imx135_lib_func_ptr;
}
