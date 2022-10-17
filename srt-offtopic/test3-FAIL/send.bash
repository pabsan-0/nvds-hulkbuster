#! /usr/bin/bash

gst-launch-1.0                                        \
      v4l2src device=/dev/video2                                    \
    ! video/x-raw, width=640, height=480 \
    ! videoconvert \
    ! x264enc tune=zerolatency                        \
    ! video/x-h264, profile=high                      \
    ! mpegtsmux                                       \
    ! srtsink uri=srt://:8888/ mode=2 latency=10      \
      v4l2src device=/dev/video4                                    \
    ! video/x-raw, width=640, height=480 \
    ! videoconvert \
    ! x264enc tune=zerolatency                        \
    ! video/x-h264, profile=high                      \
    ! mpegtsmux                                       \
    ! srtsink uri=srt://:8889/ mode=2 latency=10      ;