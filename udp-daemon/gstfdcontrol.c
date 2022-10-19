#include "gstfdcontrol.h"


/* 
 * Using fdsrc to read from stdin:
 *      If pipeline freezes on boot, pass a character and it should run
 *      gst_util_dump_mem (map.data, map.size);     // this to dump bytes into memory, debugging most useful!
 */
GstPadProbeReturn
control_handler (GstPad * pad, GstPadProbeInfo * info, gpointer u_data)
{
    GstElement *pipe = u_data;   
    GstBuffer *buf = info->data;

    GstMapInfo map;
    if (!gst_buffer_map (buf, &map, GST_MAP_READ)) {
        GST_ERROR ("Custom probe at fdsrc: could not map buffer.");
        return GST_PAD_PROBE_OK;
    }

    if (map.size < 2){
        GST_ERROR ("Empty-like stdin message.");
        gst_buffer_unmap (buf, &map);
        return GST_PAD_PROBE_OK;
    }

    char* text_command = (char*) calloc(1, map.size);
    memcpy(text_command, map.data, map.size);

    // purge trailing newline in-place 
    text_command  = strtok(text_command, "\n");
    char* text_backup = strdup(text_command);

    // get the three first words
    char *first  = strtok(text_command, " ");
    char *second = strtok(NULL, " ");
    char *third  = strtok(NULL, " ");
    char *fourth = strtok(NULL, " ");

    if (!(first && second && third)){ 
        printf("@ Too few input words! Expected 3, got: '%s' '%s' '%s' from '%s' \n",
            first, second, third, text_backup);
    } else if (fourth) {
        printf("@ Too many input words! Expected 3, got: '%s' '%s' '%s' from '%s' \n",
            first, second, third, text_backup);
    } else {
        printf("@ Apparently great! Got: '%s' '%s' '%s' from '%s' \n",
            first, second, third, text_backup);
    }

    GstElement* element = gst_bin_get_by_name (GST_BIN (pipe), first);
    g_object_set (G_OBJECT (element), second, third, NULL);

    free(text_backup); 
    free(text_command);
    gst_buffer_unmap (buf, &map);
    return GST_PAD_PROBE_OK;
}