/*============================================================================

  Copyright (c) 2013-2014 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/

#include "af_tuning.h"
#include "actuator.h"

static actuator_ctrl_t lc898122_camera_lib_ptr = {
#include "lc898122_camera_lib.h"
};

void *lc898122_camera_open_lib(void)
{
  return &lc898122_camera_lib_ptr;
}
