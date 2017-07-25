#ifndef _SNS_REG_PLATFORM_H_
#define _SNS_REG_PLATFORM_H_
/*============================================================================
  @file sns_reg_platform.h

  @brief
  Header file for the Linux Android specific Sensors Registry definitions.

  <br><br>

  DEPENDENCIES: None.

  Copyright (c) 2013 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Qualcomm Technologies Confidential and Proprietary
  ============================================================================*/
//Gionee liujiang 2013-10-25 add for ps-calib start
#ifdef CONFIG_GN_Q_BSP_PS_CALIB_SUPPORT
#define SNS_REG_DATA_FILENAME "/persist/sns.reg"
#else
#define SNS_REG_DATA_FILENAME "/data/misc/sensors/sns.reg"
#endif
//Gionee liujiang 2013-10-25 add for ps-calib start

#define SNS_REG_DEFAULT_CONF_PATH "/etc/"

#define SNS_REG_FILE_MASK S_IRWXU

#endif /* _SNS_REG_PLATFORM_H_ */
