/*============================================================================

  Copyright (c) 2012-2013 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/
/*
 *ov13860
*/
#include <stdio.h>
#include "sensor_lib.h"

#include <dlfcn.h>
#include <math.h>
#include <stdlib.h>
//#include "sensor_common.h"
//#include "chromatix.h"
//#include "eeprom.h"

#define SENSOR_MODEL_NO_ov13860 "ov13860"

#define ov13860_LOAD_CHROMATIX(n) \
  "libchromatix_"SENSOR_MODEL_NO_ov13860"_"#n".so"
#undef DEBUG_INFO
#define OV13860_DEBUG
#ifdef OV13860_DEBUG
#include <utils/Log.h>
#define SERR(fmt, args...) \
    ALOGE("%s:%d "fmt"\n", __func__, __LINE__, ##args)
#define DEBUG_INFO(fmt, args...) SERR(fmt, ##args)
#else
#define DEBUG_INFO(fmt, args...) do { } while (0)
#endif

static sensor_lib_t sensor_lib_ptr;

static struct msm_sensor_power_setting ov13860_power_setting[] = {

    {
	    .seq_type = SENSOR_VREG,
		.seq_val =  CAM_VDIG,   //SUB DVDD GPIO POWER
		.config_val = 0,
		.delay = 10,
	},
	#if 1
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 10,
	},
	#endif
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 10,
	},
	#if 1
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VAF,
		.config_val = 0,
		.delay = 5,
	},
	#endif
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_LOW,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_HIGH,
		.delay = 5,
	},	
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},	
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_AF_PWDM,
		.config_val = GPIO_OUT_LOW,
		.delay = 5,
	},	
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 10,
	},	
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_AF_PWDM,
		.config_val = GPIO_OUT_HIGH,
		.delay = 5,
	},	
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct msm_camera_sensor_slave_info sensor_slave_info = {
  /* Camera slot where this camera is mounted */
  .camera_id = CAMERA_0,
  /* sensor slave address */
  .slave_addr = 0x20,
  /* sensor address type */
  .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
  /* sensor id info*/
  .sensor_id_info = {
  /* sensor id register address */
    .sensor_id_reg_addr = 0x300a,
  /* sensor id */
    .sensor_id = 0x0138,
  },
  /* power up / down setting */
  .power_setting_array = {
    .power_setting = ov13860_power_setting,
    .size = ARRAY_SIZE(ov13860_power_setting),
  },
};

static struct msm_sensor_init_params sensor_init_params = {
  .modes_supported = CAMERA_MODE_2D_B,
  .position = BACK_CAMERA_B,
  .sensor_mount_angle = SENSOR_MOUNTANGLE_360,
};

static sensor_output_t sensor_output = {
  .output_format = SENSOR_BAYER,
  .connection_mode = SENSOR_MIPI_CSI,
  .raw_output = SENSOR_10_BIT_DIRECT,
};

static struct msm_sensor_output_reg_addr_t output_reg_addr = {
  .x_output = 0x3808,
  .y_output = 0x380a,
  .line_length_pclk = 0x380c,
  .frame_length_lines = 0x380e,
};

static struct msm_sensor_exp_gain_info_t exp_gain_info = {
  .coarse_int_time_addr = 0x3501,
  .global_gain_addr = 0x350a,
  .vert_offset = 6,
};

static sensor_aec_data_t aec_info = {
  .max_gain = 15.5,
  .max_linecount = 20976,
};

static sensor_lens_info_t default_lens_info = {
  .focal_length = 2.93,
  .pix_size = 1.3,
  .f_number = 2.2,
  .total_f_dist = 4.5,
  .hor_view_angle = 61.7,
  .ver_view_angle = 47.7,
};

#ifndef VFE_40
static struct csi_lane_params_t csi_lane_params = {
  .csi_lane_assign = 0xe4,
  .csi_lane_mask = 0x3,
  .csi_if = 1,
  .csid_core = {0},
  .csi_phy_sel = 0,
};
#else
static struct csi_lane_params_t csi_lane_params = {
  .csi_lane_assign = 0x4320,
  .csi_lane_mask = 0x1f, // TODO 
  .csi_if = 1,
  .csid_core = {0},
  .csi_phy_sel = 0,
};
#endif

static struct msm_camera_i2c_reg_array init_reg_array0[] = {
{0x0103, 0x01},
};

static struct msm_camera_i2c_reg_array init_reg_array1[] = {
{0x0302,0x32},
{0x0303,0x00},
{0x030e,0x02},
{0x030f,0x03},
{0x0312,0x03},
{0x0313,0x03},
{0x031e,0x01},
{0x3010,0x01},
{0x3012,0x41},
{0x340c,0xff},
{0x340d,0xff},
{0x3501,0x0d},
{0x3502,0x88},
{0x3503,0x00},
{0x3507,0x00},
{0x3508,0x00},
{0x3509,0x12},
{0x350a,0x00},
{0x350b,0xff},
{0x350f,0x10},
{0x3541,0x02},
{0x3542,0x00},
{0x3543,0x00},
{0x3547,0x00},
{0x3548,0x00},
{0x3549,0x12},
{0x354b,0x10},
{0x354f,0x10},
{0x3600,0x41},
{0x3601,0xd4}, //0xd2
{0x3603,0x97},
{0x3604,0x08},
{0x360a,0x35},
{0x360c,0xa0},
{0x360d,0x53},
{0x3618,0x0c},
{0x3620,0x55},
{0x3622,0x7c},
{0x3628,0xc0},
{0x3660,0xd0},
{0x3662,0x00},
{0x3664,0x04},
{0x366b,0x00},
{0x3701,0x20},
{0x3702,0x30},
{0x3703,0x3b},
{0x3704,0x26},
{0x3705,0x07},
{0x3706,0x3f},
{0x3709,0x18}, 
{0x370a,0x23}, 
{0x370e,0x32}, 
{0x3710,0x10},
{0x3712,0x12},
{0x3714,0x00}, // 0x00---wanguocheng modified 
{0x3719,0x03},
{0x3720,0x18},
{0x3726,0x22},
{0x3727,0x44},
{0x3728,0x40},
{0x3729,0x00},
{0x372e,0x2b},
{0x372f,0xa0},
{0x372b,0x00},
{0x3744,0x01},
{0x3745,0x5e},
{0x3746,0x01},
{0x3747,0x1f},
{0x374a,0x4b},
{0x3760,0xd1},
{0x3761,0x31},
{0x376d,0x01},
{0x376e,0x53},
{0x376c,0x43},
{0x378c,0x1f},
{0x378d,0x13}, 
{0x378f,0x88},
{0x3790,0x5a}, // 0x5a---wanguocheng modified 
{0x3791,0x5a},
{0x3792,0x21},
{0x3794,0x71},
{0x3796,0x01},
{0x379f,0x3e},
{0x37a0,0x44},
{0x37a1,0x00},
{0x37a2,0x44},
{0x37a3,0x41},
{0x37a4,0x88},
{0x37a5,0xa9},
{0x37b3,0xe0},
{0x37b4,0x0e},
{0x37b7,0x84},
{0x37b8,0x02},
{0x37b9,0x08},
{0x3842,0x00},
{0x3800,0x00},
{0x3801,0x14},
{0x3802,0x00},
{0x3803,0x0c},
{0x3804,0x10},
{0x3805,0x8b},
{0x3806,0x0c},
{0x3807,0x43},
{0x3808,0x10},
{0x3809,0x80},
{0x380a,0x0c},
{0x380b,0x30},
{0x380c,0x11},
{0x380d,0xd6},
{0x380e,0x0d},
{0x380f,0xa8},
{0x3810,0x00},
{0x3811,0x04},
{0x3813,0x02},
{0x3814,0x11},
{0x3815,0x11},
{0x3820,0x00},
{0x3821,0x04},
{0x382a,0x04},
{0x3835,0x04},
{0x3836,0x0c},
{0x3837,0x02},
{0x382f,0x84},
{0x383c,0x88},
{0x383d,0xff},
{0x3845,0x10},
{0x3d85,0x16},
{0x3d8c,0x79},
{0x3d8d,0x7f},
{0x4000,0x17},
{0x4008,0x00},
{0x4009,0x13},
{0x4010,0xf0}, // wanguocheng added from celery's suggestion
{0x4011,0xfb}, // wanguocheng added from celery's suggestion
{0x400f,0x80},
{0x4017,0x08},
{0x401a,0x0e},
{0x4019,0x18},
{0x4051,0x03},
{0x4052,0x00},
{0x4053,0x80},
{0x4054,0x00},
{0x4055,0x80},
{0x4056,0x00},
{0x4057,0x80},
{0x4058,0x00},
{0x4059,0x80},
{0x4066,0x04},
{0x4202,0x00},
{0x4203,0x01},
{0x4b03,0x80},
{0x4d00,0x05},
{0x4d01,0x03},
{0x4d02,0xca},
{0x4d03,0xd7},
{0x4d04,0xae},
{0x4d05,0x13},
{0x4813,0x10},
{0x4815,0x40},
{0x4837,0x0d},
{0x486e,0x03},
{0x4b06,0x00},
{0x4c01,0xdf},
{0x5000,0x9d},// 0x99 ---wanguocheng modified
{0x5001,0x40},
{0x5002,0x04},
{0x5003,0x00},
{0x5004,0x00},
{0x5005,0x00},
{0x501d,0x00},
{0x501f,0x06},
{0x5021,0x00},
{0x5022,0x13},
{0x5058,0x00},
{0x5200,0x00},
{0x5201,0x80},
{0x5204,0x01},
{0x5205,0x00},
{0x5209,0x00},
{0x520a,0x80},
{0x520b,0x04},
{0x520c,0x01},
{0x5210,0x10},
{0x5211,0xa0},
{0x5280,0x00},
{0x5292,0x00},
{0x5c80,0x06},
{0x5c81,0x80},
{0x5c82,0x09},
{0x5c83,0x5f},
{0x5d00,0x00},
{0x4001,0x60},
//{0x3008,0x01},
//{0x3663,0x60},
//{0x3002,0x01},
//{0x3c00,0x3c},
{0x560f,0xfc},
{0x5610,0xf0},
{0x5611,0x10},
{0x562f,0xfc},
{0x5630,0xf0},
{0x5631,0x10},
{0x564f,0xfc},
{0x5650,0xf0},
{0x5651,0x10},
{0x566f,0xfc},
{0x5670,0xf0},
{0x5671,0x10},
{0x556c,0x32},
{0x5568,0xf8},
{0x5569,0x40},
{0x556a,0x40},
{0x556b,0x10},
{0x556d,0x02},
{0x556e,0x67},
{0x556f,0x01},
{0x5570,0x4c},
};

static struct msm_camera_i2c_reg_setting init_reg_setting[] = {
  {
    .reg_setting = init_reg_array0,
    .size = ARRAY_SIZE(init_reg_array0),
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 50,
  },
  {
    .reg_setting = init_reg_array1,
    .size = ARRAY_SIZE(init_reg_array1),
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 0,
  },
};

static struct sensor_lib_reg_settings_array init_settings_array = {
  .reg_settings = init_reg_setting,
  .size = 2,
};

static struct msm_camera_i2c_reg_array start_reg_array[] = {
  {0x0100, 0x01},

};

static  struct msm_camera_i2c_reg_setting start_settings = {
  .reg_setting = start_reg_array,
  .size = ARRAY_SIZE(start_reg_array),
  .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
  .data_type = MSM_CAMERA_I2C_BYTE_DATA,
  .delay = 0,
};

static struct msm_camera_i2c_reg_array stop_reg_array[] = {

  {0x0100, 0x00},
};

static struct msm_camera_i2c_reg_setting stop_settings = {
  .reg_setting = stop_reg_array,
  .size = ARRAY_SIZE(stop_reg_array),
  .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
  .data_type = MSM_CAMERA_I2C_BYTE_DATA,
  .delay = 0,
};

static struct msm_camera_i2c_reg_array groupon_reg_array[] = {
  {0x3208, 0x00},
};

static struct msm_camera_i2c_reg_setting groupon_settings = {
  .reg_setting = groupon_reg_array,
  .size = ARRAY_SIZE(groupon_reg_array),
  .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
  .data_type = MSM_CAMERA_I2C_BYTE_DATA,
  .delay = 0,
};

static struct msm_camera_i2c_reg_array groupoff_reg_array[] = {
  {0x3208, 0x10},
  {0x3208, 0xA0},
};

static struct msm_camera_i2c_reg_setting groupoff_settings = {
  .reg_setting = groupoff_reg_array,
  .size = ARRAY_SIZE(groupoff_reg_array),
  .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
  .data_type = MSM_CAMERA_I2C_BYTE_DATA,
  .delay = 0,
};

static struct msm_camera_csid_vc_cfg ov13860_cid_cfg[] = {
  {0, CSI_RAW10, CSI_DECODE_10BIT},
  {1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params ov13860_csi_params = {
  .csid_params = {
    .lane_cnt = 4,
    .lut_params = {
      .num_cid = 2,
      .vc_cfg = {
         &ov13860_cid_cfg[0],
         &ov13860_cid_cfg[1],
      },
    },
  },
  .csiphy_params = {
    .lane_cnt = 4,
    .settle_cnt = 0x14, //gionee add by chenqiang for data not stable 20141028 .settle_cnt = 0x1a, // TODO
  },
};

static struct msm_camera_csi2_params ov13860_csi_params_hfr = {
  .csid_params = {
    .lane_cnt = 4,
    .lut_params = {
      .num_cid = 2,
      .vc_cfg = {
         &ov13860_cid_cfg[0],
         &ov13860_cid_cfg[1],
      },
    },
  },
  .csiphy_params = {
    .lane_cnt = 4,
    .settle_cnt = 0x1a, // TODO
  },
};

static struct msm_camera_csi2_params *csi_params[] = {
  &ov13860_csi_params, /* RES 0*/
  &ov13860_csi_params, /* RES 1*/
};

static struct sensor_lib_csi_params_array csi_params_array = {
  .csi2_params = &csi_params[0],
  .size = ARRAY_SIZE(csi_params),
};

static struct sensor_pix_fmt_info_t ov13860_pix_fmt0_fourcc[] = {
  { V4L2_PIX_FMT_SBGGR10 },
};

static struct sensor_pix_fmt_info_t ov13860_pix_fmt1_fourcc[] = {
  { MSM_V4L2_PIX_FMT_META },
};

static sensor_stream_info_t ov13860_stream_info[] = {
  {1, &ov13860_cid_cfg[0], ov13860_pix_fmt0_fourcc},
  {1, &ov13860_cid_cfg[1], ov13860_pix_fmt1_fourcc},
};

static sensor_stream_info_array_t ov13860_stream_info_array = {
  .sensor_stream_info = ov13860_stream_info,
  .size = ARRAY_SIZE(ov13860_stream_info),
};

static struct msm_camera_i2c_reg_array res0_reg_array[] = {
//4-Lane 4224x3120 10-bit 20fps 800Mbps/lane
{0x0303,0x00},
{0x030f,0x03},
{0x3501,0x0d},
{0x3502,0x88},
{0x370a,0x23},
{0x372f,0xa0},
{0x3808,0x10},
{0x3809,0x80},
{0x380a,0x0c},
{0x380b,0x30},
{0x380c,0x11},// 11
{0x380d,0xd6},// d6
{0x380e,0x0d},
{0x380f,0xa8},
{0x3815,0x11},
{0x3820,0x00},
{0x3842,0x00},
{0x4008,0x00},
{0x4009,0x13},
{0x4019,0x18},
{0x4051,0x03},
{0x4066,0x04},
{0x5000,0x9d},// 99---wanguocheng modified
{0x5201,0x80},
{0x5204,0x01},
{0x5205,0x00},
{0x4837,0x0d},
};

static struct msm_camera_i2c_reg_array res1_reg_array[] = {
//4-Lane 2112x1560 10-bit 30fps 600Mbps/lane
{0x0303,0x01},
{0x030f,0x07},
{0x3501,0x06},
{0x3502,0xb8},
{0x370a,0x63},
{0x372f,0x88},
{0x3808,0x08},
{0x3809,0x40},
{0x380a,0x06},
{0x380b,0x18},
{0x380c,0x11},
{0x380d,0xd6},
{0x380e,0x06},
{0x380f,0xd8},
{0x3815,0x31},
{0x3820,0x02},
{0x3842,0x40},
{0x4008,0x02},
{0x4009,0x09},
{0x4019,0x0c},
{0x4051,0x01},
{0x4066,0x02},
{0x5000,0xdd},//0xd9},
{0x5201,0x62},
{0x5204,0x00},
{0x5205,0x40},
{0x4837,0x1a},
};

static struct msm_camera_i2c_reg_setting res_settings[] = {
  {
    .reg_setting = res0_reg_array,
    .size = ARRAY_SIZE(res0_reg_array),
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 0,
  },
  {
    .reg_setting = res1_reg_array,
    .size = ARRAY_SIZE(res1_reg_array),
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 0,
  },
};

static struct sensor_lib_reg_settings_array res_settings_array = {
  .reg_settings = res_settings,
  .size = ARRAY_SIZE(res_settings),
};

static struct sensor_crop_parms_t crop_params[] = {
  {0, 0, 0, 0}, /* RES 0 */
  {0, 0, 0, 0}, /* RES 1 */

};

static struct sensor_lib_crop_params_array crop_params_array = {
  .crop_params = crop_params,
  .size = ARRAY_SIZE(crop_params),
};

static struct sensor_lib_out_info_t sensor_out_info[] = {
  {
    .x_output = 4224,
    .y_output = 3120,
    .line_length_pclk = 0x11d6, // 4566
    .frame_length_lines = 0xda8, // 3496
    .vt_pixel_clk = 480000000,
    .op_pixel_clk = 480000000,
    .binning_factor = 1,
    .max_fps = 30,
    .min_fps = 10,
    .mode = SENSOR_DEFAULT_MODE,
  },
  {
    .x_output = 2112,
    .y_output = 1560,
    .line_length_pclk = 0x11d6, // 4566
    .frame_length_lines = 0x6d8, // 1752
    .vt_pixel_clk = 240000000,
    .op_pixel_clk = 240000000,
    .binning_factor = 1,
    .max_fps = 30,
    .min_fps = 7.5,
    .mode = SENSOR_DEFAULT_MODE,
  },
 
};

static struct sensor_lib_out_info_array out_info_array = {
  .out_info = sensor_out_info,
  .size = ARRAY_SIZE(sensor_out_info),
};

static sensor_res_cfg_type_t ov13860_res_cfg[] = {
  SENSOR_SET_STOP_STREAM,
  SENSOR_SET_NEW_RESOLUTION, /* set stream config */
  SENSOR_SET_CSIPHY_CFG,
  SENSOR_SET_CSID_CFG,
  SENSOR_LOAD_CHROMATIX, /* set chromatix prt */
  SENSOR_SEND_EVENT, /* send event */
  SENSOR_SET_START_STREAM,
};

static struct sensor_res_cfg_table_t ov13860_res_table = {
  .res_cfg_type = ov13860_res_cfg,
  .size = ARRAY_SIZE(ov13860_res_cfg),
};

static struct sensor_lib_chromatix_t ov13860_chromatix[] = {
  {
    .common_chromatix = ov13860_LOAD_CHROMATIX(common),
    .camera_preview_chromatix = ov13860_LOAD_CHROMATIX(snapshot), /* RES0 */
    .camera_snapshot_chromatix = ov13860_LOAD_CHROMATIX(snapshot), /* RES0 */
    .camcorder_chromatix = ov13860_LOAD_CHROMATIX(default_video), /* RES0 */
    .liveshot_chromatix = ov13860_LOAD_CHROMATIX(liveshot), /* RES0 */
  },
  {
    .common_chromatix = ov13860_LOAD_CHROMATIX(common),
    .camera_preview_chromatix = ov13860_LOAD_CHROMATIX(preview), /* RES1 */
    .camera_snapshot_chromatix = ov13860_LOAD_CHROMATIX(preview), /* RES1 */
    .camcorder_chromatix = ov13860_LOAD_CHROMATIX(default_video), /* RES1 */
    .liveshot_chromatix = ov13860_LOAD_CHROMATIX(liveshot), /* RES1 */
  },

};

static struct sensor_lib_chromatix_array ov13860_lib_chromatix_array = {
  .sensor_lib_chromatix = ov13860_chromatix,
  .size = ARRAY_SIZE(ov13860_chromatix),
};

/*===========================================================================
 * FUNCTION    - ov13860_real_to_register_gain -
 *
 * DESCRIPTION:
 *==========================================================================*/
static uint16_t ov13860_real_to_register_gain(float gain)
{
  uint16_t reg_gain = 0;
  if (gain < 1.0)
      gain = 1.0;
  else if (gain > 15.5)  // wanguocheng test 8/12/15
      gain = 15.5;
  gain = gain*16.0;
  reg_gain = (uint16_t)gain;	
  return reg_gain;
}


/*===========================================================================
 * FUNCTION    - ov13860_register_to_real_gain -
 *
 * DESCRIPTION:
 *==========================================================================*/
static float ov13860_register_to_real_gain(uint16_t reg_gain)
{
  float real_gain;
  real_gain = (float) (((float)(reg_gain))/16.0);// 4 0x0f 16.0
  return real_gain;
}


/*===========================================================================
 * FUNCTION    - ov13860_calculate_exposure -
 *
 * DESCRIPTION:
 *==========================================================================*/
static int32_t ov13860_calculate_exposure(float real_gain,
  uint16_t line_count, sensor_exposure_info_t *exp_info)
{
  if (!exp_info) {
    return -1;
  }
  exp_info->reg_gain = ov13860_real_to_register_gain(real_gain);
  float sensor_real_gain = ov13860_register_to_real_gain(exp_info->reg_gain);
  exp_info->digital_gain = real_gain / sensor_real_gain;
  ALOGE("wanguocheng_ov13860 realgain = %f,sensor_gain=%f",real_gain,sensor_real_gain);  
  exp_info->line_count = line_count;
  return 0;
}

/*===========================================================================
 * FUNCTION    - ov13860_fill_exposure_array -
 *
 * DESCRIPTION:
 *==========================================================================*/
static int32_t ov13860_fill_exposure_array(uint16_t gain, uint32_t line,
  uint32_t fl_lines, int32_t luma_avg, uint32_t fgain,
  struct msm_camera_i2c_reg_setting* reg_setting)
{
  int32_t rc = 0;
  uint16_t reg_count = 0;
  uint16_t i = 0;
  ALOGE("%s:wanguocheng-0_gain=%d,line=%d,fl_lines=%d,luma_avg=%d,fgain=%d",
  	       __func__,gain,line,fl_lines,luma_avg,fgain) ;

  if (!reg_setting) {
    return -1;
  }
  
  for (i = 0; i < sensor_lib_ptr.groupon_settings->size; i++)
  {
    reg_setting->reg_setting[reg_count].reg_addr =
      sensor_lib_ptr.groupon_settings->reg_setting[i].reg_addr;
    reg_setting->reg_setting[reg_count].reg_data =
      sensor_lib_ptr.groupon_settings->reg_setting[i].reg_data;
    reg_count = reg_count + 1;
  }

  reg_setting->reg_setting[reg_count].reg_addr =
    sensor_lib_ptr.output_reg_addr->frame_length_lines;
  reg_setting->reg_setting[reg_count].reg_data = (fl_lines & 0xFF00) >> 8;
  /*ALOGE("%s:wanguocheng-1_fl_lines:addr=0x%x,data=0x%x",__func__,
  		reg_setting->reg_setting[reg_count].reg_addr,
  		reg_setting->reg_setting[reg_count].reg_data) ;*/
  reg_count++;

  reg_setting->reg_setting[reg_count].reg_addr =
    sensor_lib_ptr.output_reg_addr->frame_length_lines + 1;
  reg_setting->reg_setting[reg_count].reg_data = (fl_lines & 0xFF);
  /*ALOGE("%s:wanguocheng-2_fl_lines:addr=0x%x,data=0x%x",__func__,
    		reg_setting->reg_setting[reg_count].reg_addr,
    		reg_setting->reg_setting[reg_count].reg_data) ;*/
  reg_count++;

  reg_setting->reg_setting[reg_count].reg_addr =
    sensor_lib_ptr.exp_gain_info->coarse_int_time_addr;
  reg_setting->reg_setting[reg_count].reg_data = line >> 8;
  /*ALOGE("%s:wanguocheng-3_lines:addr=0x%x,data=0x%x",__func__,
    		reg_setting->reg_setting[reg_count].reg_addr,
    		reg_setting->reg_setting[reg_count].reg_data) ;*/
  reg_count++;

  reg_setting->reg_setting[reg_count].reg_addr =
    sensor_lib_ptr.exp_gain_info->coarse_int_time_addr + 1;
  reg_setting->reg_setting[reg_count].reg_data = (line & 0xFF);
  /*ALOGE("%s:wanguocheng-4_lines:addr=0x%x,data=0x%x",__func__,
    		reg_setting->reg_setting[reg_count].reg_addr,
    		reg_setting->reg_setting[reg_count].reg_data) ;*/
  reg_count++;
  
  ALOGE("wanguocheng_ov13860 AE line = %d", line);

  reg_setting->reg_setting[reg_count].reg_addr =
    sensor_lib_ptr.exp_gain_info->global_gain_addr;
  reg_setting->reg_setting[reg_count].reg_data = (gain & 0xFF00) >> 8;
  /*ALOGE("%s:wanguocheng-5_gain:addr=0x%x,data=0x%x",__func__,
    		reg_setting->reg_setting[reg_count].reg_addr,
    		reg_setting->reg_setting[reg_count].reg_data) ;*/
  reg_count++;

  reg_setting->reg_setting[reg_count].reg_addr =
    sensor_lib_ptr.exp_gain_info->global_gain_addr + 1;
  reg_setting->reg_setting[reg_count].reg_data = (gain & 0xFF);
  /*ALOGE("%s:wanguocheng-6_gain:addr=0x%x,data=0x%x",__func__,
    		reg_setting->reg_setting[reg_count].reg_addr,
    		reg_setting->reg_setting[reg_count].reg_data) ;*/
  reg_count++;

  ALOGE("wanguocheng-7_ov13860 AE gain = %d", gain);
  
  for (i = 0; i < sensor_lib_ptr.groupoff_settings->size; i++) 
  {
    reg_setting->reg_setting[reg_count].reg_addr =
      sensor_lib_ptr.groupoff_settings->reg_setting[i].reg_addr;
    reg_setting->reg_setting[reg_count].reg_data =
      sensor_lib_ptr.groupoff_settings->reg_setting[i].reg_data;
    reg_count = reg_count + 1;
  }

  reg_setting->size = reg_count;
  reg_setting->addr_type = MSM_CAMERA_I2C_WORD_ADDR;
  reg_setting->data_type = MSM_CAMERA_I2C_BYTE_DATA;
  reg_setting->delay = 0;

  return rc;
}

static sensor_exposure_table_t ov13860_expsoure_tbl = {
  .sensor_calculate_exposure = ov13860_calculate_exposure,
  .sensor_fill_exposure_array = ov13860_fill_exposure_array,
};

static sensor_lib_t sensor_lib_ptr = {
  /* sensor slave info */
  .sensor_slave_info = &sensor_slave_info,  
  /* sensor init params */
  .sensor_init_params = &sensor_init_params,
  /* sensor eeprom name */
  .eeprom_name = "sunny_p13v04a",   
  /* sensor actuator name */
  .actuator_name = "dw9800",  
  /* sensor output settings */
  .sensor_output = &sensor_output,
  /* sensor output register address */
  .output_reg_addr = &output_reg_addr,
  /* sensor exposure gain register address */
  .exp_gain_info = &exp_gain_info,
  /* sensor aec info */
  .aec_info = &aec_info,
  /* sensor snapshot exposure wait frames info */
  .snapshot_exp_wait_frames = 1,
  /* number of frames to skip after start stream */
  .sensor_num_frame_skip = 2,
  
  /* sensor pipeline immediate delay */
  .sensor_max_pipeline_frame_delay = 1,
  /* sensor exposure table size */
  .exposure_table_size = 10,
  /* sensor lens info */
  .default_lens_info = &default_lens_info,
  /* csi lane params */
  .csi_lane_params = &csi_lane_params,
  /* csi cid params */
  .csi_cid_params = ov13860_cid_cfg,
  /* csi csid params array size */
  .csi_cid_params_size = ARRAY_SIZE(ov13860_cid_cfg),
  /* init settings */
  .init_settings_array = &init_settings_array,
  /* start settings */
  .start_settings = &start_settings,
  /* stop settings */
  .stop_settings = &stop_settings,
  /* group on settings */
  .groupon_settings = &groupon_settings,
  /* group off settings */
  .groupoff_settings = &groupoff_settings,
  /* resolution cfg table */
  .sensor_res_cfg_table = &ov13860_res_table,
  /* res settings */
  .res_settings_array = &res_settings_array,
  /* out info array */
  .out_info_array = &out_info_array,
  /* crop params array */
  .crop_params_array = &crop_params_array,
  /* csi params array */
  .csi_params_array = &csi_params_array,
  /* sensor port info array */
  .sensor_stream_info_array = &ov13860_stream_info_array,
  /* exposure funtion table */
  .exposure_func_table = &ov13860_expsoure_tbl,
  /* chromatix array */
  .chromatix_array = &ov13860_lib_chromatix_array,
};

/*===========================================================================
 * FUNCTION    - ov13860_open_lib -
 *
 * DESCRIPTION:
 *==========================================================================*/
void *ov13860_open_lib(void)
{
  return &sensor_lib_ptr;
}
