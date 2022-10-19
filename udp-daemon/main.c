#include <gst/gst.h>
#include <gst/rtp/rtp.h>
#include <glib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <cuda_runtime_api.h>
#include <unistd.h>
#include <stdlib.h>

#include "gstnvdsmeta.h"
#include "gst-nvmessage.h"
#include "gstcustommeta.h"
#include "gstfdcontrol.h"

#define CAM_0 "/dev/video7"
#define CAM_1 "/dev/video8"
#define CAM_2 "/dev/video4"
#define CAM_3 "/dev/video2"


#define CAP_W  "640"
#define CAP_H  "480"

#define TILER_WIDTH "640"
#define TILER_HEIGHT "480"
#define BATCH "4"

#define PGIE_CFG "/nvds/assets/coco_config_infer_primary.txt"
#define TRACKER_SO "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so"

#define MQTT_CONN_STR    "127.0.0.1;5554"
#define MQTT_PROTO_SO    "/nvds/lib/libnvds_mqtt_proto.so"
#define MQTT_MSGCONV_CFG "/host/udp-single-stream/msgconv_config.txt"

#define DET_MESSAGE_SIZE (200)
#define UDP_PORT "1234"


#define V4L2_DECODE \
    " identity                                                       " \
    "   ! image/jpeg, width="CAP_W",height="CAP_H", framerate=30/1   " \
    "   ! queue                                                      " \
    "   ! nvv4l2decoder low-latency-mode=1                           " \
    "   ! nvvideoconvert ! video/x-raw(memory:NVMM),format=NV12      """

#define REMUX \
    " nvstreamdemux name=demux                                           " \
    "     demux.src_0 ! valve name=valve_cam_0 drop=false ! remux.sink_0 " \
    "     demux.src_1 ! valve name=valve_cam_1 drop=false ! remux.sink_1 " \
    "     demux.src_2 ! valve name=valve_cam_2 drop=false ! remux.sink_2 " \
    "     demux.src_3 ! valve name=valve_cam_3 drop=false ! remux.sink_3 " \
    " nvstreammux name=remux nvbuf-memory-type=0                         " \
    "       batch-size="BATCH" width=640 height=640                      " \
    "       sync-inputs=1 batched-push-timeout=500000                    """

#define TILE_PLUS_OSD \
    "   nvmultistreamtiler width="TILER_WIDTH" height="TILER_HEIGHT" " \
    " ! queue                                                        " \
    " ! nvvideoconvert                                               " \
    " ! nvdsosd                                                      " \
    " ! nvvideoconvert                                               """

#define MQTT_SINK \
    " queue leaky=2                                " \
    "   ! valve name=valve_mqtt_dets drop=False    " \
    "   ! nvmsgconv msg2p-newapi=1                 " \
    "       config="MQTT_MSGCONV_CFG"              " \
    "       payload-type=1                         " \
    "       frame-interval=1                       " \
    "   ! nvmsgbroker conn-str="MQTT_CONN_STR"     " \
    "       proto-lib="MQTT_PROTO_SO"              " \
    "       topic=hulkbuster                       """


static void
meta_to_str (GstBuffer* buf, char* str)
{
    NvDsObjectMeta *obj_meta = NULL;
    NvDsMetaList * l_frame = NULL;
    NvDsMetaList * l_obj = NULL;

    char *cursor = str;
    const char *end = str + DET_MESSAGE_SIZE;

    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta (buf);
    if (!batch_meta->num_frames_in_batch)
        return;

    for (l_frame = batch_meta->frame_meta_list;
         l_frame != NULL;
         l_frame = l_frame->next)
    {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *) (l_frame->data);
        for (l_obj = frame_meta->obj_meta_list;
             l_obj != NULL;
             l_obj = l_obj->next)
        {
            obj_meta = (NvDsObjectMeta *) (l_obj->data);

            // Compute frame id, as it is lost from meta at nvmultistreamtiler
            // Assumes a 2x2 4-stream tiler configuration
            int x = (obj_meta->rect_params.top < atof(TILER_HEIGHT) / 2) ? 0: 1;
            int y = (obj_meta->rect_params.left < atof(TILER_WIDTH) / 2) ? 0: 1;

            // Build the message via string-append
            if (cursor < end)
            {
                cursor += snprintf(cursor, end-cursor, "%d%d:%d:%s:%d:%d:%d:%d\n",
                        (int) x,
                        (int) y,
                        (int) obj_meta->class_id,
                        (char*) obj_meta->obj_label,
                        (int) obj_meta->rect_params.left,
                        (int) obj_meta->rect_params.top,
                        (int) obj_meta->rect_params.width,
                        (int) obj_meta->rect_params.height
                        );
            }
        }
    }
    return;
}


GstPadProbeReturn
meta_nvds_to_gst (GstPad * pad, GstPadProbeInfo * info, gpointer u_data)
{
    GstBuffer *buf = (GstBuffer *) info->data;

    char message[DET_MESSAGE_SIZE];
    meta_to_str (buf, message);

    GstMetaMarking* mymeta = GST_META_MARKING_ADD(buf);
    strcpy(mymeta->detections, message);
    return GST_PAD_PROBE_OK;
}


GstPadProbeReturn
meta_gst_to_rtp (GstPad * pad, GstPadProbeInfo * info, gpointer u_data)
{
    GstBuffer *buf = info->data;

    GstMetaMarking* mymeta = GST_META_MARKING_GET(buf);
    char* message = mymeta->detections;

    // use GST_MAP_READ or you will truncate the image content
    GstRTPBuffer rtpbuf = GST_RTP_BUFFER_INIT;
    if (gst_rtp_buffer_map (buf, GST_MAP_READ, &rtpbuf) &&
        gst_rtp_buffer_get_marker (&rtpbuf))
        gst_rtp_buffer_add_extension_twobytes_header (&rtpbuf, 0, 1, message, DET_MESSAGE_SIZE);

    return GST_PAD_PROBE_OK;
}



gint
place_probe (GstElement *pipeline, gchar *elementName,
    GstPadProbeCallback cb_probe, gpointer u_data)
{
    GstElement* id;
    GstPad* id_src;

    id = gst_bin_get_by_name (GST_BIN (pipeline), elementName);
    id_src = gst_element_get_static_pad (id, "src");
    gst_pad_add_probe (id_src, GST_PAD_PROBE_TYPE_BUFFER,
                cb_probe, u_data, NULL);
    gst_object_unref(id_src);
    gst_object_unref(id);
    return 0;
}


static gboolean
bus_call (GstBus * bus, GstMessage * msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop *) data;
    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS:
        {
            g_print ("End of stream\n");
            g_main_loop_quit (loop);
            break;
        }
        case GST_MESSAGE_WARNING:
        {
            gchar *debug;
            GError *error;
            gst_message_parse_warning (msg, &error, &debug);
            g_printerr ("WARNING from element %s: %s\n",
                GST_OBJECT_NAME (msg->src), error->message);
            g_free (debug);
            g_printerr ("Warning: %s\n", error->message);
            g_error_free (error);
            break;
        }
        case GST_MESSAGE_ERROR:
        {
            gchar *debug;
            GError *error;
            gst_message_parse_error (msg, &error, &debug);
            g_printerr ("ERROR from element %s: %s\n",
                GST_OBJECT_NAME (msg->src), error->message);
            if (debug)
                g_printerr ("Error details: %s\n", debug);
            g_free (debug);
            g_error_free (error);
            g_main_loop_quit (loop);
            break;
        }
        case GST_MESSAGE_ELEMENT:
        {
            if (gst_nvmessage_is_stream_eos (msg)) {
                guint stream_id;
                if (gst_nvmessage_parse_stream_eos (msg, &stream_id)) {
                    g_print ("Got EOS from stream %d\n", stream_id);
                }
            }
            break;
        }
        default:
            break;
        }
    return TRUE;
}


int
main (int argc, char **argv)
{
    GMainLoop *loop = NULL;
    GstElement *pipeline = NULL;
    GstBus *bus = NULL;
    guint bus_watch_id;
    GError *error = NULL;

    /* Standard GStreamer initialization */
    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);

    const gchar *desc_templ = \
        " fdsrc fd=0 name=control ! fakesink dump=false                  " // IF PIPE FREEZES, JUST FEED INPUT TO STDIN
        " v4l2src device="CAM_0" ! "V4L2_DECODE" ! valve name=valve0 ! mux.sink_0            "
        " v4l2src device="CAM_1" ! "V4L2_DECODE" ! mux.sink_1            "
        " v4l2src device="CAM_2" ! "V4L2_DECODE" ! mux.sink_2            "
        " v4l2src device="CAM_3" ! "V4L2_DECODE" ! mux.sink_3            "
        "                                                                "
        "   nvstreammux name=mux nvbuf-memory-type=0                     "
        "       batch-size="BATCH" width=640 height=640                  "
        "       sync-inputs=1 batched-push-timeout=500000                "
        " ! nvvideoconvert flip-method=clockwise                         "
        " ! nvinfer config-file-path="PGIE_CFG" interval=4               "
        " ! nvtracker display-tracking-id=0 compute-hw=0                 "
        "       ll-lib-file="TRACKER_SO"                                 "
        "                                                                "
        // " ! "REMUX"                                                      "
        " ! tee name=teee1                                               "
        "   teee1.                                                       "
        "       ! "MQTT_SINK"                                            "
        "   teee1.                                                       "
        "       ! "TILE_PLUS_OSD"                                        "
        "       ! tee name=teee2                                         "
        "   teee2.                                                       "
        "       ! queue                                                  "
        "       ! identity name=nvds_to_gst                              "
        "       ! queue                                                  "
        "       ! videoconvert                                           "
        "       ! video/x-raw, format=I420, width=640, height=480        "
        "       ! x265enc tune=zerolatency                               "
        "       ! rtph265pay                                             "
        "       ! identity name=gst_to_rtp                               "
        "       ! udpsink host=127.0.0.1 port="UDP_PORT"                 "
        "   teee2.                                                       "
        "       ! nveglglessink async=0 sync=0                           "
        ;;;;;;;;

    gchar *desc = g_strdup (desc_templ);
    pipeline = gst_parse_launch (desc, &error);
    if (error) {
        g_printerr ("pipeline parsing error: %s\n", error->message);
        g_error_free (error);
        return 1;
    }

    // we add a bus message handler */
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    // Meta injection on RTP packets
    place_probe(pipeline, "nvds_to_gst", meta_nvds_to_gst, NULL);
    place_probe(pipeline, "gst_to_rtp", meta_gst_to_rtp, NULL);
    place_probe(pipeline, "control", control_handler, pipeline);

    // Set the pipeline to "playing" state
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    // Wait till pipeline encounters an error or EOS
    g_print ("Running...\n");
    g_main_loop_run (loop);

    // Out of the main loop, clean up nicely
    g_print ("Returned, stopping playback\n");
    gst_element_set_state (pipeline, GST_STATE_NULL);
    g_print ("Deleting pipeline. Allow 2 seconds to shut down...\n");
    gst_object_unref (GST_OBJECT (pipeline));
    g_source_remove (bus_watch_id);
    g_main_loop_unref (loop);
    return 0;
}
