/*============================================================================

  Copyright (c) 2013-2014 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/

#include "af_tuning.h"
#include "actuator.h"

static actuator_ctrl_t iu074_camera_lib_ptr = {
#include "iu074_camera_lib.h"
};

void *iu074_camera_open_lib(void)
{
  return &iu074_camera_lib_ptr;
}
