/***************************************************************************
* Copyright (c) 2013-2014 Qualcomm Technologies, Inc. All Rights Reserved. *
* Qualcomm Technologies Proprietary and Confidential.                      *
****************************************************************************/

#include "module_imgbase.h"

/** UBIFOCUS_BURST_CNT:
 *
 *  Burst count
 **/
#define UBIFOCUS_BURST_CNT 5

/** UBIFOCUS_REFOCUS:
 *
 *  indicate if refocus is enabled
 **/
static int g_c_ubi_refocus = 1;

/**
 *  Static functions
 **/
static boolean module_ubifocus_query_mod(mct_pipeline_cap_t *p_mct_cap);
static boolean module_ubifocus_init_params(img_init_params_t *p_params);

/** g_focus_steps:
 *
 *  Focus steps
 **/
uint8_t g_focus_steps[MAX_AF_BRACKETING_VALUES] =
  {1, 1, 1, 1, 1};

/** g_caps:
 *
 *  Set the capabilities for ubifocus module
 **/
static img_caps_t g_caps = {
  UBIFOCUS_BURST_CNT,/*num_input*/
  1,/*num_output*/
  1,/*num_meta*/
  0,/*inplace_algo*/
};

/** g_params:
 *
 *  imgbase parameters
 **/
static module_imgbase_params_t g_params = {
  module_ubifocus_query_mod,
  module_ubifocus_init_params,
};

/**
 * Function: module_ubifocus_get_refocus
 *
 * Description: This function is used to check whether refocus
 *            is enabled
 *
 * Arguments:
 *   none
 *
 * Return values:
 *     true/false
 *
 * Notes: none
 **/
boolean module_ubifocus_get_refocus()
{
  char prop[PROPERTY_VALUE_MAX];
  memset(prop, 0, sizeof(prop));
  property_get("persist.camera.imglib.refocus", prop, "0");
  int enable_refocus = atoi(prop);
  return enable_refocus == 1;
}

/**
 * Function: module_ubifocus_init_params
 *
 * Description: This function is used to init parameters
 *
 * Arguments:
 *   p_params - ubifocus init params
 *
 * Return values:
 *     true/false
 *
 * Notes: none
 **/
boolean module_ubifocus_init_params(img_init_params_t *p_params)
{
  boolean ret = FALSE;
  if (p_params) {
    IDBG_HIGH("%s:%d] refocus %d", __func__, __LINE__,
      g_c_ubi_refocus);
    p_params->refocus_encode = g_c_ubi_refocus;
    ret = TRUE;
  }
  return ret;
}

/**
 * Function: module_ubifocus_deinit
 *
 * Description: This function is used to deinit ubifocus
 *               module
 *
 * Arguments:
 *   p_mct_mod - MCTL module instance pointer
 *
 * Return values:
 *     none
 *
 * Notes: none
 **/
void module_ubifocus_deinit(mct_module_t *p_mct_mod)
{
  module_imgbase_deinit(p_mct_mod);
}

/**
 * Function: module_ubifocus_query_mod
 *
 * Description: This function is used to query ubifocus
 *               caps
 *
 * Arguments:
 *   p_mct_cap - capababilities
 *
 * Return values:
 *     true/false
 *
 * Notes: none
 **/
boolean module_ubifocus_query_mod(mct_pipeline_cap_t *p_mct_cap)
{
  mct_pipeline_imaging_cap_t *buf;

  if (!p_mct_cap) {
    IDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    return FALSE;
  }

  buf = &p_mct_cap->imaging_cap;
  buf->ubifocus_af_bracketing.burst_count = UBIFOCUS_BURST_CNT;
  buf->ubifocus_af_bracketing.enable = TRUE;
  buf->ubifocus_af_bracketing.output_count =
    (g_c_ubi_refocus) ? UBIFOCUS_BURST_CNT + 1 : 1;
  memcpy(&buf->ubifocus_af_bracketing.focus_steps, &g_focus_steps,
    sizeof(g_focus_steps));
  return TRUE;
}

/** module_ubifocus_init:
 *
 *  Arguments:
 *  @name - name of the module
 *
 * Description: This function is used to initialize the ubifocus
 * module
 *
 * Return values:
 *     MCTL module instance pointer
 *
 * Notes: none
 **/
mct_module_t *module_ubifocus_init(const char *name)
{
  g_c_ubi_refocus = module_ubifocus_get_refocus();
  return module_imgbase_init(name,
    IMG_COMP_GEN_FRAME_PROC,
    "qcom.gen_frameproc",
    NULL,
    &g_caps,
    "libmmcamera_ubifocus_lib.so",
    CAM_QCOM_FEATURE_UBIFOCUS,
    &g_params);
}

