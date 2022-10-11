#include <gst/gst.h>
#include <gst/gstmeta.h>
#include <gst/gstbuffer.h>
#include <gst/rtp/rtp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

static GMainLoop *loop;



GstPadProbeReturn
meta_retrieve (GstPad * pad, GstPadProbeInfo * info, gpointer u_data)
{
    GstBuffer *buffer = info->data;
    GstRTPBuffer rtpbuf = GST_RTP_BUFFER_INIT;
    gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtpbuf);

    // This would pop every single rtp packet
    printf("This guy's got somethin'?  ");
    if (gst_rtp_buffer_get_marker (&rtpbuf)){
        printf("This guy's got something!\n");

        guint16 * bits = NULL;
        gpointer message = NULL;
        guint* wordlen = NULL;

        gst_rtp_buffer_get_extension_data (&rtpbuf, bits, &message, wordlen);
        printf("%s\n\n", (char*) message);
    }

    return GST_PAD_PROBE_OK;
}


gint
place_probe (GstElement *pipeline, gchar *elementName,
    GstPadProbeCallback cb_probe)
{
    GstElement* id;
    GstPad* id_src;

    id = gst_bin_get_by_name (GST_BIN (pipeline), elementName);
    id_src = gst_element_get_static_pad (id, "src");
    gst_pad_add_probe (id_src, GST_PAD_PROBE_TYPE_BUFFER,
                cb_probe, elementName, NULL);
    gst_object_unref(id_src);
    gst_object_unref(id);
    return 0;
}


gint
main (gint argc, gchar *argv[])
{
    GstElement *pipeline;
	GError *error = NULL;

    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);

    // define pipeline
    const gchar *desc_templ =
        "   udpsrc port=1234  "
        " ! identity name=retriever "
        " ! application/x-rtp"
        " ! rtph264depay "
        " ! h264parse "
        " ! avdec_h264  "
        " ! videoconvert "
        " ! autovideosink ";


	/* Launch the pipeline*/
	gchar *desc = g_strdup (desc_templ);
	pipeline = gst_parse_launch (desc, &error);
	if (error) {
		g_printerr ("pipeline parsing error: %s\n", error->message);
		g_error_free (error);
		return 1;
	}

    // Probe with callback every time a buffer flows through
    place_probe(pipeline, "retriever", meta_retrieve);

    /* play */
    gst_element_set_state (pipeline, GST_STATE_PLAYING);
    g_main_loop_run (loop);

    /* clean up */
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (pipeline));
    g_main_loop_unref (loop);
    return 0;
}



