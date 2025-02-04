﻿/**
 * @file remote-webrtc-pipeline.c
 * @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
 * @brief 
 * @version 1.0
 * @date 2021-12-08
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include <remote-webrtc-pipeline.h>
#include <remote-webrtc-type.h>
#include <remote-webrtc-data-channel.h>
#include <remote-webrtc-signalling.h>
#include <remote-webrtc-pipeline.h>

#include <enum.h>
#include <global-var.h>
#include <constant.h>
#include <remote-config.h>

#include <gst/gst.h>
#include <glib-2.0/glib.h>
#include <gst/webrtc/webrtc.h>





/**
 * @brief 
 * naming of gstelement
 */
enum
{
    /**
     * @brief 
     * 
     */
    VIDEO_SINK,
    VIDEO_QUEUE_SINK,

    /**
     * @brief 
     * 
     */
    VIDEO_CONVERT,
    VIDEO_QUEUE_CONVERT,

    /**
     * @brief 
     * 
     */
    VIDEO_DECODER,
    VIDEO_QUEUE_DECODER,

    /**
     * @brief 
     * 
     */
    VIDEO_DEPAYLOAD,
    VIDEO_QUEUE_DEPAYLOAD,

    VIDEO_ELEMENT_LAST
};

/**
 * @brief 
 * naming of gstelement
 */
enum
{
    AUDIO_SINK,
    AUDIO_QUEUE_SINK,

    /**
     * @brief 
     * 
     */
    AUDIO_RESAMPLE,
    AUDIO_QUEUE_RESAMPLE,

    /**
     * @brief 
     * 
     */
    AUDIO_CONVERT,
    AUDIO_QUEUE_CONVERT,


    /**
     * @brief 
     * 
     */
    AUDIO_DECODER,
    AUDIO_QUEUE_DECODER,

    AUDIO_ELEMENT_LAST
};


struct _Pipeline
{
    GstElement* pipeline;
    GstElement* webrtcbin;

    GstElement* video_element[VIDEO_ELEMENT_LAST];
    GstElement* audio_element[AUDIO_ELEMENT_LAST];
};


void
setup_video_sink_navigator(RemoteApp* core);



Pipeline*
pipeline_initialize(RemoteApp* core)
{
    Pipeline* pipeline = malloc(sizeof(Pipeline));
    memset(pipeline,0,sizeof(Pipeline));
    return pipeline;
}

void
free_pipeline(Pipeline* pipeline)
{
    gst_element_set_state (pipeline->pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline->pipeline);
    memset(pipeline,0,sizeof(Pipeline));
}


static gboolean
start_pipeline(RemoteApp* core)
{
    GstStateChangeReturn ret;
    Pipeline* pipe = remote_app_get_pipeline(core);

    ret = GST_IS_ELEMENT(pipe->pipeline);    

    ret = gst_element_set_state(GST_ELEMENT(pipe->pipeline), GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        GError error;
        error.message = "Fail to start pipeline, this may due to pipeline setup failure";
        remote_app_finalize(core, &error);
    }
    return TRUE;
}



static void
handle_audio_stream (GstPad * pad, 
                     RemoteApp* core)
{
    Pipeline* pipeline = remote_app_get_pipeline(core);

    pipeline->audio_element[AUDIO_CONVERT] = gst_element_factory_make ("audioconvert", NULL);
    pipeline->audio_element[AUDIO_RESAMPLE]= gst_element_factory_make ("audioresample", NULL);
    pipeline->audio_element[AUDIO_SINK] =    gst_element_factory_make ("autoaudiosink", NULL);

    /* Might also need to resample, so add it just in case.
    * Will be a no-op if it's not required. */
    gst_bin_add_many (GST_BIN (pipeline->pipeline), 
        pipeline->audio_element[AUDIO_CONVERT], 
        pipeline->audio_element[AUDIO_RESAMPLE], 
        pipeline->audio_element[AUDIO_SINK], NULL);

    gst_element_sync_state_with_parent (pipeline->audio_element[AUDIO_CONVERT]);
    gst_element_sync_state_with_parent (pipeline->audio_element[AUDIO_RESAMPLE]);
    gst_element_sync_state_with_parent (pipeline->audio_element[AUDIO_SINK]);

    gst_element_link_many ( 
        pipeline->audio_element[AUDIO_QUEUE_CONVERT], 
        pipeline->audio_element[AUDIO_CONVERT], 
        pipeline->audio_element[AUDIO_QUEUE_RESAMPLE],
        pipeline->audio_element[AUDIO_RESAMPLE],
        pipeline->audio_element[AUDIO_QUEUE_SINK],
        pipeline->audio_element[AUDIO_SINK], NULL);

    GstPad* queue_pad = gst_element_get_static_pad (pipeline->audio_element[AUDIO_QUEUE_CONVERT], "sink");
    GstPadLinkReturn ret = gst_pad_link (pad, queue_pad);
    g_assert_cmphex (ret, ==, GST_PAD_LINK_OK);
}

static void
handle_video_stream (GstPad * pad, 
                     RemoteApp* core)
{
    Pipeline* pipeline = remote_app_get_pipeline(core);

#ifdef G_OS_WIN32
    pipeline->video_element[VIDEO_SINK] = gst_element_factory_make ("d3d11videosink", NULL);
#else
    pipeline->video_element[VIDEO_SINK] = gst_element_factory_make ("autovideosink", NULL);
#endif

    gst_bin_add_many (GST_BIN (pipeline->pipeline),
        pipeline->video_element[VIDEO_SINK], NULL);

    gst_element_sync_state_with_parent (pipeline->video_element[VIDEO_SINK]);

    gst_element_link_many ( 
        pipeline->video_element[VIDEO_QUEUE_SINK], 
        pipeline->video_element[VIDEO_SINK], NULL);

    GstPad* queue_pad = gst_element_get_static_pad (pipeline->video_element[VIDEO_QUEUE_SINK], "sink");


    GstPadLinkReturn ret = gst_pad_link (pad, queue_pad);
    g_assert_cmphex (ret, ==, GST_PAD_LINK_OK);


    gst_element_set_state(GST_ELEMENT(pipeline->pipeline), GST_STATE_PLAYING);

    GstCaps* caps = gst_pad_get_current_caps (pad);
    if (!caps)
        caps = gst_pad_query_caps (pad, NULL);

#ifdef G_OS_WIN32
    GUI* gui = remote_app_get_gui(core);
    setup_video_overlay(gui,
        caps,
        pipeline->video_element[VIDEO_SINK],
        pipeline->pipeline);
#endif
}



/**
 * @brief 
 * 
 * @param decodebin 
 * @param pad 
 * @param data 
 */
static void
on_incoming_decodebin_stream (GstElement * decodebin, 
                              GstPad * pad,
                              gpointer data)
{
    RemoteApp* core = (RemoteApp*)data;
    Pipeline* pipeline = remote_app_get_pipeline(core);

    if (!gst_pad_has_current_caps (pad)) 
    {
        g_printerr ("Pad '%s' has no caps, can't do anything, ignoring\n",
            GST_PAD_NAME (pad));
        return;
    }

    GstCaps* caps = gst_pad_get_current_caps (pad);
    GstStructure* structure = gst_caps_get_structure (caps, 0);
    gchar*   name = gst_structure_get_name (structure);

    if (g_str_has_prefix (name, "video")) 
    {
        handle_video_stream(pad, core);
    } 
    else if (g_str_has_prefix (name, "audio")) 
    {
        handle_audio_stream(pad, core);
    } 
}


gboolean
is_d3d11_capable()
{
    static gboolean result;
    static gboolean initialize = FALSE;
    if(!initialize)
    {
        GstElementFactory* factory = gst_element_factory_find("d3d11h265dec");
        result = factory ? TRUE : FALSE;
        initialize = TRUE;
    }
    return result;
}



gint
video_element_select(GstElement * bin,
                    GstPad * pad,
                    GstCaps * caps,
                    GstElementFactory * factory,
                    gpointer udata)
{
    gint i = 0;
    gboolean select = FALSE; 
    gchar** keys;
    keys = gst_element_factory_get_metadata_keys (factory);

    if(DEVELOPMENT_ENVIRONMENT)
        g_print("\n\nQuerying a new video element\n");

    while (keys[i]) {
        gchar * value = gst_element_factory_get_metadata (factory,keys[i]);

        if(DEVELOPMENT_ENVIRONMENT) 
            g_print("%s : %s\n",keys[i],value);

        if (g_str_has_prefix(value,"RTP H264") ||
            g_str_has_prefix(value,"RTP H265")) 
            select = TRUE;
        
        if (g_str_has_prefix(value,"H.264 parser") || 
            g_str_has_prefix(value,"H.265 parser")) 
            select = TRUE;

        
        if (g_str_has_prefix(value,"Codec/Decoder/Video")) 
        {
            gboolean d3d11 = is_d3d11_capable();
            gchar* des = gst_element_factory_get_metadata (factory,"description");
            if (g_str_has_prefix(des,"Direct3D11") && d3d11) 
                select = TRUE;

            if (g_str_has_prefix(des,"libav") && !d3d11) 
                select = TRUE;
            
            if(select)
                g_print("Staring with %s",des);
        }
        i++;
    }

    return select ? 0 : 2;
}

gint
audio_element_select(GstElement * bin,
                    GstPad * pad,
                    GstCaps * caps,
                    GstElementFactory * factory,
                    gpointer udata)
{
    gint i = 0;
    gboolean select = FALSE; 
    gchar** keys;
    keys = gst_element_factory_get_metadata_keys (factory);

    if(DEVELOPMENT_ENVIRONMENT)
        g_print("Querying a new audio element\n");
        
    while (keys[i]) {
        gchar * value = gst_element_factory_get_metadata (factory,keys[i]);
        if(DEVELOPMENT_ENVIRONMENT)
            g_print("%s : %s\n",keys[i],value);

        i++;
    }

    return 0;
}


/**
 * @brief 
 * 
 * @param webrtc 
 * @param webrtcbin_pad 
 * @param data 
 */
static void
on_incoming_stream (GstElement * webrtc, 
                    GstPad * webrtcbin_pad, 
                    gpointer data)
{
    RemoteApp* core = (RemoteApp*)data;
    if (GST_PAD_DIRECTION (webrtcbin_pad) != GST_PAD_SRC)
      return;

    Pipeline* pipeline = remote_app_get_pipeline(core);
    
    GstCaps* caps =             gst_pad_get_current_caps(webrtcbin_pad);
    GstStructure* structure =   gst_caps_get_structure(caps, 0);
    gchar* encoding =           gst_structure_get_string(structure, "encoding-name");
    gchar* name =               gst_structure_get_name(structure);

    if(!g_strcmp0("application/x-rtp",name) &&
       !g_strcmp0("OPUS",encoding))
    {
        pipeline->audio_element[AUDIO_DECODER] = gst_element_factory_make ("decodebin", "audiodecoder");

        g_object_set(pipeline->audio_element[AUDIO_DECODER], "max-size-time", 0, NULL);
        g_object_set(pipeline->audio_element[AUDIO_DECODER], "max-size-bytes", 0, NULL);
        g_object_set(pipeline->audio_element[AUDIO_DECODER], "max-size-buffers", 3, NULL);

        g_signal_connect (pipeline->audio_element[AUDIO_DECODER], "pad-added",
            G_CALLBACK (on_incoming_decodebin_stream), core);
        g_signal_connect (pipeline->audio_element[AUDIO_DECODER], "autoplug-select",
            G_CALLBACK (audio_element_select), core);
        gst_bin_add (GST_BIN (pipeline->pipeline), pipeline->audio_element[AUDIO_DECODER]);

        gst_element_sync_state_with_parent (pipeline->audio_element[AUDIO_DECODER]);

        GstCaps* cap = gst_element_get_static_pad (pipeline->audio_element[AUDIO_DECODER], "sink");
        gst_pad_link (webrtcbin_pad, cap);
        gst_object_unref (cap);
    }

    if(!g_strcmp0("application/x-rtp",name) &&
       (!g_strcmp0("H265",encoding) ||
        !g_strcmp0("H264",encoding)))
    {
        pipeline->video_element[VIDEO_DECODER] = gst_element_factory_make ("decodebin", "videodecoder");

        g_object_set(pipeline->video_element[VIDEO_DECODER], "max-size-time", 0, NULL);
        g_object_set(pipeline->video_element[VIDEO_DECODER], "max-size-bytes", 0, NULL);
        g_object_set(pipeline->video_element[VIDEO_DECODER], "max-size-buffers", 3, NULL);

        g_signal_connect (pipeline->video_element[VIDEO_DECODER], "pad-added",
            G_CALLBACK (on_incoming_decodebin_stream), core);
        g_signal_connect (pipeline->video_element[VIDEO_DECODER], "autoplug-select",
            G_CALLBACK (video_element_select), core);
        gst_bin_add (GST_BIN (pipeline->pipeline), pipeline->video_element[VIDEO_DECODER]);

        gst_element_sync_state_with_parent (pipeline->video_element[VIDEO_DECODER]);

        GstCaps* cap = gst_element_get_static_pad (pipeline->video_element[VIDEO_DECODER], "sink");
        gst_pad_link (webrtcbin_pad, cap);
        gst_object_unref (cap);
    }
}









 
static void
setup_pipeline_queue(Pipeline* pipeline)
{
    GstElement* queue_array[9];
    for (gint i = 0; i < 9; i++)
    {
        queue_array[i] = gst_element_factory_make ("queue", NULL);
        g_object_set(queue_array[i], "max-size-time", 0, NULL);
        g_object_set(queue_array[i], "max-size-bytes", 0, NULL);
        g_object_set(queue_array[i], "max-size-buffers", 3, NULL);

        gst_bin_add(GST_BIN(pipeline->pipeline),queue_array[i]);
        gst_element_sync_state_with_parent(queue_array[i]);
    }

    pipeline->audio_element[AUDIO_QUEUE_SINK] =             queue_array[0];
    pipeline->audio_element[AUDIO_QUEUE_RESAMPLE] =         queue_array[1];
    pipeline->audio_element[AUDIO_QUEUE_CONVERT] =          queue_array[2];
    pipeline->audio_element[AUDIO_QUEUE_DECODER] =          queue_array[3];

    pipeline->video_element[VIDEO_QUEUE_SINK] =             queue_array[5];
    pipeline->video_element[VIDEO_QUEUE_CONVERT] =          queue_array[6];
    pipeline->video_element[VIDEO_QUEUE_DECODER] =          queue_array[7];
    pipeline->video_element[VIDEO_QUEUE_DEPAYLOAD] =        queue_array[8];
}


#define RTP_CAPS_AUDIO "application/x-rtp,media=audio,payload=96,encoding-name="
gpointer
setup_pipeline(RemoteApp* core)
{
    GstCaps *video_caps;
    GstWebRTCRTPTransceiver *trans = NULL;
    SignallingHub* signalling = remote_app_get_signalling_hub(core);
    Pipeline* pipe = remote_app_get_pipeline(core);

    if(pipe->pipeline)
        free_pipeline(pipe);

    GError* error = NULL;

    pipe->pipeline = gst_parse_launch(
        "webrtcbin name=webrtcbin bundle-policy=max-bundle audiotestsrc ! "
        "audioconvert ! opusenc ! rtpopuspay ! "RTP_CAPS_AUDIO" ! webrtcbin",&error);
    pipe->webrtcbin =  gst_bin_get_by_name(GST_BIN(pipe->pipeline),"webrtcbin");
    g_object_set(pipe->webrtcbin, "latency", 0, NULL);

    #ifdef DEFAULT_TURN
    g_object_set(pipe->webrtcbin,"ice-transport-policy",1,NULL);
    #endif

    setup_pipeline_queue(pipe);

    /* Incoming streams will be exposed via this signal */
    g_signal_connect(pipe->webrtcbin, "pad-added",
        G_CALLBACK (on_incoming_stream),core);

    GstStateChangeReturn result = gst_element_change_state(pipe->pipeline, GST_STATE_READY);
    if(result == GST_STATE_CHANGE_FAILURE)
    {
        g_print("remote app fail to start, aborting ...\n");
        remote_app_finalize(core,NULL);
    }
    connect_signalling_handler(core);
    connect_data_channel_signals(core);
    start_pipeline(core);
}







GstElement*
pipeline_get_webrtc_bin(Pipeline* pipe)
{
    return pipe->webrtcbin;
}

GstElement*
pipeline_get_pipline(Pipeline* pipe)
{
    return pipe->pipeline;
}



GstElement*         
pipeline_get_pipeline_element(Pipeline* pipeline)
{
    return pipeline->pipeline;
}