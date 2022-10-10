#include <gst/gst.h>
#include <glib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <cuda_runtime_api.h>
#include <unistd.h>

#include "gstnvdsmeta.h"
#include "gst-nvmessage.h"

// NVDS Meta accessing
/*
static GstPadProbeReturn
tiler_src_pad_buffer_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer u_data)
{
    GstBuffer *buf = (GstBuffer *) info->data;
    guint num_rects = 0;
    NvDsObjectMeta *obj_meta = NULL;
    guint vehicle_count = 0;
    guint person_count = 0;
    NvDsMetaList * l_frame = NULL;
    NvDsMetaList * l_obj = NULL;

    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta (buf);

    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
      l_frame = l_frame->next) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *) (l_frame->data);
        for (l_obj = frame_meta->obj_meta_list; l_obj != NULL;
                l_obj = l_obj->next) {
            obj_meta = (NvDsObjectMeta *) (l_obj->data);
            if (obj_meta->class_id == PGIE_CLASS_ID_VEHICLE) {
                vehicle_count++;
                num_rects++;
            }
            if (obj_meta->class_id == PGIE_CLASS_ID_PERSON) {
                person_count++;
                num_rects++;
            }
        }
          g_print ("%s: Frame Number = %d Number of objects = %d "
            "Vehicle Count = %d Person Count = %d\n",
            (gchar*) u_data,
            frame_meta->frame_num, num_rects, vehicle_count, person_count);
    }
    return GST_PAD_PROBE_OK;
}
*/

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


    #define IM_W  "1280"
    #define IM_H  "720"
    #define BATCH "4"
    #define RTSP_LOCATION "rtsp://192.168.21.49:8554/video"
    #define RTSP_PROTOCOL "udp"
    #define MQTT_CONN_STR "127.0.0.1;1883"

    #define CAM_0 "/dev/video2"
    #define CAM_1 "/dev/video4"
    #define CAM_2 "/dev/video6"
    #define CAM_3 "/dev/video8"

    // Do not use single quotes in C pipelines!
    const gchar *desc_templ = \
        " v4l2src device="CAM_0"                                                                "
        "     ! image/jpeg, width="IM_W",height="IM_H"                                          "
        "     ! nvv4l2decoder ! nvvideoconvert ! video/x-raw(memory:NVMM),format=NV12           "
        "     ! m.sink_0                                                                        "
        " v4l2src device="CAM_1"                                                                "
        "     ! image/jpeg, width="IM_W",height="IM_H"                                          "
        "     ! nvv4l2decoder ! nvvideoconvert ! video/x-raw(memory:NVMM),format=NV12           "
        "     ! m.sink_1                                                                        "
        " v4l2src device="CAM_2"                                                                "
        "     ! image/jpeg, width="IM_W",height="IM_H"                                          "
        "     ! nvv4l2decoder ! nvvideoconvert ! video/x-raw(memory:NVMM),format=NV12           "
        "     ! m.sink_2                                                                        "
        " v4l2src device="CAM_3"                                                                "
        "     ! image/jpeg, width="IM_W",height="IM_H"                                          "
        "     ! nvv4l2decoder ! nvvideoconvert ! video/x-raw(memory:NVMM),format=NV12           "
        "     ! m.sink_3                                                                        "
        "   nvstreammux name=m batch-size="BATCH" width=640 height=480 nvbuf-memory-type=0      "
        "       sync-inputs=true batched-push-timeout=50000                                     "
        " ! nvvideoconvert flip-method=clockwise                                                "
        " ! nvinfer  config-file-path=/nvds/assets/coco_config_infer_primary.txt  interval=1    "
        " ! nvtracker  display-tracking-id=1  compute-hw=0                                      "
        "     ll-lib-file=/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so "
        " ! nvmultistreamtiler width=1920 height=1080                                             "
        " ! nvdsosd                                                                             "
        " ! tee name=teee                                                                       "
    //    "     teee.                                                                             "
    //    "         ! valve name=valve-stream drop=False                                          "
    //    "         ! queue max-size-time=0 max-size-bytes=0 max-size-buffers=0                   "
    //    "         ! nvvideoconvert                                                              "
    //    "         ! video/x-raw,format=I420                                                   "
    //    "         ! x264enc speed-preset=veryfast tune=zerolatency bitrate=20000                "
    //    "         ! rtspclientsink                                                              "
    //    "             payloader=pay0                                                            "
    //    "             location="RTSP_LOCATION"                                                   "
    //    "             protocols="RTSP_PROTOCOL"                                                  "
    //    "             latency=0                                                                 "
    //    "             sync=false                                                                "
    //    "         rtph264pay name=pay0 pt=127                                                   "
        "     teee.                                                                             "
        "         ! queue leaky=2                                                               "
        "         ! valve name=valve-bboxes drop=False                                          "
        "         ! nvmsgconv msg2p-newapi=1                                                    "
        "             config=/nvds/assets/msgconv_config.txt                                    "
        "             payload-type=1                                                            "
        "             frame-interval=1                                                          "
        "         ! nvmsgbroker conn-str="MQTT_CONN_STR"                                        "
        "             proto-lib=/nvds/lib/libnvds_mqtt_proto.so                                 "
        "             topic=hulkbuster                                                          "
        "     teee.                                                                             "
        "         ! queue leaky=2                                                               "
        "         ! valve name=valve-display drop=False                                         "
        "         ! nveglglessink async=0 sync=0                                                ";
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


    // Probe for nvdsmeta
    /*
    gchar *element_name = g_strdup("nvinfer0");
    pgie = gst_bin_get_by_name (GST_BIN (pipeline), element_name);
    tiler_src_pad = gst_element_get_static_pad (pgie, "src");
    if (!tiler_src_pad)
        g_print ("Unable to get src pad\n");
    else
        gst_pad_add_probe (tiler_src_pad, GST_PAD_PROBE_TYPE_BUFFER,
            tiler_src_pad_buffer_probe, element_name, NULL);
    gst_object_unref (tiler_src_pad);
    */

    // Set the pipeline to "playing" state
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    // Wait till pipeline encounters an error or EOS
    g_print ("Running...\n");
    g_main_loop_run (loop);

    // Out of the main loop, clean up nicely
    g_print ("Returned, stopping playback\n");
    // gst_object_unref(element_name);
    gst_element_set_state (pipeline, GST_STATE_NULL);
    g_print ("Deleting pipeline. Allow 2 seconds to shut down...\n");
    sleep(2);
    gst_object_unref (GST_OBJECT (pipeline));
    g_source_remove (bus_watch_id);
    g_main_loop_unref (loop);
    return 0;
}
