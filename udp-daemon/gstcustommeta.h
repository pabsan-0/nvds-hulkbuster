#ifndef __GST_MARK_H__
#define __GST_MARK_H__

#include <gst/gst.h>
#include <gst/gstmeta.h>

G_BEGIN_DECLS

typedef struct _GstMetaMarking GstMetaMarking;

struct _GstMetaMarking {
    GstMeta meta;
    char detections[200];
};

GType gst_meta_marking_api_get_type (void);
const GstMetaInfo* gst_meta_marking_get_info (void);
#define GST_META_MARKING_GET(buf) ((GstMetaMarking *)gst_buffer_get_meta(buf,gst_meta_marking_api_get_type()))
#define GST_META_MARKING_ADD(buf) ((GstMetaMarking *)gst_buffer_add_meta(buf,gst_meta_marking_get_info(),(gpointer)NULL))

G_END_DECLS

#endif /* __GST_MARK_H__ */
