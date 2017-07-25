/* is_interface.h
 *
 * Copyright (c) 2013 Qualcomm Technologies, Inc. All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */

#ifndef __IS_INTERFACE_H__
#define __IS_INTERFACE_H__


#include "modules.h"


typedef enum {
  DIS,
  GA_DIS,
  EIS_1,
  EIS_2
} is_mode_t;

/** rs_cs_config_t
 *    @num_row_sum: number of row sums
 *    @num_col_sum: number of column sums
 */
typedef struct {
  unsigned long num_row_sum;
  unsigned long num_col_sum;
} rs_cs_config_t;

typedef struct {
  int32_t  dis_frame_width;   /* Original */
  int32_t  dis_frame_height;  /* Original */
  int32_t  vfe_output_width;  /* Padded */
  int32_t  vfe_output_height; /* Padded */
  uint16_t frame_fps;         /* image fps */
} frame_cfg_t;

typedef struct {
  unsigned long long sof;
  unsigned long long frame_time;
  float exposure_time;
} frame_times_t;

typedef struct {
  frame_cfg_t frame_cfg;
  rs_cs_config_t rs_cs_config;
  is_mode_t is_mode;
  unsigned int sensor_mount_angle;
  enum camb_position_t camera_position;
} is_init_data_t;

typedef struct {
  unsigned long frame_id;
  int has_output;
  int x;
  int y;
  int prev_dis_output_x;
  int prev_dis_output_y;
  unsigned long eis_output_valid;
  int eis_output_x;
  int eis_output_y;

  float transform_matrix[9];

  /* Time interval used to calculate EIS offsets */
  unsigned long long t_start;
  unsigned long long t_end;
} is_output_type;

#endif /* _IS_INTERFACE_H_ */
