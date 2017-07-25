/*============================================================================

  Copyright (c) 2013-2014 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/

#include "af_tuning.h"
#include "actuator.h"

static actuator_ctrl_t ov8825_camera_lib_ptr = {
#include "ov8825_camera_lib.h"
};

void *ov8825_camera_open_lib(void)
{
  return &ov8825_camera_lib_ptr;
}
