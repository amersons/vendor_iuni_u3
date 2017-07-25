/*============================================================================

  Copyright (c) 2013-2014 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/

#include "af_tuning.h"
#include "actuator.h"

static actuator_ctrl_t ov12830_camcorder_lib_ptr = {
#include "ov12830_camcorder_lib.h"
};

void *ov12830_camcorder_open_lib(void)
{
  return &ov12830_camcorder_lib_ptr;
}
