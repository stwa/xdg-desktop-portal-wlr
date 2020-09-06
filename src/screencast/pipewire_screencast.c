#include "pipewire_screencast.h"
#include "pipewire_screencast_scp_shm.h"

#include <pipewire/pipewire.h>
#include <spa/utils/result.h>
#include <spa/param/props.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/format-utils.h>

#include "wlr_screencast.h"
#include "xdpw.h"
#include "logger.h"

static void pwr_on_event(void *data, uint64_t expirations) {
	struct xdpw_screencast_instance *cast = data;
	struct pw_buffer *pw_buf;
	struct spa_buffer *spa_buf;
	struct spa_meta_header *h;
	struct spa_data *d;

	logprint(TRACE, "********************");
	logprint(TRACE, "pipewire: event fired");

	if ((pw_buf = pw_stream_dequeue_buffer(cast->stream)) == NULL) {
		logprint(WARN, "pipewire: out of buffers");
		return;
	}

	spa_buf = pw_buf->buffer;
	d = spa_buf->datas;
	if ((d[0].data) == NULL) {
		logprint(TRACE, "pipewire: data pointer undefined");
		return;
	}
	if ((h = spa_buffer_find_meta_data(spa_buf, SPA_META_Header, sizeof(*h)))) {
		h->pts = -1;
		h->flags = 0;
		h->seq = cast->seq++;
		h->dts_offset = 0;
	}

	switch (cast->type) {
		case XDPW_INSTANCE_SCP_SHM:
			pwr_copydata_scp_shm(cast, d);
			break;
		default:
			abort();
	}
	logprint(TRACE, "********************");

	pw_stream_queue_buffer(cast->stream, pw_buf);

	xdpw_wlr_frame_free(cast);
}

static void pwr_handle_stream_state_changed(void *data,
		enum pw_stream_state old, enum pw_stream_state state, const char *error) {
	struct xdpw_screencast_instance *cast = data;
	cast->node_id = pw_stream_get_node_id(cast->stream);

	logprint(INFO, "pipewire: stream state changed to \"%s\"",
		pw_stream_state_as_string(state));
	logprint(INFO, "pipewire: node id is %d", cast->node_id);

	switch (state) {
	case PW_STREAM_STATE_STREAMING:
		cast->pwr_stream_state = true;
		break;
	default:
		cast->pwr_stream_state = false;
		break;
	}
}

static void pwr_handle_stream_param_changed(void *data, uint32_t id,
		const struct spa_pod *param) {
	struct xdpw_screencast_instance *cast = data;
	struct pw_stream *stream = cast->stream;
	uint8_t params_buffer[1024];
	struct spa_pod_builder b =
		SPA_POD_BUILDER_INIT(params_buffer, sizeof(params_buffer));
	const struct spa_pod *params[2];

	if (!param || id != SPA_PARAM_Format) {
		return;
	}

	spa_format_video_raw_parse(param, &cast->pwr_format);

	switch (cast->type) {
		case XDPW_INSTANCE_SCP_SHM:
			pwr_param_buffer_scp_shm(params[0], &b, cast);
			pwr_param_meta_scp_shm(params[1], &b, cast);
			break;
		default:
			abort();
	}

	pw_stream_update_params(stream, params, 2);
}

static const struct pw_stream_events pwr_stream_events = {
	PW_VERSION_STREAM_EVENTS,
	.state_changed = pwr_handle_stream_state_changed,
	.param_changed = pwr_handle_stream_param_changed,
};

void xdpw_pwr_stream_init(struct xdpw_screencast_instance *cast) {
	struct xdpw_screencast_context *ctx = cast->ctx;
	struct xdpw_state *state = ctx->state;

	pw_loop_enter(state->pw_loop);

	uint8_t buffer[1024];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

	char name[] = "xdpw-stream-XXXXXX";
	randname(name + strlen(name) - 6);
	cast->stream = pw_stream_new(ctx->core, name,
		pw_properties_new(
			PW_KEY_MEDIA_CLASS, "Video/Source",
			NULL));

	if (!cast->stream) {
		logprint(ERROR, "pipewire: failed to create stream");
		abort();
	}
	cast->pwr_stream_state = false;

	/* make an event to signal frame ready */
	cast->event =
		pw_loop_add_event(state->pw_loop, pwr_on_event, cast);
	logprint(DEBUG, "pipewire: registered event %p", cast->event);

	const struct spa_pod *param;
	switch (cast->type) {
		case XDPW_INSTANCE_SCP_SHM:
			pwr_param_format_scp_shm(param, &b, cast);
			break;
		default:
			abort();
	}

	pw_stream_add_listener(cast->stream, &cast->stream_listener,
		&pwr_stream_events, cast);

	pw_stream_connect(cast->stream,
		PW_DIRECTION_OUTPUT,
		PW_ID_ANY,
		(PW_STREAM_FLAG_DRIVER |
			PW_STREAM_FLAG_MAP_BUFFERS),
		&param, 1);

}

int xdpw_pwr_core_connect(struct xdpw_state *state) {
	struct xdpw_screencast_context *ctx = &state->screencast;

	logprint(DEBUG, "pipewire: establishing connection to core");

	if (!ctx->pwr_context) {
		ctx->pwr_context = pw_context_new(state->pw_loop, NULL, 0);
		if (!ctx->pwr_context) {
			logprint(ERROR, "pipewire: failed to create context");
			return -1;
		}
	}

	if (!ctx->core) {
		ctx->core = pw_context_connect(ctx->pwr_context, NULL, 0);
		if (!ctx->core) {
			logprint(ERROR, "pipewire: couldn't connect to context");
			return -1;
		}
	}
	return 0;
}

void xdpw_pwr_stream_destroy(struct xdpw_screencast_instance *cast) {
	logprint(DEBUG, "pipewire: destroying stream");
	pw_stream_flush(cast->stream, false);
	pw_stream_disconnect(cast->stream);
	pw_stream_destroy(cast->stream);
	cast->stream = NULL;
}
