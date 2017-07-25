/*============================================================================

  Copyright (c) 2013-2014 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/

#include "af_tuning.h"
#include "actuator.h"

static actuator_ctrl_t ROHM_BU64243GWZ_camera_lib_ptr = {
#include "ROHM_BU64243GWZ_camera_lib.h"
};

void *ROHM_BU64243GWZ_camera_open_lib(void)
{
  return &ROHM_BU64243GWZ_camera_lib_ptr;
}
