/*============================================================================

  @file sensors_hal_ma.c

  @brief
  Handle all requests for the motion accel sensor

  Copyright (c) 2013 Qualcomm Technologies, Inc.  All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.
============================================================================*/

/*===========================================================================
                           INCLUDE FILES
===========================================================================*/
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include "sensors_hal.h"
#include "sns_sam_rmd_v01.h"
#include "sns_smgr_api_v01.h"
#include "sensor1.h"
#include "fixed_point.h"

/*===========================================================================
                         PREPROCESSOR DEFINITIONS
===========================================================================*/

#define MOTION_ACCEL_SAMPLE_RATE 15
#define MOTION_ACCEL_WAIT_TIME .5

/*===========================================================================
                   DATA TYPES
===========================================================================*/

typedef struct hal_ma_control_t {
  bool              ma_enabled;        /* flag to check if client has registered for screen orientation sensor */
  bool              accel_enabled;     /* Whether the accel stream is currently active */
  bool              rmd_enabled;       /* Whether the rmd stream is currently active */
  int               accel_wait_cnt;    /* Number of accel samples to wait for until we disable the stream */

  pthread_mutex_t   ma_mutex;  /* mutex lock for sensor1 callback, to be used for motion accel only */
  pthread_cond_t    ma_cond;   /* cond variable to signal callback has arrived, to be used with motion accel only */

  pthread_t         ma_thread;            /* Motion Accel thread id */
  sensor1_handle_s  *sensor1_hndl;

  sns_sam_motion_state_e_v01 motion_state;         /* Indicates whether device is in motion/rest */
} hal_ma_control_t;

/*===========================================================================
                         STATIC VARIABLES
===========================================================================*/

static hal_ma_control_t *g_ma_control = NULL;

/*==========================================================================
                         EXTERNAL FUNCTION DECLARATIONS
===========================================================================*/

void hal_process_buffering_ind( sns_smgr_buffering_ind_msg_v01* smgr_ind );

/*==========================================================================
                         STATIC FUNCTION DEFINITIONS
===========================================================================*/

/*===========================================================================
  FUNCTION:  hal_ma_start_accel()
  ===========================================================================*/
static void hal_ma_start_accel( void )
{
  /* Hard coding to 15hz for the initial version.
   * Need to use the report rate stored in current_freq */
  const uint32_t sample_rate = MOTION_ACCEL_SAMPLE_RATE;
  const bool dont_wake = false,
             buffer = true,
             wait_for_resp = true;
  int handle = HANDLE_MOTION_ACCEL;

  sns_smgr_buffering_req_msg_v01 *smgr_req;
  sensor1_msg_header_s req_hdr;
  sensor1_error_e error;

  HAL_LOG_VERBOSE("%s: Enabling Accel", __FUNCTION__ );

  error = sensor1_alloc_msg_buf( g_ma_control->sensor1_hndl,
                                 sizeof(sns_smgr_buffering_req_msg_v01),
                                 (void**)&smgr_req );
  if( SENSOR1_SUCCESS != error )
  {
    HAL_LOG_ERROR("%s: sensor1_alloc_msg_buf() error: %d", __FUNCTION__, error );
  }
  else
  {
    smgr_req->ReportId = HANDLE_MOTION_ACCEL;
    smgr_req->Action = SNS_SMGR_BUFFERING_ACTION_ADD_V01;
    smgr_req->ReportRate = sample_rate*UNIT_Q16;

    smgr_req->Item_len = 1;
    smgr_req->Item[0].DataType = SNS_SMGR_DATA_TYPE_PRIMARY_V01;

    smgr_req->notify_suspend_valid = true;
    smgr_req->notify_suspend.proc_type = SNS_PROC_APPS_V01;
    smgr_req->notify_suspend.send_indications_during_suspend = false;

    smgr_req->Item[0].SamplingRate = sample_rate;
    smgr_req->Item[1].SamplingRate = sample_rate;
    smgr_req->Item[0].Calibration = SNS_SMGR_CAL_SEL_FULL_CAL_V01;
    smgr_req->Item[0].Decimation = SNS_SMGR_DECIMATION_FILTER_V01;

    smgr_req->Item[0].SensorId = SNS_SMGR_ID_ACCEL_V01;

    req_hdr.service_number = SNS_SMGR_SVC_ID_V01;
    req_hdr.msg_id = SNS_SMGR_BUFFERING_REQ_V01;
    req_hdr.msg_size = sizeof( sns_smgr_buffering_req_msg_v01 );
    req_hdr.txn_id = 0;

    if( (error = sensor1_write( g_ma_control->sensor1_hndl, &req_hdr,
                                smgr_req )) != SENSOR1_SUCCESS )
    {
      sensor1_free_msg_buf( g_ma_control->sensor1_hndl, smgr_req );
      HAL_LOG_ERROR( "%s: sensor1_write() error: %d", __FUNCTION__, error );
    }
  }

  g_ma_control->accel_enabled = true;

  return;
}

/*===========================================================================
  FUNCTION:  hal_ma_start_rmd()
  ===========================================================================*/
static void hal_ma_start_rmd( void )
{
  sensor1_error_e error;
  sensor1_msg_header_s req_hdr;
  sns_sam_qmd_enable_req_msg_v01 *sam_req;

  HAL_LOG_DEBUG("%s: Enabling RMD", __FUNCTION__ );

  error = sensor1_alloc_msg_buf( g_ma_control->sensor1_hndl,
                                 sizeof(sns_sam_qmd_enable_req_msg_v01),
                                 (void**)&sam_req );
  if( SENSOR1_SUCCESS != error )
  {
    HAL_LOG_ERROR("%s: sensor1_alloc_msg_buf() error: %d", __FUNCTION__, error );
  }
  else
  {
    req_hdr.service_number = SNS_SAM_RMD_SVC_ID_V01;
    req_hdr.msg_id = SNS_SAM_RMD_ENABLE_REQ_V01;
    req_hdr.msg_size = sizeof( sns_sam_qmd_enable_req_msg_v01 );
    req_hdr.txn_id = 0;

    sam_req->config_valid = false;
    sam_req->report_period = 0; /* Operate in asynchronous mode */

    sam_req->notify_suspend_valid = true;
    sam_req->notify_suspend.proc_type = SNS_PROC_APPS_V01;
    sam_req->notify_suspend.send_indications_during_suspend = false;

    if( (error = sensor1_write( g_ma_control->sensor1_hndl, &req_hdr,
                                sam_req )) != SENSOR1_SUCCESS )
    {
      /* free the message buffer */
      sensor1_free_msg_buf( g_ma_control->sensor1_hndl, sam_req );
      HAL_LOG_ERROR("%s: sensor1_write() error: %d", __FUNCTION__, error );
    }
  }

  g_ma_control->rmd_enabled = true;

  return;
}

/*===========================================================================

  FUNCTION:  hal_ma_stop()

  This function stops motion accel service by stopping RMD and accel
  streams.  When both streams are stopped, g_ma_control->ma_enabled
  flag is set to false in hal_ma_sensor1_data_cb() to tear down hal_ma_thread

===========================================================================*/
/*!
*/
static void hal_ma_stop()
{
  HAL_LOG_DEBUG( "%s: Stopping Motion Accel sensor", __FUNCTION__ );

  hal_sam_send_cancel( g_ma_control->sensor1_hndl, SNS_SAM_RMD_SVC_ID_V01 );
  hal_sam_send_cancel( g_ma_control->sensor1_hndl, SNS_SMGR_SVC_ID_V01 );

  return;
}

/*===========================================================================

  FUNCTION:  hal_ma_thread
  This is the motion_accel thread entry function that handles the logic of
  starting/stopping accelerometer data based on RMD indications.
  It also handles add and delete request for motion accel.

  ===========================================================================*/
static void*
hal_ma_thread( void *unused )
{
  pthread_mutex_lock( &g_ma_control->ma_mutex );
  HAL_LOG_DEBUG( "%s: Starting Motion Accel thread", __FUNCTION__ );

  /* Start accel stream immediately so that Android knows the current orientation */
  g_ma_control->accel_wait_cnt = 0;
  hal_ma_start_accel();
  hal_ma_start_rmd();
  g_ma_control->ma_enabled = true;

  /* Report has been added, wait for indications to start/stop accel streaming */
  while( 1 )
  {
    HAL_LOG_DEBUG( "%s: wait on RMD ind",__FUNCTION__ );
    pthread_cond_wait( &g_ma_control->ma_cond, &g_ma_control->ma_mutex );

    // Check if we are still enabled before processing
    if( !g_ma_control->ma_enabled ) {
      break;
    }

    if( g_ma_control->ma_enabled )
    {
      HAL_LOG_VERBOSE( "%s: MA thread received updated state %d",
                        __FUNCTION__, g_ma_control->motion_state );

      if( SNS_SAM_MOTION_MOVE_V01 == g_ma_control->motion_state )
      {
        if( g_ma_control->accel_enabled )
        {
          HAL_LOG_VERBOSE( "%s: Accel already enabled, no request sent", __FUNCTION__ );
        }
        else
        {
          HAL_LOG_VERBOSE( "%s: Enabling Accel", __FUNCTION__ );
          hal_ma_start_accel();
        }

        g_ma_control->accel_wait_cnt = 0;
      }
      else if( SNS_SAM_MOTION_REST_V01 == g_ma_control->motion_state )
      {
        // Wait for ~500ms until stopping the accel stream
        g_ma_control->accel_wait_cnt = MOTION_ACCEL_SAMPLE_RATE * MOTION_ACCEL_WAIT_TIME + 1;
      }
    }
  }

  HAL_LOG_DEBUG( "%s: Leaving Motion Accel thread",__FUNCTION__ );
  pthread_mutex_unlock( &g_ma_control->ma_mutex );
  return NULL;
}

/*===========================================================================
  FUNCTION:  hal_sensor1_data_cb
===========================================================================*/
static void
hal_ma_sensor1_data_cb( intptr_t cb_data, sensor1_msg_header_s *msg_hdr,
                     sensor1_msg_type_e msg_type, void *msg_ptr )
{
  // Completly syncronize the sensor1 callbacks (mostly RX data) with the
  // hal_ma_thread processing thread.
  pthread_mutex_lock( &g_ma_control->ma_mutex );

  if( msg_hdr != NULL )
  {
    HAL_LOG_VERBOSE( "%s: msg_type %d, Sn %d, msg Id %d, txn Id %d", __FUNCTION__,
                     msg_type, msg_hdr->service_number, msg_hdr->msg_id, msg_hdr->txn_id );
  }
  else
  {
    HAL_LOG_VERBOSE( "%s: msg_type %d", __FUNCTION__, msg_type );
    // If the header is null, we don't care what type of message it is.
    // Set to type that does not dereference msg_hdr to avoid static analysis errors
    msg_type = SENSOR1_MSG_TYPE_RESP_INT_ERR;
  }

  if( SENSOR1_MSG_TYPE_BROKEN_PIPE == msg_type )
  {
    HAL_LOG_VERBOSE("%s: SENSOR1_MSG_TYPE_BROKEN_PIPE", __FUNCTION__);
    // We must unlock our mutex to ensure the reference count is 0
    // when performing a re-init.  We will then re-aquire the new
    // mutex to complete this sensor1 callback
    pthread_mutex_unlock( &g_ma_control->ma_mutex );
    hal_ma_init( true );
    pthread_mutex_lock( &g_ma_control->ma_mutex );
  }
  else if ( SENSOR1_MSG_TYPE_RETRY_OPEN == msg_type )
  {
    HAL_LOG_VERBOSE("%s: SENSOR1_MSG_TYPE_RETRY_OPEN", __FUNCTION__);
    // Try to re-aquire the sensor1 handle
    hal_ma_init (false );
  }
  else if( SENSOR1_MSG_TYPE_RESP == msg_type )
  {
    if( SNS_SMGR_SVC_ID_V01 == msg_hdr->service_number )
    {
      HAL_LOG_DEBUG("%s: Recieved SNS_SMGR_SVC_ID_V01 response", __FUNCTION__);
      sns_smgr_buffering_resp_msg_v01 *smgr_resp = (sns_smgr_buffering_resp_msg_v01*)msg_ptr;

      if ( SNS_SMGR_BUFFERING_RESP_V01 == msg_hdr->msg_id )
      {
        if( SNS_RESULT_SUCCESS_V01 != smgr_resp->Resp.sns_result_t )
        {
          HAL_LOG_ERROR( "%s: Result: %u, Error: %u", __FUNCTION__,
          smgr_resp->Resp.sns_result_t, smgr_resp->Resp.sns_err_t  );
        }

        if( smgr_resp->AckNak != SNS_SMGR_RESPONSE_ACK_SUCCESS_V01 &&
            smgr_resp->AckNak != SNS_SMGR_RESPONSE_ACK_MODIFIED_V01 )
        {
          HAL_LOG_ERROR( "%s: %d Error: %u Reason: %u", __FUNCTION__, smgr_resp->ReportId,
          smgr_resp->AckNak, smgr_resp->ReasonPair[0].Reason );
        }

        HAL_LOG_DEBUG( "%s: Id: %u Resp: %u txn id %d", __FUNCTION__,
        smgr_resp->ReportId, smgr_resp->AckNak, msg_hdr->txn_id );
      }
      else if ( 0 == msg_hdr->msg_id ) // Cancel response
      {
        HAL_LOG_DEBUG("%s: Disabled SNS_SMGR_SVC_ID_V01", __FUNCTION__);
        g_ma_control->accel_enabled = false;

        // In hal_ma_stop() we call cancel on SNS_SAM_RMD_SVC_ID_V01 and SNS_SMGR_SVC_ID_V01
        // If SNS_SAM_RMD_SVC_ID_V01 and SNS_SMGR_SVC_ID_V01 are disabled, then it is
        // safe to disable all of motion accel sensor.
        if (g_ma_control->rmd_enabled == false) {
          g_ma_control->ma_enabled = false;
          // Signal to hal_ma_thread that it's safe to teardown
          pthread_cond_signal( &g_ma_control->ma_cond );
        }
      }
    }
    else if( SNS_SAM_RMD_SVC_ID_V01 == msg_hdr->service_number )
    {
      HAL_LOG_DEBUG("%s: Recieved SNS_SAM_RMD_SVC_ID_V01 response", __FUNCTION__);
      sns_common_resp_s_v01 *common_resp = (sns_common_resp_s_v01*)msg_ptr;

      if( SNS_RESULT_SUCCESS_V01 != common_resp->sns_result_t )
      {
        HAL_LOG_ERROR( "%s: Msg %i; Result: %u, Error: %u", __FUNCTION__,
                       msg_hdr->msg_id, common_resp->sns_result_t, common_resp->sns_err_t );
      }

      if (0 == msg_hdr->msg_id) // Cancel response
      {
        HAL_LOG_DEBUG("%s: Disabled SNS_SAM_RMD_SVC_ID_V01", __FUNCTION__);
        g_ma_control->rmd_enabled = false;

        // In hal_ma_stop() we call cancel on SNS_SAM_RMD_SVC_ID_V01 and SNS_SMGR_SVC_ID_V01
        // If SNS_SAM_RMD_SVC_ID_V01 and SNS_SMGR_SVC_ID_V01 are disabled, then it is
        // safe to disable all of motion accel sensor.
        if (g_ma_control->rmd_enabled == false) {
          g_ma_control->ma_enabled = false;
          // Signal to hal_ma_thread that it's safe to teardown
          pthread_cond_signal( &g_ma_control->ma_cond );
        }
      }
    }
  }
  else if( SENSOR1_MSG_TYPE_IND == msg_type )
  {
    if( SNS_SMGR_SVC_ID_V01 == msg_hdr->service_number &&
        SNS_SMGR_BUFFERING_IND_V01 == msg_hdr->msg_id )
    {
      hal_process_buffering_ind( msg_ptr );
      if( 1 == g_ma_control->accel_wait_cnt )
      {
        // g_ma_control->accel_enabled = false;
        hal_sam_send_cancel( g_ma_control->sensor1_hndl, SNS_SMGR_SVC_ID_V01 );
      }

      if( 0 != g_ma_control->accel_wait_cnt )
      {
        g_ma_control->accel_wait_cnt--;
      }
    }
    else if( SNS_SAM_RMD_SVC_ID_V01 == msg_hdr->service_number &&
             SNS_SAM_RMD_REPORT_IND_V01 == msg_hdr->msg_id )
    {
      sns_sam_qmd_report_ind_msg_v01 *ind_msg =
        (sns_sam_qmd_report_ind_msg_v01*)msg_ptr;

      HAL_LOG_DEBUG( "%s: Update motion state %d -> %d", __FUNCTION__,
                     g_ma_control->motion_state, ind_msg->state );
      g_ma_control->motion_state = ind_msg->state;
      pthread_cond_signal( &g_ma_control->ma_cond );
    }
  }
  else if( SENSOR1_MSG_TYPE_RESP_INT_ERR == msg_type )
  {
    /* Do nothing */
  }

  if( NULL != msg_ptr )
  {
    sensor1_free_msg_buf( g_ma_control->sensor1_hndl, msg_ptr );
  }

  pthread_mutex_unlock( &g_ma_control->ma_mutex );
}

/*==========================================================================
                         PUBLIC FUNCTION DEFINITIONS
===========================================================================*/

/*===========================================================================
  FUNCTION:  hal_ma_destroy
===========================================================================*/
int hal_ma_destroy()
{
  HAL_LOG_VERBOSE("%s", __FUNCTION__);
  if( NULL != g_ma_control )
  {
    sensor1_close( g_ma_control->sensor1_hndl );
    pthread_mutex_destroy( &g_ma_control->ma_mutex );
    pthread_cond_destroy( &g_ma_control->ma_cond );
    free( g_ma_control );
    g_ma_control = NULL;
  }

  return 0;
}

/*===========================================================================
  FUNCTION:  hal_ma_init
===========================================================================*/
int hal_ma_init(bool reset)
{
  sensor1_error_e error;
  pthread_mutexattr_t attr;

  HAL_LOG_VERBOSE("%s: reset = %d", __FUNCTION__, reset);

  // Called from sensor1 callback context
  if (reset == true) {
    pthread_mutex_lock(&g_ma_control->ma_mutex);
    // Signal to hal_ma_thread that it's safe to teardown
    g_ma_control->ma_enabled = false;
    pthread_cond_signal( &g_ma_control->ma_cond );
    pthread_mutex_unlock( &g_ma_control->ma_mutex );
    hal_ma_activate( false );
    // Do not destroy - allow ma_cond condvar to signal thread
    //hal_ma_destroy();
  }

  // Called first time in HAL init
  if(NULL == g_ma_control)
  {
    g_ma_control = malloc(sizeof(hal_ma_control_t));
    if(NULL == g_ma_control) {
      HAL_LOG_ERROR("%s: ERROR: malloc error", __FUNCTION__);
      return -1;
    }

    g_ma_control->ma_enabled = false;
    g_ma_control->accel_enabled = false;
    g_ma_control->rmd_enabled = false;
    g_ma_control->accel_wait_cnt = 0;
    g_ma_control->sensor1_hndl = NULL;
    g_ma_control->ma_thread = -1;
    g_ma_control->motion_state = SNS_SAM_MOTION_UNKNOWN_V01;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_ma_control->ma_mutex, &attr);
    pthread_cond_init(&g_ma_control->ma_cond, NULL);
    pthread_mutexattr_destroy(&attr);
  }

  // May be called on broken pipe to re-init sensor1 connection
  if (NULL != g_ma_control) {
    error = sensor1_open(&g_ma_control->sensor1_hndl, hal_ma_sensor1_data_cb, (intptr_t)NULL);
    if(SENSOR1_SUCCESS != error)
    {
      HAL_LOG_ERROR("%s: sensor1_open failed %d", __FUNCTION__, error);
      return -1;
    }
  }

  return 0;
}

/*===========================================================================
  FUNCTION:  hal_ma_activate
===========================================================================*/
int hal_ma_activate( bool enabled )
{
  pthread_attr_t thread_attr;

  pthread_mutex_lock( &g_ma_control->ma_mutex );
  HAL_LOG_VERBOSE("%s: enabled = %d", __FUNCTION__, enabled);

  if(enabled) {
    // Create hal_ma_thread on enable
    if( -1 == g_ma_control->ma_thread ) {
      pthread_attr_init( &thread_attr );
      pthread_attr_setdetachstate( &thread_attr, PTHREAD_CREATE_JOINABLE );
      pthread_create( &g_ma_control->ma_thread, &thread_attr, hal_ma_thread, NULL );
      pthread_attr_destroy( &thread_attr );
    }
  } else {
    // Teardown hal_ma_thread on disable
    if( -1 != g_ma_control->ma_thread )
    {
      hal_ma_stop();
      // Need to unlock the mutex here to allow pending transactions in hal_ma_sensor1_data_cb
      // to complete before joining the thread
      pthread_mutex_unlock( &g_ma_control->ma_mutex );
      HAL_LOG_VERBOSE("%s: Thread waiting join", __FUNCTION__);
      pthread_join( g_ma_control->ma_thread, NULL );
      pthread_mutex_lock( &g_ma_control->ma_mutex );
      HAL_LOG_VERBOSE("%s: Thread complete", __FUNCTION__);
      g_ma_control->ma_thread = -1;
    }
  }

  pthread_mutex_unlock( &g_ma_control->ma_mutex );
  return 0;
}
