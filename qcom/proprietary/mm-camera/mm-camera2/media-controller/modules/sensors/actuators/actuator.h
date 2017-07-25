/*============================================================================

  Copyright (c) 2012-2014 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/
#ifndef __ACTUATOR_H__
#define __ACTUATOR_H__

#define ACTUATOR_NUM_MODES_MAX 4

typedef struct {
  af_tune_parms_t af_tune;
} actuator_ctrl_t;

typedef struct {
  int32_t fd;
  actuator_ctrl_t *ctrl;
  int16_t curr_step_pos;  
  /* add by tanrifei, 20140829 */
  int16_t curr_3a_step_pos;
  /* add end */
  int16_t cur_restore_pos;
  uint16_t total_steps;
  uint8_t is_af_supported;
  uint8_t cam_name;
  uint8_t load_params;
  uint16_t curr_lens_pos;
  char *name;
  uint8_t params_loaded;
  void* lib_handle[ACTUATOR_NUM_MODES_MAX];
  actuator_ctrl_t* lib_data[ACTUATOR_NUM_MODES_MAX];
} actuator_data_t;

typedef struct {
  uint8_t af_support;
  af_tune_parms_t *af_tune_ptr;
} actuator_get_t;

#endif
