/*
* Hisi Video Sink
*/
 
#ifndef __GST_HISI_VIDEO_SINK_H__
#define __GST_HISI_VIDEO_SINK_H__

#include <gst/video/gstvideosink.h>

G_BEGIN_DECLS

#define GST_TYPE_HISIVIDEOSINK              (gst_hisivideosink_get_type())
#define GST_HISIVIDEOSINK(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_HISIVIDEOSINK, GstHisiVideoSink))
#define GST_HISIVIDEOSINK_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_HISIVIDEOSINK, GstHisiVideoSinkClass))
#define GST_IS_HISIVIDEOSINK(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_HISIVIDEOSINK))
#define GST_IS_HISIVIDEOSINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_HISIVIDEOSINK))

typedef struct _GstHisiVideoSink GstHisiVideoSink;
typedef struct _GstHisiVideoSinkClass GstHisiVideoSinkClass;

#define GST_TYPE_HISI_BUFFER_POOL     (gst_hisi_buffer_pool_get_type())
#define GST_HISI_BUFFER_POOL_CAST(obj) ((GstHisiBufferPool*)(obj))

GType gst_meta_hisi_memory_api_get_type (void);
const GstMetaInfo * gst_meta_hisi_memory_get_info (void);
typedef struct _GstMetaHisiMemoryInfo GstMetaHisiMemoryInfo;
typedef struct _GstHisiMMZBufInfo GstHisiMMZBufInfo;
typedef struct _GstHisiFrameBufInfo GstHisiFrameBufInfo;

#define GST_META_HISI_MEMORY_GET(buf) ((GstMetaHisiMemoryInfo *)gst_buffer_get_meta(buf,gst_meta_hisi_memory_api_get_type()))
#define GST_META_HISI_MEMORY_ADD(buf) ((GstMetaHisiMemoryInfo *)gst_buffer_add_meta(buf,gst_meta_hisi_memory_get_info(),NULL))

struct _GstHisiFrameBufInfo {
	unsigned char *bufferaddr;
	guint32 buffer_len;
	guint32 data_offset;
	guint32 data_len;
};

struct _GstHisiMMZBufInfo {
	unsigned char *bufferaddr;
	guint32 phyaddr;
	guint32 buffer_len;
	guint32 data_offset;
	guint32 data_len;
};

struct _GstMetaHisiMemoryInfo {
  GstMeta meta;
  gboolean locked;
  GstHisiMMZBufInfo *mmz;
  GstHisiVideoSink *videosink;
};

typedef struct _GstHisiBufferPool GstHisiBufferPool;

struct _GstHisiBufferPool
{
  GstBufferPool bufferpool; /* parent class*/
  GstHisiVideoSink *videosink; /* link to instance of video sink*/
  GstCaps *caps; /* caps of current instance*/
};

typedef struct _GstHisiBufferPoolClass GstHisiBufferPoolClass;

struct _GstHisiBufferPoolClass
{
  GstBufferPoolClass parent_class;
};

struct _GstHisiVideoSink {
  GstVideoSink videosink;

  /* for buffer pool */
  GstBufferPool *pool;
  gboolean setup;
  gpointer vo_context;

  /* Framerate numerator and denominator */
  gint fps_n;
  gint fps_d;
  
  gint video_width, video_height; /* size of incoming video */
  gint out_width, out_height;
  gint x, y, width, height;
  gboolean freeze, stop_keep_frame;
  guint64 current_timestamp;
  glong frame_count;  
};

struct _GstHisiVideoSinkClass {
  GstVideoSinkClass parent_class;
};

GType gst_hisivideosink_get_type (void);
GType gst_hisi_buffer_pool_get_type (void);

G_END_DECLS

#endif /* __GST_HISI_VIDEO_SINK_H__ */
