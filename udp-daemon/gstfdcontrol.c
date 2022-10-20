#include "gstfdcontrol.h"


#define N_INPUTS (5)

#define SEP (" ")

#define RAISE_NO_IMPLEMENTED(string) \
    GST_ERROR ("Passed '%s' to control handler. Not implemented", string)





/* 
 * Using fdsrc to read from stdin:
 *      If pipeline freezes on boot, pass a character and it should run
 *      gst_util_dump_mem (map.data, map.size);     // this to dump bytes into memory, debugging most useful!
 */
GstPadProbeReturn
control_handler (GstPad * pad, GstPadProbeInfo * info, gpointer u_data)
{
    GstBuffer *buf = info->data;
    GstElement *pipe = u_data;   
    char* text_command;
    char* words[N_INPUTS] = { 0 };
    int i = 0;

    

    GstMapInfo map;
    if (!gst_buffer_map (buf, &map, GST_MAP_READ)) {
        GST_ERROR ("Probe at fdsrc could not map buffer.");
        return GST_PAD_PROBE_OK;
    }

    // Fetch command and purge trailing newline in-place
    text_command = (char*) calloc(1, map.size);
    memcpy(text_command, map.data, map.size);
    gst_buffer_unmap (buf, &map);

    // Deal with trailing newline
    // Do NOT reassign to itself -> would return NULL if no '\n'
    // strtok ignores first/last chars. Input of "\n" handled as NotImplemented
    strtok(text_command, "\n"); 
    
    GST_DEBUG ("Command buffer: '%s'.", text_command);

    // Split command into a sequence of words
    i = 0;
    char *p = strtok (text_command, SEP);
    while (p != NULL) {

        // condition here so `i` counts all arguments, even if too many
        if (i <= N_INPUTS - 1) 
            words[i] = p;   
        
        p = strtok (NULL, SEP);    
        i++;
    }

    if (i > N_INPUTS) {
        GST_ERROR ("Command buffer overflow: Too many words.");
    } else if (i == 0) {
        GST_ERROR ("Command buffer underflow: No words provided.");
    } else if ('\n' == text_command[0]) {
        GST_ERROR ("Command buffer underflow: Passed single '\n'.");
    }

    

    if (words[0] == 0)
        ;

    else if (strcmp(words[0], "help") == 0)
        RAISE_NO_IMPLEMENTED(words[0]);

    else if (strcmp(words[0], "set") == 0) 
        control_set_property(words, pipe);

    else if (strcmp(words[0], "get") == 0) 
        control_get_property(words, pipe);

    else if (strcmp(words[0], "echo") == 0) 
        control_echo(words);

    else
        RAISE_NO_IMPLEMENTED(words[0]);


    free(text_command);   
    return GST_PAD_PROBE_OK;
}



// set valve0 drop 1
static int
control_set_property(char** words, GstElement* pipe)
{
    // Need at least 1cmd + 3args
    if (words[3] == 0){
        GST_ERROR ("Not enough arguments: passed '%s' '%s' '%s'", 
            words[1], words[2], words[3]);
    }

    char* element_name = words[1];
    char* property_name = words[2];
    char* property_value = atoi(words[3]);   //--------------------------------- fixme for anytype

    GstElement* element = gst_bin_get_by_name (GST_BIN (pipe), element_name);
    
    GValue old_value = G_VALUE_INIT;
    g_object_get_property (G_OBJECT (element), property_name, &old_value);
    char* old_value_str = g_strdup_value_contents (&old_value);
    
    gst_element_set_state (pipe, GST_STATE_PAUSED);
    g_object_set (G_OBJECT (element), property_name, property_value, NULL);
    usleep(10 * 1000);
    gst_element_set_state (pipe, GST_STATE_PLAYING);

    GValue new_value = G_VALUE_INIT;
    g_object_get_property (G_OBJECT (element), property_name, &new_value);
    char* new_value_str = g_strdup_value_contents (&new_value);

    printf(">> Element '%s', property '%s': '%s' -> '%s' \n",
        element_name, 
        property_name,
        old_value_str,
        new_value_str
        );

    gst_object_unref(element);
    g_value_unset(&new_value);   
    g_value_unset(&old_value);   
    free(old_value_str);
    free(new_value_str);    
    return 0;
}


static int
control_get_property(char** words, GstElement* pipe)
{   
    // Need at least 1cmd + 2args
    if (words[2] == 0){
        GST_ERROR ("Not enough arguments: passed '%s' '%s' ", 
            words[1], words[2]);
    }

    char* element_name = words[1];
    char* property_name = words[2];
    GValue value = G_VALUE_INIT;

    GstElement* element = gst_bin_get_by_name (GST_BIN (pipe), element_name);  
    g_object_get_property (G_OBJECT (element), property_name, &value);
    printf(">> Element '%s', property '%s': '%s' \n", 
        element_name, 
        property_name, 
        g_strdup_value_contents (&value)
        );
    
    gst_object_unref(element);
    g_value_unset(&value);
    return 0;
}




static int
control_help(void)
{

}



static int
control_echo(char** words)
{
    printf("@ echo: ");
    for (int i = 0; i < N_INPUTS; ++i) 
        printf("'%s' ", words[i]);
    
    printf("\n");

    return 0;
}