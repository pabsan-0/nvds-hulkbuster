#ifndef __GST_FDCONTROL_H__
#define __GST_FDCONTROL_H__

#include <gst/gst.h>

#define INPUT_BUFFER_WORDS (5)   // input text buffer: max words to be stored
#define INPUT_BUFFER_SEP (" ")   // input text buffer: word separator 

#define CMD_PROMPT (">>> ")      // input text prompt symbols

#define PIPE_RESET_TIME_MS (10)  // pipeline pause time after property change 


/// @brief Probe pad to receive and process file descriptor input.
/// @param pad Should be source pad of a `fdsrc fd=0` element.
/// @param info 
/// @param u_data Should be pointer to `GstElement *pipeline` object. 
/// @return 
GstPadProbeReturn
control_handler (GstPad * pad, GstPadProbeInfo * info, gpointer u_data);


#endif  /* __GST_FDCONTROL_H__ */
