/*
* Hisi Video Sink
*/

#include "config.h"

#include <gst/video/video.h>
#include <hi_unf_common.h>


/* Object header */
#include "gstvideosink.h"

#include <string.h>
#include <stdlib.h>
#include "gst_hisi_vo_context.h"

/* Debugging category */
GST_DEBUG_CATEGORY_STATIC (hisivideosink_debug);
#define GST_CAT_DEFAULT hisivideosink_debug

/* Default template */
static GstStaticPadTemplate gst_hisivideosink_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
    );

#define gst_hisivideosink_parent_class parent_class

#define DEFAULT_ALIGN_SIZE    (4096)
#define DEF_MAX_OUT_BUF_CNT   (10) 
#define DEF_MIN_OUT_BUF_CNT   (10)
#define DEFAULT_FRAME_WIDTH	   1920  
#define DEFAULT_FRAME_HEIGHT   1080 
#define FRAME_SIZE(w, h)       (((w) * (h) * 3) / 2)
#define ALIGN_UP(val, align)   (((val) + ((align)-1)) & ~((align)-1))
#define DEFAULT_WINDOW_FREEZE       0
#define DEFAULT_STOP_KEEP_FRAME     0
#define DEFAULT_CURRENT_TIMESTAMP   0

GType
gst_meta_hisi_memory_api_get_type (void)
{
  static volatile GType type;
  static const gchar *tags[] = { "memory", NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstMetaHisiMemoryAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

/* our metadata */
const GstMetaInfo *
gst_meta_hisi_memory_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter (&meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (gst_meta_hisi_memory_api_get_type (),
        "GstMetaHisiMemory", sizeof (GstMetaHisiMemoryInfo),
        (GstMetaInitFunction) NULL, (GstMetaFreeFunction) NULL,
        (GstMetaTransformFunction) NULL);
    g_once_init_leave (&meta_info, meta);
  }
  return meta_info;
}

G_DEFINE_TYPE (GstHisiBufferPool, gst_hisi_buffer_pool, GST_TYPE_BUFFER_POOL);

static gboolean
gst_hisi_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstHisiBufferPool *hisi_pool = GST_HISI_BUFFER_POOL_CAST (pool);
  GstCaps *caps;

  if (!hisi_pool->videosink->setup) {
    GST_WARNING_OBJECT (pool, "Hisi Video Sink hasn't been initialized yet.");
    return FALSE;
  }

  if (!gst_buffer_pool_config_get_params (config, &caps, NULL, NULL, NULL)) {
    GST_WARNING_OBJECT (pool, "invalid config");
    return FALSE;
  }

  hisi_pool->caps = gst_caps_ref (caps);
  
  return GST_BUFFER_POOL_CLASS (gst_hisi_buffer_pool_parent_class)->set_config
      (pool, config);
}


static void free_contigous_buffer(GstHisiMMZBufInfo *puser_buf)
{
	HI_MMZ_BUF_S buffer;
    
	if (NULL == puser_buf)
	{
		return;
	}
	
    buffer.phyaddr = puser_buf->phyaddr;
    buffer.user_viraddr = puser_buf->bufferaddr;
    HI_MMZ_Free(&buffer);
}

static void
gst_hisi_buffer_pool_free_buffer (GstBufferPool * bpool, GstBuffer * surface)
{
  GstMetaHisiMemoryInfo *meta;

  meta = GST_META_HISI_MEMORY_GET (surface);

  if (meta->mmz) {
	GstHisiMMZBufInfo *mmz = meta->mmz;
	free_contigous_buffer(mmz);
	free(mmz);
	meta->mmz = 0;
  }

  if (meta->videosink)
    /* Release the ref to our sink */
    gst_object_unref (meta->videosink);

  GST_BUFFER_POOL_CLASS (gst_hisi_buffer_pool_parent_class)->free_buffer (bpool,
      surface);
  
}

static gint32 alloc_contigous_buffer(guint32 buf_size, 
									guint32 align, 
									GstHisiMMZBufInfo *pvdec_buf)
{
	gint16 ret = -1;
	HI_MMZ_BUF_S buffer;
    GstHisiMMZBufInfo *puser_buf = pvdec_buf;
	guint32 ss = buf_size;

	if (0 == puser_buf)
	{
		return -1;
	}

	buf_size = (buf_size + align - 1) & ~(align - 1);
	buf_size += 0x40;
	buffer.bufsize = buf_size;
	strncpy(buffer.bufname, "VDEC_OUT_SINK", sizeof(buffer.bufname));
	ret = HI_MMZ_Malloc(&buffer);
	if(ret < 0)
	{
		return -1;
	}

	puser_buf->bufferaddr       = buffer.user_viraddr;
	puser_buf->phyaddr          = buffer.phyaddr;
	puser_buf->buffer_len	= buf_size;
	puser_buf->data_len	       = 0;
	puser_buf->data_offset	= 0;

	return 0;
}

static GstFlowReturn
gst_hisi_buffer_pool_alloc_buffer (GstBufferPool * bpool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params)
{
  GstHisiBufferPool *hisi_pool = GST_HISI_BUFFER_POOL_CAST (bpool);
  GstBuffer *surface;
  GstFlowReturn result = GST_FLOW_ERROR;
  GstMetaHisiMemoryInfo *meta;
  GstStructure *structure;
  gsize alloc_size;
  GstHisiVideoSink *hisi_sink;
  gsize max_size;
  const gchar *str;
  GstVideoFormat format;
  
  surface = gst_buffer_new ();
  meta = GST_META_HISI_MEMORY_ADD (surface);

  /* Keep a ref to our sink */
  meta->videosink = gst_object_ref (hisi_pool->videosink);
  hisi_sink = GST_HISIVIDEOSINK(hisi_pool->videosink);

  /* Surface is not locked yet */
  meta->locked = FALSE;
  meta->mmz = malloc((sizeof(GstHisiMMZBufInfo)));
  
  g_return_val_if_fail((surface != NULL), result);
  g_return_val_if_fail((meta->mmz != NULL), result);
  

  structure = gst_buffer_pool_get_config (hisi_pool);
  gst_buffer_pool_config_get_params (structure, NULL, &alloc_size, NULL, NULL);

  if (alloc_size <= 0){
	alloc_size = FRAME_SIZE((ALIGN_UP(hisi_sink->video_width, 16)), hisi_sink->video_height);
  }

  if ( 0 > alloc_contigous_buffer(alloc_size, DEFAULT_ALIGN_SIZE, meta->mmz)) {
	 free(meta->mmz);
	 meta->mmz = 0;
	 goto beach;
  }

  structure = gst_caps_get_structure (hisi_pool->caps, 0);
  gst_buffer_append_memory (surface,
	  gst_memory_new_wrapped (0, meta->mmz->bufferaddr, alloc_size, 0, alloc_size,
		  NULL, NULL));
  gst_mini_object_set_qdata(GST_MINI_OBJECT_CAST(surface), 
		g_quark_from_string("omx"),
		meta->mmz->bufferaddr, 0);
  /*gst_buffer_add_video_meta (surface, GST_VIDEO_FRAME_FLAG_NONE,
      format, meta->width, meta->height);
  */
  result = GST_FLOW_OK;
  
beach:
  if (result != GST_FLOW_OK) {
    gst_hisi_buffer_pool_free_buffer (bpool, surface);
    *buffer = NULL;
  } else
    *buffer = surface;

  return result;
}

static GstBufferPool *
gst_hisi_buffer_pool_new (GstHisiVideoSink * videosink)
{
  GstHisiBufferPool *pool;

  g_return_val_if_fail (GST_IS_HISIVIDEOSINK (videosink), NULL);

  pool = g_object_new (GST_TYPE_HISI_BUFFER_POOL, NULL);
  pool->videosink = gst_object_ref (videosink);

  GST_LOG_OBJECT (pool, "new hisi buffer pool %p", pool);
  
  return GST_BUFFER_POOL_CAST (pool);
}

static void
gst_hisi_buffer_pool_finalize (GObject * object)
{
  GstHisiBufferPool *pool = GST_HISI_BUFFER_POOL_CAST (object);

  if (pool->caps)
    gst_caps_unref (pool->caps);
  gst_object_unref (pool->videosink);

  G_OBJECT_CLASS (gst_hisi_buffer_pool_parent_class)->finalize (object);
}

static void
gst_hisi_buffer_pool_init (GstHisiBufferPool * pool)
{
  /* No processing */
}

/* prototypes */
enum
{
    PROP_0,
    PROP_WINDOW_RECT,
    PROP_WINDOW_FREEZE,
    PROP_STOP_KEEP_FRAME,
    PROP_CURRENT_TIMESTAMP,
};

static void gst_hivosink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_hivosink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

static void
gst_hisi_buffer_pool_class_init (GstHisiBufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  gobject_class->finalize = gst_hisi_buffer_pool_finalize;

  gstbufferpool_class->alloc_buffer = gst_hisi_buffer_pool_alloc_buffer;
  gstbufferpool_class->set_config = gst_hisi_buffer_pool_set_config;
  gstbufferpool_class->free_buffer = gst_hisi_buffer_pool_free_buffer;
}

G_DEFINE_TYPE (GstHisiVideoSink, gst_hisivideosink, GST_TYPE_VIDEO_SINK);

static gboolean
gst_hisivideosink_setup (GstHisiVideoSink * videosink)
{
  g_return_val_if_fail (GST_IS_HISIVIDEOSINK (videosink), FALSE);

  videosink->video_width = 0;
  videosink->video_height = 0;
  videosink->out_width = 0;
  videosink->out_height = 0;
  videosink->fps_d = 0;
  videosink->fps_n = 0;
  videosink->setup = TRUE;

  return videosink->setup;
}

static void
gst_hisivideosink_cleanup (GstHisiVideoSink * videosink)
{
  g_return_if_fail (GST_IS_HISIVIDEOSINK (videosink));

  GST_DEBUG_OBJECT (videosink, "cleaning up DirectFB environment");
  
  if (videosink->pool) 
  {
    gst_object_unref (videosink->pool);
    videosink->pool = NULL;
  }

  (*((HisiVideoOutputContext*)(videosink->vo_context))->close)();
  videosink->setup = FALSE;
  
}

static GstCaps *
gst_hisivideosink_getcaps (GstBaseSink * bsink, GstCaps * filter)
{
  GstHisiVideoSink *videosink;
  GstCaps *caps = NULL;
  GstCaps *returned_caps;
  gint i;

  videosink = GST_HISIVIDEOSINK (bsink);
 
  if (!videosink->setup) 
  {
    caps = gst_caps_copy (gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD
            (videosink)));
    GST_DEBUG_OBJECT (videosink, "getcaps called and we are not setup yet, "
        "returning template %" GST_PTR_FORMAT, caps);
    goto beach;
  } 
  else 
  {
    GST_DEBUG_OBJECT (videosink, "getcaps called, checking our internal "
        "format");

	caps = gst_caps_copy (gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD
            (videosink)));
    
  }

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    GstStructure *structure = gst_caps_get_structure (caps, i);

    gst_structure_set (structure,
        "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
        "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
        "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

   /* {
      int nom = 0, den = 0;
      //nom = gst_value_get_fraction_numerator (videosink->par);
      //den = gst_value_get_fraction_denominator (videosink->par);
      gst_structure_set (structure, "pixel-aspect-ratio",
          GST_TYPE_FRACTION, nom, den, NULL);
    }*/
  }

beach:
  if (filter) {
    returned_caps = gst_caps_intersect_full (filter, caps,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
  } else
    returned_caps = caps;

  GST_DEBUG_OBJECT (videosink, "returning our caps %" GST_PTR_FORMAT,
      returned_caps);
 
  return returned_caps;
}

static gboolean
gst_hisivideosink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstHisiVideoSink *videosink;
  GstStructure *structure;
  gboolean res, result = FALSE;
  gint video_width, video_height;
  const GValue *framerate;

  videosink = GST_HISIVIDEOSINK (bsink);

  structure = gst_caps_get_structure (caps, 0);
  res = gst_structure_get_int (structure, "width", &video_width);
  res &= gst_structure_get_int (structure, "height", &video_height);
  framerate = gst_structure_get_value (structure, "framerate");
  res &= (framerate != NULL);

  videosink->fps_n = gst_value_get_fraction_numerator (framerate);
  videosink->fps_d = gst_value_get_fraction_denominator (framerate);
  
  GST_DEBUG_OBJECT (videosink, "setcaps called with %" GST_PTR_FORMAT, caps);
  GST_DEBUG_OBJECT (videosink, "our format is: %dx%d %s video at %d/%d fps",
      video_width, video_height,
      "xxxxx", videosink->fps_n,
      videosink->fps_d);

  {
    GST_VIDEO_SINK_WIDTH (videosink) = video_width;
    GST_VIDEO_SINK_HEIGHT (videosink) = video_height;
  }

  videosink->video_width = video_width;
  videosink->video_height = video_height;

  if (videosink->pool) {
    if (gst_buffer_pool_is_active (videosink->pool))
      gst_buffer_pool_set_active (videosink->pool, FALSE);
    gst_object_unref (videosink->pool);
  }

  videosink->pool = gst_hisi_buffer_pool_new (videosink);

  structure = gst_buffer_pool_get_config (videosink->pool);
  gst_buffer_pool_config_set_params (structure, caps, 
	  FRAME_SIZE((ALIGN_UP(video_width, 16)), video_height),
	  DEF_MIN_OUT_BUF_CNT, 
	  DEF_MIN_OUT_BUF_CNT);

  if (!gst_buffer_pool_set_config (videosink->pool, structure)) {
    GST_WARNING_OBJECT (videosink,
        "failed to set buffer pool configuration");
    goto beach;
  }
  if (!gst_buffer_pool_set_active (videosink->pool, TRUE)) {
    GST_WARNING_OBJECT (videosink, "failed to activate buffer pool");
    goto beach;
  }

  result = TRUE;
 
beach:
  return result;

/* ERRORS */
wrong_aspect:
  {
    GST_INFO_OBJECT (videosink, "pixel aspect ratio does not match");
    return FALSE;
  }
}

static GstStateChangeReturn
gst_hisivideosink_change_state (GstElement * element, GstStateChange transition)
{
  GstHisiVideoSink *videosink;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  videosink = GST_HISIVIDEOSINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!videosink->setup) 
	  {
        if (!gst_hisivideosink_setup (videosink)) 
		{
          GST_DEBUG_OBJECT (videosink, "setup failed when changing state "
              "from NULL to READY");
          GST_ELEMENT_ERROR (videosink, RESOURCE, OPEN_WRITE,
              (NULL), ("Failed initializing VO system"));

		  return GST_STATE_CHANGE_FAILURE;
        }
      }
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      /* Blank surface if we have one */
       
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      videosink->fps_d = 0;
      videosink->fps_n = 0;
      videosink->video_width = 0;
      videosink->video_height = 0;
      if (videosink->pool)
        gst_buffer_pool_set_active (videosink->pool, FALSE);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      if (videosink->setup) {
        gst_hisivideosink_cleanup (videosink);
      }
      break;
    default:
      break;
  }
  
  return ret;
}

static void
gst_hisivideosink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstHisiVideoSink *videosink;

  videosink = GST_HISIVIDEOSINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf)) {
      *end = *start + GST_BUFFER_DURATION (buf);
    } else {
      if (videosink->fps_n > 0) {
        *end =
            *start + (GST_SECOND * videosink->fps_d) / videosink->fps_n;
      }
    }
  }
}

static GstFlowReturn
gst_hisivideosink_show_frame (GstBaseSink * bsink, GstBuffer * buf)
{
  GstHisiVideoSink *videosink = GST_HISIVIDEOSINK(bsink);
  GstFlowReturn ret = GST_FLOW_OK;
  GstMetaHisiMemoryInfo *meta;
  GstHisiFrameBufInfo *frame;
  frame =
        (GstHisiFrameBufInfo*)gst_mini_object_steal_qdata(GST_MINI_OBJECT_CAST (buf), g_quark_from_string("omx.buf"));

  if (frame){
    gint width = videosink->video_width;
    gint height = videosink->video_height;
    unsigned long addr;

    HI_MPI_MMZ_GetPhyAddr(frame->bufferaddr, &addr, &(frame->buffer_len));

    if (addr > 0){
        (*((HisiVideoOutputContext*)(videosink->vo_context))->render)(width,height,addr);
    }
  } else {
    meta = GST_META_HISI_MEMORY_GET (buf);
    if (meta) {
        GstHisiMMZBufInfo *frame = (GstHisiMMZBufInfo*)meta->mmz;
        gint width = videosink->video_width;
        gint height = videosink->video_height;
        unsigned long addr = frame->phyaddr;

        if (addr > 0){
            (*((HisiVideoOutputContext*)(videosink->vo_context))->render)(width,height,addr);
        }
    }
  }
  
  videosink->current_timestamp = GST_BUFFER_TIMESTAMP(buf);

  return ret;
}
    
static gboolean
gst_hisivideosink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstHisiVideoSink *hisivideosink;
  GstBufferPool *pool;
  GstCaps *caps;
  gboolean need_pool;
  guint size;

  hisivideosink = GST_HISIVIDEOSINK (bsink);
  
  gst_query_parse_allocation (query, &caps, &need_pool);

  if ((pool = hisivideosink->pool))
    gst_object_ref (pool);

  if (pool != NULL) 
  {
    GstCaps *pcaps;
    GstStructure *config;

    /* we had a pool, check caps */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);

    GST_DEBUG_OBJECT (hisivideosink,
        "buffer pool configuration caps %" GST_PTR_FORMAT, pcaps);

	if (!gst_caps_is_equal (caps, pcaps)) {
      gst_structure_free (config);
      gst_object_unref (pool);
      GST_WARNING_OBJECT (hisivideosink, "pool has different caps");
	  printf ("\e[31mpool has different caps\e[0m\n");
      return FALSE;
    }

    gst_structure_free (config);
	
	if (size <= 0 )
	  size = FRAME_SIZE((ALIGN_UP(hisivideosink->video_width, 16)), hisivideosink->video_height);
	
	gst_query_add_allocation_pool (query, pool, size, DEF_MIN_OUT_BUF_CNT, DEF_MIN_OUT_BUF_CNT);
	
	/* we also support various metadata */
	gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
	
	if (pool) 
		gst_object_unref (pool);
	}

	return TRUE;
}

static void
gst_hisivideosink_finalize (GObject * object)
{
  GstHisiVideoSink *hisivideosink;

  hisivideosink = GST_HISIVIDEOSINK (object);

  if (hisivideosink->setup) {
    gst_hisivideosink_cleanup (hisivideosink);
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_hisivideosink_init (GstHisiVideoSink * hisivideosink)
{
  hisivideosink->pool = NULL;
  hisivideosink->video_height = hisivideosink->out_height = 0;
  hisivideosink->video_width = hisivideosink->out_width = 0;
  hisivideosink->fps_d = 0;
  hisivideosink->fps_n = 0;
  hisivideosink->setup = FALSE;
  
  hisivideosink->vo_context = hisi_vo_context_get();
  (*((HisiVideoOutputContext*)(hisivideosink->vo_context))->open)();
}

static void
gst_hisivideosink_class_init (GstHisiVideoSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;

  parent_class = g_type_class_peek_parent (klass);
  
  gobject_class->finalize = gst_hisivideosink_finalize;
  gobject_class->set_property = gst_hivosink_set_property;
  gobject_class->get_property = gst_hivosink_get_property;
  gst_element_class_set_static_metadata (gstelement_class,
      "Hisi video sink", "Sink/Video", "A Hisi based videosink",
      "xxxxx");
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_hisivideosink_sink_template_factory));
  gstelement_class->change_state = gst_hisivideosink_change_state;
  gstbasesink_class->get_caps = gst_hisivideosink_getcaps;
  gstbasesink_class->set_caps = gst_hisivideosink_setcaps;
  gstbasesink_class->get_times = gst_hisivideosink_get_times;
  gstbasesink_class->preroll = gst_hisivideosink_show_frame;
  gstbasesink_class->render = gst_hisivideosink_show_frame;
  gstbasesink_class->propose_allocation = gst_hisivideosink_propose_allocation;
  
  g_object_class_install_property (gobject_class, PROP_WINDOW_RECT,
  g_param_spec_string ("window-rect", "Window Rect",
    "The overylay window rect (x,y,width,height)",
    NULL,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_WINDOW_FREEZE,
  g_param_spec_boolean ("freeze", "Window Freeze",
    "freeze/unfreeze video",
    DEFAULT_WINDOW_FREEZE,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_STOP_KEEP_FRAME,
  g_param_spec_boolean ("stop-keep-frame", "stop-keep-frame",
    "Keep displaying the last frame when stop",
    DEFAULT_STOP_KEEP_FRAME,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CURRENT_TIMESTAMP,
  g_param_spec_uint64("current-timestamp", "current-timestamp",
    "Video decoder handle in use",
    0, G_MAXUINT64, DEFAULT_CURRENT_TIMESTAMP,
    G_PARAM_READABLE));

}


void gst_hivosink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
    GstHisiVideoSink *hivosink;
    HI_HANDLE win_handle;
    HisiVideoOutputContext *vo_ctx;
    
    g_return_if_fail (GST_IS_HISIVIDEOSINK (object));
    hivosink = GST_HISIVIDEOSINK (object);
    vo_ctx = hivosink->vo_context;
    win_handle = vo_ctx->get_window_handle();   
    
    switch (property_id)
    {
        case PROP_WINDOW_RECT:
            {
                gint ret;
                gint tempx,tempy,tempw,temph;
                HI_UNF_WINDOW_ATTR_S WinAttr;

                ret = sscanf(g_value_get_string (value), "%d,%d,%d,%d",
                    &tempx, &tempy, &tempw, &temph);
                if( ret == 4 )
                {
                    hivosink->x = tempx;
                    hivosink->y = tempy;
                    hivosink->width = tempw;
                    hivosink->height = temph;
                    hivosink->video_height = temph;
                    hivosink->video_width = tempw;
                    
                    if(win_handle)
                    {
                        HI_UNF_VO_GetWindowAttr(win_handle, &WinAttr);
                        WinAttr.stOutputRect.s32X = hivosink->x;
                        WinAttr.stOutputRect.s32Y = hivosink->y;
                        WinAttr.stOutputRect.s32Width = hivosink->width;
                        WinAttr.stOutputRect.s32Height = hivosink->height;
                        HI_UNF_VO_SetWindowAttr(win_handle, &WinAttr);
                    }
                }
            }
            break;
        case PROP_WINDOW_FREEZE:
            hivosink->freeze = g_value_get_boolean(value);
            HI_UNF_VO_FreezeWindow(win_handle, hivosink->freeze, HI_UNF_WINDOW_FREEZE_MODE_LAST);
            break;
        case PROP_STOP_KEEP_FRAME:
            hivosink->stop_keep_frame = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

void gst_hivosink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
    GstHisiVideoSink *hivosink;

    g_return_if_fail (GST_IS_HISIVIDEOSINK (object));
    hivosink = GST_HISIVIDEOSINK (object);
    
    switch (property_id)
    {
        case PROP_WINDOW_RECT:
            {
                char rect_str[64];
                sprintf(rect_str, "%d,%d,%d,%d",
                    hivosink->x, hivosink->y, hivosink->video_width, hivosink->video_height);
                g_value_set_string (value, rect_str);
            }
            break;
        case PROP_WINDOW_FREEZE:
            g_value_set_boolean(value, hivosink->freeze);
            break;
        case PROP_STOP_KEEP_FRAME:
            g_value_set_boolean(value, hivosink->stop_keep_frame);
            break;
        case PROP_CURRENT_TIMESTAMP:
            g_value_set_uint64(value, hivosink->current_timestamp);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "hisivideosink", GST_RANK_PRIMARY,
          GST_TYPE_HISIVIDEOSINK))
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (hisivideosink_debug, "hisivideosink", 0,
      "Hisi video sink element");

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    hisivideosink,
    "hisi video output plugin",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
