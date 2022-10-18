#! /usr/bin/bash

gst-launch-1.0                                                 \
      videotestsrc                                             \
    ! x264enc tune=zerolatency                                 \
    ! video/x-h264, profile=high                               \
    ! mpegtsmux                                                \
    ! srtsink uri=srt://:8888/ mode=2 latency=10     ;