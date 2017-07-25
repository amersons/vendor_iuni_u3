/***************************************************************************
* Copyright (c) 2013-2014 Qualcomm Technologies, Inc. All Rights Reserved. *
* Qualcomm Technologies Proprietary and Confidential.                      *
***************************************************************************/
#include "module_imglib.h"
#include "module_imglib_defs.h"
#include "module_imglib_common.h"
#include "mct_pipeline.h"

/* Static declaration of imglib topology */
MOD_IMGLIB_TOPOLOGY_REGISTER(mod_imglib_topology);

/** imglib_port_data_t
 *   @sessionid: Session id
 *   @query_buf: Pipeline capabilities buffer
 *
 *   imglib query capabilities private data
 **/
typedef struct {
  unsigned int sessionid;
  void *query_buf;
} imglib_query_mod_data_t;

/**
 * Function: module_imglib_start_mod_session
 *
 * Description: List traverse handler function will start session
 *  in the module of the list
 *
 * Arguments:
 *   @data: mct module pointer
 *   @user_data: pointer to session id
 *
 * Return values:
 *     TRUE/FALSE
 *
 * Notes: none
 **/
static boolean module_imglib_start_mod_session(void *data, void *user_data)
{
  mct_module_t *module =  (mct_module_t *)data;
  unsigned int *sessionid = (unsigned int *)user_data;

  if (!(module && module->module_private) || !sessionid)
    return FALSE;

  return module->start_session(module, *sessionid);
}

/**
 * Function: module_imglib_stop_mod_session
 *
 * Description: List traverse handler function will stop session
 *  in the module of the list
 *
 * Arguments:
 *   @data: mct module pointer
 *   @user_data: pointer to session id
 *
 * Return values:
 *     TRUE/FALSE
 *
 * Notes: none
 **/
static boolean module_imglib_stop_mod_session(void *data, void *user_data)
{
  mct_module_t *module =  (mct_module_t *)data;
  unsigned int *sessionid = (unsigned int *)user_data;

  if (!(module && module->module_private) || !sessionid)
    return FALSE;

  return module->stop_session(module, *sessionid);
}

/**
 * Function: module_imglib_query_mod_session
 *
 * Description: List traverse handler function will start session
 *  in the module of the list
 *
 * Arguments:
 *   @data: mct module pointer
 *   @user_data: query data pointer
 *
 * Return values:
 *     TRUE/FALSE
 *
 * Notes: none
 **/
static boolean module_imglib_query_mod_session(void *data, void *user_data)
{
  mct_module_t *module =  (mct_module_t *)data;
  imglib_query_mod_data_t *query_d = (imglib_query_mod_data_t *)user_data;

  if (!module || !query_d)
    return FALSE;

  return module->query_mod(module, query_d->query_buf, query_d->sessionid);
}
/**
 * Function: module_imglib_check_name
 *
 * Description: List find custom handler function used for matching
 *  module name
 *
 * Arguments:
 *   @mod: Module
 *   @name: Name to match

 * Return values:
 *     TRUE/FALSE
 *
 * Notes: none
 **/
static boolean module_imglib_check_name(void *mod, void *name)
{
  boolean ret_val;

  if (!mod || !name)
    return FALSE;

  ret_val = !strncmp(MCT_OBJECT_NAME(mod),
    (char *)name, MODULE_IMGLIB_MAX_NAME_LENGH);

  return ret_val;
}

/**
 * Function: module_imglib_find_session_params
 *
 * Description: Function used in list find custom to
 *  find matching session params with session id
 *
 * Arguments:
 *   @data1: session params
 *   @data2: sessionid
 *
 * Return values:
 *   TRUE/FALSE
 *
 **/
static boolean module_imglib_find_session_params(void *data1, void *data2)
{
  module_imglib_session_params_t *session_params =
    (module_imglib_session_params_t *)data1;
  unsigned int *sessionid = (unsigned int *)data2;

  if (!data1 || !data2) {
    return FALSE;
  }

  if (session_params->sessionid == *sessionid) {
    return TRUE;
  }

  return FALSE;
}

/** module_imglib_free_session_params
 *    @data: Pointer to session params to be freed
 *    @user_data: should be NULL
 *
 *  Function used in traverse list for freeing session params
 *
 *  Return TRUE on success.
 **/
static boolean  module_imglib_free_session_params(void *data, void *user_data)
{

  if (!data) {
    return FALSE;
  }

  free(data);

  return TRUE;
}
/**
 * Function: module_imglib_get_module
 *
 * Description: This function will search module with given name in module list
 *
 * Arguments:
 *   @mods: List of modules
 *   @name: Name to match

 * Return values:
 *     TRUE/FALSE
 *
 * Notes: none
 **/
static mct_module_t *module_imglib_get_module(mct_list_t *mods, const char *name)
{
  mct_list_t *p_node;

  if (!mods || !name)
    return NULL;

  p_node = mct_list_find_custom(mods, (char *)name, module_imglib_check_name);

  return p_node ? (mct_module_t *)(p_node->data) : NULL;
}

/**
 * Function: module_imglib_is_new_candidate
 *
 * Description: Return True if new topology should be chosen
 *
 * Arguments:
 *   @p_new: New topology candidate
 *   @p_old: Old topology candidate
 *   @stream_info: Stream info struct
 *
 * Return values:
 *     TRUE/FALSE
 *
 * Notes: none
 **/
static boolean module_imglib_is_new_candidate(module_imglib_topology_t *p_new,
    module_imglib_topology_t *p_old, mct_stream_info_t *stream_info)
{
  uint32_t stream_feature_mask, temp_feature_mask;
  uint32_t i, new_cand_bits, old_cand_bits;

  stream_feature_mask =
      stream_info->reprocess_config.pp_feature_config.feature_mask;

  /* Is new candidate feature mask is not in stream feature mask return false */
  if ((stream_feature_mask & p_new->feature_mask) != p_new->feature_mask) {
    return FALSE;
  }

  /* If there is no old candidate make it new candidate */
  if (NULL == p_old) {
    return TRUE;
  }

  /* The candidate with features will be new candidate */
  temp_feature_mask = p_new->feature_mask;
  for (new_cand_bits = 0; temp_feature_mask != 0; new_cand_bits++) {
    temp_feature_mask &= temp_feature_mask - 1;
  }

  temp_feature_mask =  p_old->feature_mask;
  for (old_cand_bits = 0; temp_feature_mask != 0; old_cand_bits++) {
    temp_feature_mask &= temp_feature_mask - 1;
  }

  return (new_cand_bits > old_cand_bits) ? TRUE : FALSE;
}

/**
 * Function: module_imglib_init_session_params
 *
 * Description: Init session parameters which need to be stored
 *
 * Arguments:
 *   @p_mod: Imagelib module instance
 *   @topo_holder: Topology holder
 *   @param_type_array: Array of parameters need to be stored per session
 *
 * Return values:
 *     TRUE on success
 *
 * Notes: none
 **/
static boolean module_imglib_init_session_params(module_imglib_t *p_mod,
  module_imglib_topo_holder_t *topo_holder,
  cam_intf_parm_type_t *param_type_array)
{
  int i;

  if (!p_mod || !topo_holder || !param_type_array) {
    IDBG_ERROR("%s:%d Invalid input p_mod %p topo %p array %p",
      __func__, __LINE__, p_mod, topo_holder, param_type_array);
    return FALSE;
  }

  /* If there are no session params just exit */
  if (CAM_INTF_PARM_MAX <= param_type_array[0]) {
    return TRUE;
  }

  for (i = 0; i < MODULE_IMGLIB_MAX_STORED_PARAMS; i++) {
    if (CAM_INTF_PARM_MAX <= param_type_array[i]) {
      break;
    }
    topo_holder->params_to_restore[param_type_array[i]] = TRUE;
    p_mod->params_to_store[param_type_array[i]] = TRUE;
  }

  return TRUE;
}

/**
 * Function: module_imglib_deinit_session_params
 *
 * Description: Deinitalize and free session parameters
 *
 * Arguments:
 *   @p_mod: Imagelib module instance
 *
 * Return values:
 *     none
 *
 * Notes: none
 **/
static void module_imglib_deinit_session_params(module_imglib_t *p_mod)
{
  if (p_mod && p_mod->session_params_list) {
    mct_list_free_all(p_mod->session_params_list,
      module_imglib_free_session_params);
    p_mod->session_params_list = NULL;
  }

  return;
}

/**
 * Function: module_imglib_clear_session_params
 *
 * Description: Function to clear and initialize session parameters
 *
 * Arguments:
 *   @p_mod: Imagelib module instance
 *   @sessionid: Session id
 *
 * Return values:
 *     TRUE/FALSE
 *
 * Notes: none
 **/
boolean module_imglib_clear_session_params(module_imglib_t *p_mod,
  unsigned int sessionid)
{
  mct_list_t *p_list;

  if (!p_mod) {
    IDBG_ERROR("%s:%d invalid input", __func__, __LINE__);
    return FALSE;
  }

  p_list = mct_list_find_custom(p_mod->session_params_list, &sessionid,
    module_imglib_find_session_params);

  if (p_list && p_list->data) {
    module_imglib_session_params_t *session_params = p_list->data;
    p_mod->session_params_list =
      mct_list_remove(p_mod->session_params_list, session_params);
    free(session_params);
  }

  return TRUE;
}

/**
 * Function: module_imglib_get_session_params
 *
 * Description: Function used to get session parameters based on session id
 *
 * Arguments:
 *   @module: Mct module instance
 *   @sessionid: Session id
 *
 * Return values:
 *   Pointer to session param buffer
 *
 * Notes: none
 **/
parm_buffer_t *module_imglib_get_session_params(mct_module_t *module,
  unsigned int sessionid)
{
  module_imglib_t *p_mod;
  module_imglib_session_params_t *p_session_params;
  parm_buffer_t *p_param_buffer = NULL;
  mct_list_t *p_list;

  if (!(module && module->module_private)) {
    IDBG_ERROR("%s:%d Invalid input module %p", __func__, __LINE__, module);
    return NULL;
  }

  p_mod = module->module_private;
  if (NULL == p_mod->session_params_list) {
    return NULL;
  }

  p_list = mct_list_find_custom(p_mod->session_params_list, &sessionid,
    module_imglib_find_session_params);

  if (p_list && p_list->data) {
    module_imglib_session_params_t *p_session_params = p_list->data;
    p_param_buffer = &p_session_params->params;
  }

  return p_param_buffer;
}

/**
 * Function: module_imglib_store_session_params
 *
 * Description: Function used to store session params
 *
 * Arguments:
 *   @module: mct module pointer
 *   @param_to_store: Params need to be stored
 *   @sessionid: Session id
 *
 * Return values:
 *   TRUE/FALSE
 *
 * Notes: none
 **/
boolean module_imglib_store_session_params(mct_module_t *module,
  mct_event_control_parm_t *param_to_store, unsigned int sessionid)
{
  module_imglib_t *p_mod;
  parm_buffer_t *p_table = NULL;

  if (!(module && module->module_private)  || !param_to_store) {
    IDBG_ERROR("%s:%d mct modile %p param %p", __func__, __LINE__,
      module, param_to_store);
    return FALSE;
  }

  if (param_to_store->type >= CAM_INTF_PARM_MAX) {
    IDBG_ERROR("%s:%d Invalid param type ", __func__, __LINE__);
    return FALSE;
  }

  p_mod = module->module_private;

  p_table = module_imglib_get_session_params(module, sessionid);

  /* If session params are missing create for this stream */
  if (NULL == p_table) {
    module_imglib_session_params_t *session_params;

    session_params = calloc(1, sizeof(*session_params));
    if (NULL == session_params) {
      IDBG_ERROR("%s:%d Out of memory ", __func__, __LINE__);
      return FALSE;
    }

    session_params->sessionid = sessionid;
    session_params->params.first_flagged_entry = CAM_INTF_PARM_MAX;
    p_mod->session_params_list = mct_list_append(p_mod->session_params_list,
      session_params, NULL, NULL);

    p_table = &session_params->params;
  }

  if (TRUE == p_mod->params_to_store[param_to_store->type]) {
    int position = param_to_store->type;
    parm_type_t *data = param_to_store->parm_data;
    int current, next;

    current = GET_FIRST_PARAM_ID(p_table);
    if (position == current){
      //DO NOTHING
    } else if (position < current){
      SET_NEXT_PARAM_ID(position, p_table, current);
      SET_FIRST_PARAM_ID(p_table, position);
    } else {
      /* Search for the position in the linked list where we need to slot in*/
      while (position > GET_NEXT_PARAM_ID(current, p_table))
        current = GET_NEXT_PARAM_ID(current, p_table);

      /*If node already exists no need to alter linking*/
      if (position != GET_NEXT_PARAM_ID(current, p_table)) {
        next = GET_NEXT_PARAM_ID(current, p_table);
        SET_NEXT_PARAM_ID(current, p_table, position);
        SET_NEXT_PARAM_ID(position, p_table, next);
      }
    }
    p_table->entry[param_to_store->type].data = *data;
  }
  return TRUE;
}

/**
 * Function: module_imglib_get_topology
 *
 * Description: Get internal topology based in stream info
 *
 * Arguments:
 *   @module: Imagelib module object
 *   @stream_info: mct_stream_info_t struct

 * Return values:
 *     Pointer to topology holder on success/NULL on fail
 *
 * Notes: none
 **/
module_imglib_topo_holder_t *module_imglib_get_topology(mct_module_t *module,
  mct_stream_info_t *stream_info)
{
  module_imglib_t *p_mod;
  module_imglib_topo_holder_t *p_chosen_topology = NULL;
  module_imglib_topology_t *p_topology = NULL;
  boolean new_cand;
  uint32_t topo_idx;

  if (!module || !stream_info) {
    IDBG_ERROR("%s:%d] Invalid input arguments", __func__, __LINE__);
    goto out;
  }

  p_mod = (module_imglib_t *)module->module_private;
  if (NULL == p_mod) {
    IDBG_ERROR("%s:%d] imglib module NULL", __func__, __LINE__);
    goto out;
  }

  if (!(p_mod && p_mod->imglib_modules && p_mod->topology)) {
    IDBG_ERROR("%s:%d] Invalid input arguments", __func__, __LINE__);
    goto out;
  }

  for (topo_idx = 0; topo_idx < IMGLIB_ARRAY_SIZE(mod_imglib_topology); topo_idx++) {
    if (stream_info->stream_type == mod_imglib_topology[topo_idx].stream_type) {
      /* Check if new topology will be new candidate */
      new_cand = module_imglib_is_new_candidate(&mod_imglib_topology[topo_idx],
                   p_topology, stream_info);
      if (new_cand && p_mod->topology[topo_idx].topo_attached) {
        p_chosen_topology = &p_mod->topology[topo_idx];
        p_topology = &mod_imglib_topology[topo_idx];
      }
    }
  }

out:
  return p_chosen_topology;
}

/**
 * Function: module_imglib_destroy_topology
 *
 * Description: Destroy imagelib topology
 *
 * Arguments:
 *   @p_mod: Imagelib module private data

 * Return values:
 *     TRUE/FALSE
 *
 * Notes: none
 **/
static boolean module_imglib_destroy_topology(module_imglib_t *p_mod)
{
  mct_module_t *m_temp;
  module_imglib_topology_t *curr_top;
  mct_module_init_name_t *curr_mod;
  unsigned int top_c, par_c, mod_c;

  if (!(p_mod && p_mod->imglib_modules && p_mod->topology))
    return FALSE;

  /* Add to separate function As Create topology  */
  for (top_c = 0; top_c < IMGLIB_ARRAY_SIZE(mod_imglib_topology); top_c++) {
    curr_top = &mod_imglib_topology[top_c];

    /* Go thru all parallel topologies for this port */
    for (par_c = 0; par_c < MODULE_IMGLIB_MAX_PAR_TOPO; par_c++) {
      if (!curr_top->modules[par_c][0].name)
        break;

      for (mod_c = 0; mod_c < MODULE_IMGLIB_MAX_TOPO_MOD; mod_c++) {
        curr_mod = &curr_top->modules[par_c][mod_c];
        if (!curr_mod->name || !curr_mod->init_mod || !curr_mod->deinit_mod)
          break;
        /* Check if module is already registered */
        m_temp = module_imglib_get_module(p_mod->imglib_modules, curr_mod->name);
        if (m_temp) {
          curr_mod->deinit_mod(m_temp);
          p_mod->imglib_modules = mct_list_remove(p_mod->imglib_modules, m_temp);
        }
      }
      mct_list_free_list(p_mod->topology[top_c].topo_list[par_c]);
      p_mod->topology[top_c].topo_list[par_c] = NULL;
    }
  }
  mct_list_free_list(p_mod->imglib_modules);
  p_mod->imglib_modules = NULL;

  return TRUE;
}

/**
 * Function: module_imglib_create_topology
 *
 * Description: Create image lib topology based on static declaration
 *   and description in imglib_defs.h file.
 *   There are two lists
 *   1. List for all initialized modules
 *   2. List per stream containing modules which need to be connected in
 *     for internal topology
 *
 * Arguments:
 *   @p_mod: Imagelib module private data

 * Return values:
 *     TRUE/FALSE
 *
 * Notes: none
 **/
static boolean module_imglib_create_topology(module_imglib_t *p_mod)
{
  mct_module_t *m_temp;
  mct_list_t *temp_list;
  module_imglib_topology_t *curr_top;
  mct_module_init_name_t *curr_mod;
  unsigned int top_c, par_c, mod_c;

  if (NULL == p_mod)
    return FALSE;

  for (top_c = 0; top_c < IMGLIB_ARRAY_SIZE(mod_imglib_topology); top_c++) {
    curr_top = &mod_imglib_topology[top_c];

    /* Go thru all parallel topologies and initiate their list */
    for (par_c = 0; par_c < MODULE_IMGLIB_MAX_PAR_TOPO; par_c++) {
      if (!curr_top->modules[par_c][0].name)
        break;

      for (mod_c = 0; mod_c < MODULE_IMGLIB_MAX_TOPO_MOD; mod_c++) {
        curr_mod = &curr_top->modules[par_c][mod_c];
        if (!curr_mod->name || !curr_mod->init_mod || !curr_mod->deinit_mod)
          break;

        /* Check if module is already registered */
        m_temp = NULL;
        if (p_mod->imglib_modules) {
          m_temp = module_imglib_get_module(p_mod->imglib_modules,
            curr_mod->name);
        }

        /* If there is no module initiated call module init and add to the list */
        if (!m_temp) {
          m_temp = curr_mod->init_mod(curr_mod->name);
          if (!m_temp) {
            IDBG_ERROR("%s:%d] Can not init the module %s", __func__, __LINE__,
              curr_mod->name);
            break;
          }

          temp_list = mct_list_append(p_mod->imglib_modules, m_temp, NULL, NULL);
          if (!temp_list) {
            IDBG_ERROR("%s:%d] Can not add new module to module list",
              __func__, __LINE__);
            goto error;
          }
          p_mod->imglib_modules = temp_list;
        }
        /* Add module to topology list */
        temp_list = mct_list_append(p_mod->topology[top_c].topo_list[par_c],
          m_temp, NULL, NULL);
        if (!temp_list) {
          IDBG_ERROR("%s:%d] Can not add module to stream list",
            __func__, __LINE__);
          goto error;
        }
        p_mod->topology[top_c].topo_list[par_c] = temp_list;
        p_mod->topology[top_c].topo_attached = (par_c + 1);
        p_mod->topology[top_c].port_events_mask = curr_top->port_events_mask;
        /* Init session parameters */
        module_imglib_init_session_params(p_mod, &p_mod->topology[top_c],
          curr_top->session_params);
      }
    }
  }

  return TRUE;
error:
  module_imglib_destroy_topology(p_mod);
  return FALSE;
}

/**
 * module_imglib_create_port_and_forward_event:
 *   @module: the module instance
 *   @event: mct event
 *
 * If port is not present we need to reserve first and after that forward the event
 *
 * This function executes in Imaging Server context
 *
 * Return values: TRUE in case of success
 **/
static boolean module_imglib_create_port_and_forward_event(mct_module_t *module,
    mct_event_t *event)
{
  mct_stream_t *stream;
  mct_port_t *port;
  module_imglib_t *p_mod;
  boolean ret_val;

  stream = mod_imglib_find_module_parent(event->identity, module);
  if (!stream) {
    IDBG_ERROR("Module is orphan does not have parent %s\n", __func__);
    return FALSE;
  }
  p_mod = (module_imglib_t *) module->module_private;

  MCT_OBJECT_LOCK(module);
  port = module_imglib_get_and_reserve_port(module, &stream->streaminfo,
    MCT_PORT_SINK, &p_mod->dummy_port->caps);
  MCT_OBJECT_UNLOCK(module);

  if (NULL == port) {
    /* Do not lock here module function is protected */
    port = module->request_new_port(&stream->streaminfo,
        MCT_PORT_SINK, module, &p_mod->dummy_port->caps);
    if (NULL == port) {
      IDBG_ERROR("Cannot reserve imglib port %s\n", __func__);
      goto error;
    }
  }

  ret_val = port->ext_link(event->identity, port, p_mod->dummy_port);
  if (FALSE == ret_val) {
    IDBG_ERROR("Cannot establish link %s\n", __func__);
    goto error;
  }

  ret_val = port->event_func(port, event);
  if (FALSE == ret_val) {
    IDBG_ERROR("Port event function failed %s\n", __func__);
    goto error;
  }

  return TRUE;

error:
  IDBG_ERROR("Cannot process stream on in %s\n", __func__);

  if (port) {
    port->un_link(event->identity, port, p_mod->dummy_port);
    port->check_caps_unreserve(port, event->identity);
  }

  return FALSE;
}

/**
 * module_imglib_forward_event_and_destroy_port:
 *   @module: the module instance
 *   @event: mct event
 *
 * This function will forward event and unerserve port for
 *  redirecting module events, should be called on last module event
 *
 * This function executes in Imaging Server context
 *
 * Return values: TRUE in case of success
 **/
static boolean module_imglib_forward_event_and_destroy_port(mct_module_t *module,
  mct_event_t *event)
{
  mct_list_t *list_match;
  mct_port_t *port;
  module_imglib_t *p_mod = module->module_private;
  boolean ret_val = FALSE;

  MCT_OBJECT_LOCK(module);
  port = module_imglib_get_port_with_identity(module, event->identity,
    MCT_PORT_SINK);
  MCT_OBJECT_UNLOCK(module);

  if (!port) {
    IDBG_ERROR("Cannot find port with identity 0x%x in %s\n",
      event->identity, __func__);
   return ret_val;
  }

  if (!port->un_link|| !port->event_func || !port->check_caps_unreserve) {
    IDBG_ERROR("Port functions are missing %s\n", __func__);
    return ret_val;
  }

  ret_val = port->event_func(port, event);

  port->un_link(event->identity, port, p_mod->dummy_port);

  ret_val &= port->check_caps_unreserve(port, event->identity);
  if (FALSE == ret_val)
    IDBG_ERROR("Cannot process stream off in %s\n", __func__);

  return ret_val;
}

/**
 * module_imglib_forward_event_to_port:
 *   @module: the module instance
 *   @event: mct event
 *
 * Handler function in imglib module for forwarding events
 *   to corresponding ports
 *
 * This function executes in Imaging Server context
 *
 * Return values: TRUE in case of success
 **/
static int module_imglib_forward_event_to_port(mct_module_t *module,
  mct_event_t *event)
{
  mct_port_t *port;
  boolean ret_val;

  MCT_OBJECT_LOCK(module);
  port = module_imglib_get_port_with_identity(module, event->identity,
    MCT_PORT_SINK);
  MCT_OBJECT_UNLOCK(module);

  if (NULL == port) {
   return IMG_ERR_NOT_FOUND;
  }

  ret_val = port->event_func(port, event);

  return ret_val ? IMG_SUCCESS : IMG_ERR_GENERAL;
}

/**
 * Function: module_imglib_query_mod
 *
 * Description: This function is used to query the imglib module info
 *
 * Arguments:
 *   @module: mct module pointer
 *   @query_buf: pipeline capability
 *   @sessionid: session id
 *
 * Return values:
 *     success/failure
 *
 * Notes: none
 **/
static boolean module_imglib_query_mod(mct_module_t *module,
  void *query_buf,
  unsigned int sessionid)
{
  module_imglib_t *p_mod;
  imglib_query_mod_data_t query_data;
  boolean ret;

  if (!(module && module->module_private) || !query_buf) {
    IDBG_ERROR("%s:%d Invalid input", __func__, __LINE__);
    return FALSE;
  }

  p_mod = (module_imglib_t *)module->module_private;

  query_data.sessionid = sessionid;
  query_data.query_buf = query_buf;

  /* Call query cap of every module in the list */
  MCT_OBJECT_LOCK(module);
  ret = mct_list_traverse(p_mod->imglib_modules,
    module_imglib_query_mod_session, &query_data);
  MCT_OBJECT_UNLOCK(module);

  if (FALSE == ret) {
    IDBG_ERROR("%s:%d Query capabilities failed", __func__, __LINE__);
  }

  return ret;
}

/**
 * Function: module_imglib_process_event
 *
 * Description: Event handler function for the imglib module
 *
 * Arguments:
 *   @module: mct module pointer
 *   @p_event: mct event
 *
 * Return values:
 *     error/success
 *
 * Notes: none
 **/
static boolean module_imglib_process_event(mct_module_t *module,
  mct_event_t *event)
{
  boolean ret_val = TRUE;
  int rc;

  if (!module || !event) {
    IDBG_ERROR("%s:%d Invalid input", __func__, __LINE__);
    return FALSE;
  }

  /* Do not lock module here each handler function will do that */
  switch (event->type) {
  case MCT_EVENT_CONTROL_CMD: {
    mct_event_control_t *p_ctrl_event = &event->u.ctrl_event;
    IDBG_MED("%s:%d] Ctrl type %d", __func__, __LINE__, p_ctrl_event->type);
    switch (p_ctrl_event->type) {
    case MCT_EVENT_CONTROL_STREAMOFF:
      /*  Unreserve the port which is needed for redirecting module events */
      ret_val = module_imglib_forward_event_and_destroy_port(module, event);
      break;
    default:
      rc = module_imglib_forward_event_to_port(module, event);
      if (IMG_ERR_NOT_FOUND == rc) {
        /* We are just redirecting module events to our sink port, if port
         * is not reserved reserve and forward the event */
        ret_val = module_imglib_create_port_and_forward_event(module, event);
      } else {
        ret_val = IMG_ERROR(rc) ? FALSE : TRUE;
      }
      break;

    }
    break;
  }
  /* Todo missing implementatnion */
  case MCT_EVENT_MODULE_EVENT:
  default:
    break;
  }

  return ret_val;
}

/**
 * Function: module_imglib_start_session
 *
 * Description: This function is called when a new camera
 *  session is started, it will invoke also start session in all
 *  internal modules registered in create topology
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
static boolean module_imglib_start_session(mct_module_t *module,
  unsigned int sessionid)
{
  cam_stream_type_t stream;
  module_imglib_t *p_mod;
  boolean ret;

  if (!(module && module->module_private)) {
    IDBG_ERROR("%s:%d Invalid input", __func__, __LINE__);
    return FALSE;
  }

  p_mod = (module_imglib_t *)module->module_private;

  MCT_OBJECT_LOCK(module);
  ret = mct_list_traverse(p_mod->imglib_modules,
    module_imglib_start_mod_session, &sessionid);
  MCT_OBJECT_UNLOCK(module);

  if (FALSE == ret) {
    IDBG_ERROR("%s:%d Can not start the session", __func__, __LINE__);
  }

  return ret;
}

/**
 * Function: module_imglib_stop_session
 *
 * Description: This function is called when a new camera
 *  session is started, it will invoke also stop session in all
 *  internal modules registered in create topology.
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
static boolean module_imglib_stop_session(mct_module_t *module,
  unsigned int sessionid)
{
  cam_stream_type_t stream;
  module_imglib_t *p_mod;
  mct_port_t *port;
  boolean ret;

  if (!(module && module->module_private)) {
    IDBG_ERROR("%s:%d Invalid input", __func__, __LINE__);
    return FALSE;
  }
  p_mod = (module_imglib_t *)module->module_private;

  MCT_OBJECT_LOCK(module);

  /* Go thru all internal modules and call stop session */
  ret = mct_list_traverse(p_mod->imglib_modules,
    module_imglib_stop_mod_session, &sessionid);
  if (FALSE == ret) {
    IDBG_ERROR("%s:%d Can not stop the session", __func__, __LINE__);
    goto out;
  }

  /* Free dynamic allocated ports*/
  do {
    port = module_imglib_get_dyn_port_with_sessionid(module,
      sessionid, MCT_PORT_SRC);
    if (port)
      module_imglib_free_port(module, port);
  } while (port);

  do {
    port = module_imglib_get_dyn_port_with_sessionid(module,
      sessionid, MCT_PORT_SINK);
    if (port)
      module_imglib_free_port(module, port);
  } while (port);

  ret = module_imglib_clear_session_params(p_mod, sessionid);
  if (FALSE == ret) {
    IDBG_ERROR("%s:%d Can not clear session parameters", __func__, __LINE__);
  }

out:
  MCT_OBJECT_UNLOCK(module);
  return ret;
}

/**
 * Function: module_imglib_set_mod
 *
 * Description: This function is used to set the imglib module
 *
 * Arguments:
 *   @module: mct module pointer
 *   @module_type: module type
 *   @identity: Stream identity
 *
 * Return values:
 *     none
 *
 * Notes: none
 **/
static void module_imglib_set_mod(mct_module_t *module,
  unsigned int module_type,
  unsigned int identity)
{
  if (!(module && module->module_private)) {
    IDBG_ERROR("%s:%d failed", __func__, __LINE__);
    return;
  }

  MCT_OBJECT_LOCK(module);
  mct_module_add_type(module, module_type, identity);
  MCT_OBJECT_UNLOCK(module);

  return;
}

/**
 * Function: module_imglib_request_new_port
 *
 * Description: This function is called by the mct framework
 *         when new port needs to be created
 *
 * Arguments:
 *   @stream_info: stream information
 *   @direction: direction of port
 *   @module: mct module pointer
 *   @peer_cap: port peer capabilities
 *
 * Return values:
 *     error/success
 *
 * Notes: none
 **/
mct_port_t *module_imglib_request_new_port(void *vstream_info,
  mct_port_direction_t direction,
  mct_module_t *module,
  void *peer_cap)
{
  mct_port_t *p_port = NULL;

  if (!vstream_info || !module ||  !peer_cap) {
    IDBG_ERROR("%s:%d Can not create new port invalid argument", __func__, __LINE__);
    return NULL;
  }

  MCT_OBJECT_LOCK(module);

  /* Create new port */
  p_port = module_imglib_create_port(module, direction, FALSE,
    MODULE_IMGLIB_MAX_PAR_TOPO);
  if (NULL == p_port) {
    IDBG_ERROR("%s:%d] Create port failed", __func__, __LINE__);
    goto out;
  }

  /* Try to reserve new requested port */
  p_port = module_imglib_get_and_reserve_port(module, vstream_info,
      direction, peer_cap);
  if (NULL == p_port) {
    IDBG_ERROR("%s:%d] Create port failed", __func__, __LINE__);
    goto out;
  }

out:
  MCT_OBJECT_UNLOCK(module);
  return p_port;
}
/**
 * Function: module_imglib_free_mod
 *
 * Description: This function is used to free the imglib module
 *
 * Arguments:
 *   p_mct_mod - MCTL module instance pointer
 *
 * Return values:
 *     none
 *
 * Notes: none
 **/
void module_imglib_deinit(mct_module_t *p_mct_mod)
{
  module_imglib_t *p_mod;
  mct_list_t *list;
  int i;

  if (NULL == p_mct_mod) {
    IDBG_ERROR("%s:%d] MCTL module NULL", __func__, __LINE__);
    return;
  }

  p_mod = (module_imglib_t *)p_mct_mod->module_private;
  if (NULL == p_mod) {
    IDBG_ERROR("%s:%d] imglib module NULL", __func__, __LINE__);
    return;
  }

  /* Free dynamic allocated ports*/
  MCT_OBJECT_LOCK(p_mct_mod);
  do {
    list = mct_list_find_custom(MCT_MODULE_SINKPORTS(p_mct_mod), p_mct_mod,
      module_imglib_get_next_from_list);
    if (list)
      module_imglib_free_port(p_mct_mod, list->data);
  } while (list);

  do {
    list = mct_list_find_custom(MCT_MODULE_SRCPORTS(p_mct_mod), p_mct_mod,
      module_imglib_get_next_from_list);
    if (list)
      module_imglib_free_port(p_mct_mod, list->data);
  } while (list);
  MCT_OBJECT_UNLOCK(p_mct_mod);

  module_imglib_deinit_session_params(p_mod);

  module_imglib_destroy_topology(p_mod);

  /* Destroy module ports */
  mct_list_free_list(MCT_MODULE_CHILDREN(p_mct_mod));
  mct_module_destroy(p_mct_mod);
  p_mct_mod = NULL;
}

/** module_imglib_init:
 *
 *  Arguments:
 *  @name - name of the module
 *
 * Description: This function is used to initialize the imglib module
 *
 * Return values:
 *     MCTL module instance pointer
 *
 * Notes: none
 **/
mct_module_t *module_imglib_init(const char *name)
{
  mct_module_t *p_mct_mod;
  module_imglib_t *p_mod = NULL;
  mct_port_t *p_port;
  int i;
  boolean ret_val;

  if (!name)
    return NULL;

  p_mct_mod = mct_module_create(name);
  if (NULL == p_mct_mod) {
    IDBG_ERROR("%s:%d cannot allocate mct module", __func__, __LINE__);
    goto error;
  }
  p_mod = calloc(1, sizeof(module_imglib_t));
  if (NULL == p_mod) {
    IDBG_ERROR("%s:%d failed", __func__, __LINE__);
    goto error;
  }

  p_mct_mod->module_private = (void *)p_mod;

  mct_module_set_set_mod_func(p_mct_mod, module_imglib_set_mod);
  mct_module_set_query_mod_func(p_mct_mod, module_imglib_query_mod);
  mct_module_set_start_session_func(p_mct_mod, module_imglib_start_session);
  mct_module_set_stop_session_func(p_mct_mod, module_imglib_stop_session);
  mct_module_set_process_event_func(p_mct_mod, module_imglib_process_event);
  mct_module_set_request_new_port_func(p_mct_mod,
      module_imglib_request_new_port);

  /* Set internal static ports */
  for (i = 0; i < MODULE_IMGLIB_STATIC_PORTS; i++) {
    p_port = module_imglib_create_port(p_mct_mod, MCT_PORT_SINK, TRUE,
      MODULE_IMGLIB_MAX_PAR_TOPO);
    if (NULL == p_port) {
      IDBG_ERROR("%s:%d] Create port failed", __func__, __LINE__);
      goto error;
    }
  }

  /* Set internal topology */
  ret_val = module_imglib_create_topology(p_mod);
  if (FALSE == ret_val) {
    IDBG_ERROR("%s:%d] Can not create topology ", __func__, __LINE__);
    goto error;
  }

  p_mod->dummy_port = module_imglib_create_dummy_port(p_mct_mod, MCT_PORT_SRC);
  if (NULL == p_mod->dummy_port) {
    IDBG_ERROR("%s:%d] Create dummy port failed", __func__, __LINE__);
    goto error;
  }

  return p_mct_mod;

error:
  if (p_mod) {
    module_imglib_deinit(p_mct_mod);
  } else if (p_mct_mod) {
    mct_module_destroy(p_mct_mod);
  }
  return NULL;
}
