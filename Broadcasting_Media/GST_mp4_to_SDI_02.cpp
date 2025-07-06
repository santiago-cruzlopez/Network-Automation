#include <gst/gst.h>
#include <string>

int main(int argc, char *argv[]) {
    // Initialize GStreamer
    gst_init(&argc, &argv);

    // Define pipeline string
    std::string pipeline_str = 
        "filesrc location=/home/sebastian/swappr/Video_Samples/Clip_1.mp4 ! "
        "qtdemux name=demux demux.video_0 ! "
        "queue ! h264parse ! avdec_h264 ! "
        "videoscale ! video/x-raw,width=1280,height=720 ! "
        "videorate ! video/x-raw,framerate=30000/1001 ! "
        "videoconvert ! video/x-raw,format=UYVY ! "
        "decklinkvideosink device-number=0 mode=720p2997";

    GError *error = NULL;
    GstElement *pipeline = gst_parse_launch(pipeline_str.c_str(), &error);
    if (!pipeline || error) {
        g_printerr("Error creating pipeline: %s\n", error ? error->message : "Unknown error");
        if (error) g_error_free(error);
        return -1;
    }

    // Start pipeline
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Error: Unable to start the pipeline.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    g_print("Pipeline started. Check SDI output on DeckLink Port 0.\n");

    // Monitor pipeline messages
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, 
        static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    if (msg) {
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError *err;
                gchar *debug;
                gst_message_parse_error(msg, &err, &debug);
                g_printerr("Error: %s, Debug: %s\n", err->message, debug);
                g_error_free(err);
                g_free(debug);
                break;
            }
            case GST_MESSAGE_EOS:
                g_print("End of stream reached.\n");
                break;
            default:
                break;
        }
        gst_message_unref(msg);
    }

    // Cleanup
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(pipeline);
    g_print("Pipeline stopped.\n");

    return 0;
}