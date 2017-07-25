/* actuator.c
 *
 * Copyright (c) 2012-2014 Qualcomm Technologies, Inc. All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */

#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <media/msmb_camera.h>
#include <dlfcn.h>
#include "mct_event_stats.h"
#include "sensor_common.h"
#include "af_tuning.h"
#include "actuator.h"

/** af_actuator_set_default_focus: function to move lens to
 *  infinity position
 *
 *  @ptr: pointer to actuator_data_t struct
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function moves lens to infinity position **/

static int af_actuator_set_default_focus(void *ptr)
{
  int rc = 0;
  actuator_data_t *af_actuator_ptr = (actuator_data_t *)ptr;
  struct msm_actuator_cfg_data cfg;
  af_tune_parms_t *af_tune_ptr = &(af_actuator_ptr->ctrl->af_tune);
  uint16_t curr_scene = 0;
  uint16_t scenario_size = 0;
  uint16_t index = 0;

  if (af_actuator_ptr->fd <= 0)
    return -EINVAL;

  cfg.cfgtype = CFG_SET_DEFAULT_FOCUS;
  cfg.cfg.move.dir = MOVE_FAR;
  cfg.cfg.move.sign_dir = -1;
  cfg.cfg.move.num_steps = af_actuator_ptr->curr_step_pos;
  cfg.cfg.move.dest_step_pos = 0;
  curr_scene = 0;
  /* Determine scenario */
  scenario_size = af_tune_ptr->actuator_tuned_params.
    scenario_size[MOVE_FAR];
  for (index = 0; index < scenario_size; index++) {
    if (af_actuator_ptr->curr_step_pos <=
      af_tune_ptr->actuator_tuned_params.
      ringing_scenario[MOVE_FAR][index]) {
      curr_scene = index;
      break;
    }
  }
  cfg.cfg.move.ringing_params =
    &(af_tune_ptr->actuator_tuned_params.
    damping[MOVE_FAR][curr_scene].ringing_params[0]);

  SLOW("dir:%d, steps:%d", cfg.cfg.move.dir, cfg.cfg.move.num_steps);

  /* Invoke the IOCTL to set the default focus */
  rc = ioctl(af_actuator_ptr->fd, VIDIOC_MSM_ACTUATOR_CFG, &cfg);
  if (rc < 0) {
    SERR("failed");
  }

  af_actuator_ptr->curr_step_pos = 0;
  return rc;
}

/** af_actuator_move_focus: function to move lens to desired
 *  position
 *
 *  @ptr: pointer to actuator_data_t struct
 *  @data: pointer to af_update_t struct
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function moves lens to desired position as dictated
 *  by 3A algorithm **/

static int af_actuator_move_focus(void *ptr, void *data)
{
  int rc = 0;
  actuator_data_t *af_actuator_ptr = (actuator_data_t *)ptr;
  struct msm_actuator_cfg_data cfg;
  af_tune_parms_t *af_tune_ptr = &(af_actuator_ptr->ctrl->af_tune);
  af_update_t *af_update = (af_update_t *)data;
  uint16_t scenario_size = 0;
  uint16_t index = 0;
  uint16_t curr_scene = 0;
  int16_t dest_step_pos = 0;
  int8_t sign_dir = 0;
  int32_t direction;
  int32_t num_steps;

  SLOW("Enter");
  if (af_actuator_ptr->fd <= 0 || !data) {
    SERR("failed fd %d data %p", af_actuator_ptr->fd, data);
    return -EINVAL;
  }

  if (af_update->reset_lens == TRUE) {
    SLOW("calling af_actuator_set_default_focus");
    rc = af_actuator_set_default_focus(ptr);
    if (rc < 0) {
      SERR("failed rc %d",  rc);
    }
    return rc;
  }

  if (af_update->move_lens != TRUE) {
    SERR("error");
    return rc;
  }
  num_steps = af_update->num_of_steps;
  direction = af_update->direction;

  SLOW("num steps %d dir %d",  num_steps, direction);
  if (direction == MOVE_NEAR)
    sign_dir = 1;
  else if (direction == MOVE_FAR)
    sign_dir = -1;

  dest_step_pos = af_actuator_ptr->curr_step_pos +
    (sign_dir * num_steps);

  if (dest_step_pos < 0)
    dest_step_pos = 0;
  else if (dest_step_pos > af_actuator_ptr->total_steps)
    dest_step_pos = af_actuator_ptr->total_steps;

  cfg.cfgtype = CFG_MOVE_FOCUS;
  cfg.cfg.move.dir = direction;
  cfg.cfg.move.sign_dir = sign_dir;
  cfg.cfg.move.num_steps = num_steps;
  cfg.cfg.move.dest_step_pos = dest_step_pos;
  curr_scene = 0;
  /* Determine scenario */
  scenario_size = af_tune_ptr->actuator_tuned_params.scenario_size[direction];
  for (index = 0; index < scenario_size; index++) {
    if (num_steps <=
      af_tune_ptr->actuator_tuned_params.ringing_scenario[direction][index]) {
      curr_scene = index;
      break;
    }
  }
  cfg.cfg.move.ringing_params =
    &(af_tune_ptr->actuator_tuned_params.
    damping[direction][curr_scene].ringing_params[0]);

  SLOW("dir:%d, steps:%d", cfg.cfg.move.dir, cfg.cfg.move.num_steps);

  /* Invoke the IOCTL to move the focus */
  rc = ioctl(af_actuator_ptr->fd, VIDIOC_MSM_ACTUATOR_CFG, &cfg);
  if (rc < 0) {
    SERR("failed rc %d", rc);
  }
  af_actuator_ptr->curr_step_pos = dest_step_pos;
  af_actuator_ptr->curr_lens_pos = cfg.cfg.move.curr_lens_pos;

  SLOW("Exit");
  return rc;
}

//gionee zhaocuiqin add for ois mode begin 20140626
static int af_actuator_set_oismode(void *ptr, void *data)
{
  int rc = 0;
  struct msm_actuator_cfg_data cfg;
  actuator_data_t *af_actuator_ptr = (actuator_data_t *)ptr;
  int32_t *oismode = (int32_t*)data;
  SLOW("oismode = %d", *oismode);

  cfg.cfgtype = CFG_SET_OIS_MODE;
  cfg.cfg.ois_mode = *oismode;

  /* Invoke the IOCTL to move the focus */
  //rc = ioctl(af_actuator_ptr->fd, VIDIOC_MSM_ACTUATOR_CFG, &cfg);
  if (rc < 0) {
    SERR("failed rc %d", rc);
  }

  SLOW("Exit");
  return rc;
}
//gionee zhaocuiqin add for ois mode end 20140626

/** af_actuator_restore_focus: function to move lens to desired
 *  position
 *
 *  @ptr: pointer to actuator_data_t struct
 *  @data: pointer to af_update_t struct
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function moves lens to desired position as dictated
 *  by 3A algorithm **/

static int af_actuator_restore_focus(void *ptr, int32_t direction)
{
  int rc = 0;
  int16_t new_restore_pos = 0;
  actuator_data_t *af_actuator_ptr = (actuator_data_t *)ptr;
  uint8_t af_restore =
    af_actuator_ptr->ctrl->af_tune.actuator_params.af_restore_pos;
  af_update_t af_update;
  if (af_restore) {
    if (direction == MOVE_NEAR) {
      new_restore_pos = af_actuator_ptr->cur_restore_pos;
    } else if (direction == MOVE_FAR) {
      new_restore_pos = af_actuator_ptr->curr_step_pos;
      af_actuator_ptr->cur_restore_pos = af_actuator_ptr->curr_step_pos;
    }
    SLOW("dir:%d,steps:%d", direction, new_restore_pos);
    af_update.direction = direction;
    af_update.num_of_steps = new_restore_pos;
    rc = af_actuator_move_focus(ptr, &af_update);
  }
  return rc;
}

/** af_actuator_get_info: function to return whether af is
 *  supported
 *
 *  @ptr: pointer to actuator_data_t struct
 *  @data: pointer to uint8_t
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function returns 1 if af is supported, 0 otherwise **/

static int af_actuator_get_info(void *ptr, void *data)
{
  int rc = 0;
  actuator_data_t *af_actuator_ptr = (actuator_data_t *)ptr;
  af_tune_parms_t *af_tune_ptr = &(af_actuator_ptr->ctrl->af_tune);
  uint8_t *af_support = (uint8_t *)data;
  if (!af_support) {
    SERR("failed");
    return -EINVAL;
  }
  *af_support = af_actuator_ptr->is_af_supported;
  return rc;
}

/** af_actuator_load_params: loads the header params to the
 *  af driver
 *
 *  @ptr: pointer to actuator_data_t struct
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function returns 1 if af is supported, 0 otherwise **/

static int af_actuator_load_params(void *ptr)
{
  int rc = 0;
  struct msm_actuator_cfg_data cfg;
  actuator_data_t *af_actuator_ptr = (actuator_data_t *)ptr;
  uint8_t cnt = 0;
  uint16_t total_steps = 0;
  af_tune_parms_t *af_tune_ptr = &(af_actuator_ptr->ctrl->af_tune);
  actuator_tuned_params_t *actuator_tuned_params = NULL;
  actuator_params_t *actuator_params = NULL;

  if (af_actuator_ptr->is_af_supported) {
    actuator_tuned_params = &af_tune_ptr->actuator_tuned_params;
    actuator_params = &af_tune_ptr->actuator_params;

    SERR("E");
    cfg.cfgtype = CFG_SET_ACTUATOR_INFO;
    total_steps = af_tune_ptr->af_algo.position_far_end + 1;
    total_steps += af_tune_ptr->af_algo.undershoot_adjust;

    af_actuator_ptr->total_steps = total_steps;
    cfg.cfg.set_info.af_tuning_params.total_steps = total_steps;
    cfg.cfg.set_info.actuator_params.act_type =
      actuator_params->act_type;
    cfg.cfg.set_info.af_tuning_params.pwd_step =
      actuator_tuned_params->region_params[0].step_bound[1];
    cfg.cfg.set_info.af_tuning_params.initial_code =
      actuator_tuned_params->initial_code;
    cfg.cfg.set_info.actuator_params.reg_tbl_size =
      actuator_params->reg_tbl.reg_tbl_size;
    cfg.cfg.set_info.actuator_params.reg_tbl_params =
      &(actuator_params->reg_tbl.reg_params[0]);
    cfg.cfg.set_info.actuator_params.data_size =
      actuator_params->data_size;
    cfg.cfg.set_info.actuator_params.i2c_addr =
      actuator_params->i2c_addr;
    cfg.cfg.set_info.actuator_params.i2c_addr_type =
      actuator_params->i2c_addr_type;

    cfg.cfg.set_info.af_tuning_params.region_size =
      actuator_tuned_params->region_size;
    cfg.cfg.set_info.af_tuning_params.region_params =
      &(actuator_tuned_params->region_params[0]);
    cfg.cfg.set_info.actuator_params.init_setting_size =
      actuator_params->init_setting_size;
    cfg.cfg.set_info.actuator_params.i2c_data_type =
      actuator_params->i2c_data_type;
    cfg.cfg.set_info.actuator_params.init_settings =
      &(actuator_params->init_settings[0]);

    /* Invoke the IOCTL to set the af parameters to the kernel driver */
    rc = ioctl(af_actuator_ptr->fd, VIDIOC_MSM_ACTUATOR_CFG, &cfg);
    if (rc < 0) {
      SERR("failed rc %d", rc);
    }
  }

  return rc;
}

/** af_actuator_init: function to initialize actuator
 *
 *  @ptr: pointer to actuator_data_t struct
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function checks whether actuator is supported, gets
 *  cam name index and initializes actuator control pointer **/

static int af_actuator_init(void *ptr, void* data)
{
  int rc = 0;
  actuator_data_t *af_actuator_ptr = (actuator_data_t *)ptr;
  char *name = (char *)data;
  struct msm_actuator_cfg_data cfg;

  if (af_actuator_ptr == NULL) {
    SERR("Invalid Argument - af_actuator_ptr");
    return -EINVAL;
  }

  SHIGH("name = %s", (name) ? name : "null");

  af_actuator_ptr->ctrl = NULL;
  af_actuator_ptr->curr_step_pos = 0;
  af_actuator_ptr->cur_restore_pos = 0;
  af_actuator_ptr->name = name;
  af_actuator_ptr->is_af_supported = (name == NULL) ? 0 : 1;
  af_actuator_ptr->params_loaded = 0;
  int i;
  for (i=0; i<ACTUATOR_NUM_MODES_MAX; i++) {
    af_actuator_ptr->lib_handle[i] = NULL;
    af_actuator_ptr->lib_data[i] = NULL;
  }
  return rc;
}

/** af_load_header: function to load the actuator header
 *
 *  @ptr: pointer to actuator_data_t struct
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function gets cam name index and initializes actuator
 *  control pointer **/

static int actuator_load_lib(void *ptr, actuator_cam_mode_t cam_mode)
{
  int rc = 0;
  actuator_data_t *af_actuator_ptr = (actuator_data_t *)ptr;

  if (af_actuator_ptr == NULL) {
    SERR("Invalid Argument - af_actuator_ptr");
    return -EINVAL;
  }
  if (af_actuator_ptr->is_af_supported) {
    char *mode_str;
    void *(*open_lib_func)(void) = NULL;
    char open_lib_func_name[64];
    char lib_name[64];

    if (cam_mode == ACTUATOR_CAM_MODE_CAMERA) {
      mode_str = "camera";
    } else if (cam_mode == ACTUATOR_CAM_MODE_CAMCORDER) {
      mode_str = "camcorder";
    } else {
      SERR("failed, invalid mode=%d", cam_mode);
    }
    if (!af_actuator_ptr->name) {
      SERR("failed, actuator_name=NULL");
      return -EINVAL;
    }
    SERR("name=%s, mode=%s", af_actuator_ptr->name, mode_str);

    if (af_actuator_ptr->lib_handle[cam_mode] == NULL) {
      snprintf(lib_name, 64, "libactuator_%s_%s.so",
        af_actuator_ptr->name, mode_str);
      snprintf(open_lib_func_name, 64, "%s_%s_open_lib",
        af_actuator_ptr->name, mode_str);

      SLOW("loading lib=%s", lib_name);
      af_actuator_ptr->lib_handle[cam_mode] = dlopen(lib_name, RTLD_NOW);
      if (!af_actuator_ptr->lib_handle[cam_mode]) {
        SERR("dlopen() failed to load %s", lib_name);
        return -EINVAL;
      }
      *(void **)&open_lib_func = dlsym(af_actuator_ptr->lib_handle[cam_mode],
                                   open_lib_func_name);
      if (!open_lib_func) {
        SERR("dlsym() failed");
        return -EINVAL;
      }
      af_actuator_ptr->lib_data[cam_mode] = (actuator_ctrl_t *)open_lib_func();
      if (!af_actuator_ptr->lib_data[cam_mode]) {
        SERR("open_lib_func() failed");
        return -EFAULT;
      }
      SHIGH("library %s successfully loaded, idx=%d", lib_name, cam_mode);
    }
    af_actuator_ptr->ctrl = af_actuator_ptr->lib_data[cam_mode];
    if (af_actuator_ptr->params_loaded == 0) {
//add by gionee zhaocq for camera CR01246208 AF begin
#ifdef ORIGINAL_VERSION
      af_actuator_load_params(ptr);
      af_actuator_ptr->params_loaded == 1;
#else
      //af_actuator_load_params(ptr);
      af_actuator_ptr->params_loaded = 1;
#endif
//add by gionee zhaocq for camera CR01246208 AF endif
    }
  }
  return rc;
}

/** af_actuator_linear_test: function for linearity test
 *
 *  @ptr: pointer to actuator_data_t struct
 *  @stepsize: step size for linearity test
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function runs linearity test by moving alternatively in
 *  both direction with above mentioned step size **/

static int af_actuator_linear_test(void *ptr, uint8_t stepsize)
{
  int rc = 0;
  actuator_data_t *af_actuator_ptr = (actuator_data_t *)ptr;
  actuator_ctrl_t *ctrl = af_actuator_ptr->ctrl;
  uint8_t index;
  af_update_t af_update;
  SLOW("set default focus");
  rc = af_actuator_set_default_focus(ptr);
  usleep(1000000);

  SLOW("linear test MOVE_NEAR");
  for (index = 0; index < af_actuator_ptr->total_steps;
    index+=stepsize) {
    af_update.move_lens = TRUE;
    af_update.direction = MOVE_NEAR;
    af_update.num_of_steps = stepsize;
    rc = af_actuator_move_focus(ptr, &af_update);
    usleep(1000000);
  }

  SLOW("linear test MOVE_FAR");
  for (index = 0; index < af_actuator_ptr->total_steps;
    index+=stepsize) {
    af_update.move_lens = TRUE;
    af_update.direction = MOVE_FAR;
    af_update.num_of_steps = stepsize;
    rc = af_actuator_move_focus(ptr, &af_update);
    usleep(1000000);
  }
  return rc;
}

/** af_actuator_ring_test: function for ringing test
 *
 *  @ptr: pointer to actuator_data_t struct
 *  @stepsize: step size for linearity test
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function runs ringing test by moving lens from macro
 *  position to infintity position **/

static int af_actuator_ring_test(void *ptr, uint8_t stepsize)
{
  int rc = 0;
  actuator_data_t *af_actuator_ptr = (actuator_data_t *)ptr;
  actuator_ctrl_t *ctrl = af_actuator_ptr->ctrl;
  uint8_t index;
  af_update_t af_update;

  rc = af_actuator_set_default_focus(ptr);
  usleep(1000000);

  for (index = 0; index < af_actuator_ptr->total_steps;
    index+=stepsize) {
    af_update.direction = MOVE_NEAR;
    af_update.num_of_steps = stepsize;
    rc = af_actuator_move_focus(ptr, &af_update);
    usleep(60000);
  }

  rc = af_actuator_set_default_focus(ptr);
  usleep(1000000);

  return rc;
}

/** actuator_get_af_tune_ptr: function to return af tuned
 *  pointer
 *
 *  @ptr: pointer to actuator_data_t struct
 *  @data: pointer to af_tune_parms_t *
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function runs ringing test by moving lens from macro
 *  position to infintity position **/

static int actuator_get_af_tune_ptr(void *ptr, void *data)
{
  actuator_data_t *af_actuator_ptr = (actuator_data_t *)ptr;
  af_tune_parms_t **af_tune = (af_tune_parms_t **)data;
  if (!af_actuator_ptr || !af_tune) {
    SERR("failed af_actuator_ptr %p af_tune %p", af_actuator_ptr, af_tune);
    return -EINVAL;
  }
  *af_tune = &af_actuator_ptr->ctrl->af_tune;
  return 0;
}


/** actuator_set_position: function to move lens to desired of
 *  positions with delay
 *
 *  @ptr: pointer to actuator_data_t struct
 *  @data: pointer to af_update_t struct
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function moves lens to set of desired positions as
 *  dictated by 3A algorithm **/

static int actuator_set_position(void *ptr, void *data) {
  int rc = 0;
  int index;
  actuator_data_t *af_actuator_ptr = (actuator_data_t *)ptr;
  struct msm_actuator_cfg_data cfg;
  af_update_t *af_update = (af_update_t *)data;
  if (af_actuator_ptr->fd <= 0)
    return -EINVAL;

  cfg.cfgtype = CFG_SET_POSITION;
  cfg.cfg.setpos.number_of_steps = af_update->num_of_interval;
  for (index = 0; index < cfg.cfg.setpos.number_of_steps; index++) {
     cfg.cfg.setpos.pos[index] = af_update->pos[index];
     cfg.cfg.setpos.delay[index] = af_update->delay[index];
     SLOW("pos:%d, delay:%d\n", cfg.cfg.setpos.pos[index],
       cfg.cfg.setpos.delay[index]);
  }

  /* Invoke the IOCTL to set the positions */
  rc = ioctl(af_actuator_ptr->fd, VIDIOC_MSM_ACTUATOR_CFG, &cfg);
  if (rc < 0) {
    SERR("failed");
  }

  return rc;
}
/** actuator_open: function for actuator open
 *
 *  @ptr: pointer to actuator_data_t *
 *  @data: pointer to subdevice name
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function open subdevice and initializes actuator **/

static int32_t actuator_open(void **actuator_ctrl, const char *subdev_name)
{
  int32_t rc = 0;
  actuator_data_t *ctrl = NULL;
  struct msm_actuator_cfg_data cfg;
  char subdev_string[32];

  if (!actuator_ctrl || !subdev_name) {
    SERR("failed sctrl %p subdev name %p", actuator_ctrl, subdev_name);
    return -EINVAL;
  }
  ctrl = malloc(sizeof(actuator_data_t));
  if (!ctrl) {
    SERR("failed");
    return -EINVAL;
  }
  memset(ctrl, 0, sizeof(actuator_data_t));

  snprintf(subdev_string, sizeof(subdev_string), "/dev/%s", subdev_name);
  /* Open subdev */
  ctrl->fd = open(subdev_string, O_RDWR);
  if (ctrl->fd < 0) {
    SERR("failed");
    rc = -EINVAL;
    goto ERROR;
  }
  ctrl->load_params = 1;
  *actuator_ctrl = (void *)ctrl;
  return rc;

ERROR:
  free(ctrl);
  return rc;
}

/** actuator_set_af_tuning: function to perform af tuning
 *
 *  @ptr: pointer to actuator_data_t struct
 *  @data: pointer to tune_actuator_t *
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function runs different tuning test for actuator
 *  tuning based on the input parameters **/

static int actuator_set_af_tuning(void *actuator_ctrl, void *data)
{
  int rc = 0;
  tune_actuator_t *tdata = (tune_actuator_t *)data;
  actuator_tuning_type_t ttype = (actuator_tuning_type_t)tdata->ttype;
  SERR("ttype =%d tdata->stepsize=%d", ttype, tdata->stepsize);
  switch (ttype) {
  case ACTUATOR_TUNE_RELOAD_PARAMS:
    rc = af_actuator_load_params(actuator_ctrl);
    break;
  case ACTUATOR_TUNE_TEST_LINEAR:
    rc = af_actuator_linear_test(actuator_ctrl, tdata->stepsize);
    break;
  case ACTUATOR_TUNE_TEST_RING:
    rc = af_actuator_ring_test(actuator_ctrl, tdata->stepsize);
    break;
  case ACTUATOR_TUNE_DEF_FOCUS:
    rc = af_actuator_set_default_focus(actuator_ctrl);
    break;
  case ACTUATOR_TUNE_MOVE_FOCUS: {
    af_update_t movedata;
    movedata.move_lens = TRUE;
    movedata.reset_lens = FALSE;
    movedata.direction = tdata->direction;
    movedata.num_of_steps = tdata->num_steps;
    rc = af_actuator_move_focus(actuator_ctrl, &movedata);
    }
    break;
  }
  return rc;
}

/** actuator_process: function to drive actuator config
 *
 *  @ptr: pointer to actuator_data_t
 *  @data: pointer to data sent by other modules
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function calls corresponding config function based on
 *  event type **/

static int32_t actuator_process(void *actuator_ctrl,
  sensor_submodule_event_type_t event, void *data)
{
  int32_t rc = 0;
  if (!actuator_ctrl) {
    SERR("failed");
    return -EINVAL;
  }
  switch (event) {
  /* Set params */
  case ACTUATOR_INIT:
    rc = af_actuator_init(actuator_ctrl, data);
    break;
  case ACTUATOR_MOVE_FOCUS:
    rc = af_actuator_move_focus(actuator_ctrl, data);
    break;
  case ACTUATOR_LOAD_LIB:
    rc = actuator_load_lib(actuator_ctrl, *((actuator_cam_mode_t*)data));
    break;
  case ACTUATOR_SET_PARAMETERS: {
    actuator_data_t *af_actuator_ptr = (actuator_data_t *)actuator_ctrl;
    if (af_actuator_ptr->load_params) {
      af_actuator_load_params(actuator_ctrl);
      af_actuator_ptr->load_params = 0;
    }
    break;
  }
  case ACTUATOR_FOCUS_TUNING:
    SERR("ACTUATOR_FOCUS_TUNING");
    rc = actuator_set_af_tuning(actuator_ctrl, data);
    break;
    /* Get params */
  case ACTUATOR_GET_AF_TUNE_PTR:
    rc = actuator_get_af_tune_ptr(actuator_ctrl, data);
    break;
  case ACTUATOR_GET_DAC_VALUE: {
    uint32_t *dac_value = (uint32_t *)data;
    actuator_data_t *af_actuator_ptr = (actuator_data_t *)actuator_ctrl;
    *dac_value = af_actuator_ptr->curr_lens_pos;
    break;
  }
  /* Set position */
  case ACTUATOR_SET_POSITION:
    rc = actuator_set_position(actuator_ctrl, data);
    break;
//Gionee <zhuangxiaojian> <2014-06-26> modify for CR01310542 begin
#ifdef ORIGINAL_VERSION
#else
  case ACTUATOR_SET_OIS_MODE:
  	rc = af_actuator_set_oismode(actuator_ctrl, data);
  	break;
#endif
//Gionee <zhuangxiaojian> <2014-06-26> modify for CR01310542 end
  default:
    SERR("invalid event %d",  event);
    rc = -EINVAL;
    break;
  }
  if (rc < 0) {
    SERR("failed rc %d",  rc);
  }
  return rc;
}

/** actuator_close: function for actuator close
 *
 *  @ptr: pointer to actuator_data_t
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function calls close file description and frees all
 *  control data **/

static int32_t actuator_close(void *actuator_ctrl)
{
  int32_t rc = 0;
  actuator_data_t *ctrl = (actuator_data_t *)actuator_ctrl;
  struct msm_actuator_cfg_data cfg;
  struct msm_camera_csi_lane_params csi_lane_params;

  /* unload libs */
  int i;
  for (i=0; i<ACTUATOR_NUM_MODES_MAX; i++) {
   if (ctrl->lib_handle[i]) {
    dlclose(ctrl->lib_handle[i]);
    ctrl->lib_handle[i] = NULL;
   }
  }
  /* close subdev */
  close(ctrl->fd);

  free(ctrl);
  return rc;
}

/** actuator_sub_module_init: function for initializing actuator
 *  sub module
 *
 *  @ptr: pointer to sensor_func_tbl_t
 *
 *  Return: 0 for success and negative error on failure
 *
 *  This function initializes sub module function table with
 *  actuator specific functions **/

int32_t actuator_sub_module_init(sensor_func_tbl_t *func_tbl)
{
  SLOW("Enter");
  if (!func_tbl) {
    SERR("failed");
    return -EINVAL;
  }
  func_tbl->open = actuator_open;
  func_tbl->process = actuator_process;
  func_tbl->close = actuator_close;
  return 0;
}
