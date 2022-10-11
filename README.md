# NVDS-Hulkbuster

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