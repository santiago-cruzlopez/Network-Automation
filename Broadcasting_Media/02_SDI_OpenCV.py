import vlc
import cv2
import ctypes
import time
import sys
import numpy as np
from PIL import Image

# VLC video resolution
VIDEOWIDTH = 1280
VIDEOHEIGHT = 720
BYTES_PER_PIXEL = 4  # for RV32 (RGBA)

# Allocate two frame buffers
frame_size = VIDEOWIDTH * VIDEOHEIGHT * BYTES_PER_PIXEL
buf1 = (ctypes.c_ubyte * frame_size)()
buf2 = (ctypes.c_ubyte * frame_size)()

# Global buffer pointer
buf_p = ctypes.cast(buf1, ctypes.c_void_p)

# Initialize global variables
frame_number = 0
last_frame = np.zeros((VIDEOHEIGHT, VIDEOWIDTH, 4), dtype=np.uint8)

# Lock callback function
CorrectVideoLockCb = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p))

@CorrectVideoLockCb
def _lockcb(opaque, planes):
    global buf_p, buf1, buf2, frame_size
    # Copy buf1 to buf2 to allow reading outside of lock
    ctypes.memmove(buf2, buf1, frame_size)
    planes[0] = buf_p
    return None

# Display callback function
@vlc.CallbackDecorators.VideoDisplayCb
def _displaycb(opaque, picture):
    global frame_number, last_frame

    # Convert buffer to PIL Image
    img = Image.frombuffer("RGBA", (VIDEOWIDTH, VIDEOHEIGHT), buf2, "raw", "BGRA", 0, 1)
    frame_np = np.array(img)

    # Draw something on the frame
    cv2.putText(frame_np, f"Frame {frame_number}", (50, 50), cv2.FONT_HERSHEY_SIMPLEX,
                0.7, (250, 20, 30), 2, cv2.LINE_AA)

    # Save frame for display
    last_frame = frame_np
    frame_number += 1

# Create VLC player and set media
player = vlc.MediaPlayer("dshow://")  # Use 'v4l2://' on Linux

# Set VLC callbacks and video format
vlc.libvlc_video_set_callbacks(player, _lockcb, None, _displaycb, None)
player.video_set_format("RV32", VIDEOWIDTH, VIDEOHEIGHT, VIDEOWIDTH * BYTES_PER_PIXEL)
player.play()

# Give the VLC player time to initialize
time.sleep(2)

# Main loop to display frames
try:
    while True:
        if last_frame is not None:
            cv2.imshow("VLC Frame", last_frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
finally:
    player.stop()
    cv2.destroyAllWindows()

# https://stackoverflow.com/questions/29638812/capture-frame-with-opencv-using-blackmagic-card-intensity-shuttle