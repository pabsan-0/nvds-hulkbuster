#ifndef __GST_FDCONTROL_H__
#define __GST_FDCONTROL_H__

#include <stdio.h>
#include <gst/gst.h>
#include <stdlib.h>


GstPadProbeReturn
control_handler (GstPad * pad, GstPadProbeInfo * info, gpointer u_data);

#endif /* __GST_CONTROL_H__*/
