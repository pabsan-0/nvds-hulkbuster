#include "gstfdcontrol.h"


#define N_INPUTS (5)

#define SEP (" ")

#define CMD_PROMPT (">>> ")

#define PIPE_RESET_TIME_MS (10)

#define RAISE_NO_IMPLEMENTED(str) {                                          \
    GST_ERROR ("Passed '%s' to control handler. Not implemented", str);      \
    printf("Unknown command: '%s'. Use 'help' to see all options. \n", str); \
    }

#define RAISE_BAD_ARGUMENTS(fname, words, nw) {        \
        GST_ERROR ("Bad arguments for %s", fname);     \
        printf("Bad arguments for '%s': \n\t", fname); \
        control_echo(words, n_words);                  \
        fflush(stdout);                                \
    }



static int control_echo(char** words, int n_words);
static int control_set_property(char** words, int n_words, GstElement* pipeline);
static int control_get_property(char** words, int n_words, GstElement* pipeline);
static int control_help(void);
static int control_dummy(void);



GstPadProbeReturn
control_handler (GstPad * pad, GstPadProbeInfo * info, gpointer u_data)
{
    GstBuffer *buf = info->data;
    GstElement *pipe = u_data;   
    char* text_command;
    char* words[N_INPUTS] = { 0 };
    int nw = 0;
    

    GstMapInfo map;
    if (!gst_buffer_map (buf, &map, GST_MAP_READ)) {
        GST_ERROR ("Probe at fdsrc could not map buffer.");
        return GST_PAD_PROBE_OK;
    }

    // Fetch command and purge trailing newline in-place
    text_command = (char*) calloc(1, map.size);
    memcpy(text_command, map.data, map.size);
    gst_buffer_unmap (buf, &map);
    GST_DEBUG ("Command buffer: '%s'.", text_command);

    // Deal with trailing newline, note it returns NULL if no '\n'
    // strtok ignores first/last chars. Input of "\n" handled as NotImplemented
    strtok(text_command, "\n"); 
    

    // Split command into a sequence of words
    nw = 0;
    char *p = strtok (text_command, SEP);
    while (p != NULL) {

        // condition here so `i` counts all arguments, even if too many
        if (nw <= N_INPUTS - 1) 
            words[nw] = p;   
        
        p = strtok (NULL, SEP);    
        nw++;
    }

    if (nw > N_INPUTS) {
        GST_ERROR ("Command buffer overflow: Too many words.");
    } else if (nw == 0) {
        GST_ERROR ("Command buffer underflow: No words provided.");
    } else if ('\n' == text_command[0]) {
        GST_ERROR ("Command buffer underflow: Passed single '\n'.");
    }


    if (words[0] == 0 || words[0][0] == '\n')
        ;

    else if (strcmp(words[0], "help") == 0)
        control_help();

    else if (strcmp(words[0], "set") == 0) 
        control_set_property(words, nw, pipe);

    else if (strcmp(words[0], "get") == 0) 
        control_get_property(words, nw, pipe);

    else if (strcmp(words[0], "echo") == 0) 
        control_echo(words, nw);

    else if (strcmp(words[0], "dummy") == 0) 
        control_dummy();

    else
        RAISE_NO_IMPLEMENTED(words[0]);



    // A command prompt for further action 
    // Only prints after first buffer passes
    printf(CMD_PROMPT);
    fflush(stdout);

    free(text_command);   
    return GST_PAD_PROBE_OK;
}



// set valve0 drop 1
static int
control_set_property(char** words, int n_words, GstElement* pipe)
{
    if (n_words != 4) {
        RAISE_BAD_ARGUMENTS("control_set_property", words, n_words);
        printf("Usage: set <element_name> <property_name> <value>\n");
        return 1;
    }
    
    char* element_name = words[1];
    char* property_name = words[2];
    char* property_value_str = words[3];  

    GstElement* element = gst_bin_get_by_name (GST_BIN (pipe), element_name);
    
    GValue old_value = G_VALUE_INIT;
    g_object_get_property (G_OBJECT (element), property_name, &old_value);
    char* old_value_str = g_strdup_value_contents (&old_value);
    
    gst_element_set_state (pipe, GST_STATE_PAUSED);
    gst_util_set_object_arg (G_OBJECT (element), property_name, property_value_str);
    usleep(PIPE_RESET_TIME_MS * 1000);
    gst_element_set_state (pipe, GST_STATE_PLAYING);

    GValue new_value = G_VALUE_INIT;
    g_object_get_property (G_OBJECT (element), property_name, &new_value);
    char* new_value_str = g_strdup_value_contents (&new_value);

    printf("Element '%s', property '%s': '%s' -> '%s' \n",
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
control_get_property(char** words, int n_words, GstElement* pipe)
{   
    if (n_words != 3) {
        RAISE_BAD_ARGUMENTS("control_get_property", words, n_words);
        printf("Usage: get <element_name> <property_name> \n");
        return 1;
    }

    char* element_name = words[1];
    char* property_name = words[2];
    GValue value = G_VALUE_INIT;

    GstElement* element = gst_bin_get_by_name (GST_BIN (pipe), element_name);  
    g_object_get_property (G_OBJECT (element), property_name, &value);
    printf("Element '%s', property '%s': '%s' \n", 
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
    printf("Gstreamer Control Handler.                      \n"
           "                                                \n"
           "Helper CLI for interacting with live pipelines. \n"
           "Your STDIN is being sent to Gstreamer.          \n"
           "                                                \n"
           "Usage:                                          \n"
           "    set <element_name> <property_name> <value>  \n"
           "    get <element_name> <property_name>          \n"
           "    echo [args]                                 \n"
           "    help                                        \n"
           "    dummy                                       \n"
           "                                                \n"
          );;;;
    return 0;
}


static int
control_echo(char** words, int n_words)
{
    printf("@ echo: ");
    for (int i = 0; i < N_INPUTS; ++i) 
        printf("'%s' ", words[i]);
    printf(" (counted %d words) \n", n_words);

    return 0;
}


static int
control_dummy(void)
{
    RAISE_NO_IMPLEMENTED("dummy");
    return 0;
}