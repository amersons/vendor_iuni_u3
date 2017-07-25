/***************************************************************************
* Copyright (c) 2013-2014 Qualcomm Technologies, Inc. All Rights Reserved. *
* Qualcomm Technologies Proprietary and Confidential.                      *
****************************************************************************/

#ifndef __MODULE_IMGLIB_BASE_H__
#define __MODULE_IMGLIB_BASE_H__

#include "img_common.h"
#include "img_comp.h"
#include "img_comp_factory.h"
#include "module_imgbase.h"
#include "module_imglib_common.h"
#include "camera_dbg.h"
#include "modules.h"
#include "mct_pipeline.h"

#define MAX_IMGLIB_BASE_STATIC_PORTS 5

/** imgbase_client_t
 *   @mutex: client lock
 *   @cond: conditional variable for the client
 *   @comp: component ops structure
 *   @identity: MCT session/stream identity
 *   @frame: frame info from the module
 *   @state: state of face detection client
 *   @p_sinkport: sink port associated with the client
 *   @p_srcport: source port associated with the client
 *   @frame: array of image frames
 *   @parent_mod: pointer to the parent module
 *   @stream_on: Flag to indicate whether streamon is called
 *   @p_mod: pointer to the module
 *   @mode: IMBLIB mode
 *   @cur_buf_cnt: current buffer count
 *   @caps: imglib capabilities
 *   @current_meta: current meta data
 *   @stream_parm_q: stream param queue
 *   @frame_id: frame id
 *   @p_current_meta:  pointer to current meta
 *
 *   IMGLIB_BASE client structure
 **/
typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  img_component_ops_t comp;
  uint32_t identity;
  int state;
  mct_port_t *p_sinkport;
  mct_port_t *p_srcport;
  mct_stream_info_t *stream_info;
  mct_module_t *parent_mod;
  int8_t stream_on;
  void *p_mod;
  img_comp_mode_t mode;
  int cur_buf_cnt;
  img_caps_t caps;
  img_meta_t current_meta;
  img_queue_t stream_parm_q;
  int frame_id;
  img_meta_t *p_current_meta;
} imgbase_client_t;

/** module_imgbase_params_t
 *   @imgbase_query_mod: function pointer for module query
 *   @imgbase_client_init_params: function ptr for init params
 *
 *   module parameters for imglib base
 **/
typedef struct {
  boolean (*imgbase_query_mod)(mct_pipeline_cap_t *);
  boolean (*imgbase_client_init_params)(img_init_params_t *p_params);
} module_imgbase_params_t;

/** module_imgbase_t
 *   @imgbase_client_cnt: Variable to hold the number of IMGLIB_BASE
 *              clients
 *   @mutex: client lock
 *   @cond: conditional variable for the client
 *   @comp: core operation structure
 *   @lib_ref_count: reference count for imgbase library access
 *   @imgbase_client: List of IMGLIB_BASE clients
 *   @msg_thread: message thread
 *   @parent: pointer to the parent module
 *   @caps: Capabilities for imaging component
 *   @subdevfd: Buffer manager subdev FD
 *   @modparams: module parameters
 *
 *   IMGLIB_BASE module structure
 **/
typedef struct {
  int imgbase_client_cnt;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  img_core_ops_t core_ops;
  int lib_ref_count;
  mct_list_t *imgbase_client;
  mod_imglib_msg_th_t msg_thread;
  mct_module_t *parent_mod;
  uint32_t extra_buf;
  uint32_t feature_mask;
  void *mod_private;
  img_caps_t caps;
  int subdevfd;
  module_imgbase_params_t modparams;
} module_imgbase_t;

/**
 * Macro: mod_imgbase_send_event
 *
 * Description: This macro is used for sending an event between
 *            the modules
 *
 * Arguments:
 *   @id: identity
 *   @is_upstream: flag to indicate whether its upstream event
 *   @evt_type: event type
 *   @evt_data: event payload
 *
 * Return values:
 *     none
 *
 * Notes: none
 **/
#define mod_imgbase_send_event(id, is_upstream, evt_type, evt_data) ({ \
  boolean rc = TRUE; \
  mct_port_t *p_port; \
  mct_event_t event; \
  memset(&event, 0x0, sizeof(mct_event_t)); \
  event.type = MCT_EVENT_MODULE_EVENT; \
  event.identity = id; \
  if (is_upstream) { \
    event.direction = MCT_EVENT_UPSTREAM; \
    p_port = p_client->p_sinkport; \
  } else { \
    event.direction = MCT_EVENT_DOWNSTREAM; \
    p_port = p_client->p_srcport; \
  } \
  event.u.module_event.type = evt_type; \
  event.u.module_event.module_event_data = &evt_data; \
  rc =  mct_port_send_event_to_peer(p_port, &event); \
  rc; \
})

/*IMGLIB_BASE client APIs*/

/**
 * Function: module_imgbase_deinit
 *
 * Description: This function is used to free the imgbase module
 *
 * Arguments:
 *   p_mct_mod - MCTL module instance pointer
 *
 * Return values:
 *     none
 *
 * Notes: none
 **/
void module_imgbase_deinit(mct_module_t *p_mct_mod);

/** module_imgbase_init:
 *
 *  Arguments:
 *  @name - name of the module
 *  @comp_role: imaging component role
 *  @comp_name: imaging component name
 *  @mod_private: derived structure pointer
 *  @p_caps: imaging capability
 *  @lib_name: library name
 *  @feature_mask: feature mask of imaging algo
 *  @p_modparams: module parameters
 *
 * Description: This function is used to initialize the imgbase
 * module
 *
 * Return values:
 *     MCTL module instance pointer
 *
 * Notes: none
 **/
mct_module_t *module_imgbase_init(const char *name,
  img_comp_role_t comp_role,
  char *comp_name,
  void *mod_private,
  img_caps_t *p_caps,
  char *lib_name,
  uint32_t feature_mask,
  module_imgbase_params_t *p_modparams);

/** Function: module_imgbase_client_create
 *
 * Description: This function is used to create the IMGLIB_BASE client
 *
 * Arguments:
 *   @p_mct_mod: mct module pointer
 *   @p_port: mct port pointer
 *   @identity: identity of the stream
 *   @stream_info: stream information
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
int module_imgbase_client_create(mct_module_t *p_mct_mod,
  mct_port_t *p_port,
  uint32_t identity,
  mct_stream_info_t *stream_info);

/**
 * Function: module_imgbase_client_destroy
 *
 * Description: This function is used to destroy the imgbase client
 *
 * Arguments:
 *   @p_client: imgbase client
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
void module_imgbase_client_destroy(imgbase_client_t *p_client);

/**
 * Function: module_imgbase_client_stop
 *
 * Description: This function is used to stop the IMGLIB_BASE
 *              client
 *
 * Arguments:
 *   @p_client: IMGLIB_BASE client
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
int module_imgbase_client_stop(imgbase_client_t *p_client);

/**
 * Function: module_imgbase_client_start
 *
 * Description: This function is used to start the IMGLIB_BASE
 *              client
 *
 * Arguments:
 *   @p_client: IMGLIB_BASE client
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
int module_imgbase_client_start(imgbase_client_t *p_client);

/**
 * Function: module_imgbase_client_handle_buffer
 *
 * Description: This function is used to start the IMGLIB_BASE
 *              client
 *
 * Arguments:
 *   @p_client: IMGLIB_BASE client
 *   @p_buf_divert: Buffer divert structure
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
int module_imgbase_client_handle_buffer(imgbase_client_t *p_client,
  isp_buf_divert_t *p_buf_divert);

#endif //__MODULE_IMGLIB_BASE_H__
