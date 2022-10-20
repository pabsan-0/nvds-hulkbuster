# NVDS-Hulkbuster


## Tips
- Do not use single quotes in gst-parse launch pipelines
- Run rtsp server outside of docker
- Funky fixes for 4 cameras' goblins:
    - 4 cameras seen to fail on 640x480, but work on 720
    - If they fail at 1280x720, try at 800x600 then go back again to 1280x720
    - Close the display window to end the application, try to avoid ^C
    - Rerun docker image after replugging cameras







https://stackoverflow.com/questions/52364752/injecting-inserting-adding-h-264-sei

gst-launch-1.0
    funnel name=f \
    appsrc ! video/x-h264, stream-format=byte-stream, alignment=au ! queue ! f.
    videotestsrc is-live=true ! x264enc ! video/x-h264, stream-format=byte-stream, alignment=au, profile=baseline ! queue ! f. \
    f. ! queue ! h264parse ! video/x-h264, stream-format=byte-stream, alignment=nal ! filesink location=dump.h264




# tested docker in host out

gst-launch-1.0 videotestsrc        \
    ! videoconvert  \
    ! x264enc tune=zerolatency  \
    ! rtph264pay  \
    ! udpsink host=127.0.0.1 port=1234                               ;


gst-launch-1.0 udpsrc port=1234                                                  \
    ! application/x-rtp ! rtph264depay                                           \
    ! queue leaky=1 max-size-buffers=100 max-size-time=0 max-size-bytes=0        \
    ! h264parse config-interval=-1 ! avdec_h264                                  \
    ! queue leaky=1 max-size-buffers=10 max-size-time=200000000 max-size-bytes=0 \
    ! videoconvert ! queue leaky=2 ! xvimagesink sync=false async=false          ;



