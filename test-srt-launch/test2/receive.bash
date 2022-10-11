#! /usr/bin/bash

gst-launch-1.0 srtsrc uri=srt://127.0.0.1:8888/ mode=1 latency=10         \
    ! queue                                       \
    ! tsdemux                                     \
    ! queue                                       \
    ! h264parse                                   \
    ! avdec_h264                                  \
    ! queue                                       \
    ! videoconvert                                \
    ! autovideosink                               \
     srtsrc uri=srt://127.0.0.1:8889/ mode=1 latency=10         \
    ! queue                                       \
    ! tsdemux                                     \
    ! queue                                       \
    ! h264parse                                   \
    ! avdec_h264                                  \
    ! queue                                       \
    ! videoconvert                                \
    ! autovideosink                               ;