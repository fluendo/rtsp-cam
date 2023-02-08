#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <gst/gst.h>
#include <gst/app/app.h>

#define NUM_BUFFERS 50

#ifndef OUTDIR
#define OUTDIR "out"
#endif

#ifndef CONSUMMER_START_AT_BUFFER
#define CONSUMMER_START_AT_BUFFER (NUM_BUFFERS/2)
#endif

#ifndef NVVIDCONV_OUTPUT_BUFFERS
#define NVVIDCONV_OUTPUT_BUFFERS 4
#endif
//#define USE_NVVIDCONV

#define STR(x) #x
#define XSTR(x) STR(x)

static GstPadProbeReturn probe_cb(GstPad *pad, GstPadProbeInfo *info, void *udata);
static gboolean bus_msg_cb(GstBus *bus, GstMessage *msg, void *udata);

static GstElement *create_pipeline(const char *pipeline_str);
static void link_pipelines(
	GstElement *producer_pipeline,
	GstElement *consumer_pipeline,
	GstPadProbeCallback probe_cb
);

int main(int argc, char *argv[])
{
	GstElement *producer_pipeline;
	GstElement *consumer_pipeline;
	GMainLoop *main_loop;
	GstBus *bus;

	gst_init(&argc, &argv);

	producer_pipeline = create_pipeline(
		"videotestsrc"
			" is-live=true"
			" pattern=ball"
			" num-buffers=" XSTR(NUM_BUFFERS)
		" ! video/x-raw,width=1024,height=768,framerate=30/1"
		" ! identity name=prod_identity"
		" ! timeoverlay halignment=left"
#ifdef USE_NVVIDCONV
		" ! nvvidconv"
			" output-buffers=" XSTR(NVVIDCONV_OUTPUT_BUFFERS)
		" ! video/x-raw(memory:NVMM)"
#endif
		" ! fakesink"
			" name=sink"
			" enable-last-sample=true"
			" sync=true"
	);

	consumer_pipeline = create_pipeline(
		"appsrc"
			" name=src"
			" is-live=true"
			" max-bytes=0"
			" max-buffers=2"
			" leaky-type=downstream"
			" block=true"
		" ! identity name=cons_identity sleep-time=5000"
#ifdef USE_NVVIDCONV
		" ! nvvidconv"
#else
		" ! videoconvert"
#endif
		" ! timeoverlay halignment=right"
		" ! pngenc"
		" ! multifilesink"
			" location=" OUTDIR "/frame_%04d.png"
	);

	link_pipelines(producer_pipeline, consumer_pipeline, probe_cb);
	bus = gst_element_get_bus(consumer_pipeline);

	gst_element_set_state(producer_pipeline, GST_STATE_PLAYING);
	// NOTE:
	// If the consumer pipeline is started here, the bug is not happening.
	// We going to start it at the middle of the streaming instead.
	// gst_element_set_state(consumer_pipeline, GST_STATE_PLAYING);
	
	main_loop = g_main_loop_new(NULL, false);
	gst_bus_add_watch(bus, bus_msg_cb, main_loop);
	g_main_loop_run(main_loop);

	gst_element_set_state(producer_pipeline, GST_STATE_NULL);
	gst_element_set_state(consumer_pipeline, GST_STATE_NULL);

	gst_object_unref(producer_pipeline);
	gst_object_unref(consumer_pipeline);
	gst_object_unref(bus);
	g_main_loop_unref(main_loop);

	return EXIT_SUCCESS;
}

static GstPadProbeReturn probe_cb(GstPad *pad, GstPadProbeInfo *info, void *udata)
{
	GstBin *consumer_pipeline = GST_BIN(udata);
	bool is_buffer = info->type & GST_PAD_PROBE_TYPE_BUFFER;
	bool is_event = info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM;

	GstAppSrc *consumer_src;
	GstBuffer *buffer;
	GstCaps *caps;
	(void) pad;

	consumer_src = GST_APP_SRC(gst_bin_get_by_name(consumer_pipeline, "src"));
	assert(consumer_src);

	if (is_buffer) {
		buffer = gst_buffer_ref(info->data);
		gst_app_src_push_buffer(consumer_src, buffer);

		static int i = 0;
		printf("\r%04d/%04d", i++, NUM_BUFFERS - 1);
		fflush(stdout);

		// NOTE:
		// Start the consumer pipeline at the middle of the streaming
		if (i == CONSUMMER_START_AT_BUFFER)
			gst_element_set_state(GST_ELEMENT(consumer_pipeline),
				GST_STATE_PLAYING);
	} else if (is_event) {
		switch (GST_EVENT(info->data)->type) {
			case GST_EVENT_CAPS:
				gst_event_parse_caps(info->data, &caps);
				g_object_set(consumer_src, "caps", caps, NULL);
				break;
			case GST_EVENT_EOS:
				gst_app_src_end_of_stream(consumer_src);
				break;
			default:
				break;
		}
	}

	gst_object_unref(consumer_src);
	return GST_PAD_PROBE_OK;
}

static gboolean bus_msg_cb(GstBus *bus, GstMessage *msg, void *udata)
{
	GMainLoop *main_loop = (GMainLoop *)udata;
	GError *error;
	(void) bus;

	switch (GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_ERROR:
			gst_message_parse_error(msg, &error, NULL);
			fprintf(stderr, "Error from %s: %s\n",
				GST_OBJECT_NAME(msg->src),
				error->message
			);
			g_clear_error(&error);
			g_main_loop_quit(main_loop);
			break;
		case GST_MESSAGE_EOS:
			printf("EOS\n");
			g_main_loop_quit(main_loop);
			break;
		default:
			break;
	}

	return true;
}

static GstElement *create_pipeline(const char *pipeline_str)
{
	GstElement *pipeline;
	GError *error = NULL;

	pipeline = gst_parse_launch(pipeline_str, &error);
	if (error) {
		fputs(error->message, stderr);
		fputc('\n', stderr);
		exit(EXIT_FAILURE);
	}
	assert(pipeline);

	return pipeline;
}

static void link_pipelines(
	GstElement *producer_pipeline,
	GstElement *consumer_pipeline,
	GstPadProbeCallback probe_cb
) {
	GstElement *producer_sink;
	GstPad *producer_sink_pad;

	producer_sink = gst_bin_get_by_name(GST_BIN(producer_pipeline), "sink");
	assert(producer_sink);

        producer_sink_pad = gst_element_get_static_pad(producer_sink, "sink");
	assert(producer_sink_pad);

	gst_pad_add_probe(
		producer_sink_pad,
		GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
		probe_cb,
		consumer_pipeline,
		NULL
	);

	gst_object_unref(producer_sink_pad);
	gst_object_unref(producer_sink);
}
