/* awb_port.c
 *
 * Copyright (c) 2013-2014 Qualcomm Technologies, Inc. All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */

#include "awb_port.h"
#include "awb.h"

#include "q3a_thread.h"
#include "q3a_port.h"
#include "mct_stream.h"
#include "mct_module.h"
#include "modules.h"

#if 0
#undef CDBG
#define CDBG CDBG_ERROR
#endif
/* Every AWB sink port ONLY corresponds to ONE session */

typedef enum {
  AWB_PORT_STATE_CREATED,
  AWB_PORT_STATE_RESERVED,
  AWB_PORT_STATE_LINKED,
  AWB_PORT_STATE_UNLINKED,
  AWB_PORT_STATE_UNRESERVED
} awb_port_state_t;

/** awb_port_private
 *    @awb_object: session index
 *    @port:       stream index
 *
 * Each awb moduld object should be used ONLY for one Bayer
 * serssin/stream set - use this structure to store session
 * and stream indices information.
 **/
typedef struct _awb_port_private {
  unsigned int      reserved_id;
  cam_stream_type_t stream_type;
  awb_port_state_t  state;
  stats_update_t    awb_update_data;
  boolean           awb_update_flag;
  awb_object_t      awb_object;
  q3a_thread_data_t *thread_data;
  uint32_t          cur_sof_id;
  
  /* keep aec parameters , added by tanrifei, 20131218*/
  awb_set_aec_parms aec_parms;
  uint32_t	instant_color_temp;
  uint32_t	new_stream_cnt;
  float 	outdoor_coe;
  /* add end*/  
  
  /* keep chromatix pointer, added by tanrifei, 20131108*/
  chromatix_parms_type *chromatixPtr;
  chromatix_VFE_common_type *chromatixComPtr;
  /* add end*/
} awb_port_private_t;

/********************************************************************************************************************/
//block starts, added by tanrifei, 20140728
#define BRIGHT_TRIG_ENTRY 5

struct bright_trig_t {
	int32_t trig_idx_start; 
	int32_t trig_idx_end;	
	float r_gain;
	float b_gain;
};
struct cct_trig_t {
	uint32_t cct;
	struct bright_trig_t trig[BRIGHT_TRIG_ENTRY];
};
struct awb_refine_para {
	float max_reasonable_rg;
	float min_reasonable_bg;
	float f_a_ref_ratio;
	float f_a_upper_bg_coe;
	float cwf_cf_ref_ratio;
	float cf_upper_ref_ratio;
	float cf_left_margin;
	float cwf_red_compensation_coe;
	float cwf_red_com_limit;
	float cwf_blue_compensation_coe;
	float cwf_blue_com_limit;
	float cwf_green_compensation_coe;
	float cwf_left_margin;
	float tl84_red_compensation_coe;
	float tl84_red_com_limit;
	float tl84_blue_compensation_coe;
	float tl84_blue_com_limit;
	float tl84_a_upper_coe;
	float a_horizon_ref_ratio_rg;
	float a_horizon_ref_ratio_bg;	
	float a_red_compensation_coe;
	float a_red_com_limit;	
	float a_blue_compensation_coe;
	float a_blue_com_limit;
	float d50_d65_left_ref_ratio;
	float d50_d65_left_ref_coe;
	float d50_d65_left_min_ratio;
	float d50_d65_left_min_max_ratio;
	float d50_d65_right_ref_ratio;
	float d50_d65_right_ref_coe;
	float d50_d65_right_brightness_coe;
	float d50_d65_right_green_bias;
	float d50_cf_ref_ratio;

	//green zone
	float rg_offset;
	float bg_offset;
};

const struct awb_refine_para front_camera_refine_para = {
	.max_reasonable_rg = 1.02,//0.9,
	.min_reasonable_bg = 0.30,
	.f_a_ref_ratio = 1.0,//0.5,
	.f_a_upper_bg_coe = 0.03,
	.cwf_cf_ref_ratio = 0.8,
	.cf_upper_ref_ratio = 0.2,
	.cf_left_margin = 0.98,
	.cwf_red_compensation_coe = 0.90,//0.7,
	.cwf_red_com_limit = 0.23,	
	.cwf_blue_compensation_coe = 0.85,
	.cwf_blue_com_limit = 0.1,
	.cwf_green_compensation_coe = 1.5,
	.cwf_left_margin = 0.9,	
	.tl84_red_compensation_coe = 0.80,//0.90,
	.tl84_red_com_limit = 0.23,
	.tl84_blue_compensation_coe = 0.0,
	.tl84_blue_com_limit = 0.0,
	.tl84_a_upper_coe = 0.0,
	.a_horizon_ref_ratio_rg = 0.6,
	.a_horizon_ref_ratio_bg = 1.0, //0.8,	
	.a_red_compensation_coe = 1.0,	
	.a_red_com_limit = 0.1,
	.a_blue_compensation_coe = 0.0,
	.a_blue_com_limit = 0.0,
	.d50_d65_left_ref_ratio = 0.90,
	.d50_d65_left_ref_coe = 0.5,//0.8,
	.d50_d65_left_min_ratio = 0.0,
	.d50_d65_left_min_max_ratio = 0.8,
	.d50_d65_right_ref_ratio = 0.80,
	.d50_d65_right_ref_coe = 1.0,
	.d50_d65_right_brightness_coe = 0.5,
	.d50_d65_right_green_bias = 0.00,
	.d50_cf_ref_ratio = 0.0,

	//green zone
	.rg_offset = -0.02,
	.bg_offset = 0.1,
};

const struct awb_refine_para back_camera_refine_para = {
	.max_reasonable_rg = 0.85,//0.9,
	.min_reasonable_bg = 0.2,
	.f_a_ref_ratio = 1.0,//0.5,
	.f_a_upper_bg_coe = 0.03,
	.cwf_cf_ref_ratio = 0.8,
	.cf_upper_ref_ratio = 0.2,
	.cf_left_margin = 0.98,
	.cwf_red_compensation_coe = 0.90,//0.7,
	.cwf_red_com_limit = 0.23,	
	.cwf_blue_compensation_coe = 0.85,
	.cwf_blue_com_limit = 0.1,
	.cwf_green_compensation_coe = 1.5,
	.cwf_left_margin = 0.9,	
	.tl84_red_compensation_coe = 0.80,//0.90,
	.tl84_red_com_limit = 0.23,
	.tl84_blue_compensation_coe = 0.0,
	.tl84_blue_com_limit = 0.0,
	.tl84_a_upper_coe = 1.0,
	.a_horizon_ref_ratio_rg = 0.3,
	.a_horizon_ref_ratio_bg = 0.5,	
	.a_red_compensation_coe = 1.0,	
	.a_red_com_limit = 0.1,
	.a_blue_compensation_coe = 0.0,
	.a_blue_com_limit = 0.0,
	.d50_d65_left_ref_ratio = 0.90,
	.d50_d65_left_ref_coe = 0.8,
	.d50_d65_left_min_ratio = 0.0,
	.d50_d65_left_min_max_ratio = 0.8,
	.d50_d65_right_ref_ratio = 0.90,
	.d50_d65_right_ref_coe = 1.0,
	.d50_d65_right_brightness_coe = 0.5,
	.d50_d65_right_green_bias = 0.02,
	.d50_cf_ref_ratio = 0.1,
	
	//green zone
	.rg_offset = 0.0,
	.bg_offset = 0.0,
};
//block ends
/********************************************************************************************************************/

/** awb_send_event
 *
 **/
static void awb_send_events(mct_port_t *port, stats_update_t *awb_update_data)
{
  awb_port_private_t *private = (awb_port_private_t *)(port->port_private);
  mct_event_t event;
  MCT_OBJECT_LOCK(port);
  if (private->awb_update_flag == FALSE) {
    CDBG("No AWB update event to send");
    MCT_OBJECT_UNLOCK(port);
    return;
  }else
    private->awb_update_flag = FALSE;
  MCT_OBJECT_UNLOCK(port);
  /* pack it in a event and send out*/
  event.direction = MCT_EVENT_UPSTREAM;
  event.identity = private->reserved_id;
  event.type = MCT_EVENT_MODULE_EVENT;
  event.u.module_event.type = MCT_EVENT_MODULE_STATS_AWB_UPDATE;
  event.u.module_event.module_event_data = (void *)(awb_update_data);
  CDBG("%s: send MCT_EVENT_MODULE_STATS_AWB_UPDATE to port =%p, event =%p",
        __func__,port, &event);
  MCT_PORT_EVENT_FUNC(port)(port, &event);
  return;
}
#if 0
static void awb_port_send_exif_info_update(
   mct_port_t  *port,
   awb_output_data_t *output)
{
  mct_event_t event;
  mct_bus_msg_t bus_msg;
  unsigned short q3a_msg[sizeofAWBExifDataArray];
  awb_port_private_t *private;
  int size;
  if (!output || !port) {
    ALOGE("%s: input error", __func__);
    return;
  }
  private = (awb_port_private_t *)(port->port_private);
  bus_msg.sessionid = (private->reserved_id >> 16);
  bus_msg.type = MCT_BUS_MSG_AWB_EXIF_INFO;
  bus_msg.msg = (void *)&q3a_msg;
  size = ((int)sizeof(q3a_msg) < output->exif_info_size)?
    (int)sizeof(q3a_msg) : output->exif_info_size;
  bus_msg.size = size;
  memcpy(&q3a_msg[0], output->pexif_info, size);
  output->exif_info_size = 0;
  event.direction = MCT_EVENT_UPSTREAM;
  event.identity = private->reserved_id;
  event.type = MCT_EVENT_MODULE_EVENT;
  event.u.module_event.type = MCT_EVENT_MODULE_STATS_POST_TO_BUS;
  event.u.module_event.module_event_data = (void *)(&bus_msg);
  CDBG("%s: send AWB exif to port", __func__);
  MCT_PORT_EVENT_FUNC(port)(port, &event);
}
#endif

/** awb_port_send_awb_info_to_metadata
 *  update awb info which required by eztuing
 **/

static void awb_port_send_awb_info_to_metadata(
  mct_port_t  *port,
  awb_output_data_t *output)
{
  mct_event_t               event;
  mct_bus_msg_t             bus_msg;
  awb_output_eztune_data_t  awb_info;
  awb_port_private_t        *private;
  int                       size;

  if (!output || !port) {
    CDBG_ERROR("%s: input error", __func__);
    return;
  }

  /* If eztune is not running, no need to send eztune metadata */
  if (FALSE == output->eztune_data.ez_running) {
    return;
  }

  private = (awb_port_private_t *)(port->port_private);
  if (private == NULL)
    return;

  bus_msg.sessionid = (private->reserved_id >> 16);
  bus_msg.type = MCT_BUS_MSG_AWB_EZTUNING_INFO;
  bus_msg.msg = (void *)&awb_info;
  size = (int)sizeof(awb_output_eztune_data_t);
  bus_msg.size = size;

  memcpy(&awb_info, &output->eztune_data,
    sizeof(awb_output_eztune_data_t));

  CDBG("%s: r_gain:%f, g_gain:%f, b_gain:%f,color_temp:%d", __func__,
    awb_info.r_gain, awb_info.g_gain, awb_info.b_gain, awb_info.color_temp);
  event.direction = MCT_EVENT_UPSTREAM;
  event.identity = private->reserved_id;
  event.type = MCT_EVENT_MODULE_EVENT;
  event.u.module_event.type = MCT_EVENT_MODULE_STATS_POST_TO_BUS;
  event.u.module_event.module_event_data = (void *)(&bus_msg);
  MCT_PORT_EVENT_FUNC(port)(port, &event);
}

/** awb_port_update_awb_info:
 * right now,just update cct value
 * Return nothing
 **/
static void awb_port_update_awb_info(mct_port_t *port,
  awb_output_data_t *output)
{
  mct_event_t        event;
  mct_bus_msg_t      bus_msg;
  cam_ae_params_t    aec_info;

  cam_awb_params_t    awb_info;
  awb_port_private_t *private;
  int                size;

  if (!output || !port) {
    CDBG_ERROR("%s: input error", __func__);
    return;
  }
  private = (awb_port_private_t *)(port->port_private);
  if (private == NULL)
    return;

  bus_msg.sessionid = (private->reserved_id >> 16);
  bus_msg.type = MCT_BUS_MSG_AWB_INFO;
  bus_msg.msg = (void *)&awb_info;
  size = (int)sizeof(cam_awb_params_t);
  bus_msg.size = size;
  memset(&awb_info, 0, size);
  awb_info.cct_value= output->stats_update.awb_update.color_temp;
  awb_info.decision = output->decision;
  CDBG("%s: cct:%d, decision: %d", __func__, awb_info.cct_value, awb_info.decision);
  event.direction = MCT_EVENT_UPSTREAM;
  event.identity = private->reserved_id;
  event.type = MCT_EVENT_MODULE_EVENT;
  event.u.module_event.type = MCT_EVENT_MODULE_STATS_POST_TO_BUS;
  event.u.module_event.module_event_data = (void *)(&bus_msg);
  MCT_PORT_EVENT_FUNC(port)(port, &event);
}

/****************************************************************************************/
//block start, added by tanrifei, 20140728
typedef struct {
	float *r_array;
	float *g_array;
	float *b_array;
	float *y_array;
	float *rg_array;
	float *bg_array;
	float simple_brightness;
	float simple_rg;
	float simple_bg;
	float hi_bright_rg;
	float hi_bright_bg;
	float hi_bright_pct;
	float low_bright_rg;
	float low_bright_bg;
	float low_bright_pct;
	int valid_smp;
	int valid_hi_smp;
	int valid_low_smp;
} awb_analysis_output_t;

typedef struct {
	float rg_offset;
	float bg_offset;
	float green_rg;
	float green_bg;
	float non_green_rg;
	float non_green_bg;
	int green_cnt;
	int non_green_cnt;
} awb_green_zone_output_t;

static float R[MAX_BG_STATS_NUM], G[MAX_BG_STATS_NUM], B[MAX_BG_STATS_NUM], Y[MAX_BG_STATS_NUM];
static float RG[MAX_BG_STATS_NUM], BG[MAX_BG_STATS_NUM];

static void awb_port_analysis_stats(stats_t *stats, awb_analysis_output_t *output)
{
	q3a_bg_stats_t *bg_stats;
	int h, v, index;
	float simple_rg, simple_bg;
	float hi_bright_rg, hi_bright_bg, low_bright_rg, low_bright_bg;
	float hi_bright_pct, low_bright_pct;
	float avg_brightness;
	int valid_smp, valid_hi_smp, valid_low_smp;

	bg_stats = &stats->bayer_stats.q3a_bg_stats;
	memset(output, 0, sizeof(awb_analysis_output_t));
	
	valid_smp = 0;
	avg_brightness = 0.0;
	
	for (v=0; v<bg_stats->bg_region_v_num; v++) {
		for (h=0; h<bg_stats->bg_region_h_num; h++) {
			index = bg_stats->bg_region_h_num*v + h;
			if (bg_stats->bg_r_num[index]>0 && bg_stats->bg_gr_num[index]>0 &&
				bg_stats->bg_gb_num[index]>0 && bg_stats->bg_b_num[index]>0) {

				R[valid_smp] = (float)bg_stats->bg_r_sum[index]/bg_stats->bg_r_num[index];
				G[valid_smp] = ((float)bg_stats->bg_gr_sum[index]/bg_stats->bg_gr_num[index] +
					(float)bg_stats->bg_gb_sum[index]/bg_stats->bg_gb_num[index]) / 2;
				B[valid_smp] = (float)bg_stats->bg_b_sum[index]/bg_stats->bg_b_num[index];
				Y[valid_smp] = 0.299*R[index] + 0.587*G[index] + 0.114*B[index];

				if ((R[valid_smp]>5 && R[valid_smp]<240) && (G[valid_smp]>5 && G[valid_smp]<240) &&
					(B[valid_smp]>5 && B[valid_smp]<240)) {
					avg_brightness += Y[valid_smp];
					RG[valid_smp] = ((float)R[valid_smp])/G[valid_smp];
					BG[valid_smp] = ((float)B[valid_smp])/G[valid_smp];
					valid_smp++;
				}
			}
		}
	}
	if (valid_smp > 0) {
		avg_brightness /= valid_smp;
	} else {
		return;
	}

	simple_rg = 0.0;
	simple_bg = 0.0;
	hi_bright_rg = 0.0;
	hi_bright_bg = 0.0;
	hi_bright_pct = 0.0;
	low_bright_rg = 0.0;
	low_bright_bg = 0.0;
	low_bright_pct = 0.0;
	valid_hi_smp = 0;
	valid_low_smp = 0;
	hi_bright_pct = 0;
	low_bright_pct = 0;
	
	for (index=0; index<valid_smp; index++) {
		if (Y[index] > (avg_brightness*1.3)) {
			hi_bright_rg += RG[index];
			hi_bright_bg += BG[index];
			valid_hi_smp++;
		} else if (Y[index] < (avg_brightness*0.7)) {
			low_bright_rg += RG[index];
			low_bright_bg += BG[index];
			valid_low_smp++;
		}
		simple_rg += RG[index];
		simple_bg += BG[index];
	}

	if (valid_smp > 0) {
		simple_rg /= valid_smp;
		simple_bg /= valid_smp;
		hi_bright_pct = (float)valid_hi_smp / valid_smp;
		low_bright_pct = (float)valid_low_smp / valid_smp;
	}
	if (valid_hi_smp > 0) {
		hi_bright_rg /= valid_hi_smp;
		hi_bright_bg /= valid_hi_smp;
	}
	if (valid_low_smp > 0) {
		low_bright_rg /= valid_low_smp;
		low_bright_bg /= valid_low_smp;
	}
	CDBG("%s, %d, simple_rg %f, simple_bg %f, hi_bright_rg %f, hi_bright_bg %f, hi_bright_pct %f\
		low_bright_rg %f, low_bright_bg %f, low_bright_pct %f", __func__, __LINE__, 
		simple_rg, simple_bg, hi_bright_rg, hi_bright_bg, hi_bright_pct,
		low_bright_rg, low_bright_bg, low_bright_pct);

	output->r_array = R;
	output->g_array = G;
	output->b_array = B;
	output->y_array = Y;
	output->rg_array = RG;
	output->bg_array = BG;
	output->simple_brightness = avg_brightness;
	output->simple_bg = simple_bg;
	output->simple_rg = simple_rg;
	output->hi_bright_bg = hi_bright_bg;
	output->hi_bright_rg = hi_bright_rg;
	output->hi_bright_pct = hi_bright_pct;
	output->low_bright_bg = low_bright_bg;
	output->low_bright_rg = low_bright_rg;
	output->low_bright_pct = low_bright_pct;
	output->valid_smp = valid_smp;
	output->valid_hi_smp = valid_hi_smp;
	output->valid_low_smp = valid_low_smp;
}

#if 0
static float awb_port_get_bluesky_severity(awb_analysis_output_t *analysis, chromatix_parms_type *chromatix)
{
	float blue_sky_severity = 0.0;
	
	if ((analysis->valid_hi_smp > 0) && (analysis->hi_bright_pct >= 0.01)) {
		float slope, slope1, slope2;
		slope = analysis->hi_bright_bg / analysis->hi_bright_rg;
		slope1 = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio / 
			chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].RG_ratio;
		slope2 = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio / 
			chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].RG_ratio;
		if (slope > slope2) {
			blue_sky_severity = 1.0;
		} else if (slope > slope1) {
			blue_sky_severity = (slope - slope1) / (slope2 - slope1);
		} else {
			blue_sky_severity = 0.0;
		}
	
		if (analysis->hi_bright_bg < 
			chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio) {
			blue_sky_severity *= 0.0;
		} else if (analysis->hi_bright_bg < 
			chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio) {
			blue_sky_severity *= (analysis->hi_bright_bg - 
				chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio) / 
				(chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio - 
				chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio);
		} else {
			blue_sky_severity *= 1.0;
		}

		if (analysis->hi_bright_pct < 0.11) {
			blue_sky_severity *= (analysis->hi_bright_pct - 0.01) / 0.1;
		}
	}
	CDBG("%s, %d, blue_sky_severity %f", __func__, __LINE__, blue_sky_severity);			
	return blue_sky_severity;
}
#else
static float awb_port_get_bluesky_severity(awb_analysis_output_t *analysis, chromatix_parms_type *chromatix)
{
	float blue_sky_severity, num_severity;
	float ref_min_rg, ref_max_rg, ref_min_bg, ref_bg;	
	float blue_sky_rg, blue_sky_bg, non_blue_sky_rg, non_blue_sky_bg;
	//float dist1, dist2, dist_ref;;
	float ref_dist, dist, tmp_weight;
	float ratio;
	
	float blue_sky_cnt, non_blue_sky_cnt;
	int i;

	/*
	if (private->aec_parms.lux_idx >= (private->aec_parms.indoor_index+100)) {
		ratio = 0.9;
	} else if (private->aec_parms.lux_idx >= private->aec_parms.indoor_index) {
		ratio = 0.9 + 0.1*((private->aec_parms.indoor_index+100) - private->aec_parms.lux_idx) / 100;
	} else {
		ratio = 1.0;
	}
	*/
		
	ref_min_rg = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].RG_ratio-0.1;
	ref_max_rg = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].RG_ratio+0.3;
	ref_min_bg = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio;

    blue_sky_severity = 0;
	blue_sky_rg = 0; 
	blue_sky_bg = 0; 
	non_blue_sky_rg = 0; 
	non_blue_sky_bg = 0;
	blue_sky_cnt = 0;
	non_blue_sky_cnt = 0;
	
	//memset(output, 0, sizeof(awb_green_zone_output_t));

	if (!analysis->valid_smp) {
		return 0.0;
	}

	for (i=0; i<analysis->valid_smp; i++) {
		if (chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].RG_ratio < 
			chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].RG_ratio) {
			/*if ((analysis->rg_array[i] <= (chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].RG_ratio-0.02))) {
				ref_dist = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].RG_ratio - 
							chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].RG_ratio;
				dist = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].RG_ratio-0.02-analysis->rg_array[i];
				ratio = dist / ref_dist;
				if (ratio > 1.0) {
					ref_bg = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio;
				} else {
					ref_bg = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio*ratio + 
						chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].BG_ratio*(1.0-ratio);
				}				
			} else */if (analysis->rg_array[i] <= chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].RG_ratio) {
				ref_bg = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].BG_ratio;
			} else if (analysis->rg_array[i] >= chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].RG_ratio) {
				ref_bg = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio;
			} else {
				ratio = (analysis->rg_array[i] - chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].RG_ratio) / 
					(chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].RG_ratio - 
							chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].RG_ratio);
				ref_bg = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio*ratio + 
					chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].BG_ratio*(1.0-ratio);
			}
		} else {
			if (analysis->rg_array[i] <= chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].RG_ratio) {
				ref_bg = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].BG_ratio;
			} else {
				ref_bg = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio;
			}
		}
		
		if ((analysis->rg_array[i] >= ref_min_rg) && (analysis->rg_array[i] <= ref_max_rg) &&
			(analysis->bg_array[i] >= ref_bg)) {
			dist = analysis->bg_array[i] - ref_bg;
			#if 0
			if (dist >= 0.1) {
				tmp_weight = 1.0;				
			} else if (dist <= 0.02) {
				tmp_weight = 0.0;				
			} else {
				tmp_weight = (dist-0.02) / (1.0-0.02);				
			}
			#else 
			if (dist >= 0.02) {
				tmp_weight = 1.0;				
			} else {
				tmp_weight = 0.0;				
			}
			#endif
			blue_sky_rg += analysis->rg_array[i]*tmp_weight;
			blue_sky_bg += analysis->bg_array[i]*tmp_weight;
			blue_sky_cnt += tmp_weight;
		} else {
			non_blue_sky_rg += analysis->rg_array[i];
			non_blue_sky_bg += analysis->bg_array[i];
			non_blue_sky_cnt++;
		}
	}

	if (blue_sky_cnt > 0) {
		blue_sky_rg /= blue_sky_cnt;
		blue_sky_bg /= blue_sky_cnt;
	}
	if (non_blue_sky_cnt > 0) {
		non_blue_sky_rg /= non_blue_sky_cnt;
		non_blue_sky_bg /= non_blue_sky_cnt;
	}
	CDBG("%s, %d, blue_sky zone: %f %f, %d, non blue_sky zone: %f %f, %d, total %d", __func__, __LINE__,
		blue_sky_rg, blue_sky_bg, (int)blue_sky_cnt, non_blue_sky_rg, non_blue_sky_bg, (int)non_blue_sky_cnt, analysis->valid_smp);

	num_severity = ((float)blue_sky_cnt) / analysis->valid_smp;
	if (num_severity < 0.01) {
		blue_sky_severity = 0.0;
	} else if (num_severity < 0.21) {
		blue_sky_severity = (num_severity - 0.01) / 0.2;
	} else {
		blue_sky_severity = 1.0;
	}

	CDBG("%s, %d, blue_sky_severity %f", __func__, __LINE__, 
		blue_sky_severity); 
	
	return blue_sky_severity;
}
#endif

static float awb_port_get_green_severity(awb_analysis_output_t *analysis, 
	awb_port_private_t *private, awb_green_zone_output_t *output)
{
	chromatix_parms_type *chromatix;

	float green_severity, locus_severity, num_severity, additional_severity;
	float ref_rg, ref_bg;	
	float green_rg, green_bg, non_green_rg, non_green_bg;
	float dist1, dist2, dist_ref;;
	float non_green_weight, tmp_weight;
	float ratio;
	
	int green_cnt, non_green_cnt;
	int i;

	chromatix = private->chromatixPtr;

	if (private->aec_parms.lux_idx >= (private->aec_parms.indoor_index+100)) {
		ratio = 0.9;
	} else if (private->aec_parms.lux_idx >= private->aec_parms.indoor_index) {
		ratio = 0.9 + 0.1*((private->aec_parms.indoor_index+100) - private->aec_parms.lux_idx) / 100;
	} else {
		ratio = 1.0;
	}
		
	ref_rg = (chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].RG_ratio + output->rg_offset)* ratio;
	ref_bg = (chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].BG_ratio + output->bg_offset) * ratio;

	green_rg = 0; 
	green_bg = 0; 
	non_green_rg = 0; 
	non_green_bg = 0;
	green_cnt = 0;
	non_green_cnt = 0;
	non_green_weight = 0.0;
	
	if (!analysis->valid_smp) {
		green_severity = 0.0;
		goto out;
	}

	for (i=0; i<analysis->valid_smp; i++) {
		if ((analysis->rg_array[i] < (ref_rg*0.95)) && (analysis->bg_array[i] < (ref_bg))) {
			green_rg += analysis->rg_array[i];
			green_bg += analysis->bg_array[i];
			green_cnt++;
		} else if (/*(analysis->rg_array[i] > (ref_rg*1.05)) &&*/ (analysis->bg_array[i] > (ref_bg*1.05))){
			if (analysis->rg_array[i] > (chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].RG_ratio/2)) {
				tmp_weight = (analysis->rg_array[i] - chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].RG_ratio/2) / 
					(chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].RG_ratio/2);
				non_green_rg += analysis->rg_array[i]*tmp_weight;
				non_green_bg += analysis->bg_array[i]*tmp_weight;
				non_green_weight += tmp_weight;
				non_green_cnt++;
			}
		}
	}

	if (green_cnt > 0) {
		green_rg /= green_cnt;
		green_bg /= green_cnt;
	}
	if (non_green_cnt > 0) {
		non_green_rg /= non_green_weight;
		non_green_bg /= non_green_weight;
	}
	CDBG("%s, %d, green zone: %f %f, %d, non green zone: %f %f, %d", __func__, __LINE__,
		green_rg, green_bg, green_cnt, non_green_rg, non_green_bg, non_green_cnt);

	dist_ref = ref_rg*ref_rg + ref_bg*ref_bg;
	dist1 = green_rg*green_rg + green_bg*green_bg;
	dist2 = non_green_rg*non_green_rg + non_green_bg*non_green_bg;

	num_severity = ((float)green_cnt) / analysis->valid_smp;

	if (dist1 > 0) {
		locus_severity = (dist_ref - dist1) / dist_ref;
		if (locus_severity > 0.6) {
			locus_severity = 1.0;
		} else {
			locus_severity /= 0.6;
		}
	} else {
		locus_severity = 0;
	}
	
	if (dist2/dist_ref < 1.1) {
		additional_severity = 0.0;
	} else if (dist2/dist_ref > 1.4) {
		additional_severity = 1.0;
	} else {
		additional_severity = (dist2/dist_ref - 1.1) / 0.3;
	}

	if ((((float)non_green_cnt) / analysis->valid_smp) < 0.2) {
		additional_severity *= (1.0 - num_severity);
	}

	if (num_severity < 0.5) {
		green_severity = locus_severity*num_severity/0.5;
	} else {
		green_severity = locus_severity;
	}

	green_severity *= (1.0 + additional_severity);
	if (green_severity > 1.0) {
		green_severity = 1.0;
	}

	CDBG("%s, %d, %f %f %f, green_severity %f", __func__, __LINE__, 
		locus_severity, num_severity, additional_severity, green_severity);	

out:	
	output->green_rg = green_rg;
	output->green_bg = green_bg;
	output->green_cnt = green_cnt;
	output->non_green_rg = non_green_rg;
	output->non_green_bg = non_green_bg;
	output->non_green_cnt = non_green_cnt;
		
	return green_severity;
}

/* refine AWB gains according to Gionee's rule, added by tanrifei, 20131108*/
static void awb_port_refine_gains(void *p, awb_gain_t *rgb_gain)
{
	const struct awb_refine_para *para;
	chromatix_parms_type *chromatix = NULL;
	awb_port_private_t *private = NULL;
	awb_analysis_output_t awb_analysis_out;
	int i, j;
	float rg_ratio, bg_ratio, ratio;
	float ref_rg_ratio, ref_bg_ratio, key_point_rg_ratio, key_point_bg_ratio;
	float min_gain;
	float awb_aggressivness;
	static awb_gain_t prev_input_awb_gain;
	static awb_gain_t prev_restored_awb_gain;
	static awb_gain_t prev_refined_awb_gain;
	static boolean is_back_camera = 1; 
	awb_gain_t temp_awb_gain = *rgb_gain;
	
	awb_green_zone_output_t green_zone;
	float green_severity;
	float blue_sky_severity;
	float brightness_weighted_green_severity;
	float brightness_weighted_blue_sky_severity;
	
	private = (awb_port_private_t *)(((mct_port_t *)p)->port_private);
	chromatix = private->chromatixPtr;
	
	if (private->aec_parms.aec_flash_settled == 3 ||
		private->aec_parms.use_led_estimation == TRUE) {
		return;
	}
	/* need refine ?*/
	if ((!chromatix) || (!(private->chromatixComPtr)) || 
		(!(private->chromatixComPtr->revision_number & CAMERA_TYPE_MASK))) {
		CDBG("%s, no need to refine", __func__); 
		return;
	} 
	
	if (private->chromatixComPtr->revision_number & BACK_CAMERA) {
		para = &back_camera_refine_para;
		if (is_back_camera == 0) {
			is_back_camera = 1;
			prev_restored_awb_gain.r_gain = 0;
			prev_restored_awb_gain.g_gain = 0;
			prev_restored_awb_gain.b_gain = 0;
		}
	} else {
		para = &front_camera_refine_para;
		if (is_back_camera == 1) {
			is_back_camera = 0;
			prev_restored_awb_gain.r_gain = 0;
			prev_restored_awb_gain.g_gain = 0;
			prev_restored_awb_gain.b_gain = 0;
		}
	}
	
	/*** refine AWB gains ***/
	CDBG("%s, stream_type %d", __func__, private->stream_type);

	/*************************************************************************************/
	//restore awb gain, eliminate platform's smoothing effect
	if (chromatix->AWB_bayer_algo_data.awb_aggressiveness == 0) {
		awb_aggressivness = 0.05;
	} else if (chromatix->AWB_bayer_algo_data.awb_aggressiveness == 1) {
		awb_aggressivness = 0.10;
	} else {
		awb_aggressivness = 0.30;
	}

	if ((prev_restored_awb_gain.r_gain != 0) && (prev_restored_awb_gain.g_gain != 0) && 
		(prev_restored_awb_gain.b_gain != 0)) {
		if (private->new_stream_cnt < 2) {
			//prev_input_awb_gain.r_gain = chromatix->chromatix_MWB.MWB_tl84.r_gain;
			//prev_input_awb_gain.g_gain = chromatix->chromatix_MWB.MWB_tl84.g_gain;
			//prev_input_awb_gain.b_gain = chromatix->chromatix_MWB.MWB_tl84.b_gain;
			*rgb_gain = prev_restored_awb_gain;
		} else {
			rgb_gain->r_gain = (rgb_gain->r_gain-(1.0-awb_aggressivness)*prev_input_awb_gain.r_gain) / 
				awb_aggressivness;
			rgb_gain->g_gain = (rgb_gain->g_gain-(1.0-awb_aggressivness)*prev_input_awb_gain.g_gain) / 
				awb_aggressivness;
			rgb_gain->b_gain = (rgb_gain->b_gain-(1.0-awb_aggressivness)*prev_input_awb_gain.b_gain) / 
				awb_aggressivness;
			CDBG("%s, restored awb %f, %f, %f, prev_input_awb_gain %f, %f, %f", __func__, 
				rgb_gain->r_gain, rgb_gain->g_gain, rgb_gain->b_gain,
				prev_input_awb_gain.r_gain, prev_input_awb_gain.g_gain, prev_input_awb_gain.b_gain);
			
			if ((private->new_stream_cnt < 50) && (((rgb_gain->r_gain-prev_restored_awb_gain.r_gain)*
				(temp_awb_gain.r_gain-prev_restored_awb_gain.r_gain)<0) || 
				((rgb_gain->b_gain-prev_restored_awb_gain.b_gain)*
				(temp_awb_gain.b_gain-prev_restored_awb_gain.b_gain)<0))) {
				*rgb_gain = prev_restored_awb_gain;
			} else {			
				
				rgb_gain->r_gain = rgb_gain->r_gain*awb_aggressivness + 
					prev_restored_awb_gain.r_gain*(1.0-awb_aggressivness);
				rgb_gain->g_gain = rgb_gain->g_gain*awb_aggressivness + 
					prev_restored_awb_gain.g_gain*(1.0-awb_aggressivness);
				rgb_gain->b_gain = rgb_gain->b_gain*awb_aggressivness + 
					prev_restored_awb_gain.b_gain*(1.0-awb_aggressivness);
			}
		}
	}
	
	prev_input_awb_gain = temp_awb_gain;
	prev_restored_awb_gain = *rgb_gain;
	CDBG("%s, new_stream_cnt %d, before smoothing %f, %f, %f, after smoothing %f, %f, %f", 
		__func__, private->new_stream_cnt, 
		prev_input_awb_gain.r_gain, prev_input_awb_gain.g_gain,
		prev_input_awb_gain.b_gain,
		rgb_gain->r_gain, rgb_gain->g_gain, rgb_gain->b_gain);
	private->new_stream_cnt++;
	/*************************************************************************************/
	
	awb_port_analysis_stats(&private->awb_object.stats, &awb_analysis_out);
	green_zone.rg_offset = para->rg_offset;
	green_zone.bg_offset = para->bg_offset;
	green_severity = awb_port_get_green_severity(&awb_analysis_out, private, &green_zone);	
	blue_sky_severity = awb_port_get_bluesky_severity(&awb_analysis_out, chromatix);

	if (private->aec_parms.lux_idx > (private->aec_parms.indoor_index-0)) {
		ratio = 0.0;
	} else if (private->aec_parms.lux_idx > (private->aec_parms.indoor_index-50)) {
		ratio = ((private->aec_parms.indoor_index-0) - private->aec_parms.lux_idx) / 
			((private->aec_parms.indoor_index-0) - (private->aec_parms.indoor_index-50));
	} else {
		ratio = 1.0;
	}
	brightness_weighted_blue_sky_severity = ratio*blue_sky_severity;					
	brightness_weighted_green_severity = ratio*green_severity;					


	//refine AWB gains if orginal value is valid
	if (rgb_gain->b_gain>0 && rgb_gain->g_gain>0 && rgb_gain->r_gain>0 && (private->new_stream_cnt>2)) {		
		float red_compensation_coe;
		float red_com_limit;
		float blue_compensation_coe;
		float blue_com_limit;
		
		rg_ratio = rgb_gain->g_gain / rgb_gain->r_gain; 
		bg_ratio = rgb_gain->g_gain / rgb_gain->b_gain;
		float orig_rg_ratio = rg_ratio;
		float orig_bg_ratio = bg_ratio;

		if (rg_ratio < chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].RG_ratio) {
			//medium color temperature
			red_compensation_coe = para->cwf_red_compensation_coe;
			red_com_limit = para->cwf_red_com_limit;
			blue_compensation_coe = para->cwf_blue_compensation_coe;
			blue_com_limit = para->cwf_blue_com_limit;

			if (rg_ratio > chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].RG_ratio) {				
				key_point_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].RG_ratio * para->f_a_ref_ratio +
						chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].RG_ratio * (1.0-para->f_a_ref_ratio);
				key_point_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].BG_ratio * para->f_a_ref_ratio +
						chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].BG_ratio * (1.0-para->f_a_ref_ratio);
				CDBG("%s, %d, middle temprature, path 1", __func__, __LINE__);
				if ((rg_ratio > key_point_rg_ratio) && (bg_ratio < key_point_bg_ratio)) {
					ratio = (rg_ratio - key_point_rg_ratio) / 
							(chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].RG_ratio - key_point_rg_ratio);							
					ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].RG_ratio * (1.0-ratio) +
						chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].RG_ratio * ratio;
					ref_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].BG_ratio * (1.0-ratio) +
						chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].BG_ratio * ratio;

					if (bg_ratio > ref_bg_ratio) {						
						ref_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].BG_ratio*para->f_a_upper_bg_coe + 
							bg_ratio*(1.0-para->f_a_upper_bg_coe);
					}

					red_compensation_coe = para->tl84_red_compensation_coe * (1.0-ratio) + para->a_red_compensation_coe * ratio;
					red_com_limit = para->tl84_red_com_limit * (1.0-ratio) + para->a_red_com_limit * ratio;
					blue_compensation_coe = para->tl84_blue_compensation_coe * (1.0-ratio) + para->a_blue_compensation_coe * ratio;
					blue_com_limit = para->tl84_blue_com_limit * (1.0-ratio) + para->a_blue_com_limit * ratio;
					CDBG("%s, %d, %f, %f", __func__, __LINE__, ref_rg_ratio, ref_bg_ratio);
				} else {
					ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].RG_ratio*para->tl84_a_upper_coe +
						rg_ratio*(1.0-para->tl84_a_upper_coe);
					ref_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].BG_ratio*para->tl84_a_upper_coe +
						bg_ratio*(1.0-para->tl84_a_upper_coe);
					
					red_compensation_coe = para->tl84_red_compensation_coe;
					red_com_limit = para->tl84_red_com_limit;
					blue_compensation_coe = para->tl84_blue_compensation_coe;
					blue_com_limit = para->tl84_blue_com_limit;
					CDBG("%s, %d, %f, %f", __func__, __LINE__, ref_rg_ratio, ref_bg_ratio);
				}

				rg_ratio = ref_rg_ratio;
				bg_ratio = ref_bg_ratio;
			} else if (rg_ratio > chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].RG_ratio) {
				ratio = (rg_ratio-chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].RG_ratio) /
					(chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].RG_ratio -
					chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].RG_ratio);
				ref_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].BG_ratio * ratio +
					chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].BG_ratio * (1.0-ratio);
				if (bg_ratio < ref_bg_ratio) {
					CDBG("%s, %d, middle temprature, path 2", __func__, __LINE__);
					bg_ratio = ref_bg_ratio;
				}
				
				red_compensation_coe = para->cwf_red_compensation_coe * (1.0-ratio) + para->tl84_red_compensation_coe * ratio;
				red_com_limit = para->cwf_red_com_limit * (1.0-ratio) + para->tl84_red_com_limit * ratio;
				blue_compensation_coe = para->cwf_blue_compensation_coe * (1.0-ratio) + para->tl84_blue_compensation_coe * ratio;
				blue_com_limit = para->cwf_blue_com_limit * (1.0-ratio) + para->tl84_blue_com_limit * ratio;
			} else {
				ref_rg_ratio = 0.0;

				CDBG("%s, %d, middle temprature, path 3", __func__, __LINE__);
				key_point_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].RG_ratio*para->cwf_cf_ref_ratio + 
					chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].RG_ratio*(1.0-para->cwf_cf_ref_ratio);
				key_point_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].BG_ratio*para->cwf_cf_ref_ratio + 
					chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].BG_ratio*(1.0-para->cwf_cf_ref_ratio);					
					
				if (bg_ratio < chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].BG_ratio) {
					bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].BG_ratio;
					ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].RG_ratio; 				
					CDBG("%s, %d, %f", __func__, __LINE__, ref_rg_ratio);
				} else if (bg_ratio < key_point_bg_ratio) { 
					ratio = (bg_ratio-chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].BG_ratio) / 
						(key_point_bg_ratio - chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].BG_ratio);
					ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].RG_ratio * para->cf_left_margin * ratio + 
						chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].RG_ratio * (1.0-ratio);					
					CDBG("%s, %d, %f, %f", __func__, __LINE__, ref_rg_ratio, ratio);
				} else if (bg_ratio < chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].BG_ratio) {
					ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].RG_ratio * para->cf_left_margin;
					CDBG("%s, %d, %f", __func__, __LINE__, ref_rg_ratio);
				} else {
					if ((bg_ratio < chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].BG_ratio*(1+para->cf_upper_ref_ratio)) && 
						(rg_ratio < (chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].RG_ratio * para->cf_left_margin))) {
						ratio = (bg_ratio - chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].BG_ratio) / 
							(chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].BG_ratio*para->cf_upper_ref_ratio);
						ref_rg_ratio = rg_ratio*ratio + 
							chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].RG_ratio * para->cf_left_margin * (1.0-ratio);
						CDBG("%s, %d, %f, %f", __func__, __LINE__, ref_rg_ratio, ratio);
					}
				}
				if (ref_rg_ratio > 0) {
					if (rg_ratio < ref_rg_ratio) {
						rg_ratio = ref_rg_ratio;
						CDBG("%s, %d, %f", __func__, __LINE__, rg_ratio);
					}
				}

				//prevent grassland or trees to be purple
				{
					if (green_severity > 0.0) {
						ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].RG_ratio;
						ref_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio*0.96;
						
						if (green_zone.non_green_cnt > 0) {
							if ((green_zone.non_green_bg/green_zone.non_green_rg) > (ref_bg_ratio/ref_rg_ratio)) {
								ratio = ((float)green_zone.non_green_cnt) / (green_zone.green_cnt+green_zone.non_green_cnt);
								if (ratio > 0.2) {
									ratio = 1.0;
								} else {
									ratio /= 0.2;
								}

								if (private->aec_parms.lux_idx > (private->aec_parms.indoor_index)) {
									ratio *= 1.0;
								} else if (private->aec_parms.lux_idx > (private->aec_parms.indoor_index-50)) {
									ratio *= (private->aec_parms.lux_idx - (private->aec_parms.indoor_index-50)) / 
										(private->aec_parms.indoor_index - (private->aec_parms.indoor_index-50));
								} else {
									ratio *= 0.0;
								}

								//grant more weight to daylight
								if (ratio > 0.5) {
									ratio = 0.5;
								}
								ref_rg_ratio = green_zone.non_green_rg*ratio + ref_rg_ratio*(1.0-ratio);
								ref_bg_ratio = green_zone.non_green_bg*ratio + ref_bg_ratio*(1.0-ratio);
							}							
						}
						
						CDBG("%s, %d, ref_rg_ratio %f, ref_bg_ratio %f", __func__, __LINE__, ref_rg_ratio, ref_bg_ratio);

						ratio = green_severity*para->cwf_green_compensation_coe;
						if (ratio > 1.0) {
							ratio = 1.0;
						}
							
						if (rg_ratio < ref_rg_ratio) {
							rg_ratio = rg_ratio*(1.0-ratio) + ref_rg_ratio*ratio;
							CDBG("%s, %d, modify rg_ratio for green plants %f", __func__, __LINE__, 
								rg_ratio);
						}
						if (bg_ratio < ref_bg_ratio) {
							bg_ratio = bg_ratio*(1.0-ratio) + ref_bg_ratio*ratio;
							CDBG("%s, %d, modify bg_ratio for green plants %f", __func__, __LINE__, 
								bg_ratio);
						}
					}					
				}
			}
			
			CDBG("%s, %d adjust gains due to color temperature, %f, %f, %f", 
				__func__, __LINE__, 1.0/rg_ratio, 1.0, 1.0/bg_ratio);

			//decrease red/blue compensation due to green severity
			if (green_severity > 0.5) {
				ratio = 1.0;
			} else {
				ratio = green_severity / 0.5;
			}
			red_compensation_coe *= (1.0 - ratio);
			blue_compensation_coe *= (1.0 - ratio);

			//red compensation
			ratio = bg_ratio / orig_bg_ratio;
			if ((ratio > 1.0) && (rg_ratio >= chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].RG_ratio)) {
				ratio = 1.0 + red_compensation_coe*(bg_ratio/orig_bg_ratio-1.0);
				if (ratio > (1.0 + red_com_limit)) {
					ratio = (1.0 + red_com_limit);
				}

				#if 0
				if ((rg_ratio>chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].RG_ratio) &&
					(rg_ratio<chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].RG_ratio)) {
					ratio = 1.0 + (ratio-1.0) * (chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].RG_ratio - rg_ratio) /
						(chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].RG_ratio - chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].RG_ratio);
				}
				#endif

				CDBG("%s, %d, midium CCT red compensation ratio %f", __func__, __LINE__, ratio);

				if (ratio > 1.0) {
					rg_ratio = ((orig_rg_ratio/ratio)<rg_ratio) ? (orig_rg_ratio/ratio) : rg_ratio;
				}
				if (rg_ratio < chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].RG_ratio*para->cwf_left_margin) {
					rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_COLD_FLO].RG_ratio*para->cwf_left_margin;
				}
				CDBG("%s, %d, %f", __func__, __LINE__, rg_ratio);

			}
			
			//blue compensation
			ratio = 1.0 + blue_compensation_coe*(bg_ratio/orig_bg_ratio-1.0);
			if (ratio > (1.0 + blue_com_limit)) {
				ratio = (1.0 + blue_com_limit);
			}
			if (ratio > 1.0) {
				CDBG("%s, %d, midium CCT blue compensation ratio %f", __func__, __LINE__, ratio);
				bg_ratio *= ratio;
			}
		} else if (rg_ratio >= chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].RG_ratio) { 
			//low color temperature			
			red_compensation_coe = para->a_red_compensation_coe;
			red_com_limit = para->a_red_com_limit;
			blue_compensation_coe = para->a_blue_compensation_coe;
			blue_com_limit = para->a_blue_com_limit;
			
			key_point_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].RG_ratio*para->a_horizon_ref_ratio_rg + 
				chromatix->AWB_bayer_algo_data.reference[AGW_AWB_HORIZON].RG_ratio*(1.0-para->a_horizon_ref_ratio_rg);
			key_point_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].BG_ratio*para->a_horizon_ref_ratio_bg + 
				chromatix->AWB_bayer_algo_data.reference[AGW_AWB_HORIZON].BG_ratio*(1.0-para->a_horizon_ref_ratio_bg);
			
			if (bg_ratio < key_point_bg_ratio) {
				ratio = (key_point_bg_ratio-bg_ratio) / key_point_bg_ratio;
				bg_ratio =	para->min_reasonable_bg*ratio +
					chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].BG_ratio*(1.0-ratio);					
			} else if (bg_ratio < chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].BG_ratio) {
				bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].BG_ratio;
			}
			#if 0
			else if (bg_ratio < chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].BG_ratio) {				
				float tmp_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].BG_ratio * para->f_a_ref_ratio +
						chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].BG_ratio * (1.0-para->f_a_ref_ratio);

				if (bg_ratio < tmp_bg_ratio) {
					ratio = (tmp_bg_ratio - bg_ratio) / (tmp_bg_ratio - key_point_bg_ratio);
					bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].BG_ratio*ratio +
						chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].BG_ratio*(1.0-ratio);
				} else {
					bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].BG_ratio;
				}
			}
			#endif
			
			if (rg_ratio < key_point_rg_ratio) {
				CDBG("%s, %d, low temprature, path 1", __func__, __LINE__);
				rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].RG_ratio;
			} else if (rg_ratio < chromatix->AWB_bayer_algo_data.reference[AGW_AWB_HORIZON].RG_ratio) {
				CDBG("%s, %d, low temprature, path 2", __func__, __LINE__);
				ratio = (rg_ratio-key_point_rg_ratio) / (
					chromatix->AWB_bayer_algo_data.reference[AGW_AWB_HORIZON].RG_ratio - key_point_rg_ratio);
				rg_ratio = (1.0-ratio)*chromatix->AWB_bayer_algo_data.reference[AGW_AWB_A].RG_ratio + 
					ratio*(para->max_reasonable_rg);
			} else {
				CDBG("%s, %d, low temprature, path 3", __func__, __LINE__);
				rg_ratio = (para->max_reasonable_rg);
			}

			CDBG("%s, %d adjust gains due to color temperature, %f, %f, %f", 
				__func__, __LINE__, 1.0/rg_ratio, 1.0, 1.0/bg_ratio);
			
			//decrease red/blue compensation due to green severity
			if (green_severity > 0.5) {
				ratio = 1.0;
			} else {
				ratio = green_severity / 0.5;
			}
			red_compensation_coe *= (1.0 - ratio);
			blue_compensation_coe *= (1.0 - ratio);
			
			//red compensation
			ratio = bg_ratio / orig_bg_ratio;
			if ((ratio > 1.0) && (ratio > (rg_ratio/orig_rg_ratio))) {
				ratio = 1.0 + red_compensation_coe*(ratio-1.0);
				if (ratio > (1.0+red_com_limit)) {
					ratio = (1.0+red_com_limit);
				}
				CDBG("%s, %d, low CCT red compensation ratio %f", __func__, __LINE__, ratio);
				rg_ratio = ((orig_rg_ratio/ratio)<rg_ratio) ? (orig_rg_ratio/ratio) : rg_ratio;
			}

			//blue compensation
			ratio = 1.0 + blue_compensation_coe*(bg_ratio/orig_bg_ratio-1.0);
			if (ratio > (1.0+blue_com_limit)) {
				ratio = (1.0+blue_com_limit);
			}
			if (ratio > 1.0) {
				CDBG("%s, %d, low CCT blue compensation ratio %f", __func__, __LINE__, ratio);
				bg_ratio *= ratio;
			}
		}		
		CDBG("%s, %d adjust gains due to color temperature, %f, %f, %f", 
			__func__, __LINE__, 1.0/rg_ratio, 1.0, 1.0/bg_ratio);
	

		//adjust gains for high color temperature
		if (private->instant_color_temp >= 4000) {
			float upper_ref_rg_ratio, upper_ref_bg_ratio;
			float lower_ref_rg_ratio, lower_ref_bg_ratio;
			float ratio1, ratio2;

			if (bg_ratio < chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio) {
				upper_ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].RG_ratio;
				upper_ref_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio;
				lower_ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].RG_ratio;
				lower_ref_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio;
			} else {
				upper_ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].RG_ratio;
				upper_ref_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D75].BG_ratio;
				lower_ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].RG_ratio;
				lower_ref_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio;
			}

			if (upper_ref_bg_ratio > lower_ref_bg_ratio) {
				if (bg_ratio < lower_ref_bg_ratio) {
					ref_bg_ratio = lower_ref_bg_ratio;
				} else if (bg_ratio > upper_ref_bg_ratio) {
					ref_bg_ratio = upper_ref_bg_ratio;
				} else {
					ref_bg_ratio = bg_ratio;
				}
				CDBG("%s, %d, upper_ref_rg_ratio %f, upper_ref_bg_ratio %f", 
					__func__, __LINE__, upper_ref_rg_ratio, upper_ref_bg_ratio);
				CDBG("%s, %d, lower_ref_rg_ratio %f, lower_ref_bg_ratio %f", 
					__func__, __LINE__, lower_ref_rg_ratio, lower_ref_bg_ratio);
				
				ratio = (ref_bg_ratio-lower_ref_bg_ratio) / (upper_ref_bg_ratio-lower_ref_bg_ratio);
				ref_rg_ratio = upper_ref_rg_ratio*ratio + lower_ref_rg_ratio*(1.0-ratio);

				CDBG("%s, %d, ref_rg_ratio %f, ref_bg_ratio %f", 
					__func__, __LINE__, ref_rg_ratio, ref_bg_ratio);

				ratio1 = ratio2 = 0.0;
				if (rg_ratio < (ref_rg_ratio*0.99)) { 
					CDBG("%s, %d, high temprature, path 1", __func__, __LINE__);
					
					// weight from CCT & locus
					float cct_ratio, locus_ratio;
					if (private->instant_color_temp>=5300) {
						float dis = ref_rg_ratio - rg_ratio;

						cct_ratio = 1.0;
						if (dis <= 0.00) {
							locus_ratio = 0.0;
						} else if (dis <= 0.1) {
							locus_ratio = -0.5 * (dis-0.00) / (0.1-0.00);
						} else {
							locus_ratio = -0.5;
						}
					} else {
						if (private->instant_color_temp>=5000) {
							cct_ratio = 0.2 + 0.8*((float)private->instant_color_temp-5000)/300;
						} else if (private->instant_color_temp>=4700){
							cct_ratio = 0.2 * ((float)private->instant_color_temp-4700)/300;
						} else {
							cct_ratio = 0;
						}
						
						locus_ratio = cct_ratio;
						if ((rg_ratio > chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].RG_ratio) && 
							(chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].RG_ratio != 
							chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].RG_ratio)) {
							
							key_point_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].RG_ratio*para->d50_cf_ref_ratio + 
								chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].RG_ratio*(1.0-para->d50_cf_ref_ratio);
							key_point_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].BG_ratio*para->d50_cf_ref_ratio + 
								chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio*(1.0-para->d50_cf_ref_ratio);
						
							if (rg_ratio > key_point_rg_ratio) {
								locus_ratio *= 0.0;
							} else {
								locus_ratio *= -(key_point_rg_ratio - rg_ratio) /
									(key_point_rg_ratio - chromatix->AWB_bayer_algo_data.reference[AGW_AWB_CUSTOM_FLO].RG_ratio);
							}		
						} else {
							locus_ratio *= -1.0;
						}
					}

					ratio = cct_ratio + locus_ratio;
					ratio1 = ratio2 = ratio;
					CDBG("%s, %d, ratio1 %f, ratio2 %f", __func__, __LINE__, ratio1, ratio2);
					
					if (brightness_weighted_blue_sky_severity > 0) {
						#if 0
						ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].RG_ratio*para->d50_d65_left_ref_ratio +
							chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].RG_ratio*(1.0-para->d50_d65_left_ref_ratio);
						ref_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio*para->d50_d65_left_ref_ratio +
							chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio*(1.0-para->d50_d65_left_ref_ratio);
						#endif

						ratio = (brightness_weighted_blue_sky_severity>0.7) ? 0.7 : brightness_weighted_blue_sky_severity;
						ref_rg_ratio = ref_rg_ratio*(1.0-brightness_weighted_blue_sky_severity) + 
							chromatix->AWB_bayer_algo_data.reference[AGW_AWB_NOON].RG_ratio*brightness_weighted_blue_sky_severity;
						ref_bg_ratio = ref_bg_ratio*(1.0-brightness_weighted_blue_sky_severity) + 
							chromatix->AWB_bayer_algo_data.reference[AGW_AWB_NOON].BG_ratio*brightness_weighted_blue_sky_severity;
						CDBG("%s, %d, ref_rg_ratio %f, ref_bg_ratio %f", __func__, __LINE__, ref_rg_ratio, ref_bg_ratio);

						//weight from brightness
						if (private->aec_parms.lux_idx > (private->aec_parms.indoor_index-0)) {
							ratio = 0.0;
						} else if (private->aec_parms.lux_idx > (private->aec_parms.indoor_index-50)) {
							ratio = ((private->aec_parms.indoor_index-0) - private->aec_parms.lux_idx) / 
								((private->aec_parms.indoor_index-0) - (private->aec_parms.indoor_index-50));
						} else {
							ratio = 1.0;
						}
						ratio1 = ratio1 + (1.0-ratio1)*ratio;
						ratio2 = ratio2 + (1.0-ratio2)*ratio;
						CDBG("%s, %d, ratio1 %f, ratio2 %f", __func__, __LINE__, ratio1, ratio2);

						//blue sky  
						ratio1 = ratio1 + (1.0-ratio1)*brightness_weighted_blue_sky_severity;
						ratio2 = ratio2 + (1.0-ratio2)*brightness_weighted_blue_sky_severity;					
						CDBG("%s, %d, ratio1 %f, ratio2 %f", __func__, __LINE__, ratio1, ratio2);
					}

					if (ratio1 > 1.0) {
						ratio1 = 1.0;
					}
					if (ratio2 > 1.0) {
						ratio2 = 1.0;
					}				

					ratio1 *= para->d50_d65_left_ref_coe;
					ratio2 *= para->d50_d65_left_ref_coe;

					private->outdoor_coe = ratio1;
					
					#if 0
					if (awb_analysis_out.valid_hi_smp > 0) {						
						if (bg_ratio < chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio) {
							ratio = 1.0;
						} else if (bg_ratio < (chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio*para->d50_d65_left_ref_ratio+
							chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio*(1.0-para->d50_d65_left_ref_ratio))) {
							ratio = (ref_bg_ratio - chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio) /
								((chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio*para->d50_d65_left_ref_ratio+
									chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio*(1.0-para->d50_d65_left_ref_ratio)) - 
								 chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio);
							ratio = 1.0 - ratio;
						} else {
							ratio = 0.0;
						}						
						ratio = (1.0-para->d50_d65_left_ref_coe) + para->d50_d65_left_ref_coe*ratio;	
						
						if (awb_analysis_out.hi_bright_rg < ref_rg_ratio) {							
							ref_rg_ratio = awb_analysis_out.hi_bright_rg*ratio + ref_rg_ratio*(1.0-ratio);
						}
						if (awb_analysis_out.hi_bright_bg > ref_bg_ratio) {							
							ref_bg_ratio = awb_analysis_out.hi_bright_bg*ratio + ref_bg_ratio*(1.0-ratio);
						}
						if (rg_ratio < ref_rg_ratio) {
							ratio1 *= ref_rg_ratio/rg_ratio;
							if (ratio1 > 1.0) {
								ratio1 = 1.0;
							}
						}
						if (bg_ratio < ref_bg_ratio) {
							ratio2 *= ref_bg_ratio/bg_ratio;
							if (ratio2 > 1.0) {
								ratio2 = 1.0;
							}
						}
					}
					#endif
				} else {
					ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].RG_ratio*para->d50_d65_right_ref_ratio +
						chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].RG_ratio*(1.0-para->d50_d65_right_ref_ratio);
					ref_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio*para->d50_d65_right_ref_ratio +
						chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D65].BG_ratio*(1.0-para->d50_d65_right_ref_ratio);

					if (bg_ratio > chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio) {
						CDBG("%s, %d, high temprature, path 2", __func__, __LINE__);

						//weight from scene detection
						ratio = brightness_weighted_green_severity + brightness_weighted_blue_sky_severity;
						if (ratio > 0.4) {
							ratio = 1.0;
						} else {
							ratio /= 0.4;
						}
						CDBG("%s, %d, ratio %f", __func__, __LINE__, ratio);

						//weight from brightness
						float brightness_ratio;
						if (private->aec_parms.lux_idx > (private->aec_parms.indoor_index+50)) {
							brightness_ratio = 0.0;
						} else if (private->aec_parms.lux_idx > private->aec_parms.outdoor_index) {
							brightness_ratio = ((private->aec_parms.indoor_index+50) - private->aec_parms.lux_idx) / 
								((private->aec_parms.indoor_index+50) - private->aec_parms.outdoor_index);
						} else {
							brightness_ratio = 1.0;
						}	
						ratio += brightness_ratio*para->d50_d65_right_brightness_coe;
						if (ratio > 1.0) {
							ratio = 1.0;
						}
						CDBG("%s, %d, ratio %f", __func__, __LINE__, ratio);
						
						ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_NOON].RG_ratio*ratio + ref_rg_ratio*(1.0-ratio);
						ref_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_NOON].BG_ratio*ratio + ref_bg_ratio*(1.0-ratio);	
						
						//additional bias for green plants
						ratio = 2.0 * brightness_weighted_green_severity;
						if (ratio > 1.0) {
							ratio = 1.0;
						} 
						ref_rg_ratio = ref_rg_ratio*(1.0 + para->d50_d65_right_green_bias*ratio);
						ref_bg_ratio = ref_bg_ratio*(1.0 - para->d50_d65_right_green_bias*ratio); 
						
						CDBG("%s, %d, ref_rg_ratio %f, ref_bg_ratio %f", __func__, __LINE__, ref_rg_ratio, ref_bg_ratio);
						
						ratio1 = brightness_ratio*para->d50_d65_right_ref_coe;
						ratio2 = brightness_ratio*para->d50_d65_right_ref_coe;
						private->outdoor_coe = 1.0;
					} else {
						CDBG("%s, %d, high temprature, path 3", __func__, __LINE__);
						
						ratio = blue_sky_severity;
						ref_rg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_NOON].RG_ratio*ratio + ref_rg_ratio*(1.0-ratio);
						ref_bg_ratio = chromatix->AWB_bayer_algo_data.reference[AGW_AWB_NOON].BG_ratio*ratio + ref_bg_ratio*(1.0-ratio);	

						if (brightness_weighted_green_severity <= 0.35) {
							ratio = brightness_weighted_green_severity / 0.35;
							ref_rg_ratio = ref_rg_ratio*(1.0 + 0.01*ratio);
							ref_bg_ratio = ref_bg_ratio*(1.0 - 0.01*ratio); 
						} else if (brightness_weighted_green_severity <= 0.8) {
							ratio = (brightness_weighted_green_severity - 0.35) / 0.45;
							ref_rg_ratio = ref_rg_ratio*(1.01 + 0.02*ratio);
							ref_bg_ratio = ref_bg_ratio*(1.01 - 0.02*ratio); 
						} else {
							ref_rg_ratio = ref_rg_ratio*(1.0 + 0.03*ratio);
							ref_bg_ratio = ref_bg_ratio*(1.0 - 0.03*ratio); 
						}
						CDBG("%s, %d, ref_rg_ratio %f, ref_bg_ratio %f", __func__, __LINE__, ref_rg_ratio, ref_bg_ratio);
						
						#if 0
						//adjust reference point		
						if (private->aec_parms.lux_idx > (private->aec_parms.outdoor_index+50)) {
							ratio = 0.0;
						} else if (private->aec_parms.lux_idx > private->aec_parms.outdoor_index) {
							ratio = ((private->aec_parms.outdoor_index+50) - private->aec_parms.lux_idx) / 50;
						} else {
							ratio = 1.0;
						}
						
						if (awb_analysis_out.valid_hi_smp > 0) {
							if ((awb_analysis_out.hi_bright_rg > (rg_ratio-0.05) && 
								awb_analysis_out.hi_bright_bg < (bg_ratio+0.05))) {
								float dist = (awb_analysis_out.hi_bright_rg-(rg_ratio-0.05))*(awb_analysis_out.hi_bright_rg-(rg_ratio-0.05)) +
										((bg_ratio+0.05)-awb_analysis_out.hi_bright_bg)*((bg_ratio+0.05)-awb_analysis_out.hi_bright_bg);
								CDBG("%s, %d, dist*dist = %f", __func__, __LINE__, dist);
								dist /= 0.01;
								if (dist > 1.0) {
									dist = 1.0;
								}
								ratio *= (1.0 - dist);
							}
						}
						
						ref_rg_ratio = ratio*ref_rg_ratio + 
							(1.0-ratio)*chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].RG_ratio;
						ref_bg_ratio = ratio*ref_bg_ratio + 
							(1.0-ratio)*chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio;
						#endif
						
						//weight from brightness
						if (private->aec_parms.lux_idx > (private->aec_parms.indoor_index-0)) {
							ratio1 = 0.08;
							ratio2 = 0.15;
						} else if (private->aec_parms.lux_idx > (private->aec_parms.outdoor_index-0)) {
							ratio1 = 0.08 + 0.92*((private->aec_parms.indoor_index-0) - private->aec_parms.lux_idx) / 
								((private->aec_parms.indoor_index-0) - (private->aec_parms.outdoor_index-0));
							ratio2 = 0.15 + 0.85*((private->aec_parms.indoor_index-0) - private->aec_parms.lux_idx) / 
								((private->aec_parms.indoor_index-0) - (private->aec_parms.outdoor_index-0));
						} else {
							ratio1 = 1.0;
							ratio2 = 1.0;
						}
						CDBG("%s, %d, ratio1 %f, ratio2 %f", __func__, __LINE__, ratio1, ratio2);
												
						//weight from awb
						if (bg_ratio < chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].BG_ratio) {
							ratio = 0.0;
						} else if (bg_ratio < (2*chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].BG_ratio +
								chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio)/3) {
							ratio = (bg_ratio - chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].BG_ratio) /
								((chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio - 
									chromatix->AWB_bayer_algo_data.reference[AGW_AWB_WARM_FLO].BG_ratio)/3);
						} else if (bg_ratio < chromatix->AWB_bayer_algo_data.reference[AGW_AWB_D50].BG_ratio){
							ratio = 1.0;
						}
						ratio1 *= ratio;
						ratio2 *= ratio;
						CDBG("%s, %d, ratio1 %f, ratio2 %f", __func__, __LINE__, ratio1, ratio2);
						
						private->outdoor_coe = ratio1;
						
						ratio1 *= para->d50_d65_right_ref_coe;
						ratio2 *= para->d50_d65_right_ref_coe*0.5;
					}
				}
				
				CDBG("%s, %d, ref_rg_ratio %f, ref_bg_ratio %f", __func__, __LINE__, ref_rg_ratio, ref_bg_ratio);
				CDBG("%s, %d, ratio1 %f, ratio2 %f", __func__, __LINE__, ratio1, ratio2);
				rg_ratio = ratio1*ref_rg_ratio + (1.0-ratio1)*rg_ratio;
				bg_ratio = ratio2*ref_bg_ratio + (1.0-ratio2)*bg_ratio;
			}
		}
	
		rgb_gain->g_gain = 1.0;
		rgb_gain->r_gain = rgb_gain->g_gain / rg_ratio;
		rgb_gain->b_gain = rgb_gain->g_gain / bg_ratio;
		CDBG("%s, %d, adjust gains due to color temperature, %f, %f, %f", 
			__func__, __LINE__, rgb_gain->r_gain, rgb_gain->g_gain, rgb_gain->b_gain);
	
		min_gain = MIN(MIN(rgb_gain->r_gain, rgb_gain->g_gain), rgb_gain->b_gain);
		rgb_gain->r_gain = rgb_gain->r_gain / min_gain;
		rgb_gain->g_gain = rgb_gain->g_gain / min_gain;
		rgb_gain->b_gain = rgb_gain->b_gain / min_gain;

		if (rgb_gain->g_gain > 3.99) {
			rgb_gain->g_gain = 3.99;
		}
		if (rgb_gain->b_gain > 3.99) {
			rgb_gain->b_gain = 3.99;
		}
		if (rgb_gain->r_gain > 3.99) {
			rgb_gain->r_gain = 3.99;
		}
	}else if (prev_refined_awb_gain.r_gain>0 && 
		prev_refined_awb_gain.g_gain>0 && prev_refined_awb_gain.b_gain>0) {
		*rgb_gain = prev_refined_awb_gain;
	}

	if (prev_refined_awb_gain.r_gain>0 && 
		prev_refined_awb_gain.g_gain>0 && prev_refined_awb_gain.b_gain>0) {
		rgb_gain->r_gain = prev_refined_awb_gain.r_gain*(1.0-0.15) + 
							rgb_gain->r_gain*0.15;
		rgb_gain->g_gain = prev_refined_awb_gain.g_gain*(1.0-0.15) + 
							rgb_gain->g_gain*0.15;
		rgb_gain->b_gain = prev_refined_awb_gain.b_gain*(1.0-0.15) + 
							rgb_gain->b_gain*0.15;
	}
	prev_refined_awb_gain = *rgb_gain;
	ALOGE("%s, r_gain %f, g_gain %f, b_gain %f", 
		__func__, rgb_gain->r_gain, rgb_gain->g_gain, rgb_gain->b_gain);	
	return;
}


static void awb_port_refine_CCT(awb_output_data_t *output, void *p)
{
	mct_port_t	*port = (mct_port_t *)p;
	awb_port_private_t *private = NULL;
	
	if (!output || !port) {
	  ALOGE("%s: input error", __func__);
	  return;
	}
	private = (awb_port_private_t *)(port->port_private);
	if (!private)
		return;
	
	if (private->awb_update_data.awb_update.color_temp > 0) {
	  uint32_t color_temp = output->color_temp;;
	
      #if 0
	  if ((color_temp > 4000) && (color_temp < 4900)) {
		  if (private->aec_parms.lux_idx <= private->aec_parms.outdoor_index) {
			  color_temp = 4400;
		  } else if (private->aec_parms.lux_idx <= private->aec_parms.indoor_index) {
			  float ratio = (float)(private->aec_parms.lux_idx - private->aec_parms.outdoor_index) /
				  (private->aec_parms.indoor_index - private->aec_parms.outdoor_index);
			  color_temp = 4400*(1.0-ratio) + output->color_temp*ratio;
		  }
	  } else {
		  color_temp = output->color_temp;
	  }
  	  #endif
	  private->instant_color_temp = (uint32_t)(0.5 + color_temp*0.1 + 
		  ((float)private->instant_color_temp) * 0.9);
	  ////////////////////////////////////////////////////////////////////////////
	  #if 0
	  if (output->color_temp < 5000) {
		  if (private->aec_parms.lux_idx <= private->aec_parms.outdoor_index) {
			  output->color_temp = 5300;
		  } else if (private->aec_parms.lux_idx <= private->aec_parms.indoor_index) {
			  float ratio = (float)(private->aec_parms.lux_idx - private->aec_parms.outdoor_index) /
				  (private->aec_parms.indoor_index - private->aec_parms.outdoor_index);
			  output->color_temp = 5300*(1.0-ratio) + output->color_temp*ratio;
		  }
	  }
	  #endif
	  output->stats_update.awb_update.color_temp = 
		  (uint32_t)(0.5 + ((float)output->color_temp)*0.1 + 
		  ((float)private->awb_update_data.awb_update.color_temp) * 0.9);
	  private->awb_update_data.awb_update.color_temp = 
		  output->stats_update.awb_update.color_temp;
	} else {
	  output->stats_update.awb_update.color_temp = output->color_temp;
	  private->instant_color_temp = output->color_temp;
	}

	private->outdoor_coe = 0.0;
	ALOGE("%s, new CCT %d, updated CCT %d", __func__, 
	  output->color_temp,
	  output->stats_update.awb_update.color_temp);
}
//block ends
/****************************************************************************************/

/** awb_port_callback
 *
 **/
static void awb_port_callback(awb_output_data_t *output, void *p)
{
  mct_port_t  *port = (mct_port_t *)p;
  awb_gain_t rgb_gain;
  awb_port_private_t *private = NULL;

  if (!output || !port) {
    ALOGE("%s: input error", __func__);
    return;
  }
  private = (awb_port_private_t *)(port->port_private);
  if (!private)
      return;
  /* populate the stats_upate object to be sent out*/
    CDBG("%s: STATS_UPDATE_AWB", __func__);
  
    /* skip error AWB update in LED-assist snapshot, tanrifei, 20140214 */
    if ((private->stream_type == CAM_STREAM_TYPE_SNAPSHOT) &&
	    (private->aec_parms.led_state == MSM_CAMERA_LED_OFF) &&
	    (private->aec_parms.use_led_estimation == TRUE)) {
	    return;
    }
    /* Add end */
  
    if (AWB_UPDATE == output->type) {
      awb_port_update_awb_info(port, output);
      if (output->eztune_data.ez_running == TRUE) {
        awb_port_send_awb_info_to_metadata(port, output);
      }
      output->stats_update.flag = STATS_UPDATE_AWB;
      /* memset the output struct */
      memset(&output->stats_update.awb_update, 0,
        sizeof(output->stats_update.awb_update));
      /*RGB gain*/
      rgb_gain.r_gain = output->r_gain;
      rgb_gain.g_gain = output->g_gain;
      rgb_gain.b_gain = output->b_gain;
      /* color_temp */
	  
	  /* make CCT change more smoothly,  modified by tanrifei, 20131101*/
	  #if 0
      output->stats_update.awb_update.color_temp = output->color_temp;
	  #else
	  awb_port_refine_CCT(output, p);
	  #endif
	  /* modify end. */
	  
	  /* refine AWB gains, added by tanrifei, 20131108*/
	  awb_port_refine_gains(p, &rgb_gain);
	  /* add end */

	  /* refeine CCT, tanrifei, 20140902 */
	  if (output->stats_update.awb_update.color_temp > 4000 && 
	  	output->stats_update.awb_update.color_temp < 5500) {
	  	  if (private->outdoor_coe > 1.0) {
			private->outdoor_coe = 1.0;
		  } else if (private->outdoor_coe < 0) {
			private->outdoor_coe = 0.0;
		  }
		  output->stats_update.awb_update.color_temp = 
		  	output->stats_update.awb_update.color_temp * (1-private->outdoor_coe) +
		  	5500*private->outdoor_coe;
	  }
	  ALOGE("%s, outdoor_coe %f, new CCT %d, updated CCT %d", __func__, 
	  	private->outdoor_coe,
		output->color_temp,
		output->stats_update.awb_update.color_temp);
	  output->color_temp = output->stats_update.awb_update.color_temp;
	  /* add end */

      output->stats_update.awb_update.gain = rgb_gain;
      output->stats_update.awb_update.color_temp = output->color_temp;
      output->stats_update.awb_update.wb_mode = output->wb_mode;
      output->stats_update.awb_update.best_mode = output->best_mode;

      /* TBD: grey_world_stats is always true for bayer. For YUV change later */
      output->stats_update.awb_update.grey_world_stats = TRUE;
      memcpy(output->stats_update.awb_update.sample_decision,
        output->samp_decision, sizeof(int) * 64);
      CDBG("%s: awb update: (r,g,b gain)= (%f, %f, %f), color_temp=%d",
            __func__, output->stats_update.awb_update.gain.r_gain,
             output->stats_update.awb_update.gain.g_gain,
            output->stats_update.awb_update.gain.b_gain,
            output->stats_update.awb_update.color_temp);

      if(output->frame_id != private->cur_sof_id) {
        /* This will be called when the events happen in the the order
          STAT1 SOF2 OUTPUT1. It ensures the AWB outputs
          are always synchronouse with SOFs  */
        MCT_OBJECT_LOCK(port);
        private->awb_update_flag = TRUE;
        MCT_OBJECT_UNLOCK(port);
        awb_send_events(port, &(output->stats_update));
        return;
      }
       MCT_OBJECT_LOCK(port);
      memcpy(&private->awb_update_data, &output->stats_update,
        sizeof(stats_update_t));
      private->awb_update_flag = TRUE;
      MCT_OBJECT_UNLOCK(port);
    } else if(AWB_SEND_EVENT == output->type)
      awb_send_events(port, &(private->awb_update_data));
  return;
}

/** port_stats_check_identity
 *    @data1: port's existing identity;
 *    @data2: new identity to compare.
 *
 *  Return TRUE if two session index in the dentities are equalequal,
 *  Stats modules are linked ONLY under one session.
 **/
static boolean awb_port_check_identity(void *data1, void *data2)
{
  return (((*(unsigned int *)data1) ==
          (*(unsigned int *)data2)) ? TRUE : FALSE);
}

/** awb_port_check_session
 *
 **/
static boolean awb_port_check_session(void *data1, void *data2)
{
  return (((*(unsigned int *)data1) & 0xFFFF0000) ==
          ((*(unsigned int *)data2) & 0xFFFF0000) ?
          TRUE : FALSE);
}

/** awb_port_check_stream
 *
 **/
static boolean awb_port_check_stream(void *data1, void *data2)
{
  return ( ((*(unsigned int *)data1) & 0x0000FFFF )==
           ((*(unsigned int *)data2) & 0x0000FFFF) ?
          TRUE : FALSE);
}

/** awb_port_check_port_availability
 *
 *
 *
 **/
boolean awb_port_check_port_availability(mct_port_t *port, unsigned int *session)
{
  if (port->port_private == NULL)
    return TRUE;

  if (mct_list_find_custom(MCT_OBJECT_CHILDREN(port), session,
      awb_port_check_session) != NULL)
    return TRUE;

  return FALSE;
}

static boolean awb_port_event_sof( awb_port_private_t *port_private,
                               mct_event_t *event)
{
  int rc =  TRUE;
  q3a_thread_aecawb_msg_t * awb_msg = (q3a_thread_aecawb_msg_t *)
      malloc(sizeof(q3a_thread_aecawb_msg_t));
    if (awb_msg == NULL)
      return rc;
    memset(awb_msg, 0, sizeof(q3a_thread_aecawb_msg_t));
    awb_msg->type = MSG_AWB_SEND_EVENT;
   rc = q3a_aecawb_thread_en_q_msg(port_private->thread_data, awb_msg);
   return rc;
}

static boolean awb_port_event_stats_data( awb_port_private_t *port_private,
                               mct_event_t *event)
{
  boolean rc = TRUE;
  mct_event_module_t *mod_evt = &(event->u.module_event);
  mct_event_stats_isp_t *stats_event ;
  stats_t  * awb_stats;
  stats_event =(mct_event_stats_isp_t *)(mod_evt->module_event_data);
  awb_stats = &(port_private->awb_object.stats);

  awb_stats->stats_type_mask = 0; /*clear the mask*/
  boolean  send_flag = FALSE;
  if (stats_event) {
    q3a_thread_aecawb_msg_t * awb_msg = (q3a_thread_aecawb_msg_t *)
      malloc(sizeof(q3a_thread_aecawb_msg_t));
    if (awb_msg != NULL) {
      memset(awb_msg, 0, sizeof(q3a_thread_aecawb_msg_t));
      awb_msg->u.stats = awb_stats;
      awb_stats->frame_id = stats_event->frame_id;
      if ((stats_event->stats_mask & (1 << MSM_ISP_STATS_AWB)) &&
           stats_event->stats_data[MSM_ISP_STATS_AWB].stats_buf) {
        awb_msg->type = MSG_AWB_STATS;
        awb_stats->stats_type_mask |= STATS_AWB;
        CDBG("got awb stats");
        send_flag = TRUE;
        memcpy(&awb_stats->yuv_stats.q3a_awb_stats,
               stats_event->stats_data[MSM_ISP_STATS_AWB].stats_buf,
               sizeof(q3a_awb_stats_t));
      } else if ((stats_event->stats_mask & (1 << MSM_ISP_STATS_BG)) &&
                  stats_event->stats_data[MSM_ISP_STATS_BG].stats_buf) {

        CDBG("got bg stats");
        awb_stats->stats_type_mask |= STATS_BG;
        awb_msg->type = MSG_BG_AWB_STATS;
        send_flag = TRUE;
        memcpy(&awb_stats->bayer_stats.q3a_bg_stats,
               stats_event->stats_data[MSM_ISP_STATS_BG].stats_buf,
               sizeof(q3a_bg_stats_t));
      }
      if (send_flag) {
        CDBG("%s: msg type=%d, stats=%p, mask=0x%x mask addr=%p", __func__,awb_msg->type, awb_msg->u.stats,
              awb_msg->u.stats->stats_type_mask, &(awb_msg->u.stats->stats_type_mask));
        rc = q3a_aecawb_thread_en_q_msg(port_private->thread_data, awb_msg);
      } else {
        free(awb_msg);
      }
    }
  }
  return rc;
}
/** awb_port_proc_get_aec_data
 *
 *
 *
 **/
static boolean awb_port_proc_get_awb_data(mct_port_t *port,
  stats_get_data_t *stats_get_data)
{
    q3a_thread_aecawb_msg_t *awb_msg   = (q3a_thread_aecawb_msg_t *)
    malloc(sizeof(q3a_thread_aecawb_msg_t));
    awb_port_private_t *private = (awb_port_private_t *)(port->port_private);
    boolean rc = FALSE;
    if (awb_msg) {
      awb_msg->sync_flag = TRUE;
      memset(awb_msg, 0 , sizeof(q3a_thread_aecawb_msg_t));
      awb_msg->sync_flag = TRUE;
      awb_msg->type = MSG_AWB_GET;
      awb_msg->u.awb_get_parm.type = AWB_PARMS;
      rc = q3a_aecawb_thread_en_q_msg(private->thread_data, awb_msg);
      stats_get_data->flag = STATS_UPDATE_AWB;
      stats_get_data->awb_get.g_gain = awb_msg->u.awb_get_parm.u.awb_gains.curr_gains.g_gain;
      stats_get_data->awb_get.r_gain = awb_msg->u.awb_get_parm.u.awb_gains.curr_gains.r_gain;
      stats_get_data->awb_get.b_gain = awb_msg->u.awb_get_parm.u.awb_gains.curr_gains.b_gain;
      free(awb_msg);
    } else {
      CDBG_ERROR("%s:%d Not enough memory", __func__, __LINE__);
    }
  return rc;
}

/** awb_process_downstream_mod_event
 *    @port:
 *    @event:
 **/
static boolean awb_process_downstream_mod_event( mct_port_t *port,
                                                  mct_event_t *event)
{
  boolean rc = TRUE;
  q3a_thread_aecawb_msg_t * awb_msg = NULL;
  mct_event_module_t *mod_evt = &(event->u.module_event);
  awb_port_private_t * private = (awb_port_private_t *)(port->port_private);

  CDBG("%s: Proceess module event of type: %d", __func__, mod_evt->type);

  switch (mod_evt->type) {
  case MCT_EVENT_MODULE_STATS_GET_THREAD_OBJECT: {
    q3a_thread_aecawb_data_t *data =
      (q3a_thread_aecawb_data_t *)(mod_evt->module_event_data);

    private->thread_data = data->thread_data;

    data->awb_port = port;
    data->awb_cb   = awb_port_callback;
    data->awb_obj  = &(private->awb_object);
    rc = TRUE;
  } /* case MCT_EVENT_MODULE_STATS_GET_THREAD_OBJECT */
    break;

  case MCT_EVENT_MODULE_STATS_DATA: {
    rc = awb_port_event_stats_data(private, event);
  } /* case MCT_EVENT_MODULE_STATS_DATA */
    break;

  case MCT_EVENT_MODULE_SOF_NOTIFY: {
    mct_bus_msg_isp_sof_t *sof_event;
      sof_event =(mct_bus_msg_isp_sof_t *)(mod_evt->module_event_data);
    private->cur_sof_id = sof_event->frame_id;
    rc = awb_port_event_sof(private, event);
  }
  break;

  case MCT_EVENT_MODULE_SET_RELOAD_CHROMATIX:
  case MCT_EVENT_MODULE_SET_CHROMATIX_PTR: {
    modulesChromatix_t *mod_chrom =
      (modulesChromatix_t *)mod_evt->module_event_data;
    awb_msg = (q3a_thread_aecawb_msg_t *)
      malloc(sizeof(q3a_thread_aecawb_msg_t));
    if (awb_msg != NULL ) {
      memset(awb_msg, 0, sizeof(q3a_thread_aecawb_msg_t));
      awb_msg->type = MSG_AWB_SET;
      awb_msg->u.awb_set_parm.type = AWB_SET_PARAM_INIT_CHROMATIX_SENSOR;
      /*To Do: for now hard-code the stats type and op_mode for now.*/
      awb_msg->u.awb_set_parm.u.init_param.stats_type = AWB_STATS_BAYER;
      awb_msg->u.awb_set_parm.u.init_param.chromatix = mod_chrom->chromatixPtr;
      awb_msg->u.awb_set_parm.u.init_param.comm_chromatix = mod_chrom->chromatixComPtr;
      CDBG("%s:stream_type=%d op_mode=%d", __func__,
        private->stream_type, awb_msg->u.awb_set_parm.u.init_param.op_mode);
	  
	  /* keep chromatix pointer, added by tanrifei, 20131108*/
	  private->chromatixPtr = mod_chrom->chromatixPtr;
	  private->chromatixComPtr = mod_chrom->chromatixComPtr;
	  /* add end*/

      rc = q3a_aecawb_thread_en_q_msg(private->thread_data, awb_msg);
      CDBG("%s: Enqueing AWB message returned: %d", __func__, rc);
    }
    else {
      CDBG_ERROR("%s: Failure allocating memory for AWB msg!", __func__);
    }
  }
    break;

  case MCT_EVENT_MODULE_STATS_AEC_UPDATE: {
    stats_update_t *stats_update =
      (stats_update_t *)mod_evt->module_event_data;

    awb_msg = (q3a_thread_aecawb_msg_t *)
      malloc(sizeof(q3a_thread_aecawb_msg_t));
    if (awb_msg != NULL ) {
      memset(awb_msg, 0, sizeof(q3a_thread_aecawb_msg_t));
      awb_msg->type = MSG_AWB_SET;
      awb_msg->is_priority = TRUE;
      awb_msg->u.awb_set_parm.type = AWB_SET_PARAM_AEC_PARM;

      awb_msg->u.awb_set_parm.u.aec_parms.exp_index =
        stats_update->aec_update.exp_index_for_awb;
      awb_msg->u.awb_set_parm.u.aec_parms.indoor_index =
        stats_update->aec_update.indoor_index;
      awb_msg->u.awb_set_parm.u.aec_parms.outdoor_index =
        stats_update->aec_update.outdoor_index;
      awb_msg->u.awb_set_parm.u.aec_parms.lux_idx =
        stats_update->aec_update.lux_idx;
      awb_msg->u.awb_set_parm.u.aec_parms.aec_settled =
        stats_update->aec_update.settled;
      awb_msg->u.awb_set_parm.u.aec_parms.cur_luma =
        stats_update->aec_update.cur_luma;
      awb_msg->u.awb_set_parm.u.aec_parms.target_luma =
        stats_update->aec_update.target_luma;
      awb_msg->u.awb_set_parm.u.aec_parms.cur_line_cnt =
        stats_update->aec_update.linecount;
      awb_msg->u.awb_set_parm.u.aec_parms.cur_real_gain =
        stats_update->aec_update.real_gain;
      awb_msg->u.awb_set_parm.u.aec_parms.stored_digital_gain =
        stats_update->aec_update.stored_digital_gain;
      awb_msg->u.awb_set_parm.u.aec_parms.flash_sensitivity =
        *(q3q_flash_sensitivity_t *)stats_update->aec_update.flash_sensitivity;
      awb_msg->u.awb_set_parm.u.aec_parms.led_state =
        stats_update->aec_update.led_state;
      awb_msg->u.awb_set_parm.u.aec_parms.use_led_estimation  =
        stats_update->aec_update.use_led_estimation;
	  
	  /* keep aec parameters , added by tanrifei, 20131218*/
	  memcpy(&private->aec_parms, &awb_msg->u.awb_set_parm.u.aec_parms, 
	  	sizeof(awb_set_aec_parms));
	  /* add end*/
      rc = q3a_aecawb_thread_en_q_msg(private->thread_data, awb_msg);
    }
  }
  break;
  case  MCT_EVENT_MODULE_STATS_GET_DATA: {
  }
  break;

  case MCT_EVENT_MODULE_SET_STREAM_CONFIG: {
    CDBG("ddd MCT_EVENT_MODULE_SET_STREAM_CONFIG");
    q3a_thread_aecawb_msg_t *awb_msg = (q3a_thread_aecawb_msg_t *)
      malloc(sizeof(q3a_thread_aecawb_msg_t));
    if (awb_msg == NULL )
      break;
    memset(awb_msg, 0, sizeof(q3a_thread_aecawb_msg_t));
    awb_msg->type = MSG_AWB_SET;
    awb_msg->u.awb_set_parm.type = AWB_SET_PARAM_OP_MODE;
    switch (private->stream_type) {
      case CAM_STREAM_TYPE_VIDEO: {
         awb_msg->u.awb_set_parm.u.init_param.op_mode =
           Q3A_OPERATION_MODE_CAMCORDER;
      }
        break;

      case CAM_STREAM_TYPE_PREVIEW: {
        awb_msg->u.awb_set_parm.u.init_param.op_mode =
           Q3A_OPERATION_MODE_PREVIEW;
      }
        break;
      case CAM_STREAM_TYPE_RAW:
      case CAM_STREAM_TYPE_SNAPSHOT: {
        awb_msg->u.awb_set_parm.u.init_param.op_mode =
           Q3A_OPERATION_MODE_SNAPSHOT;
      }
        break;


      default:
        awb_msg->u.awb_set_parm.u.init_param.op_mode =
           Q3A_OPERATION_MODE_PREVIEW;
        break;
      } /* switch (private->stream_type) */

     rc = q3a_aecawb_thread_en_q_msg(private->thread_data, awb_msg);
     break;
   }/* MCT_EVENT_MODULE_SET_STREAM_CONFIG*/

   case MCT_EVENT_MODULE_MODE_CHANGE: {
        //Stream mode has changed
        private->stream_type =
          ((stats_mode_change_event_data*)
          (event->u.module_event.module_event_data))->stream_type;
        private->reserved_id =
          ((stats_mode_change_event_data*)
          (event->u.module_event.module_event_data))->reserved_id;
        break;
    }
    case MCT_EVENT_MODULE_PPROC_GET_AWB_UPDATE: {
        stats_get_data_t *stats_get_data =
          (stats_get_data_t *)mod_evt->module_event_data;
        if (!stats_get_data) {
          CDBG_ERROR("%s:%d failed\n", __func__, __LINE__);
          break;
          }
        awb_port_proc_get_awb_data(port, stats_get_data);
        break;
       } /* MCT_EVENT_MODULE_PPROC_GET_AEC_UPDATE X*/
    default:
    break;
  } /* switch (mod_evt->type) */
  return rc;
}

static boolean awb_port_proc_downstream_ctrl(mct_port_t *port, mct_event_t *event)
{
  boolean rc = TRUE;

  awb_port_private_t *private = (awb_port_private_t *)(port->port_private);
  mct_event_control_t *mod_ctrl = (mct_event_control_t *)&(event->u.ctrl_event);
  switch (mod_ctrl->type) {
  case MCT_EVENT_CONTROL_SET_PARM: {
    /*TO DO: some logic shall be handled by stats and q3a port
      to acheive that, we need to add the function to find the desired sub port;
      however since it is not in place, for now, handle it here
     */
    stats_set_params_type *stat_parm = (stats_set_params_type *)mod_ctrl->control_event_data;
    if (stat_parm->param_type == STATS_SET_Q3A_PARAM) {
      q3a_set_params_type  *q3a_param = &(stat_parm->u.q3a_param);

      q3a_thread_aecawb_msg_t *awb_msg = (q3a_thread_aecawb_msg_t *)
        malloc(sizeof(q3a_thread_aecawb_msg_t));
      if (awb_msg != NULL ) {
        memset(awb_msg, 0, sizeof(q3a_thread_aecawb_msg_t));
        if (q3a_param->type == Q3A_SET_AWB_PARAM) {
          awb_msg->type = MSG_AWB_SET;
          awb_msg->u.awb_set_parm = q3a_param->u.awb_param;
          rc = q3a_aecawb_thread_en_q_msg(private->thread_data, awb_msg);
        } else if (q3a_param->type == Q3A_ALL_SET_PARAM) {
          switch (q3a_param->u.q3a_all_param.type) {
          case Q3A_ALL_SET_EZTUNE_RUNNIG: {
            awb_msg->type = MSG_AWB_SET;
            awb_msg->u.awb_set_parm.type = AWB_SET_PARAM_EZ_TUNE_RUNNING;
            awb_msg->u.awb_set_parm.u.ez_running =
              q3a_param->u.q3a_all_param.u.ez_runnig;
            rc = q3a_aecawb_thread_en_q_msg(private->thread_data, awb_msg);
          }
            break;
          default: {
          }
            break;
          }
        } else {
          free(awb_msg);
          awb_msg = NULL;
        }
      }
    } else if (stat_parm->param_type == STATS_SET_COMMON_PARAM) {
      stats_common_set_parameter_t *common_param =
        &(stat_parm->u.common_param);
      switch (common_param->type) {
      case COMMON_SET_PARAM_BESTSHOT: {
        q3a_thread_aecawb_msg_t *awb_msg = (q3a_thread_aecawb_msg_t *)
          malloc(sizeof(q3a_thread_aecawb_msg_t));
        if (awb_msg != NULL ) {
          memset(awb_msg, 0, sizeof(q3a_thread_aecawb_msg_t));
          awb_msg->type = MSG_AWB_SET;
          awb_msg->u.awb_set_parm.type = AWB_SET_PARAM_BESTSHOT;
          awb_msg->u.awb_set_parm.u.awb_best_shot =
            common_param->u.bestshot_mode;
          rc = q3a_aecawb_thread_en_q_msg(private->thread_data, awb_msg);
        }
      }
      break;
      case COMMON_SET_PARAM_VIDEO_HDR: {
        q3a_thread_aecawb_msg_t *awb_msg = (q3a_thread_aecawb_msg_t *)
          malloc(sizeof(q3a_thread_aecawb_msg_t));
        if (awb_msg != NULL ) {
          memset(awb_msg, 0, sizeof(q3a_thread_aecawb_msg_t));
          awb_msg->type = MSG_AWB_SET;
          awb_msg->u.awb_set_parm.type = AWB_SET_PARAM_VIDEO_HDR;
          awb_msg->u.awb_set_parm.u.video_hdr =
            common_param->u.video_hdr;
          rc = q3a_aecawb_thread_en_q_msg(private->thread_data, awb_msg);
        }
      }
      break;
      case COMMON_SET_PARAM_STATS_DEBUG_MASK: {
        q3a_thread_aecawb_msg_t *awb_msg = (q3a_thread_aecawb_msg_t *)
          malloc(sizeof(q3a_thread_aecawb_msg_t));
        if (awb_msg != NULL ) {
          memset(awb_msg, 0, sizeof(q3a_thread_aecawb_msg_t));
          awb_msg->type = MSG_AWB_SET;
          awb_msg->u.awb_set_parm.type = AWB_SET_PARAM_STATS_DEBUG_MASK;
          awb_msg->u.awb_set_parm.u.stats_debug_mask =
            common_param->u.stats_debug_mask;
          rc = q3a_aecawb_thread_en_q_msg(private->thread_data, awb_msg);
        }
      }
      break;
      default:
        break;
      }
    }
  }
    break;
  case MCT_EVENT_CONTROL_STREAMON: {
    mct_event_t event;
    stats_update_t stats_update;
    q3a_thread_aecawb_msg_t *awb_msg   = (q3a_thread_aecawb_msg_t *)
    malloc(sizeof(q3a_thread_aecawb_msg_t));
    if (awb_msg != NULL ) {
      memset(awb_msg, 0 , sizeof(q3a_thread_aecawb_msg_t));
      memset(&stats_update, 0, sizeof(stats_update_t));
      awb_msg->sync_flag = TRUE;
      awb_msg->type = MSG_AWB_GET;
      awb_msg->u.awb_get_parm.type = AWB_GAINS;
      rc = q3a_aecawb_thread_en_q_msg(private->thread_data, awb_msg);
      if (awb_msg) {
        event.direction = MCT_EVENT_UPSTREAM;
        event.identity = private->reserved_id;
        event.type = MCT_EVENT_MODULE_EVENT;
        event.u.module_event.type = MCT_EVENT_MODULE_STATS_AWB_UPDATE;
        event.u.module_event.module_event_data = (void *)(&stats_update);;

        stats_update.flag = STATS_UPDATE_AWB;
        stats_update.awb_update.gain.r_gain =
          awb_msg->u.awb_get_parm.u.awb_gains.curr_gains.r_gain;
        stats_update.awb_update.gain.g_gain =
          awb_msg->u.awb_get_parm.u.awb_gains.curr_gains.g_gain;
        stats_update.awb_update.gain.b_gain =
         awb_msg->u.awb_get_parm.u.awb_gains.curr_gains.b_gain;
        stats_update.awb_update.color_temp =
          awb_msg->u.awb_get_parm.u.awb_gains.color_temp;
		
		/* refine AWB gains, added by tanrifei, 20131108*/
		awb_port_refine_gains(port, &(stats_update.awb_update.gain));
		/* add end */

        CDBG("%s: send AWB_UPDATE to port =%p, event =%p",
              __func__,port, &event);
        MCT_PORT_EVENT_FUNC(port)(port, &event);
        free(awb_msg);
        awb_msg = NULL;
      }
    } /* if (awb_msg != NULL ) */
  }
    break;
    case MCT_EVENT_CONTROL_START_ZSL_SNAPSHOT: {
    q3a_thread_aecawb_msg_t *awb_msg = (q3a_thread_aecawb_msg_t *)
      malloc(sizeof(q3a_thread_aecawb_msg_t));
    if (awb_msg == NULL )
      break;
    memset(awb_msg, 0, sizeof(q3a_thread_aecawb_msg_t));
    awb_msg->type = MSG_AWB_SET;
    awb_msg->u.awb_set_parm.type = AWB_SET_PARAM_OP_MODE;
    awb_msg->u.awb_set_parm.u.init_param.op_mode =
      Q3A_OPERATION_MODE_SNAPSHOT;
    rc = q3a_aecawb_thread_en_q_msg(private->thread_data, awb_msg);
    } /* MCT_EVENT_CONTROL_START_ZSL_SNAPSHOT */
    break;
    case MCT_EVENT_CONTROL_STOP_ZSL_SNAPSHOT:{
      q3a_thread_aecawb_msg_t *awb_msg = (q3a_thread_aecawb_msg_t *)
        malloc(sizeof(q3a_thread_aecawb_msg_t));
      if (awb_msg == NULL )
        break;
      memset(awb_msg, 0, sizeof(q3a_thread_aecawb_msg_t));
      awb_msg->type = MSG_AWB_SET;
      awb_msg->u.awb_set_parm.type = AWB_SET_PARAM_OP_MODE;
      awb_msg->u.awb_set_parm.u.init_param.op_mode =
        Q3A_OPERATION_MODE_PREVIEW;
      rc = q3a_aecawb_thread_en_q_msg(private->thread_data, awb_msg);
      } /* MCT_EVENT_CONTROL_STOP_ZSL_SNAPSHOT */
      break;

  default:
    break;
  }
  return rc;
}


/** awb_port_process_upstream_mod_event
 *    @port:
 *    @event:
 **/
static boolean awb_port_process_upstream_mod_event(mct_port_t *port,
                                                    mct_event_t *event)
{
  boolean rc = FALSE;
  q3a_thread_aecawb_msg_t * awb_msg = NULL;
  mct_event_module_t *mod_evt = &(event->u.module_event);
  awb_port_private_t * private = (awb_port_private_t *)(port->port_private);
  mct_port_t *peer;
  switch (mod_evt->type) {
  case MCT_EVENT_MODULE_STATS_AWB_UPDATE:
  case MCT_EVENT_MODULE_STATS_POST_TO_BUS:
    peer = MCT_PORT_PEER(port);
    rc = MCT_PORT_EVENT_FUNC(peer)(peer, event);
    break;

  default: /*shall not get here*/
    break;
  }
  return rc;
}

/** awb_port_event
 *    @port:
 *    @event:
 *
 * awb sink module's event processing function. Received events could be:
 * AEC/AWB/AF Bayer stats;
 * Gyro sensor stat;
 * Information request event from other module(s);
 * Informatin update event from other module(s);
 * It ONLY takes MCT_EVENT_DOWNSTREAM event.
 *
 * Return TRUE if the event is processed successfully.
 **/
static boolean awb_port_event(mct_port_t *port, mct_event_t *event)
{
  boolean rc = FALSE;
  awb_port_private_t *private;

  CDBG("%s: port =%p, evt_type: %d direction: %d", __func__, port, event->type,
    MCT_EVENT_DIRECTION(event));
  /* sanity check */
  if (!port || !event){
    CDBG_ERROR("%s: port or event NULL", __func__);
    return FALSE;
  }

  private = (awb_port_private_t *)(port->port_private);
  if (!private){
    ALOGE("%s: AWB private pointer NULL", __func__);
    return FALSE;
  }

  /* sanity check: ensure event is meant for port with same identity*/
  if ((private->reserved_id & 0xFFFF0000) != (event->identity & 0xFFFF0000)){
    ALOGE("%s: AWB identity didn't match!", __func__);
    return FALSE;
  }

  switch (MCT_EVENT_DIRECTION(event)) {
  case MCT_EVENT_DOWNSTREAM: {
    switch (event->type) {
    case MCT_EVENT_MODULE_EVENT: {
      rc = awb_process_downstream_mod_event( port, event);
    } /* case MCT_EVENT_MODULE_EVENT */
      break;

    case MCT_EVENT_CONTROL_CMD: {
      rc = awb_port_proc_downstream_ctrl(port,event);
    }
      break;

    default:
      break;
    }
  } /* case MCT_EVENT_DOWNSTREAM */
    break;


  case MCT_EVENT_UPSTREAM: {

    switch (event->type) {
    case MCT_EVENT_MODULE_EVENT: {
      rc = awb_port_process_upstream_mod_event(port, event);
    } /*case MCT_EVENT_MODULE_EVENT*/
    break;

    default:
      break;
    }
  } /* MCT_EVENT_UPSTREAM */
  break ;

  default:
    rc = FALSE;
    break;
  }

  CDBG("%s: X rc:%d", __func__, rc);
  return rc;
}

/** awb_port_ext_link
 *    @identity: session id + stream id
 *    @port:  awb module's sink port
 *    @peer:  q3a module's sink port
 **/
static boolean awb_port_ext_link(unsigned int identity,
  mct_port_t *port, mct_port_t *peer)
{
  boolean rc = FALSE;
  awb_port_private_t  *private;
  CDBG("%s:%d", __func__, __LINE__);

  /* awb sink port's external peer is always q3a module's sink port */
  if (!port || !peer ||
      strcmp(MCT_OBJECT_NAME(port), "awb_sink") ||
      strcmp(MCT_OBJECT_NAME(peer), "q3a_sink")) {
    CDBG_ERROR("%s: Invalid Port/Peer!", __func__);
    return FALSE;
  }

  private = (awb_port_private_t *)port->port_private;
  if (!private) {
    CDBG_ERROR("%s: Private port NULL!", __func__);
    return FALSE;
  }

  MCT_OBJECT_LOCK(port);
  switch (private->state) {
  case AWB_PORT_STATE_RESERVED:
  case AWB_PORT_STATE_UNLINKED:
  case AWB_PORT_STATE_LINKED:
    if ( (private->reserved_id & 0xFFFF0000) != (identity & 0xFFFF0000)) {
      break;
    }
  case AWB_PORT_STATE_CREATED:
    rc = TRUE;
    break;

  default:
    break;
  }

  if (rc == TRUE) {
    private->state = AWB_PORT_STATE_LINKED;
    MCT_PORT_PEER(port) = peer;
    MCT_OBJECT_REFCOUNT(port) += 1;
  }
  MCT_OBJECT_UNLOCK(port);

  return rc;
}

/** awb_port_ext_unlink
 *
 **/
static void awb_port_ext_unlink(unsigned int identity,
  mct_port_t *port, mct_port_t *peer)
{
  awb_port_private_t *private;

  if (!port || !peer || MCT_PORT_PEER(port) != peer)
    return;

  private = (awb_port_private_t *)port->port_private;
  if (!private)
    return;

  MCT_OBJECT_LOCK(port);
  if ((private->state == AWB_PORT_STATE_LINKED) &&
      (private->reserved_id & 0xFFFF0000) == (identity & 0xFFFF0000)) {

    MCT_OBJECT_REFCOUNT(port) -= 1;
    if (!MCT_OBJECT_REFCOUNT(port))
      private->state = AWB_PORT_STATE_UNLINKED;
  }
  MCT_OBJECT_UNLOCK(port);

  return;
}

/** awb_port_set_caps
 *
 **/
static boolean awb_port_set_caps(mct_port_t *port,
  mct_port_caps_t *caps)
{
  if (strcmp(MCT_PORT_NAME(port), "awb_sink"))
    return FALSE;

  port->caps = *caps;
  return TRUE;
}

/** awb_port_check_caps_reserve
 *
 *
 *  AWB sink port can ONLY be re-used by ONE session. If this port
 *  has been in use, AWB module has to add an extra port to support
 *  any new session(via module_awb_request_new_port).
 **/
static boolean awb_port_check_caps_reserve(mct_port_t *port, void *caps,
  void *stream)
{
  boolean            rc = FALSE;
  mct_port_caps_t    *port_caps;
  awb_port_private_t *private;
  mct_stream_info_t *stream_info = (mct_stream_info_t *)stream;

  CDBG("%s:\n", __func__);
  MCT_OBJECT_LOCK(port);
  if (!port || !caps || !stream_info ||
      strcmp(MCT_OBJECT_NAME(port), "awb_sink")) {
    rc = FALSE;
    goto reserve_done;
  }

  port_caps = (mct_port_caps_t *)caps;
  if (port_caps->port_caps_type != MCT_PORT_CAPS_STATS) {
    rc = FALSE;
    goto reserve_done;
  }

  private = (awb_port_private_t *)port->port_private;
  switch (private->state) {
  case AWB_PORT_STATE_LINKED:
    if ((private->reserved_id & 0xFFFF0000) ==
        (stream_info->identity & 0xFFFF0000))
      rc = TRUE;
    break;

  case AWB_PORT_STATE_CREATED:
  case AWB_PORT_STATE_UNRESERVED: {

    private->reserved_id = stream_info->identity;
    private->stream_type = stream_info->stream_type;
    private->state       = AWB_PORT_STATE_RESERVED;
    rc = TRUE;
  }
    break;

  case AWB_PORT_STATE_RESERVED:
    if ((private->reserved_id & 0xFFFF0000) ==
        (stream_info->identity & 0xFFFF0000))
      rc = TRUE;
    break;

  default:
    rc = FALSE;
    break;
  }

reserve_done:
  MCT_OBJECT_UNLOCK(port);
  return rc;
}

/** awb_port_check_caps_unreserve:
 *
 *
 *
 **/
static boolean awb_port_check_caps_unreserve(mct_port_t *port,
  unsigned int identity)
{
  awb_port_private_t *private;

  if (!port || strcmp(MCT_OBJECT_NAME(port), "awb_sink"))
    return FALSE;

  private = (awb_port_private_t *)port->port_private;
  if (!private)
    return FALSE;

  MCT_OBJECT_LOCK(port);
  if ((private->state == AWB_PORT_STATE_UNLINKED   ||
       private->state == AWB_PORT_STATE_LINKED ||
       private->state == AWB_PORT_STATE_RESERVED) &&
      ((private->reserved_id & 0xFFFF0000) == (identity & 0xFFFF0000))) {

    if (!MCT_OBJECT_REFCOUNT(port)) {
      private->state       = AWB_PORT_STATE_UNRESERVED;
      private->reserved_id = (private->reserved_id & 0xFFFF0000);
    }
  }
  MCT_OBJECT_UNLOCK(port);

  return TRUE;
}

/** awb_port_find_identity
 *
 **/
boolean awb_port_find_identity(mct_port_t *port, unsigned int identity)
{
  awb_port_private_t *private;

  if ( !port || strcmp(MCT_OBJECT_NAME(port), "awb_sink"))
    return FALSE;

  private = port->port_private;

  if (private) {
    return ((private->reserved_id & 0xFFFF0000) == (identity & 0xFFFF0000) ?
            TRUE : FALSE);
  }

  return FALSE;
}

/** awb_port_deinit
 *    @port:
 **/
void awb_port_deinit(mct_port_t *port)
{
  awb_port_private_t *private;

  if (!port || strcmp(MCT_OBJECT_NAME(port), "awb_sink"))
      return;

  private = port->port_private;
  if (private) {
  AWB_DESTROY_LOCK((&private->awb_object));
  awb_destroy(private->awb_object.awb);
    free(private);
    private = NULL;
  }
}

/** awb_port_init:
 *    @port: awb's sink port to be initialized
 *
 *  awb port initialization entry point. Becase AWB module/port is
 *  pure software object, defer awb_port_init when session starts.
 **/
boolean awb_port_init(mct_port_t *port, unsigned int *session_id)
{
  boolean            rc = TRUE;
  mct_port_caps_t    caps;
  unsigned int       *session;
  mct_list_t         *list;
  awb_port_private_t *private;

  if (!port || strcmp(MCT_OBJECT_NAME(port), "awb_sink"))
    return FALSE;

  private = (void *)malloc(sizeof(awb_port_private_t));
  if (!private)
    return FALSE;
  memset(private, 0 , sizeof(awb_port_private_t));

  /* initialize AWB object */
  AWB_INITIALIZE_LOCK(&private->awb_object);
  private->awb_object.awb = awb_init(&(private->awb_object.awb_ops));

  if (private->awb_object.awb == NULL) {
    free(private);
    return FALSE;
  }

  private->reserved_id = *session_id;
  private->state       = AWB_PORT_STATE_CREATED;

  port->port_private   = private;
  port->direction      = MCT_PORT_SINK;
  caps.port_caps_type  = MCT_PORT_CAPS_STATS;
  caps.u.stats.flag    = (MCT_PORT_CAP_STATS_Q3A | MCT_PORT_CAP_STATS_CS_RS);

  /* this is sink port of awb module */
  mct_port_set_event_func(port, awb_port_event);
  mct_port_set_ext_link_func(port, awb_port_ext_link);
  mct_port_set_unlink_func(port, awb_port_ext_unlink);
  mct_port_set_set_caps_func(port, awb_port_set_caps);
  mct_port_set_check_caps_reserve_func(port, awb_port_check_caps_reserve);
  mct_port_set_check_caps_unreserve_func(port, awb_port_check_caps_unreserve);

  if (port->set_caps)
    port->set_caps(port, &caps);

  return TRUE;
}
