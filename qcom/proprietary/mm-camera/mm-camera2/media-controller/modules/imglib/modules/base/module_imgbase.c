/***************************************************************************
* Copyright (c) 2013-2014 Qualcomm Technologies, Inc. All Rights Reserved. *
* Qualcomm Technologies Proprietary and Confidential.                      *
****************************************************************************/

#include <linux/media.h>
#include "mct_module.h"
#include "module_imgbase.h"

/**
 * STATIC function declarations
 **/
static mct_port_t *module_imgbase_create_port(mct_module_t *p_mct_mod,
  mct_port_direction_t dir);

/**
 * Function: module_imgbase_find_identity
 *
 * Description: This method is used to find the client
 *
 * Arguments:
 *   @p_data: data in the mct list
 *   @p_input: input data to be seeked
 *
 * Return values:
 *     true/false
 *
 * Notes: none
 **/
static boolean module_imgbase_find_identity(void *p_data, void *p_input)
{
  uint32_t *p_identity = (uint32_t *)p_data;
  uint32_t identity = *((uint32_t *)p_input);

  return (*p_identity == identity);
}

/**
 * Function: module_imgbase_find_client
 *
 * Description: This method is used to find the client
 *
 * Arguments:
 *   @p_fp_data: imgbase client
 *   @p_input: input data
 *
 * Return values:
 *     true/false
 *
 * Notes: none
 **/
static boolean module_imgbase_find_client(void *p_imgbase_data, void *p_input)
{
  imgbase_client_t *p_client = (imgbase_client_t *)p_imgbase_data;
  uint32_t identity = *((uint32_t *)p_input);

  return (p_client->identity == identity);
}

/**
 * Function: module_imgbase_query_mod
 *
 * Description: This function is used to query the imgbase module
 * info
 *
 * Arguments:
 *   @module: mct module pointer
 *   @query_buf: pipeline capability
 *   @sessionid: session identity
 *
 * Return values:
 *     success/failure
 *
 * Notes: none
 **/
boolean module_imgbase_query_mod(mct_module_t *module, void *buf,
  unsigned int sessionid)
{
  mct_pipeline_cap_t *p_mct_cap = (mct_pipeline_cap_t *)buf;
  mct_pipeline_pp_cap_t *p_cap;
  module_imgbase_t *p_mod;

  IDBG_MED("%s:%d: E", __func__, __LINE__);
  if (!p_mct_cap || !module || !module->module_private) {
    IDBG_ERROR("%s:%d failed", __func__, __LINE__);
    return FALSE;
  }

  p_mod = (module_imgbase_t *)module->module_private;
  if (NULL == p_mod) {
    IDBG_ERROR("%s:%d] imgbase module NULL", __func__, __LINE__);
    return FALSE;
  }

  p_cap = &p_mct_cap->pp_cap;
  p_cap->height_padding = CAM_PAD_NONE;
  p_cap->plane_padding = CAM_PAD_NONE;
  p_cap->width_padding = CAM_PAD_NONE;
  p_cap->min_required_pp_mask |= 0;
  p_cap->feature_mask |= p_mod->feature_mask;

  if (p_mod->modparams.imgbase_query_mod)
    p_mod->modparams.imgbase_query_mod(p_mct_cap);

  return TRUE;
}

/**
 * Function: module_imgbase_forward_port_event
 *
 * Description: This method is used to forward an event
 * depending on the direction.
 *
 * Arguments:
 *   @p_client: Imblib base client
 *   @port: MCT port pointer
 *   @event: MCT event pointer
 *
 * Return values:
 *     true/false
 *
 * Notes: none
 **/
static boolean module_imgbase_forward_port_event(imgbase_client_t *p_client,
  mct_port_t *port,
  mct_event_t *event)
{
  boolean rc = FALSE;
  mct_port_t *p_adj_port = NULL;

  if (MCT_PORT_IS_SINK(port)) {
    p_adj_port = p_client->p_srcport;
    if (NULL == p_adj_port) {
      /* Not an error for sink module */
      return TRUE;
    }
    switch(event->direction) {
      case MCT_EVENT_UPSTREAM : {
        IDBG_ERROR("%s:%d] Error Upstream event on Sink port %d",
          __func__, __LINE__, event->type);
        break;
      }
      case MCT_EVENT_BOTH:
      case MCT_EVENT_DOWNSTREAM: {
       rc =  mct_port_send_event_to_peer(p_adj_port, event);
       if (rc == FALSE) {
         IDBG_ERROR("%s:%d] Fowarding event %d from sink port failed",
           __func__, __LINE__, event->type);
       }
       break;
     }
     default:
       IDBG_ERROR("%s:%d] Invalid port direction for event %d",
         __func__, __LINE__, event->type);
       break;
    }
  } else if (MCT_PORT_IS_SRC(port)) {
    p_adj_port = p_client->p_sinkport;
    if (NULL == p_adj_port) {
       IDBG_HIGH("%s:%d] Invalid port", __func__, __LINE__);
       return FALSE;
    }
    switch(event->direction) {
      case MCT_EVENT_DOWNSTREAM : {
        IDBG_ERROR("%s:%d] Error Downstream event on Src port %d",
          __func__, __LINE__, event->type);
        break;
      }
      case MCT_EVENT_BOTH:
      case MCT_EVENT_UPSTREAM: {
       rc =  mct_port_send_event_to_peer(p_adj_port, event);
       if (rc == FALSE) {
         IDBG_ERROR("%s:%d] Fowarding event %d from src port failed",
           __func__, __LINE__, event->type);
       }
       break;
     }
     default:
       IDBG_ERROR("%s:%d] Invalid port direction for event %d",
         __func__, __LINE__, event->type);
       break;
    }
  }
  return rc;
}

/**
 * Function: module_imgbase_port_event_func
 *
 * Description: Event handler function for the imgbase port
 *
 * Arguments:
 *   @port: mct port pointer
 *   @event: mct event
 *
 * Return values:
 *     true/false
 *
 * Notes: none
 **/
boolean module_imgbase_port_event_func(mct_port_t *port,
  mct_event_t *event)
{
  int rc = IMG_SUCCESS;
  mct_module_t *p_mct_mod = NULL;
  module_imgbase_t *p_mod = NULL;
  imgbase_client_t *p_client;
  boolean fwd_event = TRUE;

  if (!port || !event) {
    IDBG_ERROR("%s:%d invalid input", __func__, __LINE__);
    return FALSE;
  }
  IDBG_LOW("%s:%d] port %p E", __func__, __LINE__, port);
  p_mct_mod = MCT_MODULE_CAST((MCT_PORT_PARENT(port))->data);
  if (!p_mct_mod) {
    IDBG_ERROR("%s:%d invalid module", __func__, __LINE__);
    return FALSE;
  }

  p_mod = (module_imgbase_t *)p_mct_mod->module_private;
  if (NULL == p_mod) {
    IDBG_ERROR("%s:%d] imgbase module NULL", __func__, __LINE__);
    return FALSE;
  }

  p_client = (imgbase_client_t *)port->port_private;
  if (NULL == p_client) {
    IDBG_ERROR("%s:%d] imgbase client NULL", __func__, __LINE__);
    return FALSE;
  }

  IDBG_LOW("%s:%d] type %d", __func__, __LINE__, event->type);
  switch (event->type) {
  case MCT_EVENT_CONTROL_CMD: {
    mct_event_control_t *p_ctrl_event = &event->u.ctrl_event;
    IDBG_MED("%s:%d] Ctrl type %d", __func__, __LINE__, p_ctrl_event->type);
    switch (p_ctrl_event->type) {
    case MCT_EVENT_CONTROL_STREAMON: {
      IDBG_HIGH("%s:%d] IMGLIB_BASE STREAMON", __func__, __LINE__);
      module_imgbase_client_start(p_client);
      break;
    }
    case MCT_EVENT_CONTROL_STREAMOFF: {
      module_imgbase_t *p_mod = (module_imgbase_t *)p_client->p_mod;
      IDBG_MED("%s:%d] imgbase STREAMOFF", __func__, __LINE__);
      module_imgbase_client_stop(p_client);
      img_q_flush_and_destroy(&p_mod->msg_thread.msg_q);
      img_q_flush_and_destroy(&p_client->stream_parm_q);
      break;
    }
    case MCT_EVENT_CONTROL_PARM_STREAM_BUF: {
      cam_stream_parm_buffer_t *parm_buf =
        event->u.ctrl_event.control_event_data;
      IDBG_HIGH("%s:%d] MCT_EVENT_CONTROL_PARM_STREAM_BUF %d",
        __func__, __LINE__, parm_buf ? parm_buf->type : 0xffff);
      if (parm_buf &&
        (parm_buf->type == CAM_STREAM_PARAM_TYPE_GET_IMG_PROP)) {
        cam_stream_parm_buffer_t *out_buf;
        out_buf = img_q_dequeue(&p_client->stream_parm_q);
        if (out_buf) {
          *parm_buf = *out_buf;
          free(out_buf);
        }
      }
      break;
    }
    default:
      break;
    }
    break;
  }
  case MCT_EVENT_MODULE_EVENT: {
    mct_event_module_t *p_mod_event = &event->u.module_event;
    img_component_ops_t *p_comp = &p_client->comp;
    IDBG_MED("%s:%d] Mod type %d", __func__, __LINE__, p_mod_event->type);
    switch (p_mod_event->type) {
    case MCT_EVENT_MODULE_BUF_DIVERT: {
      mod_img_msg_t msg;
      isp_buf_divert_t *p_buf_divert =
        (isp_buf_divert_t *)p_mod_event->module_event_data;

      IDBG_ERROR("%s:%d] identity %x", __func__, __LINE__,
        event->identity);

      module_imgbase_client_handle_buffer(p_client, p_buf_divert);

      /* indicate that the buffer is consumed */
      p_buf_divert->is_locked = FALSE;
      p_buf_divert->ack_flag = FALSE;
      fwd_event = FALSE;

      break;
    }
    case MCT_EVENT_MODULE_STATS_AWB_UPDATE: {
      stats_update_t *stats_update = (stats_update_t *)
        p_mod_event->module_event_data;
      break;
    }
    case MCT_EVENT_MODULE_QUERY_DIVERT_TYPE: {
      uint32_t *divert_mask = (uint32_t *)p_mod_event->module_event_data;
      *divert_mask |= PPROC_DIVERT_PROCESSED;
      break;
    }
    case MCT_EVENT_MODULE_SET_CHROMATIX_PTR:
      break;
    default:
      break;
    }
    break;
  }
  default:
   break;
  }

  if (fwd_event) {
    boolean brc = module_imgbase_forward_port_event(p_client, port, event);
    rc = (brc) ? IMG_SUCCESS : IMG_ERR_GENERAL;
  }

  return GET_STATUS(rc);
}


/**
 * Function: module_imgbase_port_acquire
 *
 * Description: This function is used to acquire the port
 *
 * Arguments:
 *   @p_mct_mod: mct module pointer
 *   @port: mct port pointer
 *   @stream_info: stream information
 *
 * Return values:
 *     true/false
 *
 * Notes: none
 **/
boolean module_imgbase_port_acquire(mct_module_t *p_mct_mod,
  mct_port_t *port,
  mct_stream_info_t *stream_info)
{
  int rc = IMG_SUCCESS;
  unsigned int p_identity ;
  mct_list_t *p_temp_list = NULL;
  imgbase_client_t *p_client = NULL;
  module_imgbase_t *p_mod = NULL;

  IDBG_MED("%s:%d] E", __func__, __LINE__);

  p_mod = (module_imgbase_t *)p_mct_mod->module_private;
  if (NULL == p_mod) {
    IDBG_ERROR("%s:%d] imgbase module NULL", __func__, __LINE__);
    return FALSE;
  }
  p_identity =  stream_info->identity;

  /* check if its sink port*/
  if (MCT_PORT_IS_SINK(port)) {

    rc = module_imglib_common_get_bfr_mngr_subdev(&p_mod->subdevfd);
    if (!rc || p_mod->subdevfd < 0) {
      IDBG_ERROR("%s:%d] Error rc %d fd %d", __func__, __LINE__, rc,
        p_mod->subdevfd);
      goto error;
    }
    /* create imgbase client */
    rc = module_imgbase_client_create(p_mct_mod, port, p_identity, stream_info);

  } else {
    /* update the internal connection with source port */
    p_temp_list = mct_list_find_custom(p_mod->imgbase_client, &p_identity,
      module_imgbase_find_client);
    if (NULL != p_temp_list) {
      p_client = p_temp_list->data;
      p_client->p_srcport = port;
      port->port_private = p_client;
      IDBG_MED("%s:%d] found client %p", __func__, __LINE__, p_client);
    } else {
      IDBG_ERROR("%s:%d] cannot find the client", __func__, __LINE__);
      goto error;
    }
  }
  IDBG_MED("%s:%d] port %p port_private %p X", __func__, __LINE__,
    port, port->port_private);
  return GET_STATUS(rc);

error:

  IDBG_MED("%s:%d] Error X", __func__, __LINE__);
  return FALSE;

}

/**
 * Function: module_imgbase_port_check_caps_reserve
 *
 * Description: This function is used to reserve the port
 *
 * Arguments:
 *   @port: mct port pointer
 *   @peer_caps: pointer to peer capabilities
 *   @stream_info: stream information
 *
 * Return values:
 *     error/success
 *
 * Notes: none
 **/
boolean module_imgbase_port_check_caps_reserve(mct_port_t *port,
  void *peer_caps,
  void *vstream_info)
{
  boolean rc = FALSE;
  mct_module_t *p_mct_mod = NULL;
  module_imgbase_t *p_mod = NULL;
  mct_stream_info_t *stream_info = (mct_stream_info_t *)vstream_info;

  if (!port || !stream_info) {
    CDBG_ERROR("%s:%d invalid input", __func__, __LINE__);
    return FALSE;
  }

  IDBG_MED("%s:%d] E %s", __func__, __LINE__, MCT_PORT_NAME(port));
  p_mct_mod = MCT_MODULE_CAST((MCT_PORT_PARENT(port))->data);
  if (!p_mct_mod) {
    CDBG_ERROR("%s:%d invalid module", __func__, __LINE__);
    return FALSE;
  }

  p_mod = (module_imgbase_t *)p_mct_mod->module_private;
  if (NULL == p_mod) {
    CDBG_ERROR("%s:%d] imgbase module NULL", __func__, __LINE__);
    return FALSE;
  }

  /* lock the module */
  pthread_mutex_lock(&p_mod->mutex);
  if (port->port_private) {
    /* port is already reserved */
    IDBG_MED("%s:%d] Error port is already reserved", __func__, __LINE__);
    pthread_mutex_unlock(&p_mod->mutex);
    return FALSE;
  }
  rc = module_imgbase_port_acquire(p_mct_mod, port, stream_info);
  if (FALSE == rc) {
    CDBG_ERROR("%s:%d] Error acquiring port", __func__, __LINE__);
    pthread_mutex_unlock(&p_mod->mutex);
    return FALSE;
  }

  pthread_mutex_unlock(&p_mod->mutex);
  IDBG_MED("%s:%d] X", __func__, __LINE__);
  return TRUE;

}

/**
 * Function: module_imgbase_port_release_client
 *
 * Description: This method is used to release the client after all the
 *                 ports are destroyed
 *
 * Arguments:
 *   @p_mod: pointer to the imgbase module
 *   @identity: stream/session id
 *   @p_client: imgbase client
 *   @identity: port identity
 *
 * Return values:
 *     none
 *
 * Notes: none
 **/
void module_imgbase_port_release_client(module_imgbase_t *p_mod,
  mct_port_t *port,
  imgbase_client_t *p_client,
  unsigned int identity)
{
  mct_list_t *p_temp_list = NULL;
  p_temp_list = mct_list_find_custom(p_mod->imgbase_client, &identity,
    module_imgbase_find_client);
  if (NULL != p_temp_list) {
    IDBG_MED("%s:%d] ", __func__, __LINE__);
    p_mod->imgbase_client = mct_list_remove(p_mod->imgbase_client,
      p_temp_list->data);
  }
  module_imgbase_client_destroy(p_client);
}

/**
 * Function: module_imgbase_port_check_caps_unreserve
 *
 * Description: This method is used to unreserve the port
 *
 * Arguments:
 *   @identity: identitity for the session and stream
 *   @port: mct port pointer
 *   @peer: peer mct port pointer
 *
 * Return values:
 *     error/success
 *
 * Notes: none
 **/
boolean module_imgbase_port_check_caps_unreserve(mct_port_t *port,
  unsigned int identity)
{
  int rc = IMG_SUCCESS;
  mct_module_t *p_mct_mod = NULL;
  module_imgbase_t *p_mod = NULL;
  imgbase_client_t *p_client = NULL;
  uint32_t *p_identity = NULL;

  if (!port) {
    IDBG_ERROR("%s:%d invalid input", __func__, __LINE__);
    return FALSE;
  }

  IDBG_MED("%s:%d] E %d", __func__, __LINE__, MCT_PORT_DIRECTION(port));

  p_mct_mod = MCT_MODULE_CAST((MCT_PORT_PARENT(port))->data);
  if (!p_mct_mod) {
    IDBG_ERROR("%s:%d invalid module", __func__, __LINE__);
    return FALSE;
  }

  p_mod = (module_imgbase_t *)p_mct_mod->module_private;
  if (NULL == p_mod) {
    IDBG_ERROR("%s:%d] imgbase module NULL", __func__, __LINE__);
    return FALSE;
  }

  p_client = (imgbase_client_t *)port->port_private;
  if (NULL == p_client) {
    IDBG_ERROR("%s:%d] imgbase client NULL", __func__, __LINE__);
    return FALSE;
  }

  /* lock the module */
  pthread_mutex_lock(&p_mod->mutex);

  if (MCT_PORT_IS_SRC(port)) {
    port->port_private = NULL;
  } else {
    module_imgbase_port_release_client(p_mod, port, p_client, identity);
    port->port_private = NULL;
    if (p_mod->subdevfd >= 0) {
      close(p_mod->subdevfd);
      p_mod->subdevfd = -1;
    }
  }
  pthread_mutex_unlock(&p_mod->mutex);

  /*Todo: free port??*/
  IDBG_MED("%s:%d] X", __func__, __LINE__);
  return GET_STATUS(rc);

error:
  pthread_mutex_unlock(&p_mod->mutex);
  IDBG_MED("%s:%d] Error rc = %d X", __func__, __LINE__, rc);
  return FALSE;

}

/**
 * Function: module_imgbase_port_ext_link
 *
 * Description: This method is called when the user establishes
 *              link.
 *
 * Arguments:
 *   @identity: identitity for the session and stream
 *   @port: mct port pointer
 *   @peer: peer mct port pointer
 *
 * Return values:
 *     error/success
 *
 * Notes: none
 **/
boolean module_imgbase_port_ext_link(unsigned int identity,
  mct_port_t* port, mct_port_t *peer)
{
  int rc = IMG_SUCCESS;
  unsigned int *p_identity = NULL;
  mct_list_t *p_temp_list = NULL;
  mct_module_t *p_mct_mod = NULL;
  module_imgbase_t *p_mod = NULL;
  imgbase_client_t *p_client = NULL;

  if (!port || !peer) {
    IDBG_ERROR("%s:%d invalid input", __func__, __LINE__);
    return FALSE;
  }

  IDBG_MED("%s:%d] port %p E", __func__, __LINE__, port);
  p_mct_mod = MCT_MODULE_CAST((MCT_PORT_PARENT(port))->data);
  if (!p_mct_mod) {
    IDBG_ERROR("%s:%d invalid module", __func__, __LINE__);
    return FALSE;
  }

  p_mod = (module_imgbase_t *)p_mct_mod->module_private;
  if (NULL == p_mod) {
    IDBG_ERROR("%s:%d] imgbase module NULL", __func__, __LINE__);
    return FALSE;
  }

  p_client = (imgbase_client_t *)port->port_private;
  if (NULL == p_client) {
    IDBG_ERROR("%s:%d] invalid client", __func__, __LINE__);
    return FALSE;
  }

  if (MCT_PORT_PEER(port)) {
    IDBG_ERROR("%s:%d] link already established", __func__, __LINE__);
    return FALSE;
  }

  MCT_PORT_PEER(port) = peer;

  /* check if its sink port*/
  if (MCT_PORT_IS_SINK(port)) {
    /* start imgbase client in case of dynamic module */
  } else {
    /* do nothing for source port */
  }
  IDBG_MED("%s:%d] X", __func__, __LINE__);
  return GET_STATUS(rc);

error:
  IDBG_MED("%s:%d] Error X", __func__, __LINE__);
  return FALSE;

}

/**
 * Function: module_imgbase_port_unlink
 *
 * Description: This method is called when the user disconnects
 *              the link.
 *
 * Arguments:
 *   @identity: identitity for the session and stream
 *   @port: mct port pointer
 *   @peer: peer mct port pointer
 *
 * Return values:
 *     none
 *
 * Notes: none
 **/
void module_imgbase_port_unlink(unsigned int identity,
  mct_port_t *port, mct_port_t *peer)
{
  int rc = IMG_SUCCESS;
  mct_list_t *p_temp_list = NULL;
  mct_module_t *p_mct_mod = NULL;
  module_imgbase_t *p_mod = NULL;
  imgbase_client_t *p_client = NULL;
  uint32_t *p_identity = NULL;

  if (!port || !peer) {
    IDBG_ERROR("%s:%d invalid input", __func__, __LINE__);
    return;
  }

  IDBG_MED("%s:%d] E", __func__, __LINE__);
  p_mct_mod = MCT_MODULE_CAST((MCT_PORT_PARENT(port))->data);
  if (!p_mct_mod) {
    IDBG_ERROR("%s:%d invalid module", __func__, __LINE__);
    return;
  }

  p_mod = (module_imgbase_t *)p_mct_mod->module_private;
  if (NULL == p_mod) {
    IDBG_ERROR("%s:%d] imgbase module NULL", __func__, __LINE__);
    return;
  }

  p_client = (imgbase_client_t *)port->port_private;
  if (NULL == p_client) {
    IDBG_ERROR("%s:%d] imgbase client NULL", __func__, __LINE__);
    return;
  }

  if (MCT_PORT_IS_SINK(port)) {
    /* stop the client in case of dynamic module */
  } else {
    /* do nothing for source port*/
  }

  MCT_PORT_PEER(port) = NULL;

  IDBG_MED("%s:%d] X", __func__, __LINE__);
  return;

error:
  IDBG_MED("%s:%d] Error X", __func__, __LINE__);

}

/**
 * Function: module_imgbase_port_set_caps
 *
 * Description: This method is used to set the capabilities
 *
 * Arguments:
 *   @port: mct port pointer
 *   @caps: mct port capabilities
 *
 * Return values:
 *     error/success
 *
 * Notes: none
 **/
boolean module_imgbase_port_set_caps(mct_port_t *port,
  mct_port_caps_t *caps)
{
  int rc = IMG_SUCCESS;
  return GET_STATUS(rc);
}

/**
 * Function: module_imgbase_free_port
 *
 * Description: This function is used to free the imgbase ports
 *
 * Arguments:
 *   p_mct_mod - MCTL module instance pointer
 *
 * Return values:
 *     none
 *
 * Notes: none
 **/
static boolean module_imgbase_free_port(void *data, void *user_data)
{
  mct_port_t *p_port = (mct_port_t *)data;
  mct_module_t *p_mct_mod = (mct_module_t *)user_data;
  boolean rc = FALSE;

  if (!p_port || !p_mct_mod) {
    IDBG_ERROR("%s:%d failed", __func__, __LINE__);
    return TRUE;
  }
  IDBG_MED("%s:%d port %p p_mct_mod %p", __func__, __LINE__, p_port,
    p_mct_mod);

  rc = mct_module_remove_port(p_mct_mod, p_port);
  if (rc == FALSE) {
    IDBG_ERROR("%s:%d failed", __func__, __LINE__);
  }
  mct_port_destroy(p_port);
  return TRUE;
}

/**
 * Function: module_imgbase_create_port
 *
 * Description: This function is used to create a port and link with the
 *              module
 *
 * Arguments:
 *   none
 *
 * Return values:
 *     MCTL port pointer
 *
 * Notes: none
 **/
mct_port_t *module_imgbase_create_port(mct_module_t *p_mct_mod,
  mct_port_direction_t dir)
{
  char portname[PORT_NAME_LEN];
  mct_port_t *p_port = NULL;
  int status = IMG_SUCCESS;
  int index = 0;

  if (!p_mct_mod || (MCT_PORT_UNKNOWN == dir)) {
    IDBG_ERROR("%s:%d failed", __func__, __LINE__);
    return NULL;
  }

  index = (MCT_PORT_SINK == dir) ? p_mct_mod->numsinkports :
    p_mct_mod->numsrcports;
  /*portname <mod_name>_direction_portIndex*/
  snprintf(portname, sizeof(portname), "%s_d%d_i%d",
    MCT_MODULE_NAME(p_mct_mod), dir, index);
  p_port = mct_port_create(portname);
  if (NULL == p_port) {
    IDBG_ERROR("%s:%d failed", __func__, __LINE__);
    return NULL;
  }
  IDBG_MED("%s:%d portname %s", __func__, __LINE__, portname);

  p_port->direction = dir;
  p_port->port_private = NULL;
  p_port->caps.port_caps_type = MCT_PORT_CAPS_FRAME;

  /* override the function pointers */
  p_port->check_caps_reserve    = module_imgbase_port_check_caps_reserve;
  p_port->check_caps_unreserve  = module_imgbase_port_check_caps_unreserve;
  p_port->ext_link              = module_imgbase_port_ext_link;
  p_port->un_link               = module_imgbase_port_unlink;
  p_port->set_caps              = module_imgbase_port_set_caps;
  p_port->event_func            = module_imgbase_port_event_func;
   /* add port to the module */
  if (!mct_module_add_port(p_mct_mod, p_port)) {
    IDBG_ERROR("%s:%d] add port failed", __func__, __LINE__);
    status = IMG_ERR_GENERAL;
    goto error;
  }

  if (MCT_PORT_SRC == dir)
    p_mct_mod->numsrcports++;
  else
    p_mct_mod->numsinkports++;

  IDBG_MED("%s:%d ", __func__, __LINE__);
  return p_port;

error:

  IDBG_ERROR("%s:%d] failed", __func__, __LINE__);
  if (p_port) {
    mct_port_destroy(p_port);
    p_port = NULL;
  }
  return NULL;
}

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
void module_imgbase_deinit(mct_module_t *p_mct_mod)
{
  module_imgbase_t *p_mod = NULL;
  img_core_ops_t *p_core_ops = NULL;
  mct_list_t* p_list;
  int rc = 0;
  int i = 0;

  if (NULL == p_mct_mod) {
    IDBG_ERROR("%s:%d] MCTL module NULL", __func__, __LINE__);
    return;
  }

  p_mod = (module_imgbase_t *)p_mct_mod->module_private;
  if (NULL == p_mod) {
    IDBG_ERROR("%s:%d] imgbase module NULL", __func__, __LINE__);
    return;
  }

  do {
    p_list = mct_list_find_custom(MCT_MODULE_SINKPORTS(p_mct_mod), p_mct_mod,
      module_imglib_get_next_from_list);
    if (p_list)
      module_imgbase_free_port(p_list->data, p_mct_mod);
  } while (p_list);

  do {
    p_list = mct_list_find_custom(MCT_MODULE_SRCPORTS(p_mct_mod), p_mct_mod,
      module_imglib_get_next_from_list);
    if (p_list)
      module_imgbase_free_port(p_list->data, p_mct_mod);
  } while (p_list);


  p_core_ops = &p_mod->core_ops;
  IDBG_MED("%s:%d lib_ref_cnt %d", __func__, __LINE__, p_mod->lib_ref_count);
  if (p_mod->lib_ref_count) {
    IMG_COMP_UNLOAD(p_core_ops);
  }
  p_mod->imgbase_client_cnt = 0;
  pthread_mutex_destroy(&p_mod->mutex);
  pthread_cond_destroy(&p_mod->cond);

  mct_module_destroy(p_mct_mod);
}

/**
 * Function: module_imgbase_start_session
 *
 * Description: This function is called when a new camera
 *              session is started
 *
 * Arguments:
 *   @module: mct module pointer
 *   @sessionid: session id
 *
 * Return values:
 *     error/success
 *
 * Notes: none
 **/
static boolean module_imgbase_start_session(mct_module_t *module,
  unsigned int sessionid)
{
  int rc = IMG_SUCCESS;
  module_imgbase_t *p_mod;

  if (!module) {
    IDBG_ERROR("%s:%d failed", __func__, __LINE__);
    return FALSE;
  }

  p_mod = (module_imgbase_t *)module->module_private;
  if (!p_mod) {
    IDBG_ERROR("%s:%d failed", __func__, __LINE__);
    return FALSE;
  }

  /* create message thread */
  rc = module_imglib_create_msg_thread(&p_mod->msg_thread);

  return GET_STATUS(rc);
}

/**
 * Function: module_imgbase_stop_session
 *
 * Description: This function is called when the camera
 *              session is stopped
 *
 * Arguments:
 *   @module: mct module pointer
 *   @sessionid: session id
 *
 * Return values:
 *     error/success
 *
 * Notes: none
 **/
static boolean module_imgbase_stop_session(mct_module_t *module,
  unsigned int sessionid)
{
  int rc = IMG_SUCCESS;
  module_imgbase_t *p_mod;

  if (!module) {
    IDBG_ERROR("%s:%d failed", __func__, __LINE__);
    return FALSE;
  }

  p_mod = (module_imgbase_t *)module->module_private;
  if (!p_mod) {
    IDBG_ERROR("%s:%d failed", __func__, __LINE__);
    return FALSE;
  }

  /* destroy message thread */
  rc = module_imglib_destroy_msg_thread(&p_mod->msg_thread);
  return GET_STATUS(rc);
}

/** module_imgbase_set_parent:
 *
 *  Arguments:
 *  @p_parent - parent module pointer
 *
 * Description: This function is used to set the parent pointer
 *
 * Return values:
 *     none
 *
 * Notes: none
 **/
void module_imgbase_set_parent(mct_module_t *p_mct_mod, mct_module_t *p_parent)
{
  module_imgbase_t *p_mod = NULL;

  if (!p_mct_mod || !p_parent)
    return;

  p_mod = (module_imgbase_t *)p_mct_mod->module_private;

  if (!p_mod)
    return;

  p_mod->parent_mod = p_parent;
}

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
  module_imgbase_params_t *p_modparams)
{
  mct_module_t *p_mct_mod = NULL;
  module_imgbase_t *p_mod = NULL;
  img_core_ops_t *p_core_ops = NULL;
  mct_port_t *p_sinkport = NULL, *p_sourceport = NULL;
  int rc = 0;
  int i = 0;

  if (!name || !comp_name || (comp_role >= IMG_COMP_ROLE_MAX)
    || !p_caps || !lib_name || (0 == feature_mask)) {
    IDBG_ERROR("%s:%d invalid input", __func__, __LINE__);
    return NULL;
  }

  IDBG_MED("%s:%d] ", __func__, __LINE__);
  p_mct_mod = mct_module_create(name);
  if (NULL == p_mct_mod) {
    IDBG_ERROR("%s:%d cannot allocate mct module", __func__, __LINE__);
    return NULL;
  }

  p_mod = malloc(sizeof(module_imgbase_t));
  if (NULL == p_mod) {
    IDBG_ERROR("%s:%d failed", __func__, __LINE__);
    goto error;
  }

  p_mct_mod->module_private = (void *)p_mod;
  memset(p_mod, 0, sizeof(module_imgbase_t));

  pthread_mutex_init(&p_mod->mutex, NULL);
  pthread_cond_init(&p_mod->cond, NULL);
  p_core_ops = &p_mod->core_ops;

  IDBG_MED("%s:%d] ", __func__, __LINE__);
  /* check if the imgbase module is present */
  rc = img_core_get_comp(comp_role, comp_name, p_core_ops);
  if (IMG_ERROR(rc)) {
    IDBG_ERROR("%s:%d] Error rc %d", __func__, __LINE__, rc);
    goto error;
  }
 /* try to load the component */
  rc = IMG_COMP_LOAD(p_core_ops, lib_name);
  if (IMG_ERROR(rc)) {
    IDBG_ERROR("%s:%d] Error rc %d", __func__, __LINE__, rc);
    goto error;
  }
  p_mod->lib_ref_count++;
  p_mod->imgbase_client = NULL;
  p_mod->mod_private = mod_private;
  p_mod->caps = *p_caps;
  p_mod->subdevfd = -1;
  p_mod->feature_mask = feature_mask;
  if (p_modparams)
    p_mod->modparams = *p_modparams;

  IDBG_MED("%s:%d] ", __func__, __LINE__);
  /* create static ports */
  for (i = 0; i < MAX_IMGLIB_BASE_STATIC_PORTS; i++) {
    p_sinkport = module_imgbase_create_port(p_mct_mod, MCT_PORT_SINK);
    if (NULL == p_sinkport) {
      IDBG_ERROR("%s:%d] create SINK port failed", __func__, __LINE__);
      goto error;
    }
    p_sourceport = module_imgbase_create_port(p_mct_mod, MCT_PORT_SRC);
    if (NULL == p_sourceport) {
      IDBG_ERROR("%s:%d] create SINK port failed", __func__, __LINE__);
      goto error;
    }
  }

  p_mct_mod->start_session    = module_imgbase_start_session;
  p_mct_mod->stop_session     = module_imgbase_stop_session;
  p_mct_mod->query_mod        = module_imgbase_query_mod;
  IDBG_MED("%s:%d] %p", __func__, __LINE__, p_mct_mod);
  return p_mct_mod;

error:
  if (p_mod) {
    module_imgbase_deinit(p_mct_mod);
  } else if (p_mct_mod) {
    mct_module_destroy(p_mct_mod);
    p_mct_mod = NULL;
  }
  return NULL;
}
