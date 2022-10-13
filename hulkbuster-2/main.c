#include <gst/gst.h>
#include <gst/rtp/rtp.h>
#include <glib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <cuda_runtime_api.h>
#include <unistd.h>

#include "gstnvdsmeta.h"
#include "gst-nvmessage.h"


static GstPadProbeReturn
probe_nvdsmeta_read (GstPad * pad, GstPadProbeInfo * info,
    gpointer u_data)
{
    //printf("did I get called?");
    static guint ii = 0;
    GstBuffer *buf = (GstBuffer *) info->data;
    NvDsObjectMeta *obj_meta = NULL;
    NvDsMetaList * l_frame = NULL;
    NvDsMetaList * l_obj = NULL;

    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta (buf);

    printf("(--------%d--------)\n", ii++);

    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
      l_frame = l_frame->next)
    {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *) (l_frame->data);
        for (l_obj = frame_meta->obj_meta_list; l_obj != NULL;
                l_obj = l_obj->next)
        {
            obj_meta = (NvDsObjectMeta *) (l_obj->data);
            // char* obj_id = strdup("%s", obj_meta->class_id);
            printf("%d:  %d  %f  %s  %f \n",
                frame_meta->source_id,
                obj_meta->class_id,
                obj_meta->confidence,
                obj_meta->obj_label,
                obj_meta->rect_params.left
                );
        }
    }
    return GST_PAD_PROBE_OK;
}


GstPadProbeReturn
meta_inject (GstPad * pad, GstPadProbeInfo * info,
    gpointer u_data)
{
    GstBuffer *buffer = info->data;
    GstRTPBuffer rtpbuf = GST_RTP_BUFFER_INIT;
    // Map our original buffer as content for the RTP buffer
    // use READ or you will truncate the image content for reasons I do not understand
    if (gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtpbuf))
    {
        // Faking a string as klv
        char* message = "hello gstreamer";
        guint message_size = 16;
        gst_rtp_buffer_add_extension_onebyte_header (&rtpbuf, 1, message, message_size);
    }
    return GST_PAD_PROBE_OK;
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
    GstPad *tiler_src_pad = NULL;
    GstBus *bus = NULL;
    guint bus_watch_id;

    GError *error = NULL;

    /* Standard GStreamer initialization */
    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);


    #define IM_W  "800"
    #define IM_H  "600"
    #define BATCH "1"

    #define CAM_0 "/dev/video2"
    #define CAM_1 "/dev/video4"
    #define CAM_2 "/dev/video6"
    #define CAM_3 "/dev/video8"

    const gchar *desc_templ = \
        " v4l2src device="CAM_0"                                                                "
        "     ! image/jpeg, width="IM_W",height="IM_H"                                          "
        "     ! nvv4l2decoder ! nvvideoconvert ! video/x-raw(memory:NVMM),format=NV12           "
        "     ! m.sink_0                                                                        "
        "   nvstreammux name=m batch-size="BATCH" width=640 height=480 nvbuf-memory-type=0      "
        "       sync-inputs=1 batched-push-timeout=500000                                        "
        " ! nvvideoconvert flip-method=clockwise                                                "
        " ! nvinfer  config-file-path=/nvds/assets/coco_config_infer_primary.txt  interval=4    "
        " ! nvtracker  display-tracking-id=1  compute-hw=0                                      "
        "     ll-lib-file=/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so "
        " ! nvstreamdemux name=demux                                                            "
        "   demux.src_0                                                                         "
        "       ! queue                                                                         "
        "       ! nvvideoconvert                                                                "
        //"       ! nvdsosd                                                                       "
        //"       ! nvvideoconvert                                                                "
#if 0
        "       ! nveglglessink async=0 sync=0                                                  "
#else
        //"       ! video/x-raw,format=I420                                                       "
        "       ! videoconvert                                                                "
        "       ! x264enc tune=zerolatency "
        "       ! rtph264pay "
        "       ! identity name=injector                                                        "
        "       ! udpsink host=127.0.0.1 port=1234 "
#endif
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


    // Probe for nvdsmeta at inference
    //  probe here to be able to link image and its src camera
    GstElement *probed_element = NULL;
    gchar *element_name = g_strdup("nvinfer0");
    probed_element = gst_bin_get_by_name (GST_BIN (pipeline), element_name);
    tiler_src_pad = gst_element_get_static_pad (probed_element, "src");
    if (!tiler_src_pad)
        g_print ("Unable to get src pad\n");
    else
        gst_pad_add_probe (tiler_src_pad, GST_PAD_PROBE_TYPE_BUFFER,
            probe_nvdsmeta_read, element_name, NULL);
    gst_object_unref (tiler_src_pad);


    // Meta injection on RTP packets
    GstElement* id = gst_bin_get_by_name (GST_BIN (pipeline), "injector");
    GstPad* id_src = gst_element_get_static_pad (id, "src");
    gst_pad_add_probe (id_src, GST_PAD_PROBE_TYPE_BUFFER, meta_inject, NULL, NULL);
    gst_object_unref(id_src);
    gst_object_unref(id);


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
