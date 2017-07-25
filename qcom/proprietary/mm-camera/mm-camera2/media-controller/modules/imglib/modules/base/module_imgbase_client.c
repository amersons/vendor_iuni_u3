/***************************************************************************
* Copyright (c) 2013-2014 Qualcomm Technologies, Inc. All Rights Reserved. *
* Qualcomm Technologies Proprietary and Confidential.                      *
****************************************************************************/

#include <linux/media.h>
#include "mct_module.h"
#include "module_imgbase.h"
#include "mct_stream.h"
#include "pthread.h"
#include "chromatix.h"
#include "mct_stream.h"
#include "module_imglib_common.h"

/**
 * Function: module_imgbase_client_handle_buf_done
 *
 * Description: Function to handle input buf done event
 *
 * Arguments:
 *   @p_client - IMGLIB_BASE client
 *   @p_frame - frame for buf done
 *
 * Return values:
 *   none
 *
 * Notes: none
 **/
static void module_imgbase_client_handle_buf_done(
  imgbase_client_t *p_client,
  img_frame_t *p_frame)
{
  img_component_ops_t *p_comp;
  isp_buf_divert_t *p_buf_divert;
  boolean ret;
  int rc;
  module_imgbase_t *p_mod;

  if (!p_client || !p_frame) {
    /* invalid input */
    return;
  }
  p_comp = &p_client->comp;
  p_mod = (module_imgbase_t *)p_client->p_mod;

  if (!p_mod)
    return;

  IDBG_MED("%s:%d] buffer idx %d streamon %d", __func__, __LINE__,
    p_frame->idx,
    p_client->stream_on);
  p_buf_divert = (isp_buf_divert_t *)p_frame->private_data;

  if (!p_buf_divert) {
    IDBG_ERROR("%s:%d] buffer divert NULL",
      __func__, __LINE__);
    goto end;
  }

  /* release buffer to kernel directly on streamoff */
  if (!p_client->stream_on) {
    rc = module_imglib_common_release_buffer(p_mod->subdevfd,
      p_client->identity,
      p_frame->idx,
      p_frame->frame_id,
      FALSE);
    goto end;
  }

  if (p_client->p_sinkport) {
    isp_buf_divert_ack_t buff_divert_ack;
    memset(&buff_divert_ack,  0,  sizeof(buff_divert_ack));
    buff_divert_ack.buf_idx = p_buf_divert->buffer.index;
    buff_divert_ack.is_buf_dirty = 1;
    buff_divert_ack.identity = p_buf_divert->identity;
    ret = mod_imgbase_send_event(p_buf_divert->identity, TRUE,
      MCT_EVENT_MODULE_BUF_DIVERT_ACK, buff_divert_ack);
    if (!ret) {
      IDBG_ERROR("%s:%d] mod_imgbase_send_event failed %d",
        __func__, __LINE__, ret);
    }
  }
  /* ToDo for src port */

end:
  if (p_frame) {
    free(p_frame);
  }
}

/**
 * Function: module_imgbase_client_handle_outbuf_done
 *
 * Description: Function to handle output buf done event
 *
 * Arguments:
 *   @p_client - IMGLIB_BASE client
 *   @p_frame - frame for buf done
 *   @buf_done - flag to indicate if the buf done needs to be
 *             called
 *
 * Return values:
 *   none
 *
 * Notes: none
 **/
static void module_imgbase_client_handle_outbuf_done(
  imgbase_client_t *p_client,
  img_frame_t *p_frame,
  int8_t buf_done)
{
  int rc = IMG_SUCCESS;
  int *p_frame_id;
  boolean ret;

  if (!p_client || !p_frame) {
    IDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    return;
  }
  module_imgbase_t *p_mod = (module_imgbase_t *)p_client->p_mod;

  p_frame_id = (int *)p_frame->private_data;
  IDBG_MED("%s:%d] buffer idx %d frame_id %d", __func__, __LINE__,
    p_frame->idx, *p_frame_id);

  if (!p_frame_id) {
    IDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    return;
  }

  if (p_client->p_srcport) {
    /* ToDo */
  } else if (!p_mod->caps.inplace_algo) {
    IDBG_MED("%s:%d] Buffer %d %x %d %d",
      __func__, __LINE__,
      p_mod->subdevfd,
      p_client->identity,
      p_frame->idx,
      *p_frame_id);
    rc = module_imglib_common_release_buffer(p_mod->subdevfd,
      p_client->identity,
      p_frame->idx,
      *p_frame_id,
      buf_done);

    if (IMG_ERROR(rc)) {
      IDBG_ERROR("%s:%d] Error getting output buffer %d",
        __func__, __LINE__,
        p_frame->idx);
    }
  }

end:
  if (p_frame) {
    free(p_frame);
  }
}

/**
 * Function: module_imgbase_client_enqueue_streambuf
 *
 * Description: function to enqueue stream buffer
 *
 * Arguments:
 *   @p_client - IMGLIB_BASE client
 *   @p_meta - pointer to the meta
 *   @prop: image properties
 *
 * Return values:
 *   none
 *
 * Notes: none
 **/
static void module_imgbase_client_enqueue_streambuf(
  imgbase_client_t *p_client,
  img_meta_t *p_meta,
  cam_stream_img_prop_t *prop)
{
  if (!p_client->stream_on) {
    IDBG_LOW("%s:%d] skip create streambuf", __func__, __LINE__);
    return;
  }

  cam_stream_parm_buffer_t *p_stream_buf =
    calloc(1, sizeof(cam_stream_parm_buffer_t));
  if (p_stream_buf && p_meta) {
    p_stream_buf->imgProp = *prop;
    p_stream_buf->type = CAM_STREAM_PARAM_TYPE_GET_IMG_PROP;
    IDBG_HIGH("%s:%d] (%d %d %d %d) (%d %d) (%d %d)",
      __func__, __LINE__,
      p_stream_buf->imgProp.crop.left,
      p_stream_buf->imgProp.crop.top,
      p_stream_buf->imgProp.crop.width,
      p_stream_buf->imgProp.crop.height,
      p_stream_buf->imgProp.input.width,
      p_stream_buf->imgProp.input.height,
      p_stream_buf->imgProp.output.width,
      p_stream_buf->imgProp.output.height);
    if (IMG_ERROR(img_q_enqueue(&p_client->stream_parm_q, p_stream_buf))) {
      free(p_stream_buf);
      p_stream_buf = NULL;
    }
  }
}
/**
 * Function: module_imgbase_client_event_handler
 *
 * Description: event handler for Imglib base client
 *
 * Arguments:
 *   @p_appdata - IMGLIB_BASE client
 *   @p_event - pointer to the event
 *
 * Return values:
 *   IMG_SUCCESS
 *   IMG_ERR_GENERAL
 *
 * Notes: none
 **/
static int module_imgbase_client_event_handler(void* p_appdata,
  img_event_t *p_event)
{
  imgbase_client_t *p_client;
  img_component_ops_t *p_comp;
  int rc = IMG_SUCCESS;

  IDBG_MED("%s:%d] ", __func__, __LINE__);
  if ((NULL == p_event) || (NULL == p_appdata)) {
    IDBG_ERROR("%s:%d] invalid event", __func__, __LINE__);
    return IMG_ERR_GENERAL;
  }

  p_client = (imgbase_client_t *)p_appdata;
  p_comp = &p_client->comp;
  IDBG_LOW("%s:%d] type %d", __func__, __LINE__, p_event->type);

  switch (p_event->type) {
  case QIMG_EVT_IMG_BUF_DONE: {
    module_imgbase_client_handle_buf_done(p_client, p_event->d.p_frame);
    break;
  }
  case QIMG_EVT_IMG_OUT_BUF_DONE: {
    module_imgbase_client_handle_outbuf_done(p_client, p_event->d.p_frame,
      TRUE);
    break;
  }
  case QIMG_EVT_META_BUF_DONE: {
    img_meta_t *p_meta = p_event->d.p_meta;
    IDBG_MED("%s:%d] QIMG_EVT_META_BUF_DONE p_meta %p",
      __func__, __LINE__, p_meta);

    if (p_meta) {
      cam_stream_img_prop_t prop;
      memset(&prop, 0x0, sizeof(cam_stream_img_prop_t));
      cam_rect_t *p_rect = &prop.crop;
      p_rect->left = p_meta->output_crop.pos.x;
      p_rect->top = p_meta->output_crop.pos.y;
      p_rect->width = p_meta->output_crop.size.width;
      p_rect->height = p_meta->output_crop.size.height;
      module_imgbase_client_enqueue_streambuf(p_client, p_meta, &prop);
      free(p_event->d.p_meta);
    }
    p_client->p_current_meta = NULL;
    break;
  }
  case QIMG_EVT_ERROR:
    IDBG_HIGH("%s %d: IMGLIB_BASE Error", __func__, __LINE__);
    break;
  default:
    break;
  }
  IDBG_LOW("%s:%d] type %d X", __func__, __LINE__, p_event->type);
  return rc;
}

/**
 * Function: module_imgbase_client_getbuf
 *
 * Description: This function is to fetching the input buffer
 *           based in buffer info
 * Arguments:
 *   @p_client: imgbase client
 *   @p_buf_divert: ISP buffer divert event structure
 *   @pframe: frame pointer
 *   @native_buf: flag to indicate if its a native buffer
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
static int module_imgbase_client_getbuf(imgbase_client_t *p_client,
  isp_buf_divert_t *p_buf_divert,
  img_frame_t *pframe,
  int native_buf)
{
  int rc = IMG_SUCCESS;
  int i = 0;
  int buf_idx;
  uint32_t size;
  uint8_t *p_addr;
  mct_module_t *p_mct_mod;
  uint32_t padded_size;
  int fd = -1;
  int stride, scanline;

  pframe->frame_cnt = 1;
  pframe->info.width = p_client->stream_info->dim.width;
  pframe->info.height = p_client->stream_info->dim.height;
  size = pframe->info.width * pframe->info.height;
  pframe->frame[0].plane_cnt = 2;
  stride = p_client->stream_info->buf_planes.plane_info.mp[0].stride;
  scanline = p_client->stream_info->buf_planes.plane_info.mp[0].scanline;
  pframe->idx = buf_idx = p_buf_divert->buffer.index;
  padded_size = stride * scanline;
  pframe->frame_id = p_buf_divert->buffer.sequence;

  if (NULL == p_client->p_sinkport) {
    IDBG_ERROR("%s:%d] NULL Sink port", __func__, __LINE__);
    return IMG_ERR_INVALID_INPUT;
  }

  p_mct_mod = MCT_MODULE_CAST((MCT_PORT_PARENT(p_client->p_sinkport))->data);
  IDBG_MED("%s:%d] Dimension %dx%d buf_idx %d %x mod %p port %p pproc %p"
    " pad %dx%d frame_id %d",
    __func__, __LINE__,
    pframe->info.width, pframe->info.height, buf_idx,
    p_client->identity,
    p_mct_mod,
    p_client->p_sinkport,
    p_client->parent_mod,
    stride,
    scanline,
    p_buf_divert->buffer.sequence);

  if (!native_buf) {
    p_addr = mct_module_get_buffer_ptr(buf_idx,
      p_mct_mod,
      IMGLIB_SESSIONID(p_client->identity),
      IMGLIB_STREAMID(p_client->identity));
  } else {
    p_addr = p_buf_divert->vaddr;
    fd = p_buf_divert->fd;
    IDBG_MED("%s:%d] Native Buffer addr = %p, fd = %d",
     __func__, __LINE__, p_addr, fd);
  }

  if (NULL == p_addr) {
    IDBG_ERROR("%s:%d] NULL address", __func__, __LINE__);
    return IMG_ERR_INVALID_INPUT;
  }

  for (i = 0; i < pframe->frame[0].plane_cnt; i++) {
    pframe->frame[0].plane[i].fd = fd;
    pframe->frame[0].plane[i].offset = 0;
    if (i == 0) { /* Y plane */
      pframe->frame[0].plane[i].addr = p_addr;
      pframe->frame[0].plane[i].width = pframe->info.width;
      pframe->frame[0].plane[i].height = pframe->info.height;
      pframe->frame[0].plane[i].stride = stride;
      pframe->frame[0].plane[i].scanline = scanline;
      pframe->frame[0].plane[i].length =
       IMG_LENGTH(pframe->frame[0].plane[i]);
    } else { /* Chroma plane */
      pframe->frame[0].plane[i].addr = p_addr + padded_size;
      pframe->frame[0].plane[i].width = pframe->info.width;
      pframe->frame[0].plane[i].height = pframe->info.height/2;
      pframe->frame[0].plane[i].stride = stride;
      pframe->frame[0].plane[i].scanline = scanline/2;
      pframe->frame[0].plane[i].length =
       IMG_LENGTH(pframe->frame[0].plane[i]);
    }
  }

  return rc;
}

/**
 * Function: module_imgbase_client_get_outputbuf
 *
 * Description: This function is to fetching the output buffer
 *           based in buffer info
 * Arguments:
 *   @p_client: imgbase client
 *   @pframe: frame pointer
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
int module_imgbase_client_get_outputbuf(imgbase_client_t *p_client,
  img_frame_t *pframe)
{
  int rc = IMG_SUCCESS;
  int i = 0;
  int buf_idx;
  uint32_t size;
  uint8_t *p_addr;
  mct_module_t *p_mct_mod;
  uint32_t padded_size;
  int fd = -1;
  int stride, scanline;
  module_imgbase_t *p_mod = (module_imgbase_t *)p_client->p_mod;
  int out_idx;

  /* get output buffer */
  out_idx = module_imglib_common_get_buffer(p_mod->subdevfd,
    p_client->identity);
  if (out_idx < 0) {
    IDBG_ERROR("%s:%d] rc %d", __func__, __LINE__, rc);
    rc = IMG_ERR_GENERAL;
    goto error;
  }

  pframe->frame_cnt = 1;
  pframe->info.width = p_client->stream_info->dim.width;
  pframe->info.height = p_client->stream_info->dim.height;
  size = pframe->info.width * pframe->info.height;
  pframe->frame[0].plane_cnt = 2;
  stride = p_client->stream_info->buf_planes.plane_info.mp[0].stride;
  scanline = p_client->stream_info->buf_planes.plane_info.mp[0].scanline;
  pframe->idx = buf_idx = out_idx;
  padded_size = stride * scanline;

  if (NULL == p_client->p_sinkport) {
    IDBG_ERROR("%s:%d] NULL Sink port", __func__, __LINE__);
    rc = IMG_ERR_INVALID_INPUT;
    goto error;
  }

  p_mct_mod = MCT_MODULE_CAST((MCT_PORT_PARENT(p_client->p_sinkport))->data);
  IDBG_MED("%s:%d] Dimension %dx%d buf_idx %d %x mod %p port %p pproc %p"
    " pad %dx%d",
    __func__, __LINE__,
    pframe->info.width, pframe->info.height, buf_idx,
    p_client->identity,
    p_mct_mod,
    p_client->p_sinkport,
    p_client->parent_mod,
    stride,
    scanline);

  p_addr = mct_module_get_buffer_ptr(buf_idx,
    p_mct_mod,
    IMGLIB_SESSIONID(p_client->identity),
    IMGLIB_STREAMID(p_client->identity));

  if (NULL == p_addr) {
    IDBG_ERROR("%s:%d] NULL address", __func__, __LINE__);
    rc = IMG_ERR_INVALID_INPUT;
    goto error;
  }

  for (i = 0; i < pframe->frame[0].plane_cnt; i++) {
    pframe->frame[0].plane[i].fd = fd;
    pframe->frame[0].plane[i].offset = 0;
    if (i == 0) { /* Y plane */
      pframe->frame[0].plane[i].addr = p_addr;
      pframe->frame[0].plane[i].width = pframe->info.width;
      pframe->frame[0].plane[i].height = pframe->info.height;
      pframe->frame[0].plane[i].stride = stride;
      pframe->frame[0].plane[i].scanline = scanline;
      pframe->frame[0].plane[i].length =
       IMG_LENGTH(pframe->frame[0].plane[i]);
    } else { /* Chroma plane */
      pframe->frame[0].plane[i].addr = p_addr + padded_size;
      pframe->frame[0].plane[i].width = pframe->info.width;
      pframe->frame[0].plane[i].height = pframe->info.height/2;
      pframe->frame[0].plane[i].stride = stride;
      pframe->frame[0].plane[i].scanline = scanline/2;
      pframe->frame[0].plane[i].length =
       IMG_LENGTH(pframe->frame[0].plane[i]);
    }
  }

  return rc;

error:
  IDBG_ERROR("%s:%d] Cannot get output buffer", __func__, __LINE__);
  return rc;
}

/**
 * Function: module_imgbase_client_handle_buffer
 *
 * Description: This function is used to handle the divert
 *            buffer event
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
  isp_buf_divert_t *p_buf_divert)
{
  int rc = IMG_SUCCESS;
  img_component_ops_t *p_comp = &p_client->comp;
  img_frame_t *p_frame, *p_out_frame;
  isp_buf_divert_t *l_buf_divert;
  module_imgbase_t *p_mod = (module_imgbase_t *)p_client->p_mod;
  uint8_t *poutframeinfo = NULL;
  uint8_t *pframeinfo = NULL;
  int *l_frameid;
  int frame_id;
  img_meta_t *p_meta = NULL;

  IDBG_LOW("%s:%d] ", __func__, __LINE__);

  pthread_mutex_lock(&p_client->mutex);
  if (!p_client->stream_on) {
    IDBG_LOW("%s:%d] Discard buffers", __func__, __LINE__);
    pthread_mutex_unlock(&p_client->mutex);
    return IMG_SUCCESS;
  }
  pthread_mutex_unlock(&p_client->mutex);

  pframeinfo = (uint8_t *)calloc(1, sizeof(img_frame_t) +
    sizeof(isp_buf_divert_t));

  if (!pframeinfo) {
    IDBG_ERROR("%s:%d] Error: Cannot allocate memory", __func__, __LINE__);
    return IMG_ERR_NO_MEMORY;
  }

  p_frame = (img_frame_t *)pframeinfo;
  l_buf_divert = (isp_buf_divert_t *)(pframeinfo + sizeof(img_frame_t));
  *l_buf_divert = *p_buf_divert;
  p_frame->private_data = l_buf_divert;

  // Get Frame
  rc = module_imgbase_client_getbuf(p_client, l_buf_divert, p_frame,
    l_buf_divert->native_buf);
  if (rc != IMG_SUCCESS) {
    IDBG_ERROR("%s:%d] Error: Cannot get frame", __func__, __LINE__);
    free(pframeinfo);
    rc = IMG_ERR_GENERAL;
    goto error;
  }

  frame_id = p_buf_divert->buffer.sequence;
  IDBG_HIGH("%s:%d] dim %dx%d id %d", __func__, __LINE__,
    p_frame->info.width, p_frame->info.height, frame_id);

  rc = IMG_COMP_Q_BUF(p_comp, p_frame, IMG_IN);
  if (rc != IMG_SUCCESS) {
    IDBG_ERROR("%s:%d] rc %d", __func__, __LINE__, rc);
    goto error;
  }
  p_client->cur_buf_cnt++;

  /* Send output buffer */
  if (p_client->cur_buf_cnt >= p_mod->caps.num_input) {
    poutframeinfo = (uint8_t *)calloc(1, sizeof(img_frame_t) + sizeof(int));
    if (!poutframeinfo) {
      IDBG_ERROR("%s:%d] Error: Cannot allocate memory", __func__, __LINE__);
      rc = IMG_ERR_NO_MEMORY;
      goto error;
    }

    p_out_frame = (img_frame_t *)poutframeinfo;
    l_frameid = p_out_frame->private_data =
      (uint8_t *)poutframeinfo + sizeof(img_frame_t);
    /* Queue output buffer */
    rc = module_imgbase_client_get_outputbuf(p_client, p_out_frame);
    if (IMG_ERROR(rc)) {
      IDBG_ERROR("%s:%d] rc %d", __func__, __LINE__, rc);
      goto error;
    }
    *l_frameid = frame_id;

    /* queue the buffer */
    rc = IMG_COMP_Q_BUF(p_comp, p_out_frame, IMG_OUT);
    if (rc != IMG_SUCCESS) {
      IDBG_ERROR("%s:%d] rc %d", __func__, __LINE__, rc);
      goto error;
    }

    /* Send meta buffer */
    p_meta = (img_meta_t *)calloc(1, sizeof(img_meta_t));
    if (!p_meta) {
      IDBG_ERROR("%s:%d] Error: Cannot allocate memory", __func__, __LINE__);
      rc = IMG_ERR_NO_MEMORY;
      goto error;
    }
    IDBG_MED("%s:%d] p_meta %p", __func__, __LINE__, p_meta);

    p_client->current_meta.zoom_factor = module_imglib_common_get_zoom_ratio(
      p_client->parent_mod,
      p_client->stream_info->reprocess_config.pp_feature_config.zoom_level);

    *p_meta = p_client->current_meta;
    p_client->p_current_meta = p_meta;

    /* queue the meta buffer */
    rc = IMG_COMP_Q_META_BUF(p_comp, p_meta);
    if (rc != IMG_SUCCESS) {
      IDBG_ERROR("%s:%d] rc %d", __func__, __LINE__, rc);
      goto error;
    }
  }

  IDBG_MED("%s:%d] X", __func__, __LINE__);
  return 0;

error:
  if (pframeinfo) {
    free(pframeinfo);
  }
  if (poutframeinfo) {
    free(poutframeinfo);
  }
  if (p_meta) {
    free(p_meta);
  }
  return rc;
}

/**
 * Function: module_imgbase_client_get_frame
 *
 * Description: This function is to fetching the output buffer
 *           based in buffer info
 * Arguments:
 *   @p_appdata: imgbase client
 *   @pframe: frame pointer
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
int module_imgbase_client_get_frame(void *p_appdata,
  img_frame_t **pp_frame)
{
  int rc = IMG_SUCCESS;

  assert(p_appdata != NULL);
  assert(pp_frame != NULL);
  *pp_frame = NULL;

  imgbase_client_t *p_client = (imgbase_client_t *)p_appdata;
  img_frame_t *p_out_frame;
  uint8_t *l_frameid;

  IDBG_HIGH("%s:%d] E", __func__, __LINE__);
  uint8_t *poutframeinfo = (uint8_t *)calloc(1,
    sizeof(img_frame_t) + sizeof(int));
  if (!poutframeinfo) {
    IDBG_ERROR("%s:%d] Error: Cannot allocate memory", __func__, __LINE__);
    rc = IMG_ERR_NO_MEMORY;
    goto error;
  }

  p_out_frame = (img_frame_t *)poutframeinfo;
  l_frameid = p_out_frame->private_data =
    (uint8_t *)poutframeinfo + sizeof(img_frame_t);
  /* Queue output buffer */
  rc = module_imgbase_client_get_outputbuf(p_client, p_out_frame);
  if (IMG_ERROR(rc)) {
    IDBG_ERROR("%s:%d] rc %d", __func__, __LINE__, rc);
    goto error;
  }
  *l_frameid = p_client->frame_id;
  *pp_frame = p_out_frame;
  return rc;

error:
  if (poutframeinfo) {
    free(poutframeinfo);
  }
  return rc;
}

/**
 * Function: module_imgbase_client_release_frame
 *
 * Description: This function is to release the output buffer
 *           based in buffer info
 * Arguments:
 *   @p_appdata: imgbase client
 *   @pframe: frame pointer
 *   @is_dirty: flag to indicate whether to do buf done
 *   @is_analysis: flag to indicate whether the image is for
 *               analysis
 *   @is_bitstream: flag to indicate whether the image is
 *              bitstream
 *
 * Return values:
 *     imaging error values
 *
 * Notes: none
 **/
int module_imgbase_client_release_frame(void *p_appdata,
  img_frame_t *p_frame,
  int is_dirty,
  int is_analysis,
  int is_bitstream,
  uint32_t size)
{
  imgbase_client_t *p_client = (void *)p_appdata;

  if (!p_frame) {
    IDBG_ERROR("%s:%d] Error invalid release", __func__, __LINE__);
    return IMG_ERR_INVALID_INPUT;
  }

  IDBG_HIGH("%s:%d] p_client %p is_dirty %d is_bitstream %d size %d",
    __func__, __LINE__,
    p_frame, is_dirty, is_bitstream, size);

  if (p_client->p_current_meta) {
    cam_stream_img_prop_t prop;
    memset(&prop, 0x0, sizeof(cam_stream_img_prop_t));
    cam_rect_t *p_rect = &prop.crop;
    img_meta_t *p_meta = p_client->p_current_meta;
    p_rect->left = p_meta->output_crop.pos.x;
    p_rect->top = p_meta->output_crop.pos.y;
    p_rect->width = p_meta->output_crop.size.width;
    p_rect->height = p_meta->output_crop.size.height;
    prop.input.width = p_frame->info.width;
    prop.input.height = p_frame->info.height;
    prop.analysis_image = TRUE;
    prop.is_raw_image = is_bitstream;
    prop.size = size;
    module_imgbase_client_enqueue_streambuf(p_client,
      p_client->p_current_meta, &prop);
  }

  module_imgbase_client_handle_outbuf_done(p_client, p_frame, is_dirty);

  return IMG_SUCCESS;
}

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
int module_imgbase_client_start(imgbase_client_t *p_client)
{
  int rc = IMG_SUCCESS;
  img_component_ops_t *p_comp = &p_client->comp;

  pthread_mutex_lock(&p_client->mutex);
  p_client->stream_on = TRUE;
  rc = IMG_COMP_START(p_comp, NULL);
  if (IMG_ERROR(rc)) {
    IDBG_ERROR("%s:%d] start failed %d", __func__, __LINE__, rc);
    pthread_mutex_unlock(&p_client->mutex);
    return rc;
  }
  p_client->state = IMGLIB_STATE_STARTED;
  pthread_mutex_unlock(&p_client->mutex);
  return rc;
}

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
int module_imgbase_client_stop(imgbase_client_t *p_client)
{
  int rc = IMG_SUCCESS;
  img_component_ops_t *p_comp = &p_client->comp;

  pthread_mutex_lock(&p_client->mutex);
  p_client->stream_on = FALSE;
  rc = IMG_COMP_ABORT(p_comp, NULL);
  if (IMG_ERROR(rc)) {
    IDBG_ERROR("%s:%d] create failed %d", __func__, __LINE__, rc);
    pthread_mutex_unlock(&p_client->mutex);
    return rc;
  }
  p_client->state = IMGLIB_STATE_INIT;
  img_q_flush_and_destroy(&p_client->stream_parm_q);
  pthread_mutex_unlock(&p_client->mutex);
  return rc;
}

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
void module_imgbase_client_destroy(imgbase_client_t *p_client)
{
  int rc = IMG_SUCCESS;
  img_component_ops_t *p_comp = NULL;

  if (NULL == p_client) {
    return;
  }

  p_comp = &p_client->comp;
  IDBG_MED("%s:%d] state %d", __func__, __LINE__, p_client->state);

  if (IMGLIB_STATE_STARTED == p_client->state) {
    module_imgbase_client_stop(p_client);
  }

  if (IMGLIB_STATE_INIT == p_client->state) {
    rc = IMG_COMP_DEINIT(p_comp);
    if (IMG_ERROR(rc)) {
      IDBG_ERROR("%s:%d] deinit failed %d", __func__, __LINE__, rc);
    }
    p_client->state = IMGLIB_STATE_IDLE;
  }

  if (IMGLIB_STATE_IDLE == p_client->state) {
    img_q_deinit(&p_client->stream_parm_q);
    pthread_mutex_destroy(&p_client->mutex);
    pthread_cond_destroy(&p_client->cond);

    free(p_client);
    p_client = NULL;
  }
  IDBG_MED("%s:%d] X", __func__, __LINE__);

}


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
  mct_stream_info_t *stream_info)
{
  int rc = IMG_SUCCESS;
  imgbase_client_t *p_client = NULL;
  img_component_ops_t *p_comp = NULL;
  img_core_ops_t *p_core_ops = NULL;
  module_imgbase_t *p_mod =
    (module_imgbase_t *)p_mct_mod->module_private;
  mct_list_t *p_temp_list = NULL;
  img_frame_ops_t l_frameops = {
    module_imgbase_client_get_frame,
    module_imgbase_client_release_frame,
    NULL
  };
  img_init_params_t init_params;
  memset(&init_params, 0x0, sizeof(img_init_params_t));
  img_init_params_t *p_params = NULL;

  /* set init params */
  if (p_mod->modparams.imgbase_client_init_params) {
    p_params = &init_params;
    p_mod->modparams.imgbase_client_init_params(p_params);
  }

  p_core_ops = &p_mod->core_ops;

  IDBG_MED("%s:%d]", __func__, __LINE__);
  p_client = (imgbase_client_t *)malloc(sizeof(imgbase_client_t));
  if (NULL == p_client) {
    IDBG_ERROR("%s:%d] IMGLIB_BASE client alloc failed", __func__, __LINE__);
    return IMG_ERR_NO_MEMORY;
  }

  /* initialize the variables */
  memset(p_client, 0x0, sizeof(imgbase_client_t));

  p_comp = &p_client->comp;
  pthread_mutex_init(&p_client->mutex, NULL);
  pthread_cond_init(&p_client->cond, NULL);
  p_client->state = IMGLIB_STATE_IDLE;
  p_client->stream_info = stream_info;
  img_q_init(&p_client->stream_parm_q, "stream_parm_q");

  rc = IMG_COMP_CREATE(p_core_ops, p_comp);
  if (IMG_ERROR(rc)) {
   IDBG_ERROR("%s:%d] create failed %d", __func__, __LINE__, rc);
    goto error;
  }

  rc = IMG_COMP_INIT(p_comp, p_client, p_params);
  if (IMG_ERROR(rc)) {
    IDBG_ERROR("%s:%d] init failed %d", __func__, __LINE__, rc);
    goto error;
  }
  p_client->state = IMGLIB_STATE_INIT;

  rc = IMG_COMP_SET_CB(p_comp, module_imgbase_client_event_handler);
  if (rc != IMG_SUCCESS) {
    IDBG_ERROR("%s:%d] rc %d", __func__, __LINE__, rc);
    goto error;
  }

  rc = IMG_COMP_SET_PARAM(p_comp, QIMG_PARAM_CAPS, &p_mod->caps);
  if (rc != IMG_SUCCESS) {
    IDBG_ERROR("%s:%d] rc %d", __func__, __LINE__, rc);
    goto error;
  }

  l_frameops.p_appdata = p_client;
  rc = IMG_COMP_SET_PARAM(p_comp, QIMG_PARAM_FRAME_OPS, &l_frameops);
  if (rc != IMG_SUCCESS) {
    IDBG_ERROR("%s:%d] rc %d", __func__, __LINE__, rc);
    goto error;
  }

  /* add the client to the list */
  p_temp_list = mct_list_append(p_mod->imgbase_client, p_client,
    NULL, NULL);
  if (NULL == p_temp_list) {
    IDBG_ERROR("%s:%d] list append failed", __func__, __LINE__);
    rc = IMG_ERR_NO_MEMORY;
    goto error;
  }

  p_mod->imgbase_client = p_temp_list;
  p_client->p_sinkport = p_port;
  p_client->identity = identity;
  p_client->parent_mod = p_mod->parent_mod ? p_mod->parent_mod : p_mct_mod;
  p_client->p_mod = p_mod;
  p_port->port_private = p_client;

  IDBG_HIGH("%s:%d] port %p client %p X", __func__, __LINE__, p_port, p_client);
  return rc;

error:
  if (p_client) {
    module_imgbase_client_destroy(p_client);
    p_client = NULL;
  }
  return rc;
}
