#ifndef __GST_FDCONTROL_H__
#define __GST_FDCONTROL_H__

#include <stdio.h>
#include <gst/gst.h>
#include <stdlib.h>
#include <unistd.h>

// these only for real time syncing of parameter change i.e. fadein /fadeout
// #include <gst/controller/gstinterpolationcontrolsource.h>
// #include <gst/controller/gstdirectcontrolbinding.h>

GstPadProbeReturn
control_handler (GstPad * pad, GstPadProbeInfo * info, gpointer u_data);

static int 
control_echo(char** words);

static int
control_set_property(char** words, GstElement* pipeline);

static int
control_help(void);


#endif /* __GST_CONTROL_H__*/
