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

#define AWB_REG_SIZE 6

#define RG_TYPICAL 0x182 //0x178
#define BG_TYPICAL 0x177 //0x173
#define ABS(x)            (((x) < 0) ? -(x) : (x))

struct otp_struct {
  uint16_t module_integrator_id;
  uint16_t lens_id;
  uint16_t rg_ratio;
  uint16_t bg_ratio;
  uint16_t user_data[2];
  uint16_t light_rg;
  uint16_t light_bg;
} otp_data;

struct msm_camera_i2c_reg_array g_reg_array[AWB_REG_SIZE];
struct msm_camera_i2c_reg_setting g_reg_setting;

/** sunny_p5v23c_get_calibration_items:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Get calibration capabilities and mode items.
 *
 * This function executes in eeprom module context
 *
 * Return: void
 **/
void sunny_p5v23c_get_calibration_items(void *e_ctrl)
{
  sensor_eeprom_data_t *ectrl = (sensor_eeprom_data_t *)e_ctrl;
  eeprom_calib_items_t *e_items = &(ectrl->eeprom_data.items);
  e_items->is_insensor = TRUE;
  e_items->is_afc = FALSE;
  e_items->is_wbc = TRUE;
  e_items->is_lsc = FALSE;
  e_items->is_dpc = FALSE;
}


/** sunny_p5v23c_calc_otp:
 *
 * Calculate R/B target value for otp white balance data format
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 */
static void sunny_p5v23c_calc_otp(uint16_t r_ratio, uint16_t b_ratio,
  uint16_t *r_target, uint16_t *b_target, uint16_t r_offset, uint16_t b_offset)
{
  if ((b_offset * ABS(RG_TYPICAL - r_ratio))
    < (r_offset * ABS(BG_TYPICAL - b_ratio))) {
    if (b_ratio < BG_TYPICAL)
      *b_target = BG_TYPICAL - b_offset;
    else
      *b_target = BG_TYPICAL + b_offset;

    if (r_ratio < RG_TYPICAL) {
      *r_target = RG_TYPICAL
        - ABS(BG_TYPICAL - *b_target)
        * ABS(RG_TYPICAL - r_ratio)
        / ABS(BG_TYPICAL - b_ratio);
    } else {
      *r_target = RG_TYPICAL
        + ABS(BG_TYPICAL - *b_target)
        * ABS(RG_TYPICAL - r_ratio)
        / ABS(BG_TYPICAL - b_ratio);
    }
  } else {
    if (r_ratio < RG_TYPICAL)
      *r_target = RG_TYPICAL - r_offset;
    else
      *r_target = RG_TYPICAL + r_offset;

    if (b_ratio < BG_TYPICAL) {
      *b_target = BG_TYPICAL
        - ABS(RG_TYPICAL - *r_target)
        * ABS(BG_TYPICAL - b_ratio)
        / ABS(RG_TYPICAL - r_ratio);
    } else {
      *b_target = BG_TYPICAL
        + ABS(RG_TYPICAL - *r_target)
        * ABS(BG_TYPICAL - b_ratio)
        / ABS(RG_TYPICAL - r_ratio);
    }
  }
}


/** sunny_p5v23c_calc_gain:
 *
 * Calculate white balance gain
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 **/
static void sunny_p5v23c_calc_gain(uint16_t R_target,
  uint16_t B_target, uint16_t *R_gain, uint16_t *B_gain, uint16_t *G_gain)
{
  /* 0x400 = 1x gain */
  if (otp_data.bg_ratio < B_target) {
    if (otp_data.rg_ratio < R_target) {
      *G_gain = 0x400;
      *B_gain = 0x400 *
        B_target /
        otp_data.bg_ratio;
      *R_gain = 0x400 *
        R_target /
        otp_data.rg_ratio;
    } else {
      *R_gain = 0x400;
      *G_gain = 0x400 *
        otp_data.rg_ratio /
        R_target;
      *B_gain = 0x400 * otp_data.rg_ratio * B_target
        / (otp_data.bg_ratio * R_target);
    }
  } else {
    if (otp_data.rg_ratio < R_target) {
      *B_gain = 0x400;
      *G_gain = 0x400 *
        otp_data.bg_ratio /
        B_target;
      *R_gain = 0x400 * otp_data.bg_ratio * R_target
        / (otp_data.rg_ratio * B_target);
    } else {
      if (B_target * otp_data.rg_ratio < R_target * otp_data.bg_ratio) {
        *B_gain = 0x400;
        *G_gain = 0x400 *
          otp_data.bg_ratio /
          B_target;
        *R_gain = 0x400 * otp_data.bg_ratio * R_target
        / (otp_data.rg_ratio * B_target);
      } else {
        *R_gain = 0x400;
        *G_gain = 0x400 *
          otp_data.rg_ratio /
          R_target;
        *B_gain = 0x400 * otp_data.rg_ratio * B_target
        / (otp_data.bg_ratio * R_target);
      }
    }
  }
}


/** sunny_p5v23c_format_wbdata:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format the data structure of white balance calibration
 *
 * This function executes in eeprom module context
 *
 * Return: void
 **/
static void sunny_p5v23c_format_wbdata(sensor_eeprom_data_t *e_ctrl)
{
  int grp_off[3] = { 0x05, 0x0E, 0x17 };
  int i = 0;
  uint8_t mid, flag, wb_mix;
  uint16_t rg, bg;
  //int r_gain, g_gain, b_gain, g_gain_b, g_gain_r;

  uint16_t R_gain, G_gain, B_gain;
  uint16_t R_offset_outside, B_offset_outside;
  uint16_t R_offset_inside, B_offset_inside;
  uint16_t R_target, B_target;

  for (i = 0; i < 3; i++) {
    mid = e_ctrl->eeprom_params.buffer[grp_off[i]];
    rg = e_ctrl->eeprom_params.buffer[grp_off[i] + 2];
    bg = e_ctrl->eeprom_params.buffer[grp_off[i] + 3];
    wb_mix = e_ctrl->eeprom_params.buffer[grp_off[i] + 6];

    rg = (rg << 2) | (wb_mix >> 6);
    bg = (bg << 2) | ((wb_mix & 0x30) >> 4);
    flag = mid & 0x80;

    if (!flag && rg && bg) {
      /* current group has valid calibration data */
      otp_data.module_integrator_id = mid & 0x7f;
      otp_data.lens_id = e_ctrl->eeprom_params.buffer[grp_off[i] + 1];
      otp_data.rg_ratio = rg;
      otp_data.bg_ratio = bg;
      otp_data.user_data[0] = e_ctrl->eeprom_params.buffer[grp_off[i] + 4];
      otp_data.user_data[1] = e_ctrl->eeprom_params.buffer[grp_off[i] + 5];
      otp_data.light_rg = e_ctrl->eeprom_params.buffer[grp_off[i] + 7];
      otp_data.light_rg = (otp_data.light_rg << 2) | ((wb_mix & 0x0c) >> 2);
      otp_data.light_bg = e_ctrl->eeprom_params.buffer[grp_off[i] + 8];
      otp_data.light_bg = (otp_data.light_bg << 2) | (wb_mix & 0x03);
      break;
    }
  }

  if (3 == i) {
    SERR("No WB calibration data valid\n");
    goto exit;
  }

  SLOW("module data : module ID 0x%x, rg 0x%x, bg 0x%x, light_rg 0x%x, light_bg 0x%x",
  	otp_data.module_integrator_id, otp_data.rg_ratio, otp_data.bg_ratio, otp_data.light_rg, otp_data.light_bg);

  if (otp_data.light_rg)
    otp_data.rg_ratio = otp_data.rg_ratio * (otp_data.light_rg + 512) / 1024;

  if (otp_data.light_bg)
    otp_data.bg_ratio = otp_data.bg_ratio * (otp_data.light_bg + 512) / 1024;


  #if 0
  if (otp_data.bg_ratio < BG_TYPICAL) {
    if (otp_data.rg_ratio < RG_TYPICAL) {
      g_gain = 0x400;
      b_gain = 0x400 * BG_TYPICAL / otp_data.bg_ratio;
      r_gain = 0x400 * RG_TYPICAL / otp_data.rg_ratio;
    } else {
      r_gain = 0x400;
      g_gain = 0x400 * otp_data.rg_ratio / RG_TYPICAL;
      b_gain = g_gain * BG_TYPICAL / otp_data.bg_ratio;
    }
  } else {
    if (otp_data.rg_ratio < RG_TYPICAL) {
      b_gain = 0x400;
      g_gain = 0x400 * otp_data.bg_ratio / BG_TYPICAL;
      r_gain = g_gain * RG_TYPICAL / otp_data.rg_ratio;
    } else {
      g_gain_b = 0x400 * otp_data.bg_ratio / BG_TYPICAL;
      g_gain_r = 0x400 * otp_data.rg_ratio / RG_TYPICAL;

      if (g_gain_b > g_gain_r) {
        b_gain = 0x400;
        g_gain = g_gain_b;
        r_gain = g_gain * RG_TYPICAL / otp_data.rg_ratio;
      } else {
        r_gain = 0x400;
        g_gain = g_gain_r;
        b_gain = g_gain * BG_TYPICAL / otp_data.bg_ratio;
      }
    }
  }
  #endif

  R_offset_inside = RG_TYPICAL / 100;
  R_offset_outside = RG_TYPICAL * 3 / 100;
  B_offset_inside = BG_TYPICAL / 100;
  B_offset_outside = BG_TYPICAL * 3 / 100;

  if ((ABS(otp_data.rg_ratio - RG_TYPICAL)
	  < R_offset_inside)
	  && (ABS(otp_data.bg_ratio - BG_TYPICAL)
	  < B_offset_inside)) {
	  R_gain = 0x400;
	  G_gain = 0x400;
	  B_gain = 0x400;
	  SLOW("both rg and bg offset be within 1%%");
  } else {
	  if ((ABS(otp_data.rg_ratio - RG_TYPICAL)
			< R_offset_outside)
			&& (ABS(otp_data.bg_ratio - BG_TYPICAL)
			< B_offset_outside)) {
			sunny_p5v23c_calc_otp(otp_data.rg_ratio, otp_data.bg_ratio
			  , &R_target, &B_target,
			  R_offset_inside, B_offset_inside);
			SLOW("both rg and bg offset be within 1%% to 3%%");
	  } else {
			sunny_p5v23c_calc_otp(otp_data.rg_ratio, otp_data.bg_ratio
			  , &R_target, &B_target,
			  R_offset_outside, B_offset_outside);
	  }
	  sunny_p5v23c_calc_gain(R_target,B_target,&R_gain,&B_gain,&G_gain);
  }

  SLOW("R G B gains for OTP : 0x%x, 0x%x, 0x%x", R_gain, G_gain, B_gain);

  if (R_gain > 0x400) {
    g_reg_array[g_reg_setting.size].reg_addr = 0x5186;
    g_reg_array[g_reg_setting.size].reg_data = R_gain >> 8;
    g_reg_setting.size++;
    g_reg_array[g_reg_setting.size].reg_addr = 0x5187;
    g_reg_array[g_reg_setting.size].reg_data = R_gain & 0x00ff;
    g_reg_setting.size++;
  }
  if (G_gain > 0x400) {
    g_reg_array[g_reg_setting.size].reg_addr = 0x5188;
    g_reg_array[g_reg_setting.size].reg_data = G_gain >> 8;
    g_reg_setting.size++;
    g_reg_array[g_reg_setting.size].reg_addr = 0x5189;
    g_reg_array[g_reg_setting.size].reg_data = G_gain & 0x00ff;
    g_reg_setting.size++;
  }
  if (B_gain > 0x400) {
    g_reg_array[g_reg_setting.size].reg_addr = 0x518a;
    g_reg_array[g_reg_setting.size].reg_data = B_gain >> 8;
    g_reg_setting.size++;
    g_reg_array[g_reg_setting.size].reg_addr = 0x518b;
    g_reg_array[g_reg_setting.size].reg_data = B_gain & 0x00ff;
    g_reg_setting.size++;
  }

exit:
  /* zero array size will cause camera crash, tanrifei, 20140102*/
  if (g_reg_setting.size ==0) {
	g_reg_array[g_reg_setting.size].reg_addr = 0x5186;
	g_reg_array[g_reg_setting.size].reg_data = 0x04;
	g_reg_setting.size++;
	g_reg_array[g_reg_setting.size].reg_addr = 0x5187;
	g_reg_array[g_reg_setting.size].reg_data = 0x00;
	g_reg_setting.size++;
  }
  /* add end */  
}

/** sunny_p5v23c_format_calibration_data:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format all the data structure of calibration
 *
 * This function executes in eeprom module context and generate
 *   all the calibration registers setting of the sensor.
 *
 * Return: void
 **/
void sunny_p5v23c_format_calibration_data(void *e_ctrl) {
  SLOW("Enter");
  sensor_eeprom_data_t *ectrl = (sensor_eeprom_data_t *)e_ctrl;
  uint8_t *data = ectrl->eeprom_params.buffer;

  g_reg_setting.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
  g_reg_setting.data_type = MSM_CAMERA_I2C_BYTE_DATA;
  g_reg_setting.reg_setting = &g_reg_array[0];
  g_reg_setting.size = 0;
  g_reg_setting.delay = 0;
  sunny_p5v23c_format_wbdata(ectrl);

  SLOW("Exit");
}

/** sunny_p5v23c_get_raw_data:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *    @data: point to the destination msm_camera_i2c_reg_setting
 *
 * Get the all the calibration registers setting of the sensor
 *
 * This function executes in eeprom module context.
 *
 * Return: void
 **/
void sunny_p5v23c_get_raw_data(void *e_ctrl, void *data) {
  if (e_ctrl && data)
    memcpy(data, &g_reg_setting, sizeof(g_reg_setting));
  else
    SERR("failed Null pointer");
  return;
}

static eeprom_lib_func_t sunny_p5v23c_lib_func_ptr = {
  .get_calibration_items = sunny_p5v23c_get_calibration_items,
  .format_calibration_data = sunny_p5v23c_format_calibration_data,
  .do_af_calibration = NULL,
  .do_wbc_calibration = NULL,
  .do_lsc_calibration = NULL,
  .do_dpc_calibration = NULL,
  .get_dpc_calibration_info = NULL,
  .get_raw_data = sunny_p5v23c_get_raw_data,
};

/** sunny_p5v23c_eeprom_open_lib:
 *
 * Get the funtion pointer of this lib.
 *
 * This function executes in eeprom module context.
 *
 * Return: function pointer of eeprom_lib_func_t.
 **/
void* sunny_p5v23c_eeprom_open_lib(void) {
  return &sunny_p5v23c_lib_func_ptr;
}
