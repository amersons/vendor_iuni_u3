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

#define RG_TYPICAL_VALUE 0xFE  // 0xF5---Gionee modified RG_TYPICAl from EC02
#define BG_TYPICAL_VALUE 0x11D //0x114---Gionee modified BG_TYPICAL from EC02

#define SUNNY_P13V04A_EEPROM_AWB_ENABLE 1
#define SUNNY_P13V04A_EEPROM_LSC_ENABLE 1

#define ABS(x)  (((x) < 0) ? -(x) : (x))

#define min(x,y) ({\
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	(void) (&_x == &_y);\
	_x < _y ? _x : _y; })


#define OTP_DRV_START_ADDR          0x7010
#define OTP_DRV_INFO_GROUP_COUNT    3
#define OTP_DRV_INFO_SIZE           5
#define OTP_DRV_AWB_GROUP_COUNT     3
#define OTP_DRV_AWB_SIZE            4
#define OTP_DRV_VCM_GROUP_COUNT     3
#define OTP_DRV_VCM_SIZE            3
#define OTP_DRV_LSC_GROUP_COUNT     3
#define OTP_DRV_LSC_SIZE            361 //360
#define OTP_DRV_LSC_REG_ADDR   0x5400

#define DRV_INFO_FLAG_OFFSET 0
#define AWB_FLAG_OFFSET (DRV_INFO_FLAG_OFFSET \
                        + OTP_DRV_INFO_GROUP_COUNT * OTP_DRV_INFO_SIZE \
                        + 1)
#define LSC_FLAG_OFFSET (AWB_FLAG_OFFSET \
                        + OTP_DRV_AWB_GROUP_COUNT * OTP_DRV_AWB_SIZE \
                        + 1)
#define VCM_FLAG_OFFSET (LSC_FLAG_OFFSET \
                        + OTP_DRV_LSC_GROUP_COUNT * OTP_DRV_LSC_SIZE \
                        + 1)

#define LSC_ENABLE_REG_DEFAULT_VALUE 0xd8
#define LSC_ENABLE_REG_ADDR 0x5000

#define AWB_REG_SIZE 5
#define LSC_ENABLE_REG_SIZE 1
#define LSC_REG_SIZE 400
#define OTP_REG_ARRAY_SIZE (AWB_REG_SIZE + LSC_ENABLE_REG_SIZE + LSC_REG_SIZE)

//yinqian add for af otp start
#define VCM_START_OFFSET  84
#define VCM_END_OFFSET  56
//yinqian add for af otp end

struct otp_struct {
  uint16_t module_integrator_id;
  uint16_t lens_id;
  uint16_t production_year;
  uint16_t production_month;
  uint16_t production_day;
  uint16_t rg_ratio;
  uint16_t bg_ratio;
  uint16_t gg_ratio;
  //uint16_t light_rg;
  //uint16_t light_bg;
  uint16_t lenc[OTP_DRV_LSC_SIZE];
  uint16_t VCM_start;
  uint16_t VCM_end;
  uint16_t VCM_dir;
} otp_data;

struct msm_camera_i2c_reg_array g_reg_array[OTP_REG_ARRAY_SIZE];
struct msm_camera_i2c_reg_setting g_reg_setting;

/** sunny_p13v04a_get_calibration_items:
 * @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Get calibration capabilities and mode items.
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 **/ 
void sunny_p13v04a_get_calibration_items(void *e_ctrl)
{
  sensor_eeprom_data_t *ectrl = (sensor_eeprom_data_t *)e_ctrl;
  eeprom_calib_items_t *e_items = &(ectrl->eeprom_data.items);
  e_items->is_insensor = TRUE;
  e_items->is_afc = TRUE; //FALSE;
  e_items->is_wbc = TRUE;
  e_items->is_lsc = TRUE;
  e_items->is_dpc = FALSE;
}

static void sunny_p13v04a_update_awb()
{
    int rg, bg, gr, gb, gg, R_gain, Gr_gain, B_gain, Gb_gain, Base_gain, temp, i;
    int r_offset,gr_offset,b_offset,gb_offset;

    //apply OTP WB Calibration
    //flag = e_ctrl->eeprom_params.buffer[AWB_FLAG_OFFSET];
    //if(flag & 0x40) 
    //{
        rg = otp_data.rg_ratio;
        bg = otp_data.bg_ratio;
        gr = otp_data.gg_ratio;
        gb = 0x200; // 1
    //calculate Gr/G, Gb/G
        gg = (gr + gb) / 2;
        gr = (0x200 * gr) / gg;
        gb = (0x200 * gb) / gg;
        R_gain = (RG_TYPICAL_VALUE*1000) / rg;
        B_gain = (BG_TYPICAL_VALUE*1000) / bg;
        Gr_gain = (0x200 * 1000) / gr;
        Gb_gain = (0x200 * 1000) / gb;
        Base_gain = min(min(R_gain, Gr_gain), min(B_gain, Gb_gain));
        R_gain = (0x100 * R_gain) / Base_gain;
        B_gain = (0x100 * B_gain) / Base_gain;
        Gr_gain = (0x100 * Gr_gain) / Base_gain;
        Gb_gain = (0x100 * Gb_gain) / Base_gain;

#if SUNNY_P13V04A_EEPROM_AWB_ENABLE
  SERR("AWB REG UPDATE START");
  //SERR("wanguocheng_R_gain:0x%0x", R_gain);
  //SERR("wanguocheng_Gr_gain:0x%0x",Gr_gain);
  //SERR("wanguocheng_B_gain:0x%0x", B_gain);
  //SERR("wanguocheng_Gb_gain:0x%0x",Gb_gain); 
  if (R_gain > 0x100) {
    g_reg_array[g_reg_setting.size].reg_addr = 0x5306;
    g_reg_array[g_reg_setting.size].reg_data = R_gain >> 8;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;
    g_reg_array[g_reg_setting.size].reg_addr = 0x5307;
    g_reg_array[g_reg_setting.size].reg_data = R_gain & 0xff;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;
	r_offset=(R_gain - 0x100) << 4;
    g_reg_array[g_reg_setting.size].reg_addr = 0x5314;
    g_reg_array[g_reg_setting.size].reg_data = (r_offset>>16) & 0x3f;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;
    g_reg_array[g_reg_setting.size].reg_addr = 0x5315;
    g_reg_array[g_reg_setting.size].reg_data = (r_offset>>8) & 0xff;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;
    g_reg_array[g_reg_setting.size].reg_addr = 0x5316;
    g_reg_array[g_reg_setting.size].reg_data = r_offset & 0xff;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;	
  }
  if (Gr_gain > 0x100) {
    g_reg_array[g_reg_setting.size].reg_addr = 0x5304;
    g_reg_array[g_reg_setting.size].reg_data = Gr_gain >> 8;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;
    g_reg_array[g_reg_setting.size].reg_addr = 0x5305;
    g_reg_array[g_reg_setting.size].reg_data = Gr_gain & 0xff;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;
	gr_offset = (Gr_gain - 0x100) << 4;
    g_reg_array[g_reg_setting.size].reg_addr = 0x5310;
    g_reg_array[g_reg_setting.size].reg_data = (gr_offset>>16) & 0x3f;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;	
    g_reg_array[g_reg_setting.size].reg_addr = 0x5311;
    g_reg_array[g_reg_setting.size].reg_data = (gr_offset>>8) & 0xff;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;
    g_reg_array[g_reg_setting.size].reg_addr = 0x5312;
    g_reg_array[g_reg_setting.size].reg_data = gr_offset & 0xff;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;	
  }
  if (B_gain > 0x100) {
    g_reg_array[g_reg_setting.size].reg_addr = 0x5300;
    g_reg_array[g_reg_setting.size].reg_data = B_gain >> 8;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;
    g_reg_array[g_reg_setting.size].reg_addr = 0x5301;
    g_reg_array[g_reg_setting.size].reg_data = B_gain & 0xff;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;
	
	b_offset = (B_gain - 0x100) << 4;
    g_reg_array[g_reg_setting.size].reg_addr = 0x5308;
    g_reg_array[g_reg_setting.size].reg_data = (b_offset>>16) & 0x3f;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;	
    g_reg_array[g_reg_setting.size].reg_addr = 0x5309;
    g_reg_array[g_reg_setting.size].reg_data = (b_offset>>8) & 0xff;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;
    g_reg_array[g_reg_setting.size].reg_addr = 0x530a;
    g_reg_array[g_reg_setting.size].reg_data = b_offset & 0xff;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;	
  }
  if (Gb_gain > 0x100) {
    g_reg_array[g_reg_setting.size].reg_addr = 0x5302;
    g_reg_array[g_reg_setting.size].reg_data = Gb_gain >> 8;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;
    g_reg_array[g_reg_setting.size].reg_addr = 0x5303;
    g_reg_array[g_reg_setting.size].reg_data = Gb_gain & 0xff;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;

	gb_offset = (Gb_gain - 0x100) << 4;
    g_reg_array[g_reg_setting.size].reg_addr = 0x530c;
    g_reg_array[g_reg_setting.size].reg_data = (gb_offset>>16) & 0x3f;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;	
    g_reg_array[g_reg_setting.size].reg_addr = 0x530d;
    g_reg_array[g_reg_setting.size].reg_data = (gb_offset>>8) & 0xff;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;
    g_reg_array[g_reg_setting.size].reg_addr = 0x530e;
    g_reg_array[g_reg_setting.size].reg_data = gb_offset & 0xff;
    SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
    g_reg_array[g_reg_setting.size].reg_data);
    g_reg_setting.size++;	
  }  
  SLOW("AWB REG UPDATE END");
#endif
}

/** sunny_p13v04a_read_wbdata:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Read the data structure of white balance calibration
 *
 * This function executes in eeprom module context
 *
 * Return: int to indicate read white balance success or not.
 **/
static int sunny_p13v04a_read_wbdata(sensor_eeprom_data_t *e_ctrl)
{
  uint8_t flag, temp;
  int index, offset;

  flag = e_ctrl->eeprom_params.buffer[AWB_FLAG_OFFSET];
  #if 0 //pangfei modify for otp
  if (((flag >> 6) && 0x03) == 1)
    index = 0;
  else if (((flag >> 4) && 0x03) == 1)
    index = 1;
  else if (((flag >> 2) && 0x03) == 1)
    index = 2;
  else
    index = -1;
  #else
  if (((flag >> 6) & 0x03) == 1)
    index = 0;
  else if (((flag >> 4) & 0x03) == 1)
    index = 1;
  else if (((flag >> 2) & 0x03) == 1)
    index = 2;
  else
    index = -1;
  #endif
  SERR("flag = 0x%x index=%d",flag,index);
  if (index < 0) {
  SERR("No WB calibration data valid\n");
    return -1;
  }

  offset = AWB_FLAG_OFFSET + index * 4 + 1;
  temp = e_ctrl->eeprom_params.buffer[offset + 3];
  SERR("wanguocheng_temp:%d", temp);
  otp_data.rg_ratio = (e_ctrl->eeprom_params.buffer[offset] << 2)+ ((temp >> 6) & 0x03);
  otp_data.bg_ratio = (e_ctrl->eeprom_params.buffer[offset + 1] << 2)+ ((temp >> 4) & 0x03);
  otp_data.gg_ratio = (e_ctrl->eeprom_params.buffer[offset + 2] << 2)+ ((temp >> 2) & 0x03);
  //otp_data.light_bg = (e_ctrl->eeprom_params.buffer[offset + 3] << 2)+ (temp & 0x03);

  SERR("wanguocheng_rg_ratio:%d", otp_data.rg_ratio);
  SERR("wanguocheng_bg_ratio:%d", otp_data.bg_ratio);
  SERR("wanguocheng_gg_ratio:%d", otp_data.gg_ratio);
  //SLOW("light_bg:0x%0x", otp_data.light_bg);
  return 0;
}

/** sunny_p13v04a_format_wbdata:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format the data structure of white balance calibration
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 **/
static void sunny_p13v04a_format_wbdata(sensor_eeprom_data_t *e_ctrl)
{
  int rc = 0;
  rc = sunny_p13v04a_read_wbdata(e_ctrl);
  if(rc < 0)
    SERR("read wbdata failed");
  sunny_p13v04a_update_awb();
}


/** sunny_p13v04a_format_lensshading:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format the data structure of lens shading correction calibration
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 **/
void sunny_p13v04a_format_lensshading(sensor_eeprom_data_t *e_ctrl)
{
  uint8_t flag, temp;
  int index, offset, i;
  flag = e_ctrl->eeprom_params.buffer[LSC_FLAG_OFFSET];
  SLOW("wanguocheng_LSC_FLAG = 0x%x",flag);
  if (((flag >> 6) && 0x03) == 1)
    index = 0;
  else if (((flag >> 4) && 0x03) == 1)
    index = 1;
  else if (((flag >> 2) && 0x03) == 1)
    index = 2;
  else
    index = -1;

  SLOW("index = 0x%x",index);
  if (index < 0) {
    SERR("No lens shading calibration data valid\n");
    return -1;
  }

  SLOW("LENC start");
  g_reg_array[g_reg_setting.size].reg_addr = LSC_ENABLE_REG_ADDR;
#if SUNNY_P13V04A_EEPROM_LSC_ENABLE
  g_reg_array[g_reg_setting.size].reg_data
    = LSC_ENABLE_REG_DEFAULT_VALUE |0x05;
#else
  g_reg_array[g_reg_setting.size].reg_data
    = LSC_ENABLE_REG_DEFAULT_VALUE & 0xFA;
#endif
  SLOW("0x%0x:0x%0x", g_reg_array[g_reg_setting.size].reg_addr,
  g_reg_array[g_reg_setting.size].reg_data);
  g_reg_setting.size++;
  offset = LSC_FLAG_OFFSET + index * OTP_DRV_LSC_SIZE + 1;
  for (i = 0; i < OTP_DRV_LSC_SIZE; i++)
  {
      g_reg_array[g_reg_setting.size].reg_addr = OTP_DRV_LSC_REG_ADDR + i;
      g_reg_array[g_reg_setting.size].reg_data =(uint8_t)(e_ctrl->eeprom_params.buffer[offset + i]);		
      //SERR("wanguocheng_lens_0x%0x:0x%0x", 
	//	   g_reg_array[g_reg_setting.size].reg_addr,g_reg_array[g_reg_setting.size].reg_data);
      g_reg_setting.size++;   
  }
  SLOW("LENC end");
  return 0;
}

//yinqian add for af otp start
static int sunny_p13v04a_read_VCMdata(sensor_eeprom_data_t *e_ctrl) 
{
	uint8_t flag, temp, temp1;
	int index, offset, i;
	
	flag = e_ctrl->eeprom_params.buffer[VCM_FLAG_OFFSET];

	if (((flag >> 6) && 0x03) == 1)
		index = 0;
	else if (((flag >> 4) && 0x03) == 1)
		index = 1;
	else if (((flag >> 2) && 0x03) == 1)
		index = 2;
	else
		index = -1;
	
	SERR("yinqian flag = 0x%x index=%d",flag,index);
	if (index < 0) {
		SERR("No VCM calibration data valid\n");
		return -1;
	}
	
	offset = VCM_FLAG_OFFSET + index * OTP_DRV_VCM_SIZE + 1;
	if(index != -1) {
		temp = e_ctrl->eeprom_params.buffer[offset + 2];
		otp_data.VCM_start = (e_ctrl->eeprom_params.buffer[offset]<<2) | ((temp>>6) & 0x03);
		otp_data.VCM_end = (e_ctrl->eeprom_params.buffer[offset + 1] << 2) | ((temp>>4) & 0x03);
		otp_data.VCM_dir = (temp>>2) & 0x03;	
	}
	else{
		otp_data.VCM_start = 0;
		otp_data.VCM_end = 0;
		otp_data.VCM_dir = 0;
	}

	SERR("yinqian VCM_start %d, VCM_end %d, VCM_dir %d",
		otp_data.VCM_start,		
		otp_data.VCM_end,	
		otp_data.VCM_dir);
	
	return 0;
}
static void sunny_p13v04a_format_VCMdata(sensor_eeprom_data_t *e_ctrl) 
{
  SLOW("Enter");
  int rc = 0;

  rc = sunny_p13v04a_read_VCMdata(e_ctrl);
  if(rc < 0) {
	SERR("read VCMdata failed");
  } else {
	  e_ctrl->eeprom_data.afc.infinity_dac = otp_data.VCM_start - VCM_START_OFFSET;
	  e_ctrl->eeprom_data.afc.macro_dac = otp_data.VCM_end + VCM_END_OFFSET;
  }

  SLOW("Exit");
}
//yinqian add for af otp end

/** sunny_p13v04a_format_calibration_data:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format all the data structure of calibration
 *
 * This function executes in eeprom module context and generate
 *   all the calibration registers setting of the sensor.
 *
 * Return: void.
 **/
void sunny_p13v04a_format_calibration_data(void *e_ctrl) {
  SLOW("Enter %s", __func__);
  sensor_eeprom_data_t *ectrl = (sensor_eeprom_data_t *)e_ctrl;
  uint8_t *data = ectrl->eeprom_params.buffer;

  g_reg_setting.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
  g_reg_setting.data_type = MSM_CAMERA_I2C_BYTE_DATA;
  g_reg_setting.reg_setting = &g_reg_array[0];
  g_reg_setting.size = 0;
  g_reg_setting.delay = 0;
  sunny_p13v04a_format_wbdata(ectrl);
  sunny_p13v04a_format_lensshading(ectrl);
  sunny_p13v04a_format_VCMdata(ectrl);  //yinqian add for af otp
  SLOW("Exit %s", __func__);
}

/** sunny_p13v04a_get_raw_data:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *    @data: point to the destination msm_camera_i2c_reg_setting
 *
 * Get the all the calibration registers setting of the sensor
 *
 * This function executes in eeprom module context.
 *
 * Return: void.
 **/
void sunny_p13v04a_get_raw_data(void *e_ctrl, void *data) {
//pangfei add for CR01090964 start 
  uint16_t reg_count = 0; 
  for(reg_count=0; reg_count< g_reg_setting.size; reg_count++) 
  { 
  	g_reg_setting.reg_setting[reg_count].delay=0; 
	//SERR("sunny_p13v04a_get_raw_data delay reg_couint=%d\n",reg_count);
  }
  //pangfei add for CR01090964 end 

  if (e_ctrl && data)
    memcpy(data, &g_reg_setting, sizeof(g_reg_setting));
  else
    SERR("failed Null pointer");
  return;
}

//yinqian add for af otp start
void sunny_p13v04a_do_af_calibration(void *e_ctrl)
{
	SLOW("Enter");
	sensor_eeprom_data_t *ectrl = (sensor_eeprom_data_t *)e_ctrl;
	actuator_tuned_params_t *aftune =
	  &(ectrl->eeprom_afchroma.af_tune_ptr->actuator_tuned_params);

	if (ectrl->eeprom_data.afc.macro_dac < ectrl->eeprom_data.afc.infinity_dac) {
		SERR("error AF OTP data");
		return;
	}

	aftune->region_params[0].code_per_step = 
	aftune->region_params[1].code_per_step =
		((ectrl->eeprom_data.afc.macro_dac - aftune->initial_code)/
		aftune->region_params[1].step_bound[0]);

	SERR("yinqian region0, %d %d, code_per_step %d, region1 %d %d, code_per_step %d",
	    aftune->region_params[0].step_bound[0],
	    aftune->region_params[0].step_bound[1],
	    aftune->region_params[0].code_per_step,
	    aftune->region_params[1].step_bound[0],
	    aftune->region_params[1].step_bound[1],
	    aftune->region_params[1].code_per_step);
	
	SLOW("Exit");
}
//yinqian add for af otp end

static eeprom_lib_func_t sunny_p13v04a_lib_func_ptr = {
  .get_calibration_items = sunny_p13v04a_get_calibration_items,
  .format_calibration_data = sunny_p13v04a_format_calibration_data,
  .do_af_calibration = sunny_p13v04a_do_af_calibration,   //yinqian add for af otp
  .do_wbc_calibration = NULL,
  .do_lsc_calibration = NULL,
  .do_dpc_calibration = NULL,
  .get_dpc_calibration_info = NULL,
  .get_raw_data = sunny_p13v04a_get_raw_data,
};

/** sunny_p13v04a_eeprom_open_lib:
 *
 * Get the funtion pointer of this lib.
 *
 * This function executes in eeprom module context.
 *
 * Return: eeprom_lib_func_t point to the function pointer.
 **/
void* sunny_p13v04a_eeprom_open_lib(void) {
  return &sunny_p13v04a_lib_func_ptr;
}
