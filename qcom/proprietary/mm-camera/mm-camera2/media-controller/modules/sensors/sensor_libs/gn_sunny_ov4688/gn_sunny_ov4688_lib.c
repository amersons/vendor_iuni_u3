/*============================================================================

  Copyright (c) 2012-2013 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/
#include <stdio.h>
#include "sensor_lib.h"

#define SENSOR_MODEL_NO_GN_SUNNY_OV4688 "gn_sunny_ov4688"
#define GN_SUNNY_OV4688_LOAD_CHROMATIX(n) \
  "libchromatix_"SENSOR_MODEL_NO_GN_SUNNY_OV4688"_"#n".so"

#define ENABLE_RES0_REGISTER_SETTINGS 0
#define ENABLE_RES1_REGISTER_SETTINGS 0
#define ENABLE_RES2_REGISTER_SETTINGS 1
#define ENABLE_RES3_REGISTER_SETTINGS 1

static sensor_lib_t sensor_lib_ptr;

static struct msm_sensor_power_setting power_setting[] = {
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VDIG,   //SUB DVDD GPIO POWER
		.config_val = GPIO_OUT_HIGH,
		.delay = 5,
	},
	  {
		.seq_type = SENSOR_VREG,
		.seq_val =  CAM_VDIG,   //SUB DVDD GPIO POWER
		.config_val = 0,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 15,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 40,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 40,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 40,
	},

	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 5,
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
  .camera_id = CAMERA_1,
  /* sensor slave address */
  .slave_addr = 0x6c,
  /* sensor address type */
  .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
  /* sensor id info*/
  .sensor_id_info = {
    /* sensor id register address */
    .sensor_id_reg_addr = 0x300A,
    /* sensor id */
    .sensor_id = 0x4688,
  },
  /* power up / down setting */
  .power_setting_array = {
    .power_setting = power_setting,
    .size = ARRAY_SIZE(power_setting),
  },
};

static struct msm_sensor_init_params sensor_init_params = {
  .modes_supported = CAMERA_MODE_2D_B,
  .position = FRONT_CAMERA_B,
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
  .global_gain_addr = 0x3508,
//Gionee <zhuangxiaojian> <2014-04-15> modify for CR01185256 begin
#ifdef ORIGINAL_VERSION
  .vert_offset = 6,
#else
  .vert_offset = 8,
#endif
//Gionee <zhuangxiaojian> <2014-04-15> modify for CR01185256 end
};

static sensor_aec_data_t aec_info = {
  .max_gain = 16.0,
  .max_linecount = 5000,
};

static sensor_lens_info_t default_lens_info = {
  .focal_length = 3.68,
  .pix_size = 2.0,
  .f_number = 2.0,
  .total_f_dist = 1.2,
  .hor_view_angle = 64.0,
  .ver_view_angle = 48.0,
};

static struct csi_lane_params_t csi_lane_params = {
  .csi_lane_assign = 0x4320,
  .csi_lane_mask = 0x1F,
  .csi_if = 1,
  .csid_core = {0},
  .csi_phy_sel = 2,
};

static struct msm_camera_i2c_reg_array init_reg_array0[] = {
  {0x0103, 0x01},
};

static struct msm_camera_i2c_reg_array init_reg_array1[] = {
  {0x0100, 0x00}, //add by chenqiang 
  {0x3638,0x00},// ADC & analog       
  {0x0300,0x00},// PLL1 prediv        
  {0x0302,0x2a},// PLL1 divm          
  {0x0304,0x03},// PLL1 div mipi      
  {0x030b,0x00},// PLL2 pre div       
  {0x030d,0x1e},// PLL2 multiplier    
  {0x030e,0x04},// PLL2 divs          
  {0x030f,0x01},// PLL2 divsp         
  {0x0312,0x01},// PLL2 divdac        
  {0x031e,0x00},// Debug mode         
  {0x3000,0x20},// FSIN output        
  {0x3002,0x00},// Vsync input, HREF i
  {0x3018,0x72},// MIPI 4 lane, Reset 
  {0x3020,0x93},// Clock switch to PLL
  {0x3021,0x03},// Sleep latch, softwa
  {0x3022,0x01},// LVDS disable, Enabl
  {0x3031,0x0a},// MIPI 10-bit mode   
  {0x3305,0xf1},// ASRAM              
  {0x3307,0x04},// ASRAM              
  {0x3309,0x29},// ASRAM              
  {0x3500,0x00},// Long exposure HH   
  {0x3501,0x60},// Long exposure H    
  {0x3502,0x00},// Long exposure L    
  {0x3503,0x04},// Gain delay 1 frame,
  {0x3504,0x00},// debug mode         
  {0x3505,0x00},// debug mode         
  {0x3506,0x00},// debug mode         
  {0x3507,0x00},// Long gain HH       
  {0x3508,0x00},// Long gain H        
  {0x3509,0x80},// Long gain L        
  {0x350a,0x00},// Middle exposure HH 
  {0x350b,0x00},// Middle exposure H  
  {0x350c,0x00},// Middle exposure L  
  {0x350d,0x00},// Middle gain HH     
  {0x350e,0x00},// Middle gain H      
  {0x350f,0x80},// Middle gain L      
  {0x3510,0x00},// Short exposure HH  
  {0x3511,0x00},// Short exposure H   
  {0x3512,0x00},// Short exposure L   
  {0x3513,0x00},// Short gain HH      
  {0x3514,0x00},// Short gain H       
  {0x3515,0x80},// Short gain L       
  {0x3516,0x00},// 4th exposure HH    
  {0x3517,0x00},// 4th exposure H     
  {0x3518,0x00},// 4th exposure L     
  {0x3519,0x00},// 4th gain HH        
  {0x351a,0x00},// 4th gain H         
  {0x351b,0x80},// 4th gian L         
  {0x351c,0x00},// 5th exposure HH    
  {0x351d,0x00},// 5th exposure H     
  {0x351e,0x00},// 5th exposure L     
  {0x351f,0x00},// 5th gain HH        
  {0x3520,0x00},// 5th gain H         
  {0x3521,0x80},// 5th gain L         
  {0x3522,0x08},// Middle digital frac
  {0x3524,0x08},// Short digital fract
  {0x3526,0x08},// 4th digital fractio
  {0x3528,0x08},// 5th digital framcti
  {0x352a,0x08},// Long digital fracti
  {0x3602,0x00},// ADC & analog       
  {0x3604,0x02},//                    
  {0x3605,0x00},//                    
  {0x3606,0x00},//                    
  {0x3607,0x00},//                    
  {0x3609,0x12},//                    
  {0x360a,0x40},//                    
  {0x360c,0x08},//                    
  {0x360f,0xe5},//                    
  {0x3608,0x8f},//                    
  {0x3611,0x00},//                    
  {0x3613,0xf7},//                    
  {0x3616,0x58},//                    
  {0x3619,0x99},//                    
  {0x361b,0x60},//                    
  {0x361c,0x7a},//                    
  {0x361e,0x79},//                    
  {0x361f,0x02},//                    
  {0x3632,0x00},//                    
  {0x3633,0x10},//                    
  {0x3634,0x10},//                    
  {0x3635,0x10},//                    
  {0x3636,0x15},//                    
  {0x3646,0x86},//                    
  {0x364a,0x0b},// ADC & analog       
  {0x3700,0x17},// Sensor control     
  {0x3701,0x22},//                    
  {0x3703,0x10},//                    
  {0x370a,0x37},//                    
  {0x3705,0x00},//                    
  {0x3706,0x63},//                    
  {0x3709,0x3c},//                    
  {0x370b,0x01},//                    
  {0x370c,0x30},//                    
  {0x3710,0x24},//                    
  {0x3711,0x0c},//                    
  {0x3716,0x00},//                    
  {0x3720,0x28},//                    
  {0x3729,0x7b},//                    
  {0x372a,0x84},//                    
  {0x372b,0xbd},//                    
  {0x372c,0xbc},//                    
  {0x372e,0x52},//                    
  {0x373c,0x0e},//                    
  {0x373e,0x33},//                    
  {0x3743,0x10},//                    
  {0x3744,0x88},//                    
  {0x374a,0x43},//                    
  {0x374c,0x00},//                    
  {0x374e,0x23},//                    
  {0x3751,0x7b},//                    
  {0x3752,0x84},//                    
  {0x3753,0xbd},//                    
  {0x3754,0xbc},//                    
  {0x3756,0x52},//                    
  {0x375c,0x00},//                    
  {0x3760,0x00},//                    
  {0x3761,0x00},//                    
  {0x3762,0x00},//                    
  {0x3763,0x00},//                    
  {0x3764,0x00},//                    
  {0x3767,0x04},//                    
  {0x3768,0x04},//                    
  {0x3769,0x08},//                    
  {0x376a,0x08},//                    
  {0x376b,0x20},//                    
  {0x376c,0x00},//                    
  {0x376d,0x00},//                    
  {0x376e,0x00},//                    
  {0x3773,0x00},//                    
  {0x3774,0x51},//                    
  {0x3776,0xbd},//                    
  {0x3777,0xbd},//                    
  {0x3781,0x18},//                    
  {0x3783,0x25},// Sensor control     
  {0x3800,0x00},// H crop start H     
  {0x3801,0x08},// H crop start L     
  {0x3802,0x00},// V crop start H     
  {0x3803,0x04},// V crop start L     
  {0x3804,0x0a},// H crop end H       
  {0x3805,0x97},// H crop end L       
  {0x3806,0x05},// V crop end H       
  {0x3807,0xfb},// V crop end L       
  {0x3808,0x0a},// H output size H    
  {0x3809,0x80},// H output size L    
  {0x380a,0x05},// V output size H    
  {0x380b,0xf0},// V output size L    
  {0x380c,0x03},// HTS H              
  {0x380d,0x5c},// HTS L              
  {0x380e,0x06},// VTS H              
  {0x380f,0x12},// VTS L              
  {0x3810,0x00},// H win off H        
  {0x3811,0x08},// H win off L        
  {0x3812,0x00},// V win off H        
  {0x3813,0x04},// V win off L        
  {0x3814,0x01},// H inc odd          
  {0x3815,0x01},// H inc even         
  {0x3819,0x01},// Vsync end L        
  {0x3820,0x00},// flip off, bin off  
  {0x3821,0x06},// mirror on, bin off 
  {0x3829,0x00},// HDR lite off       
  {0x382a,0x01},// vertical subsample 
  {0x382b,0x01},// vertical subsample 
  {0x382d,0x7f},// black column end ad
  {0x3830,0x04},// blc use num/2      
  {0x3836,0x01},// r zline use num/2  
  {0x3841,0x02},// r_rcnt_fix on      
  {0x3846,0x08},// fcnt_trig_rst_en on
  {0x3847,0x07},// debug mode         
  {0x3d85,0x36},// OTP bist enable, OT
  {0x3d8c,0x71},// OTP start address H
  {0x3d8d,0xcb},// OTP start address L
  {0x3f0a,0x00},// PSRAM              
  {0x4000,0x71},// out of range trig o
  {0x4001,0x40},// debug mode         
  {0x4002,0x04},// debug mode         
  {0x4003,0x14},// black line number  
  {0x400e,0x00},// offset for BLC bypa
  {0x4011,0x00},// offset man same off
  {0x401a,0x00},// debug mode         
  {0x401b,0x00},// debug mode         
  {0x401c,0x00},// debug mode         
  {0x401d,0x00},// debug mode         
  {0x401f,0x00},// debug mode         
  {0x4020,0x00},// Anchor left start H
  {0x4021,0x10},// Anchor left start L
  {0x4022,0x07},// Anchor left end H  
  {0x4023,0xcf},// Anchor left end L  
  {0x4024,0x09},// Anchor right start 
  {0x4025,0x60},// Andhor right start 
  {0x4026,0x09},// Anchor right end H 
  {0x4027,0x6f},// Anchor right end L 
  {0x4028,0x00},// top Zline start    
  {0x4029,0x02},// top Zline number   
  {0x402a,0x06},// top blk line start 
  {0x402b,0x04},// to blk line number 
  {0x402c,0x02},// bot Zline start    
  {0x402d,0x02},// bot Zline number   
  {0x402e,0x0e},// bot blk line start 
  {0x402f,0x04},// bot blk line number
  {0x4302,0xff},// clipping max H     
  {0x4303,0xff},// clipping max L     
  {0x4304,0x00},// clipping min H     
  {0x4305,0x00},// clipping min L     
  {0x4306,0x00},// vfifo pix swap off,
  {0x4308,0x02},// debug mode, embeded
  {0x4500,0x6c},// ADC sync control   
  {0x4501,0xc4},//                    
  {0x4502,0x40},//                    
  {0x4503,0x02},// ADC sync control   
  {0x4601,0x04},// V fifo read start  
  {0x4800,0x04},// MIPI always high sp
  {0x4813,0x08},// Select HDR VC      
  {0x481f,0x40},// MIPI clock prepare 
  {0x4829,0x78},// MIPI HS exit min   
  {0x4837,0x10},// MIPI global timing 
  {0x4b00,0x2a},//                    
  {0x4b0d,0x00},//                    
  {0x4d00,0x04},// tpm slope H        
  {0x4d01,0x42},// tpm slope L        
  {0x4d02,0xd1},// tpm offset HH      
  {0x4d03,0x93},// tpm offset H       
  {0x4d04,0xf5},// tpm offset M       
  {0x4d05,0xc1},// tpm offset L       
  {0x5000,0xf3},// digital gain on, bi
  {0x5001,0x11},// ISP EOF select, ISP
  {0x5004,0x00},// debug mode         
  {0x500a,0x00},// debug mode         
  {0x500b,0x00},// debug mode         
  {0x5032,0x00},// debug mode         
  {0x5040,0x00},// test mode off      
  {0x5050,0x0c},// debug mode         
  {0x5500,0x00},// OTP DPC start H    
  {0x5501,0x10},// OTP DPC start L    
  {0x5502,0x01},// OTP DPC end H      
  {0x5503,0x0f},// OTP DPC end L      
  {0x8000,0x00},// test mode          
  {0x8001,0x00},//                    
  {0x8002,0x00},//                    
  {0x8003,0x00},//                    
  {0x8004,0x00},//                    
  {0x8005,0x00},//                    
  {0x8006,0x00},//                    
  {0x8007,0x00},//                    
  {0x8008,0x00},// test mode          
  {0x3638,0x00},// ADC & analog       
  {0x3105,0x31},// SCCB control, debug
  {0x301a,0xf9},// enable emb clock, e
  {0x3508,0x07},// Long gain H        
  {0x484b,0x05},// line start after fi
  {0x4805,0x03},// MIPI control       
  {0x3601,0x01},// ADC & analog       
//Gionee <zhuangxiaojian> <2014-03-12> modify for CR01090346 begin
#ifdef ORIGINAL_VERSION
#else
{0x5000, 0xd3},
#endif
  {0x0100,0x01},// wake up from sleep 
//Gionee <zhuangxiaojian> <2014-03-12> modify for CR01090346 end

};
//liushengbin modify for CR00955028 delay 50ms start
static struct msm_camera_i2c_reg_array init_reg_array2[] = {
  {0x3105,0x11},// SCCB control, debug mode          
  {0x301a,0xf1},// disable mipi-phy reset            
  {0x4805,0x00},// MIPI control                      
  {0x301a,0xf0},// enable emb clock, enable strobe cl
  {0x3208,0x00},// group hold start, group bank 0    
  {0x302a,0x00},// delay?                            
  {0x302a,0x00},//                                   
  {0x302a,0x00},//                                   
  {0x302a,0x00},//                                   
  {0x302a,0x00},//                                   
  {0x3601,0x00},// ADC & analog                      
  {0x3638,0x00},// ADC & analog                      
  {0x3208,0x10},// group hold end, group select 0    
  {0x3208,0xa0},// group delay launch, group select 0
};
//liushengbin modify for CR00955028 delay 50ms end

static struct msm_camera_i2c_reg_setting init_reg_setting[] = {
  {
    .reg_setting = init_reg_array0,
    .size = ARRAY_SIZE(init_reg_array0),
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 5,
  },
  {
    .reg_setting = init_reg_array1,
    .size = ARRAY_SIZE(init_reg_array1),
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 10, //liushengbin modify for CR00955028 delay 50ms
  },
    {
    .reg_setting = init_reg_array2,
    .size = ARRAY_SIZE(init_reg_array2),
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 0,
  },
};

static struct sensor_lib_reg_settings_array init_settings_array = {
  .reg_settings = init_reg_setting,
  .size = 3,//liushengbin modify for CR00955028 delay 50ms
};

//Gionee <zhuangxiaojian> <2014-03-12> modify for CR01090346 begin
#ifdef ORIGINAL_VERSION
static struct msm_camera_i2c_reg_array start_reg_array[] = {
  {0x0100, 0x01},
  //{0x301a, 0xf9},//liushengbin modify for CR00955028
  //{0x301a, 0xf1},
  //{0x4805, 0x00},
  //{0x301a, 0xf0},
};

static  struct msm_camera_i2c_reg_setting start_settings = {
  .reg_setting = start_reg_array,
  .size = ARRAY_SIZE(start_reg_array),
  .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
  .data_type = MSM_CAMERA_I2C_BYTE_DATA,
  .delay = 0,
};

static struct msm_camera_i2c_reg_array stop_reg_array[] = {
  {0x0100, 0x00},//liushengbin modify for CR00955028
  //{0x301a, 0xf9},
  //{0x4805, 0x03},
};

static struct msm_camera_i2c_reg_setting stop_settings = {
  .reg_setting = stop_reg_array,
  .size = ARRAY_SIZE(stop_reg_array),
  .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
  .data_type = MSM_CAMERA_I2C_BYTE_DATA,
  .delay = 0,
};
#else
static struct msm_camera_i2c_reg_array start_reg_array0[] = {
//Gionee <zhaocuiqin> <2014-11-17> modify for CR01390274 begin
#ifdef ORIGINAL_VERSION
  {0x0100, 0x01},
  {0x301a, 0xf9},
#else
  {0x0100, 0x01},
#endif
//Gionee <zhaocuiqin> <2014-11-17> modify for CR01390274 end
};

static struct msm_camera_i2c_reg_array start_reg_array1[] = {
//Gionee <zhaocuiqin> <2014-11-17> modify for CR01390274 begin
#ifdef ORIGINAL_VERSION
  {0x301a, 0xf1},
  {0x4805, 0x00},
  {0x301a, 0xf0},
#else
  {0x3105, 0x11},
  {0x301a, 0xF1},
  {0x4805, 0x00},
  {0x301a, 0xF0},
  {0x3208, 0x00},
  {0x302a, 0x00},
  {0x302a, 0x00},
  {0x302a, 0x00},
  {0x302a, 0x00},
  {0x302a, 0x00},
  {0x3601, 0x00},
  {0x3638, 0x00},
  {0x3208, 0x10},
  {0x3208, 0xa0},
#endif
//Gionee <zhaocuiqin> <2014-11-17> modify for CR01390274 end
} ;

static struct msm_camera_i2c_reg_setting start_settings[] = {
	{
	  .reg_setting = start_reg_array0,
	  .size = ARRAY_SIZE(start_reg_array0),
	  .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
	  .data_type = MSM_CAMERA_I2C_BYTE_DATA,
	  .delay = 10,
	},
	{
	  .reg_setting = start_reg_array1,
	  .size = ARRAY_SIZE(start_reg_array1),
	  .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
	  .data_type = MSM_CAMERA_I2C_BYTE_DATA,
	},
};

static struct sensor_lib_reg_settings_array start_setting_array = {
  .reg_settings = start_settings,
  .size = 2,
};

static struct msm_camera_i2c_reg_array stop_reg_array0[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_array stop_reg_array1[] = {
	{0x301a, 0xf9},
	{0x4805, 0x03},
} ;

static struct msm_camera_i2c_reg_setting stop_settings[] = {
	{
	  .reg_setting = stop_reg_array0,
	  .size = ARRAY_SIZE(stop_reg_array0),
	  .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
	  .data_type = MSM_CAMERA_I2C_BYTE_DATA,
	  .delay = 135,
	},
	{
	  .reg_setting = stop_reg_array1,
	  .size = ARRAY_SIZE(stop_reg_array1),
	  .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
	  .data_type = MSM_CAMERA_I2C_BYTE_DATA,
	},
};

static struct sensor_lib_reg_settings_array stop_setting_array = {
  .reg_settings = stop_settings,
  .size = 2,
};
#endif
//Gionee <zhuangxiaojian> <2014-03-12> modify for CR01090346 end

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

static struct msm_camera_csid_vc_cfg gn_sunny_ov4688_cid_cfg[] = {
  {0, CSI_RAW10, CSI_DECODE_10BIT},
  {1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params gn_sunny_ov4688_csi_params = {
  .csid_params = {
    .lane_cnt = 4,
    .lut_params = {
      .num_cid = ARRAY_SIZE(gn_sunny_ov4688_cid_cfg),
      .vc_cfg = {
         &gn_sunny_ov4688_cid_cfg[0],
         &gn_sunny_ov4688_cid_cfg[1],
      },
    },
  },
  .csiphy_params = {
    .lane_cnt = 4,
    .settle_cnt = 0x0f,//liushengbin modify for CR00955028 0x0f--0x14   0x0c --0x16   0x07 0x0f 0x13 0x15
  },
};

static struct msm_camera_csi2_params *csi_params[] = {
#if ENABLE_RES0_REGISTER_SETTINGS
  &gn_sunny_ov4688_csi_params, /* RES 0*/
#endif
#if ENABLE_RES1_REGISTER_SETTINGS
  &gn_sunny_ov4688_csi_params, /* RES 1*/
#endif
#if ENABLE_RES2_REGISTER_SETTINGS
  &gn_sunny_ov4688_csi_params, /* RES 2*/
#endif
#if ENABLE_RES3_REGISTER_SETTINGS
  &gn_sunny_ov4688_csi_params, /* RES 3*/
#endif
};

static struct sensor_lib_csi_params_array csi_params_array = {
  .csi2_params = &csi_params[0],
  .size = ARRAY_SIZE(csi_params),
};
static struct sensor_pix_fmt_info_t gn_sunny_ov4688_pix_fmt0_fourcc[] = {
  { V4L2_PIX_FMT_SBGGR10 },
};
static struct sensor_pix_fmt_info_t gn_sunny_ov4688_pix_fmt1_fourcc[] = {
  { MSM_V4L2_PIX_FMT_META },
};

static sensor_stream_info_t gn_sunny_ov4688_stream_info[] = {
  {1, &gn_sunny_ov4688_cid_cfg[0], gn_sunny_ov4688_pix_fmt0_fourcc},
  {1, &gn_sunny_ov4688_cid_cfg[1], gn_sunny_ov4688_pix_fmt1_fourcc},
};

static sensor_stream_info_array_t gn_sunny_ov4688_stream_info_array = {
  .sensor_stream_info = gn_sunny_ov4688_stream_info,
  .size = ARRAY_SIZE(gn_sunny_ov4688_stream_info),
};
#if ENABLE_RES0_REGISTER_SETTINGS
static struct msm_camera_i2c_reg_array res0_reg_array[] = {
// @@5.1.2 Full Resolution 2688x1520 90 fps_1008 Mbps
  {0x0302,0x2a},// PLL1 divm
  {0x3501,0x60},// long exposure H
  {0x3632,0x00},// ADC & Analog
  {0x376b,0x20},// Sensor control
  {0x3800,0x00},// H crop start H
  {0x3801,0x08},// H crop start L
  {0x3803,0x04},// V crop start L
  {0x3804,0x0a},// H crop end H
  {0x3805,0x97},// H crop end L
  {0x3807,0xfb},// V crop end L
  {0x3808,0x0a},// H output size H
  {0x3809,0x80},// H output size L
  {0x380a,0x05},// V output size H
  {0x380b,0xf0},// V output size L
  {0x380c,0x04},// HTS H  0x03       .line_length_pclk = 1026,   //0x402
  {0x380d,0x02},// HTS L  0x5c
  {0x380e,0x06},// VTS H  0x06           .frame_length_lines = 1580, //0x62c
  {0x380f,0x2c},// VTS L  0x12
  {0x3811,0x08},// H win off L
  {0x3813,0x04},// V win off L
  {0x3814,0x01},// H inc odd
  {0x3820,0x00},// flip off, bin off
  {0x3821,0x06},// mirror on, bin off
  {0x382a,0x01},// vertical subsample odd increase number
  {0x3830,0x04},// blc use num/2
  {0x3836,0x01},// r zline use num/2
  {0x4001,0x40},// debug mode
  {0x4022,0x07},// Anchor left end H
  {0x4023,0xcf},// Anchor left end L
  {0x4024,0x09},// Anchor right start H
  {0x4025,0x60},// Andhor right start L
  {0x4026,0x09},// Anchor right end H
  {0x4027,0x6f},// Anchor right end L
  {0x4502,0x40},// ADC sync control
  {0x4601,0x04},// V fifo read start
  {0x4837,0x10},//{0x4837,0x18},// MIPI global timing
};
#endif
#if ENABLE_RES1_REGISTER_SETTINGS
static struct msm_camera_i2c_reg_array res1_reg_array[] = {
// @@5.2.1 Full Resolution 2688x1520 60fps_ 672Mbps
  {0x0302,0x1c},// PLL1 divm
  {0x3501,0x60},// long exposure H
  {0x3632,0x00},// ADC & Analog
  {0x376b,0x20},// Sensor control
  {0x3800,0x00},// H crop start H
  {0x3801,0x08},// H crop start L
  {0x3803,0x04},// V crop start L
  {0x3804,0x0a},// H crop end H
  {0x3805,0x97},// H crop end L
  {0x3807,0xfb},// V crop end L
  {0x3808,0x0a},// H output size H
  {0x3809,0x80},// H output size L
  {0x380a,0x05},// V output size H
  {0x380b,0xf0},// V output size L
  {0x380c,0x05},// HTS H       .line_length_pclk = 1284,//0x504
  {0x380d,0x04},// HTS L
  {0x380e,0x06},// VTS H        .frame_length_lines = 1584,//0x630
  {0x380f,0x30},// VTS L
  {0x3811,0x08},// H win off L
  {0x3813,0x04},// V win off L
  {0x3814,0x01},// H inc odd
  {0x3820,0x00},// flip off, bin off
  {0x3821,0x06},// mirror on, bin off
  {0x382a,0x01},// vertical subsample odd increase number
  {0x3830,0x04},// blc use num/2
  {0x3836,0x01},// r zline use num/2
  {0x4001,0x40},// debug mode
  {0x4022,0x07},// Anchor left end H
  {0x4023,0xcf},// Anchor left end L
  {0x4024,0x09},// Anchor right start H
  {0x4025,0x60},// Andhor right start L
  {0x4026,0x09},// Anchor right end H
  {0x4027,0x6f},// Anchor right end L
  {0x4502,0x40},// ADC sync control
  {0x4601,0x04},// V fifo read start
  {0x4837,0x18},// MIPI global timing
};
#endif
#if ENABLE_RES2_REGISTER_SETTINGS
static struct msm_camera_i2c_reg_array res2_reg_array[] = {
// @@5.2.1 Full Resolution 2688x1520 30fps_ 672Mbps
  {0x0302,0x1c},// PLL1 divm
  {0x3501,0x60},// long exposure H
  {0x3632,0x00},// ADC & Analog
  {0x376b,0x20},// Sensor control
  {0x3800,0x00},// H crop start H
  {0x3801,0x08},// H crop start L
  {0x3803,0x04},// V crop start L
  {0x3804,0x0a},// H crop end H
  {0x3805,0x97},// H crop end L
  {0x3807,0xfb},// V crop end L
  {0x3808,0x0a},// H output size H
  {0x3809,0x80},// H output size L
  {0x380a,0x05},// V output size H
  {0x380b,0xf0},// V output size L
  //{0x380c,0x09},// HTS H 0x0a     .line_length_pclk = 2478,// 0x9ae
  //{0x380d,0xae},// HTS L 0x18
  //{0x380e,0x06},// VTS H 0x06     .frame_length_lines = 1614,//0x64e
  //{0x380f,0x4e},// VTS L 0x12
  {0x380c,0x09},// HTS H 0x0a    modify by zhaocuiqin 20140805 for CR01321863
  {0x380d,0xc2},// HTS L 0x18
  {0x380e,0x06},// VTS H 0x06     
  {0x380f,0x62},// VTS L 0x12
  {0x3811,0x08},// H win off L
  {0x3813,0x04},// V win off L
  {0x3814,0x01},// H inc odd
  {0x3820,0x00},// flip off, bin off
  {0x3821,0x06},// mirror on, bin off
  {0x382a,0x01},// vertical subsample odd increase number
  {0x3830,0x04},// blc use num/2
  {0x3836,0x01},// r zline use num/2
  {0x4001,0x40},// debug mode
  {0x4022,0x07},// Anchor left end H
  {0x4023,0xcf},// Anchor left end L
  {0x4024,0x09},// Anchor right start H
  {0x4025,0x60},// Andhor right start L
  {0x4026,0x09},// Anchor right end H
  {0x4027,0x6f},// Anchor right end L
  {0x4502,0x40},//ADC sync control
  {0x4601,0x04},// /V fifo read start
  {0x4837,0x18},// /MIPI global timing
};
#endif
#if ENABLE_RES3_REGISTER_SETTINGS
static struct msm_camera_i2c_reg_array res3_reg_array[] = {
// @@5.2.11080p  1920*1080 30fps _ 672Mbps
{0x3501, 0x4c},
{0x3632, 0x00},
{0x376b, 0x20},
{0x3800, 0x01},
{0x3801, 0x88},
{0x3803, 0xe0},
{0x3804, 0x09},
{0x3805, 0x17},
{0x3807, 0x1f},
{0x3808, 0x07},
{0x3809, 0x80},
{0x380a, 0x04},
{0x380b, 0x38},
{0x380c, 0x0C},
{0x380d, 0xC0},
{0x380e, 0x04},
{0x380f, 0xce},
{0x3811, 0x08},
{0x3813, 0x04},
{0x3814, 0x01},
{0x3820, 0x00},
{0x3821, 0x06},
{0x382a, 0x01},
{0x3830, 0x04},
{0x3836, 0x01},
{0x4001, 0x40},
{0x4022, 0x06},
{0x4023, 0x13},
{0x4024, 0x07},
{0x4025, 0x40},
{0x4026, 0x07},
{0x4027, 0x50},
{0x4502, 0x40},
{0x4601, 0x77},
};
#endif
static struct msm_camera_i2c_reg_setting res_settings[] = {
#if ENABLE_RES0_REGISTER_SETTINGS
  {
    .reg_setting = res0_reg_array,
    .size = ARRAY_SIZE(res0_reg_array),
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 0,
  },
#endif
#if ENABLE_RES1_REGISTER_SETTINGS
  {
    .reg_setting = res1_reg_array,
    .size = ARRAY_SIZE(res1_reg_array),
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 0,
  },
#endif
#if ENABLE_RES2_REGISTER_SETTINGS
  {
    .reg_setting = res2_reg_array,
    .size = ARRAY_SIZE(res2_reg_array),
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 0,
  },
#endif
#if ENABLE_RES3_REGISTER_SETTINGS
  {
    .reg_setting = res3_reg_array,
    .size = ARRAY_SIZE(res3_reg_array),
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 0,
  },
#endif
};

static struct sensor_lib_reg_settings_array res_settings_array = {
  .reg_settings = res_settings,
  .size = ARRAY_SIZE(res_settings),
};


static struct sensor_crop_parms_t crop_params[] = {
#if ENABLE_RES0_REGISTER_SETTINGS
  {0, 0, 0, 0}, /* RES 0 */
#endif
#if ENABLE_RES1_REGISTER_SETTINGS
  {0, 0, 0, 0}, /* RES 1 */
#endif
#if ENABLE_RES2_REGISTER_SETTINGS
  {0, 0, 0, 0}, /* RES 2 */
#endif
#if ENABLE_RES3_REGISTER_SETTINGS
  {0, 0, 0, 0}, /* RES 3 */
#endif
};

static struct sensor_lib_crop_params_array crop_params_array = {
  .crop_params = crop_params,
  .size = ARRAY_SIZE(crop_params),
};

static struct sensor_lib_out_info_t sensor_out_info[] = {
#if ENABLE_RES0_REGISTER_SETTINGS
  {
    .x_output = 2688,
    .y_output = 1520,
    .line_length_pclk = 1026,   //0x402
    .frame_length_lines = 1580, //0x62c
    .vt_pixel_clk = 120000000,//vt_pixel_clk=line_length_pclk*frame_length_lines*max_fps
    .op_pixel_clk = 420000000,
    .binning_factor = 1,
    .max_fps = 74.0,
    .min_fps = 7.5,
    .mode = SENSOR_HFR_MODE,
  },
#endif
#if ENABLE_RES1_REGISTER_SETTINGS
  {
    .x_output = 2688,
    .y_output = 1520,
    .line_length_pclk = 1284,//0x504
    .frame_length_lines = 1584,//0x630
    .vt_pixel_clk = 120000000,//vt_pixel_clk=line_length_pclk*frame_length_lines*max_fps
    .op_pixel_clk = 268800000,
    .binning_factor = 1,
    .max_fps = 59.0,
    .min_fps = 7.5,
    .mode = SENSOR_HFR_MODE,
  },
#endif
#if ENABLE_RES2_REGISTER_SETTINGS
  {
    .x_output = 2688,
    .y_output = 1520,
    .line_length_pclk = 2498,// 0x9ae 2478 modify by zhaocuiqin 20140805 for CR01321863
    .frame_length_lines = 1634,//0x64e 1614
    .vt_pixel_clk =  120000000,
    .op_pixel_clk =  268800000,
    .binning_factor = 1,
    .max_fps = 29.4, //30,
    .min_fps = 7.5,
    .mode = SENSOR_DEFAULT_MODE,
  },
#endif
#if ENABLE_RES3_REGISTER_SETTINGS
  {
    .x_output = 1920,
    .y_output = 1080,
    .line_length_pclk = 3264,
    .frame_length_lines = 1230,
    .vt_pixel_clk =  120000000,
    .op_pixel_clk =  120000000,
    .binning_factor = 1,
    .max_fps = 30, 
    .min_fps = 7.5,
    .mode = SENSOR_DEFAULT_MODE,
  },
#endif
};

static struct sensor_lib_out_info_array out_info_array = {
  .out_info = sensor_out_info,
  .size = ARRAY_SIZE(sensor_out_info),
};

static sensor_res_cfg_type_t gn_sunny_ov4688_res_cfg[] = {
  SENSOR_SET_STOP_STREAM,
  SENSOR_SET_NEW_RESOLUTION, /* set stream config */
  SENSOR_SET_CSIPHY_CFG,
  SENSOR_SET_CSID_CFG,
  SENSOR_LOAD_CHROMATIX, /* set chromatix prt */
  SENSOR_SEND_EVENT, /* send event */
  SENSOR_SET_START_STREAM,
};

static struct sensor_res_cfg_table_t gn_sunny_ov4688_res_table = {
  .res_cfg_type = gn_sunny_ov4688_res_cfg,
  .size = ARRAY_SIZE(gn_sunny_ov4688_res_cfg),
};

static struct sensor_lib_chromatix_t gn_sunny_ov4688_chromatix[] = {
#if ENABLE_RES0_REGISTER_SETTINGS
  {
    .common_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(common),
    .camera_preview_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(preview), /* RES0 */
    .camera_snapshot_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(preview), /* RES0 */
    .camcorder_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(default_video), /* RES0 */
	.liveshot_chromatix =  GN_SUNNY_OV4688_LOAD_CHROMATIX(liveshot), /* RES0 */
  },
#endif
#if ENABLE_RES1_REGISTER_SETTINGS
  {
    .common_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(common),
    .camera_preview_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(preview), /* RES1 */
    .camera_snapshot_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(preview), /* RES1 */
    .camcorder_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(default_video), /* RES1 */
    .liveshot_chromatix =  GN_SUNNY_OV4688_LOAD_CHROMATIX(liveshot), /* RES1 */
  },
#endif
#if ENABLE_RES2_REGISTER_SETTINGS
  {
    .common_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(common),
    .camera_preview_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(preview), /* RES2 */
    .camera_snapshot_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(preview), /* RES2 */
    .camcorder_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(default_video), /* RES2 */
    .liveshot_chromatix =  GN_SUNNY_OV4688_LOAD_CHROMATIX(liveshot), /* RES2 */
  },
#endif
#if ENABLE_RES3_REGISTER_SETTINGS
  {
    .common_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(common),
    .camera_preview_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(preview), /* RES3 */
    .camera_snapshot_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(preview), /* RES3 */
    .camcorder_chromatix = GN_SUNNY_OV4688_LOAD_CHROMATIX(default_video), /* RES3 */
    .liveshot_chromatix =  GN_SUNNY_OV4688_LOAD_CHROMATIX(liveshot), /* RES3 */
  },
#endif
};

static struct sensor_lib_chromatix_array gn_sunny_ov4688_lib_chromatix_array = {

  .sensor_lib_chromatix = gn_sunny_ov4688_chromatix,
  .size = ARRAY_SIZE(gn_sunny_ov4688_chromatix),
};

/*===========================================================================
 * FUNCTION    - gn_sunny_ov4688_real_to_register_gain -
 *
 * DESCRIPTION:
 *==========================================================================*/
static uint16_t gn_sunny_ov4688_real_to_register_gain(float gain)
{
  uint16_t reg_gain, gain_regh, gain_regl;
  uint16_t i=0;

  if (gain > 10.0) {
	gain = 10.0;
  }
  
  reg_gain = gain * 128; 
  
  if(reg_gain < 128.0) {
      reg_gain = 128.0;
  } else if (reg_gain > 0x7ff) {
      reg_gain = 0x7ff;
  }
  
  if (reg_gain < 256) {
	gain_regh = 0;
	gain_regl = reg_gain;
  } else if (reg_gain < 512) {
	gain_regh = 1;
	gain_regl = reg_gain/2 - 8;
  } else if (reg_gain < 1024) {
	gain_regh = 3;
	gain_regl = reg_gain/4 - 12;
  } else {
	gain_regh = 7;
	gain_regl = reg_gain/8 - 8;
  }
  
  return ((gain_regh<<8) | gain_regl);
}

/*===========================================================================
 * FUNCTION    - gn_sunny_ov4688_register_to_real_gain -
 *
 * DESCRIPTION:
 *==========================================================================*/
static float gn_sunny_ov4688_register_to_real_gain(uint16_t reg_gain)
{
  uint16_t gain_regh, gain_regl;
  float real_gain = 0x80;

  gain_regh = (reg_gain>>8) & 0x07;
  gain_regl = reg_gain & 0xff;
  
  if (gain_regh == 0) {
	real_gain = gain_regl;
  } else if (gain_regh == 1) {
	real_gain = (gain_regl + 8) * 2;
  } else if (gain_regh == 3) {
	real_gain = (gain_regl + 12) * 4;
  } else if (gain_regh == 7) {
	real_gain = (gain_regl + 8) * 8;
  }  

  return (real_gain / 128);
}

/*===========================================================================
 * FUNCTION    - gn_sunny_ov4688_calculate_exposure -
 *
 * DESCRIPTION:
 *==========================================================================*/
static int32_t gn_sunny_ov4688_calculate_exposure(float real_gain,
  uint16_t line_count, sensor_exposure_info_t *exp_info)
{
  if (!exp_info) {
    return -1;
  }
//Gionee <zhuangxiaojian> <2014-04-15> modify for CR01185256 begin
#ifdef ORIGINAL_VERSION
#else
  if (line_count > 6312) {
	line_count = 6312;
  }
#endif
//Gionee <zhuangxiaojian> <2014-04-15> modify for CR01185256 end
  exp_info->reg_gain = gn_sunny_ov4688_real_to_register_gain(real_gain);
  exp_info->sensor_real_gain = gn_sunny_ov4688_register_to_real_gain(exp_info->reg_gain);
  exp_info->digital_gain = real_gain / exp_info->sensor_real_gain;
  exp_info->line_count = line_count;
  exp_info->sensor_digital_gain = 0x1;
  return 0;
}

/*===========================================================================
 * FUNCTION    - gn_sunny_ov4688_fill_exposure_array -
 *
 * DESCRIPTION:
 *==========================================================================*/
static int32_t gn_sunny_ov4688_fill_exposure_array(uint16_t gain, uint32_t line,
  uint32_t fl_lines, int32_t luma_avg, uint32_t fgain,
  struct msm_camera_i2c_reg_setting* reg_setting)
{
  int32_t rc = 0;
  uint16_t reg_count = 0;
  uint16_t i = 0;

  if (!reg_setting) {
    return -1;
  }

  for (i = 0; i < sensor_lib_ptr.groupon_settings->size; i++) {
    reg_setting->reg_setting[reg_count].reg_addr =
      sensor_lib_ptr.groupon_settings->reg_setting[i].reg_addr;
    reg_setting->reg_setting[reg_count].reg_data =
      sensor_lib_ptr.groupon_settings->reg_setting[i].reg_data;
    reg_count = reg_count + 1;
  }

  reg_setting->reg_setting[reg_count].reg_addr =
    sensor_lib_ptr.output_reg_addr->frame_length_lines;
  reg_setting->reg_setting[reg_count].reg_data = (fl_lines & 0xFF00) >> 8;
  reg_count++;

  reg_setting->reg_setting[reg_count].reg_addr =
    sensor_lib_ptr.output_reg_addr->frame_length_lines + 1;
  reg_setting->reg_setting[reg_count].reg_data = (fl_lines & 0xFF);
  reg_count++;

  reg_setting->reg_setting[reg_count].reg_addr =
    sensor_lib_ptr.exp_gain_info->coarse_int_time_addr - 1;
  reg_setting->reg_setting[reg_count].reg_data = ((line >> 12) & 0xFF);
  reg_count++;

  reg_setting->reg_setting[reg_count].reg_addr =
    sensor_lib_ptr.exp_gain_info->coarse_int_time_addr;
  reg_setting->reg_setting[reg_count].reg_data = ((line >> 4) & 0xFF);
  reg_count++;

  reg_setting->reg_setting[reg_count].reg_addr =
    sensor_lib_ptr.exp_gain_info->coarse_int_time_addr + 1;
  reg_setting->reg_setting[reg_count].reg_data = ((line << 4) & 0xFF);
  reg_count++;

  reg_setting->reg_setting[reg_count].reg_addr =
    sensor_lib_ptr.exp_gain_info->global_gain_addr;
  reg_setting->reg_setting[reg_count].reg_data = (gain & 0xFF00) >> 8;
  reg_count++;

  reg_setting->reg_setting[reg_count].reg_addr =
    sensor_lib_ptr.exp_gain_info->global_gain_addr + 1;
  reg_setting->reg_setting[reg_count].reg_data = (gain & 0xFF);
  reg_count++;

  for (i = 0; i < sensor_lib_ptr.groupoff_settings->size; i++) {
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
static sensor_exposure_table_t gn_sunny_ov4688_expsoure_tbl = {
  .sensor_calculate_exposure = gn_sunny_ov4688_calculate_exposure,
  .sensor_fill_exposure_array = gn_sunny_ov4688_fill_exposure_array,
};

static sensor_lib_t sensor_lib_ptr = {
  /* sensor slave info */
  .sensor_slave_info = &sensor_slave_info,
  /* sensor init params */
  .sensor_init_params = &sensor_init_params,
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
  /* to avoid random flashing after snapshot,  tanrifei, 20140317*/
  #if 0
  .sensor_num_frame_skip = 2,
  #else
  .sensor_num_frame_skip = 4,
  #endif
	/* modify end */
  /* number of frames to skip after start HDR stream */
  .sensor_num_HDR_frame_skip = 2,
  /* sensor pipeline immediate delay */
  .sensor_max_pipeline_frame_delay = 2,
	
  /* sensor exposure table size */
  .exposure_table_size = 10,//liushengbin modify
  /* sensor lens info */
  .default_lens_info = &default_lens_info,
  /* csi lane params */
  .csi_lane_params = &csi_lane_params,
  /* csi cid params */
  .csi_cid_params = gn_sunny_ov4688_cid_cfg,
  /* csi csid params array size */
  .csi_cid_params_size = ARRAY_SIZE(gn_sunny_ov4688_cid_cfg),
  /* init settings */
  .init_settings_array = &init_settings_array,
  //Gionee liushengbin 20131108 modify for otp
  .init_inform_kernel = 1,
//Gionee <zhuangxiaojian> <2014-03-12> modify for CR01090346 begin
#ifdef ORIGINAL_VERSION
  /* start settings */
  .start_settings = &start_settings,
  /* stop settings */
  .stop_settings = &stop_settings,
#else
  /* setting flag: whether use private setting */
  .private_cfg = 1,
  /* private start settings */
  .priv_start_settings = &start_setting_array,
  /* private stop settings */
  .priv_stop_settings = &stop_setting_array,
#endif
//Gionee <zhuangxiaojian> <2014-03-12> modify CR01090346 end
  /* group on settings */
  .groupon_settings = &groupon_settings,
  /* group off settings */
  .groupoff_settings = &groupoff_settings,
  /* resolution cfg table */
  .sensor_res_cfg_table = &gn_sunny_ov4688_res_table,
  /* res settings */
  .res_settings_array = &res_settings_array,
  /* out info array */
  .out_info_array = &out_info_array,
  /* crop params array */
  .crop_params_array = &crop_params_array,
  /* csi params array */
  .csi_params_array = &csi_params_array,
  /* sensor port info array */
  .sensor_stream_info_array = &gn_sunny_ov4688_stream_info_array,
  /* exposure funtion table */
  .exposure_func_table = &gn_sunny_ov4688_expsoure_tbl,
  /* chromatix array */
  .chromatix_array = &gn_sunny_ov4688_lib_chromatix_array,
  .sensor_max_immediate_frame_delay = 2,
};

/*===========================================================================
 * FUNCTION    - gn_sunny_ov4688_open_lib -
 *
 * DESCRIPTION:
 *==========================================================================*/
void *gn_sunny_ov4688_open_lib(void)
{
  return &sensor_lib_ptr;
}
