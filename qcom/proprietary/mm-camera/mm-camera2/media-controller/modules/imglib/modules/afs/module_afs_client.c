
/**********************************************************************
* Copyright (c) 2014 Qualcomm Technologies, Inc. All Rights Reserved. *
* Qualcomm Technologies Proprietary and Confidential.                 *
**********************************************************************/

#include <cutils/properties.h>
#include <linux/media.h>
#include "mct_module.h"
#include "module_afs.h"
#include "mct_stream.h"
#include "modules.h"
#include "fd_chromatix.h"
#include <dlfcn.h>

//#define TEST_AFS
//#define AFS_PROFILE

#ifdef AFS_PROFILE
#define AFS_START_MEASURE \
  struct timeval start_time, mid_time, end_time;\
  gettimeofday(&start_time, NULL); \
  mid_time = start_time \

#define AFS_MIDDLE_TIME \
do { \
  gettimeofday(&end_time, NULL); \
  IDBG_ERROR("%s]%d Middle mtime  %lu ms",  __func__, __LINE__, \
  ((end_time.tv_sec * 1000) + (end_time.tv_usec / 1000)) - \
  ((mid_time.tv_sec * 1000) + (mid_time.tv_usec / 1000))); \
  mid_time = end_time; \
} while (0)\

#define AFS_END_MEASURE \
do { \
  gettimeofday(&end_time, NULL); \
  IDBG_ERROR("End of measure Total %lu ms", \
  ((end_time.tv_sec * 1000) + (end_time.tv_usec / 1000)) - \
  ((start_time.tv_sec * 1000) + (start_time.tv_usec / 1000))); \
} while (0) \

#else
#define AFS_START_MEASURE \
  do{}while(0) \

#define AFS_MIDDLE_TIME \
  do{}while(0) \

#define AFS_END_MEASURE \
  do{}while(0) \

#endif

/** afs_lib_info_t:
 *  @ptr: library handle
 *  @af_sw_stats_iir_float: function pointer for the AF stats
 *                        algorithm
 **/
typedef struct {
  void *ptr;
  double (*af_sw_stats_iir_float) (uint8_t *img_orig,
    int N,
    int height,
    int width,
    int roi_h_offset,
    int rgn_width,
    int roi_v_offset,
    int rgn_height,
    double *coeffa,
    double *coeffb,
    int coeff_len);
} afs_lib_info_t;

static afs_lib_info_t g_afs_lib;

/**
 * Function: module_afs_client_get_buf
 *
 * Description: get the buffer for AF sw stats
 *
 * Arguments:
 *   @p_client - afs client pointer
 *
 * Return values:
 *   buffer index
 *
 * Notes: none
 **/
inline int module_afs_client_get_buf(afs_client_t *p_client)
{
  uint32_t idx = p_client->buf_idx;
  p_client->buf_idx = (p_client->buf_idx + 1) % MAX_NUM_AFS_FRAMES;
  return idx;
}

/**
 * Function: module_afs_client_destroy
 *
 * Description: This function is used to destroy the afs client
 *
 * Arguments:
 *   @p_client: afs client
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
void module_afs_client_destroy(afs_client_t *p_client)
{
  int rc = IMG_SUCCESS;
  uint32_t i;
  img_frame_t *p_frame;

  if (NULL == p_client) {
    return;
  }

  IDBG_MED("%s:%d] state %d", __func__, __LINE__, p_client->state);

  if (IMGLIB_STATE_STARTED == p_client->state) {
    module_afs_client_stop(p_client);
  }

  p_client->state = IMGLIB_STATE_IDLE;

  pthread_mutex_destroy(&p_client->mutex);
  for (i = 0; i < MAX_NUM_AFS_FRAMES; i++) {
    p_frame = &p_client->p_frame[i];
    if (IMG_ADDR(p_frame)) {
      free(IMG_ADDR(p_frame));
      IMG_ADDR(p_frame) = NULL;
    }
  }

  free(p_client);
  p_client = NULL;
  IDBG_MED("%s:%d] X", __func__, __LINE__);
}

/**
 * Function: module_afs_client_create
 *
 * Description: This function is used to create the afs client
 *
 * Arguments:
 *   @p_mct_mod: mct module pointer
 *   @p_port: mct port pointer
 *   @identity: identity of the stream
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
int module_afs_client_create(mct_module_t *p_mct_mod, mct_port_t *p_port,
  uint32_t identity, mct_stream_info_t *stream_info)
{
  int rc = IMG_SUCCESS;
  afs_client_t *p_client = NULL;
  module_afs_t *p_mod = (module_afs_t *)p_mct_mod->module_private;
  mct_list_t *p_temp_list = NULL;

  IDBG_MED("%s:%d]", __func__, __LINE__);
  p_client = (afs_client_t *)malloc(sizeof(afs_client_t));
  if (NULL == p_client) {
    IDBG_ERROR("%s:%d] client alloc failed", __func__, __LINE__);
    return IMG_ERR_NO_MEMORY;
  }

  /* initialize the variables */
  memset(p_client, 0x0, sizeof(afs_client_t));
  pthread_mutex_init(&p_client->mutex, NULL);
  p_client->state = IMGLIB_STATE_IDLE;
  p_client->stream_info = stream_info;

  /* initialize if required */
  p_client->state = IMGLIB_STATE_INIT;

  p_client->p_sinkport = p_port;
  p_client->identity = identity;
  p_port->port_private = p_client;
  p_client->port = p_port;
  p_client->buf_idx = 0;
  p_client->p_mod = p_mod;
  p_client->active = TRUE;

  p_client->frame_skip_cnt = 0;

  IDBG_MED("%s:%d] port %p client %p X", __func__, __LINE__, p_port, p_client);
  return rc;

error:
  if (p_client) {
    module_afs_client_destroy(p_client);
    p_client = NULL;
  }
  return rc;
}

/**
 * Function: afs_map_buffer
 *
 * Description: This method is used for updating the AFS
 *      buffer structure from MCT structure with AFS buffers
 *
 * Input parameters:
 *   @data - MCT stream buffer mapping
 *   @user_data - img buffer structure
 *
 * Return values:
 *     true/false
 *
 * Notes: none
 **/
boolean afs_map_buffer(void *data, void *user_data)
{
  boolean rc = FALSE;
  mct_stream_map_buf_t *p_buf = (mct_stream_map_buf_t *)data;
  afs_client_t *p_client = (afs_client_t *)user_data;
  int idx = 0;

  IDBG_MED("%s:%d] p_buf %p p_buf_info %p", __func__, __LINE__, p_buf,
    p_client);
  if (!p_buf || !p_client) {
    IDBG_ERROR("%s:%d failed", __func__, __LINE__);
    return TRUE;
  }

  /* Check the buffer count */
  if (p_client->buffer_cnt >= MAX_NUM_FRAMES)
    return TRUE;

  idx = p_client->buffer_cnt;
  p_client->p_map_buf[idx] = *p_buf;
  IDBG_MED("%s:%d] buffer cnt %d idx %d addr %p", __func__, __LINE__,
    p_client->buffer_cnt, p_buf->buf_index,
    p_buf->buf_planes[0].buf);
  p_client->buffer_cnt++;
  return TRUE;
}

/**
 * Function: module_afs_client_map_buffers
 *
 * Description: This function is used to map the buffers when
 * the stream is started
 *
 * Arguments:
 *   @p_client: afs client
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
int module_afs_client_map_buffers(afs_client_t *p_client)
{
  mct_stream_info_t *stream_info;
  uint32_t i = 0;
  img_frame_t *p_frame;

  if (!(p_client && p_client->stream_info))
    return IMG_ERR_INVALID_INPUT;

  if (p_client->state == IMGLIB_STATE_INIT ||
      p_client->state == IMGLIB_STATE_IDLE) {
     IDBG_ERROR("%s:%d] client not started", __func__, __LINE__);
     return IMG_SUCCESS;
  }

  stream_info = p_client->stream_info;
  if ((uint32_t)stream_info->dim.width > MAX_AFS_WIDTH ||
    (uint32_t)stream_info->dim.height > MAX_AFS_HEIGHT) {
    IDBG_MED("%s:%d] Exceeded max size %dx%d", __func__, __LINE__,
      stream_info->dim.width, stream_info->dim.width);
    return IMG_ERR_INVALID_INPUT;
  }

  p_client->buffer_cnt = 0;

  mct_list_traverse(stream_info->img_buffer_list, afs_map_buffer,
    p_client);

  for (i = 0; i < MAX_NUM_AFS_FRAMES; i++) {
    p_frame = &p_client->p_frame[i];
    p_frame->frame_cnt = 1;
    p_frame->idx = i;
    p_frame->info.width = stream_info->dim.width;
    p_frame->info.height = stream_info->dim.height;
    p_frame->frame[0].plane_cnt = 1;
    p_frame->frame[0].plane[0].width = stream_info->dim.width;
    p_frame->frame[0].plane[0].height = stream_info->dim.height;
    p_frame->frame[0].plane[0].plane_type = PLANE_Y;
    p_frame->frame[0].plane[0].length =
     stream_info->dim.width * stream_info->dim.height;

    IMG_ADDR(p_frame) = malloc(IMG_Y_LEN(p_frame));
    if (NULL == IMG_ADDR(p_frame)) {
      IDBG_ERROR("%s:%d] buffer create failed", __func__, __LINE__);
      return IMG_ERR_NO_MEMORY;
    }
  }
  IDBG_MED("%s:%d] dim %dx%d", __func__, __LINE__,
    stream_info->dim.width, stream_info->dim.height);
  return IMG_SUCCESS;
}

/**
 * Function: module_afs_client_unmap_buffers
 *
 * Description: This function is used to unmap the buffers when
 * the stream is started
 *
 * Arguments:
 *   @p_client: afs client
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
int module_afs_client_unmap_buffers(afs_client_t *p_client)
{
  uint32_t i = 0;
  img_frame_t *p_frame;

  for (i = 0; i < MAX_NUM_AFS_FRAMES; i++) {
    p_frame = &p_client->p_frame[i];
    if (IMG_ADDR(p_frame)) {
      free(IMG_ADDR(p_frame));
      IMG_ADDR(p_frame) = NULL;
    }
  }
  p_client->buffer_cnt = 0;
  return IMG_SUCCESS;
}

/**
 * Function: module_afs_client_set_scale_ratio
 *
 * Description: Set face detection scale ratio.
 *
 * Arguments:
 *   @p_client: afs client
 *   @stream_crop: Stream crop event structure
 *
 * Return values:
 *     imaging error values
 *
 * Notes: Calculate just scale ratio which we assume that will be applied
 *  to get preview resolution.
 **/
int module_afs_client_set_scale_ratio(afs_client_t *p_client,
  mct_bus_msg_stream_crop_t *stream_crop)
{
  int rc = IMG_SUCCESS;

  if (!(p_client && p_client->stream_info) || !stream_crop)
    return IMG_ERR_INVALID_INPUT;

  if (!stream_crop->crop_out_x || !stream_crop->crop_out_y) {
    p_client->crop_info.pos.x = 0;
    p_client->crop_info.pos.y = 0;
    p_client->crop_info.size.width = p_client->stream_info->dim.width;
    p_client->crop_info.size.height = p_client->stream_info->dim.height;
  } else {
    p_client->crop_info.pos.x = stream_crop->x;
    p_client->crop_info.pos.y = stream_crop->y;
    p_client->crop_info.size.width = stream_crop->crop_out_x;
    p_client->crop_info.size.height= stream_crop->crop_out_y;
  }

  if ((stream_crop->width_map > 0) &&
    (stream_crop->height_map > 0)) {
    p_client->camif_trans_info.h_scale =
      (float)p_client->stream_info->dim.width /
      (float)stream_crop->width_map;
      (float)p_client->stream_info->dim.width;
    p_client->camif_trans_info.v_scale =
      (float)p_client->stream_info->dim.height/
      (float)stream_crop->height_map;
  } else {
    p_client->camif_trans_info.h_scale =
      p_client->camif_trans_info.v_scale = 1.0;
  }
  p_client->camif_trans_info.h_offset =
    stream_crop->x_map;
  p_client->camif_trans_info.v_offset =
    stream_crop->y_map;
  IDBG_MED("%s:%d] [FD_CAMIF] Map(%d %d %d %d)",
    __func__, __LINE__,
    stream_crop->width_map,
    stream_crop->height_map,
    stream_crop->x_map,
    stream_crop->y_map);
  return rc;
}

/**
 * Function: module_afs_client_start
 *
 * Description: This function is used to start the afs
 *              client
 *
 * Arguments:
 *   @p_client: afs client
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
int module_afs_client_start(afs_client_t *p_client)
{
  int rc = IMG_SUCCESS;

  IDBG_MED("%s:%d] E", __func__, __LINE__);

  if (p_client->state != IMGLIB_STATE_INIT) {
    IDBG_ERROR("%s:%d] invalid state %d",
      __func__, __LINE__, rc);
    return IMG_ERR_INVALID_OPERATION;
  }

  p_client->state = IMGLIB_STATE_STARTED;
  IDBG_MED("%s:%d] X", __func__, __LINE__);

  return rc;

error:
  IDBG_MED("%s:%d] error %d X", __func__, __LINE__, rc);
  return rc;
}

/**
 * Function: module_afs_client_stop
 *
 * Description: This function is used to stop the afs
 *              client
 *
 * Arguments:
 *   @p_client: afs client
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
int module_afs_client_stop(afs_client_t *p_client)
{
  int rc = IMG_SUCCESS;
  module_afs_t *p_mod = (module_afs_t *)(p_client->p_mod);

  p_client->state = IMGLIB_STATE_INIT;
  return rc;
}

/**
 * Function: module_afs_client_check_boundary
 *
 * Description: validates the boundary of the AFS cordiantes
 *
 * Arguments:
 *   @p_rect - AFS input cordinates
 *   @width - Preview width
 *   @height - Preview height
 *
 * Return values:
 *   TRUE/FALSE
 *
 * Notes: none
 **/
static inline boolean module_afs_client_check_boundary(
  img_rect_t *p_rect,
  int width,
  int height)
{
  if ((p_rect->pos.x < 0) || (p_rect->pos.y < 0) ||
    (p_rect->size.width <= 0) || (p_rect->size.height <= 0) ||
    ((p_rect->pos.x + p_rect->size.width) > width) ||
    ((p_rect->pos.y + p_rect->size.height) > height)) {
    /* out of boundary */
    IMG_PRINT_RECT(p_rect);
    return FALSE;
  }
  IMG_PRINT_RECT(p_rect);
  return TRUE;
}

/**
 * Function: module_afs_client_process
 *
 * Description: This function is for processing the algorithm
 *
 * Arguments:
 *   @p_userdata: client
 *   @data: function parameter
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
void module_afs_client_process(void *p_userdata, void *data)
{
  afs_client_t *p_client = (afs_client_t *)p_userdata;
  img_frame_t *p_frame = (img_frame_t *)data;
  int num_skip;
  img_rect_t roi;
  mct_imglib_af_config_t af_cfg;
  double fV;

  IDBG_LOW("%s:%d] ", __func__, __LINE__);
  if (!p_userdata || !p_frame) {
    IDBG_ERROR("%s:%d] invalid input", __func__, __LINE__);
    return;
  }

  if (!g_afs_lib.ptr) {
    IDBG_ERROR("%s:%d] invalid operation", __func__, __LINE__);
    return;
  }

  pthread_mutex_lock(&p_client->mutex);
  if (IMGLIB_STATE_PROCESSING != p_client->state) {
    IDBG_MED("%s:%d] not in proper state", __func__, __LINE__);
    pthread_mutex_unlock(&p_client->mutex);
    return;
  }

  af_cfg = p_client->cur_af_cfg;

  if (!af_cfg.enable) {
    IDBG_MED("%s:%d] not enabled", __func__, __LINE__);
    pthread_mutex_unlock(&p_client->mutex);
    return;
  }
  pthread_mutex_unlock(&p_client->mutex);

  num_skip = MAX(ceil(IMG_WIDTH(p_frame) / 640),
    ceil(IMG_HEIGHT(p_frame) / 480));

  if (!num_skip)
    num_skip = 1;
  num_skip = 1;

  IDBG_MED("%s:%d] [AS_SW_DBG] %dx%d skip %d", __func__, __LINE__,
    IMG_WIDTH(p_frame), IMG_HEIGHT(p_frame), num_skip);

  roi.pos.x = IMG_TRANSLATE2(af_cfg.roi.left,
    p_client->camif_trans_info.h_scale,
    p_client->camif_trans_info.h_offset);
  roi.pos.y = IMG_TRANSLATE2(af_cfg.roi.top,
    p_client->camif_trans_info.v_scale,
    p_client->camif_trans_info.v_offset);
  roi.size.width = IMG_TRANSLATE2(af_cfg.roi.width,
    p_client->camif_trans_info.h_scale, 0);
  roi.size.height = IMG_TRANSLATE2(af_cfg.roi.height,
    p_client->camif_trans_info.v_scale, 0);

  if (!module_afs_client_check_boundary(&roi, IMG_WIDTH(p_frame),
    IMG_HEIGHT(p_frame))) {
    roi.pos.x = roi.pos.y = 0;
    roi.size.width = IMG_WIDTH(p_frame);
    roi.size.height = IMG_HEIGHT(p_frame);
  }

#ifdef TEST_AFS
  roi.size.width = IMG_WIDTH(p_frame)/6;
  roi.size.height = IMG_HEIGHT(p_frame)/6;
  roi.pos.x = (double)(2.5/6.0) * (double)af_cfg.roi.width;
  roi.pos.y = (double)(2.5/6.0) * (double)af_cfg.roi.height;
#endif

  IDBG_MED("%s:%d] [AS_SW_DBG] Map(%d %d %d %d)",
    __func__, __LINE__,
    roi.pos.x,
    roi.pos.y,
    roi.size.width,
    roi.size.height);

#ifdef TEST_AFS
  double lcoeffa[] = {1.0, -1.758880, 0.930481, 1.0, -1.817633, 0.940604};
  double lcoeffb[] = {0.034788, 0.000000, -0.034788, 0.059808, 0.000000, -0.059808};
  uint32_t i;
  for (i = 0; i < 6; i++) {
    af_cfg.coeffa[i] = lcoeffa[i];
    af_cfg.coeffb[i] = lcoeffb[i];
  }
  af_cfg.coeff_len = 6;
#endif

  AFS_START_MEASURE;
  fV = g_afs_lib.af_sw_stats_iir_float(IMG_ADDR(p_frame),
    num_skip,
    IMG_HEIGHT(p_frame),
    IMG_WIDTH(p_frame),
    roi.pos.x,
    roi.size.width,
    roi.pos.y,
    roi.size.height,
    af_cfg.coeffa,
    af_cfg.coeffb,
    af_cfg.coeff_len);

  AFS_END_MEASURE;

  /* create MCT event and send */
  if (fV > 0) {
    mct_event_t mct_event;
    mct_imglib_af_output_t af_out;
    memset(&mct_event, 0x0, sizeof(mct_event_t));
    memset(&af_out, 0x0, sizeof(mct_imglib_af_output_t));
    mct_event.u.module_event.type = MCT_EVENT_MODULE_IMGLIB_AF_OUTPUT;
    mct_event.u.module_event.module_event_data = (void *)&af_out;
    mct_event.type = MCT_EVENT_MODULE_EVENT;
    mct_event.identity = p_client->identity;
    mct_event.direction = MCT_EVENT_UPSTREAM;
    mct_port_send_event_to_peer(p_client->port, &mct_event);
  }
  IDBG_LOW("%s:%d] %f X", __func__, __LINE__, fV);
  return;
}

/**
 * Function: module_afs_client_handle_buffer
 *
 * Description: This function is for handling the buffers
 *            sent from the peer modules
 *
 * Arguments:
 *   @p_client: afs client
 *   @buf_idx: index of the buffer to be processed
 *   @frame_id: frame id
 *   @p_frame_idx: pointer to frame index
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
int module_afs_client_handle_buffer(afs_client_t *p_client,
  uint32_t buf_idx,
  uint32_t frame_id,
  int32_t *p_frame_idx)
{
  img_frame_t *p_frame;
  int rc = IMG_SUCCESS;
  int img_idx;
  mct_stream_map_buf_t *p_map_buf;

  *p_frame_idx = -1;
  /* process the frame only in IMGLIB_STATE_PROCESSING state */
  pthread_mutex_lock(&p_client->mutex);
  if (TRUE != p_client->active) {
    pthread_mutex_unlock(&p_client->mutex);
    return IMG_SUCCESS;
  }

  if (IMGLIB_STATE_PROCESSING != p_client->state) {
    pthread_mutex_unlock(&p_client->mutex);
    return IMG_SUCCESS;
  }

  IDBG_MED("%s:%d] count %d", __func__, __LINE__, p_client->current_count);

  if (0 != p_client->current_count) {
    IDBG_LOW("%s:%d] Skip frame", __func__, __LINE__);
    p_client->current_count =
      (p_client->current_count + 1)%(p_client->frame_skip_cnt + 1);
    pthread_mutex_unlock(&p_client->mutex);
    return IMG_SUCCESS;
  }
  p_client->current_count =
    (p_client->current_count + 1)%(p_client->frame_skip_cnt+1);


  img_idx = module_afs_client_get_buf(p_client);
  IDBG_MED("%s:%d] img_idx %d", __func__, __LINE__, img_idx);

  if ((buf_idx >= p_client->buffer_cnt)
    || (img_idx >= MAX_NUM_AFS_FRAMES)) {
    IDBG_ERROR("%s:%d] invalid buffer index %d img_idx %d",
      __func__, __LINE__, buf_idx, img_idx);
    pthread_mutex_unlock(&p_client->mutex);
    return IMG_ERR_OUT_OF_BOUNDS;
  }

  p_frame = &p_client->p_frame[img_idx];
  p_map_buf = &p_client->p_map_buf[buf_idx];
  IDBG_MED("%s:%d] buffer %d %p %p", __func__, __LINE__, buf_idx,
    p_frame->frame[0].plane[0].addr,
    p_map_buf->buf_planes[0].buf);

  memcpy(p_frame->frame[0].plane[0].addr, p_map_buf->buf_planes[0].buf,
    p_frame->frame[0].plane[0].length);

  *p_frame_idx = img_idx;
  /* return the buffer */
  pthread_mutex_unlock(&p_client->mutex);

  return rc;
}

/**
 * Function: module_afs_client_handle_ctrl_parm
 *
 * Description: This function is used to handle the ctrl
 *             commands passed from the MCTL
 *
 * Arguments:
 *   @p_ctrl_event: pointer to mctl ctrl events
 *   @p_client: afs client
 *
 * Return values:
 *     error/success
 *
 * Notes: none
 **/
int module_afs_client_handle_ctrl_parm(afs_client_t *p_client,
  mct_event_control_parm_t *param)
{
  int status = IMG_SUCCESS;

  if (NULL == param)
    return status;

  IDBG_LOW("%s:%d] param %d", __func__, __LINE__, param->type);
  switch(param->type) {
  default:
    break;
  }
  return status;
}

/**
 * Function: module_afs_load
 *
 * Description: This function is used to load frame
 *             algo library
 *
 * Input parameters:
 *   none
 *
 * Return values:
 *     none
 *
 * Notes: none
 **/
int module_afs_load()
{
  if (g_afs_lib.ptr) {
    IDBG_ERROR("%s:%d] library already loaded", __func__, __LINE__);
    return IMG_SUCCESS;
  }

  g_afs_lib.ptr = dlopen("libmmcamera2_frame_algorithm.so", RTLD_NOW);
  if (!g_afs_lib.ptr) {
    IDBG_ERROR("%s:%d] Error opening Frame algo library", __func__, __LINE__);
    return IMG_ERR_NOT_FOUND;
  }

  *(void **)&(g_afs_lib.af_sw_stats_iir_float) =
    dlsym(g_afs_lib.ptr, "af_sw_stats_iir_float");
  if (!g_afs_lib.af_sw_stats_iir_float) {
    IDBG_ERROR("%s:%d] Error linking af_sw_stats_iir_float",
      __func__, __LINE__);
    dlclose(g_afs_lib.ptr);
    g_afs_lib.ptr = NULL;
    return IMG_ERR_NOT_FOUND;
  }

  IDBG_HIGH("%s:%d] Frame algo library loaded successfully", __func__, __LINE__);
  return IMG_SUCCESS;
}

/**
 * Function: module_afs_unload
 *
 * Description: This function is used to unload frame
 *             algo library
 *
 * Input parameters:
 *   none
 *
 * Return values:
 *     none
 *
 * Notes: none
 **/
void module_afs_unload()
{
  int rc = 0;
  IDBG_HIGH("%s:%d] ptr %p", __func__, __LINE__, g_afs_lib.ptr);
  if (g_afs_lib.ptr) {
    rc = dlclose(g_afs_lib.ptr);
    if (rc < 0)
      IDBG_HIGH("%s:%d] error %s", __func__, __LINE__, dlerror());
    g_afs_lib.ptr = NULL;
  }
}
