#
# This bash quickie lists your connected v4l2 cameras by parsing the output of `v4l2-ctl --list-devices`.
# Useful when playing w/ external cameras since replugging will usually modify its /dev/videoX location
#
# Example output
#  ```
#  C:
#
#  #define CAM_0 "/dev/video2"
#  #define CAM_1 "/dev/video0"
#
#  Bash:
#
#  CAM_0="/dev/video2"
#  CAM_1="/dev/video0"
#  ```


echo C:
echo

v4l2-ctl --list-devices \
   | awk '/usb/{getline; print}' \
   | awk '{str=$0} {printf "#define CAM_%d \"%s\"\n", idx, str} {idx+=1}' \
   | tr -d '\t'

echo
echo Bash:
echo

v4l2-ctl --list-devices \
  | awk '/usb/{getline; print}' \
  | awk '{str=$0} {printf "CAM_%d=\"%s\"\n", idx, str} {idx+=1}' \
  | tr -d '\t'
