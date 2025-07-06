#include <gst/gst.h>
#include <iostream>

typedef struct _CustomData {
    GstElement *pipeline;
    GstElement *video_sink_bin;
} CustomData;

static void pad_added_handler(GstElement *src_element, GstPad *new_pad, CustomData *data) {
    g_print("Received new pad '%s' from '%s'.\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src_element));

    GstPad *sink_pad = gst_element_get_static_pad(data->video_sink_bin, "sink");
    if (!sink_pad) {
        g_printerr("Could not get sink pad from video bin.\n");
        return;
    }

    if (gst_pad_is_linked(sink_pad)) {
        g_print("Video sink already linked. Ignoring.\n");
        gst_object_unref(sink_pad);
        return;
    }

    GstCaps *new_pad_caps = gst_pad_get_current_caps(new_pad);
    const GstStructure *new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    const gchar *new_pad_type = gst_structure_get_name(new_pad_struct);

    if (!g_str_has_prefix(new_pad_type, "video/x-raw")) {
        g_print("It has type '%s' which is not raw video. Ignoring.\n", new_pad_type);
        gst_caps_unref(new_pad_caps);
        gst_object_unref(sink_pad);
        return;
    }

    GstPadLinkReturn ret = gst_pad_link(new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED(ret)) {
        g_printerr("  Failed to link video pad.\n");
    } else {
        g_print("  Successfully linked video pad.\n");
    }

    gst_caps_unref(new_pad_caps);
    gst_object_unref(sink_pad);
}

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
    (void)bus;
    GMainLoop *loop = (GMainLoop *)data;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            g_print("End-Of-Stream reached.\n");
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_ERROR: {
            gchar *debug_info;
            GError *err;
            gst_message_parse_error(msg, &err, &debug_info);
            g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
            g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
            g_clear_error(&err);
            g_free(debug_info);
            g_main_loop_quit(loop);
            break;
        }
        default:
            break;
    }
    return TRUE;
}

int main(int argc, char *argv[]) {
    CustomData data;
    GstBus *bus;
    guint bus_watch_id;
    GstElement *filesrc, *decodebin;
    GMainLoop *loop;

    GstElement *video_queue, *video_convert, *video_rate, *video_capsfilter, *decklink_sink;
    GstPad *ghost_pad;

    gst_init(&argc, &argv);

    const char* file_path = "/home/sebastian/swappr/Video_Samples/Clip_1.mp4";


    loop = g_main_loop_new(NULL, FALSE);

    // Create the elements
    data.pipeline = gst_pipeline_new("playout-pipeline");
    filesrc = gst_element_factory_make("filesrc", "file-source");
    decodebin = gst_element_factory_make("decodebin", "decoder");
    
    data.video_sink_bin = gst_bin_new("video-sink-bin");
    video_queue = gst_element_factory_make("queue", "video-queue");
    video_convert = gst_element_factory_make("videoconvert", "video-converter");
    video_rate = gst_element_factory_make("videorate", "video-rate");
    video_capsfilter = gst_element_factory_make("capsfilter", "video-format-filter");
    decklink_sink = gst_element_factory_make("decklinkvideosink", "decklink-sink");

    if (!data.pipeline || !filesrc || !decodebin || !data.video_sink_bin || !video_queue ||
        !video_convert || !video_rate || !video_capsfilter || !decklink_sink) {
        g_printerr("One or more elements could not be created. Exiting.\n");
        return -1;
    }

    // Configure element properties
    g_object_set(filesrc, "location", file_path, NULL);
    g_object_set(decklink_sink, "device-number", 0, "mode", 7, NULL); 
    
    GstCaps *video_caps = gst_caps_new_simple("video/x-raw",
        "format", G_TYPE_STRING, "UYVY",
        "width", G_TYPE_INT, 1280,
        "height", G_TYPE_INT, 720,
        "framerate", GST_TYPE_FRACTION, 60000, 1001,
        NULL);
    g_object_set(video_capsfilter, "caps", video_caps, NULL);
    gst_caps_unref(video_caps);

    // Build the pipeline
    bus = gst_element_get_bus(data.pipeline);
    bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);
    
    gst_bin_add_many(GST_BIN(data.pipeline), filesrc, decodebin, data.video_sink_bin, NULL);
    gst_bin_add_many(GST_BIN(data.video_sink_bin), video_queue, video_convert, video_rate, video_capsfilter, decklink_sink, NULL);
    
    if (!gst_element_link_many(video_queue, video_convert, video_rate, video_capsfilter, decklink_sink, NULL)) {
        g_printerr("Could not link the video processing chain. Exiting.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    ghost_pad = gst_element_get_static_pad(video_queue, "sink");
    gst_element_add_pad(data.video_sink_bin, gst_ghost_pad_new("sink", ghost_pad));
    gst_object_unref(ghost_pad);

    if (!gst_element_link(filesrc, decodebin)) {
        g_printerr("Could not link filesrc to decodebin. Exiting.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(pad_added_handler), &data);

    // Start playing
    g_print("Now playing: %s to DeckLink SDI Port 1\n", file_path);
    if (gst_element_set_state(data.pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    g_print("Pipeline is running... Press Ctrl+C to stop.\n");
    
    g_main_loop_run(loop);

    // Clean up
    g_print("Returned, stopping playback\n");
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    g_print("Deleting pipeline\n");
    gst_object_unref(data.pipeline);
    g_source_remove(bus_watch_id);
    g_main_loop_unref(loop);

    return 0;
}