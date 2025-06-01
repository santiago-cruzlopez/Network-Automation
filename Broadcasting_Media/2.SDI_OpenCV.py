// https://stackoverflow.com/questions/29638812/capture-frame-with-opencv-using-blackmagic-card-intensity-shuttle

import vlc,cv2
import ctypes
import time
import sys
import numpy as np

from PIL import Image


pl = vlc.MediaPlayer('dshow://')

VIDEOWIDTH = 1280
VIDEOHEIGHT = 720

# size in bytes when RV32
size = VIDEOWIDTH * VIDEOHEIGHT * 4
# allocate buffer
buf1 = (ctypes.c_ubyte * size)()
buf2 = (ctypes.c_ubyte * size)()
# get pointer to buffer
buf_p = ctypes.cast(buf1, ctypes.c_void_p)

# global frame (or actually displayed frame) counter
framenr = 0
test=[]
# vlc.CallbackDecorators.VideoLockCb is incorrect
CorrectVideoLockCb = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p))

@CorrectVideoLockCb
def _lockcb(opaque, planes):
    print "lock"
    planes[0] = buf_p
    ctypes.memmove(buf2, buf1, size)

    #time.sleep(1)

@vlc.CallbackDecorators.VideoDisplayCb
def _display(opaque, picture):

    global framenr,test

    img = Image.frombuffer("RGBA", (VIDEOWIDTH, VIDEOHEIGHT), buf2, "raw", "BGRA", 0, 1)
    #img.save('img{}.png'.format(framenr))
    #print "display {}".format(framenr)
    # shouldn't do this here! copy buffer fast and process in our own thread, or maybe cycle
    # through a couple of buffers, passing one of them in _lockcb while we read from the other(s).
        #img.save('img{}.png'.format(framenr))
    #img=cv2.cvtColor(np.array(img), cv2.COLOR_BGRA2RGB)
    frame_CV=np.array(img)
    framenr+=1    
    print "frame",framenr,frame_CV.shape        
    test=frame_CV
    font = cv2.FONT_HERSHEY_SIMPLEX    
    cv2.putText(test,"Test",(50,50), font, 0.7,(250,20,30),1,cv2.LINE_AA)
    #cv2.imshow("Test",test)
    #cv2.waitKey(1)
    #if framenr==10:      
        #cv2.imshow("Test",test)        
    #print "OpenCV"

#------------------------------------------------------------------------------    
# MAIN CYCLE
#------------------------------------------------------------------------------
vlc.libvlc_video_set_callbacks(pl, _lockcb, None, _display, None)
pl.video_set_format("RV32", VIDEOWIDTH, VIDEOHEIGHT, VIDEOWIDTH * 4)
pl.play()
#10 seconds are necessary to consolidate memory buffer exchange and library 
#syncronization

time.sleep(10)
while True:


    cv2.imshow("Test",test)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break
pl.stop()
cv2.destroyAllWindows()
#time.sleep(10)
#------------------------------------------------------------------------------
