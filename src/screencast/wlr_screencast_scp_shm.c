#include "wlr_screencast.h"

#include "wlr-screencopy-unstable-v1-client-protocol.h"
#include "wlr-export-dmabuf-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-client-protocol.h>

#include "screencast.h"
#include "pipewire_screencast.h"
#include "xdpw.h"
#include "logger.h"

static void wlr_frame_buffer_destroy(struct xdpw_screencast_instance *cast) {
	munmap(cast->simple_frame.scp_shm.data, cast->simple_frame.scp_shm.size);
	cast->simple_frame.scp_shm.data = NULL;
	// wl_buffer_destroy won't work on NULL
	if (cast->simple_frame.scp_shm.buffer != NULL) {
		wl_buffer_destroy(cast->simple_frame.scp_shm.buffer);
		cast->simple_frame.scp_shm.buffer = NULL;
	}
}

void xdpw_wlr_frame_free_scp_shm(struct xdpw_screencast_instance *cast) {
	zwlr_screencopy_frame_v1_destroy(cast->wlr_frame);
	cast->wlr_frame = NULL;
	if (cast->quit || cast->err) {
		wlr_frame_buffer_destroy(cast);
		logprint(TRACE, "xdpw: simple_frame buffer destroyed");
	}
	logprint(TRACE, "wlroots: frame destroyed");
}

static int anonymous_shm_open(void) {
	char name[] = "/xdpw-shm-XXXXXX";
	int retries = 100;

	do {
		randname(name + strlen(name) - 6);

		--retries;
		// shm_open guarantees that O_CLOEXEC is set
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);

	return -1;
}

static struct wl_buffer *create_shm_buffer(struct xdpw_screencast_instance *cast,
		enum wl_shm_format fmt, int width, int height, int stride,
		void **data_out) {
	struct xdpw_screencast_context *ctx = cast->ctx;
	int size = stride * height;

	int fd = anonymous_shm_open();
	if (fd < 0) {
		logprint(ERROR, "wlroots: shm_open failed");
		return NULL;
	}

	int ret;
	while ((ret = ftruncate(fd, size)) == EINTR);

	if (ret < 0) {
		close(fd);
		logprint(ERROR, "wlroots: ftruncate failed");
		return NULL;
	}

	void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		logprint(ERROR, "wlroots: mmap failed: %m");
		close(fd);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(ctx->shm, fd, size);
	close(fd);
	struct wl_buffer *buffer =
		wl_shm_pool_create_buffer(pool, 0, width, height, stride, fmt);
	wl_shm_pool_destroy(pool);

	*data_out = data;
	return buffer;
}

static void wlr_frame_buffer_chparam(struct xdpw_screencast_instance *cast,
		uint32_t format, uint32_t width, uint32_t height, uint32_t stride) {

	logprint(DEBUG, "wlroots: reset buffer");
	cast->simple_frame.scp_shm.width = width;
	cast->simple_frame.scp_shm.height = height;
	cast->simple_frame.scp_shm.stride = stride;
	cast->simple_frame.scp_shm.size = stride * height;
	cast->simple_frame.scp_shm.format = format;
	wlr_frame_buffer_destroy(cast);
}

static void wlr_frame_buffer(void *data, struct zwlr_screencopy_frame_v1 *frame,
		uint32_t format, uint32_t width, uint32_t height, uint32_t stride) {
	struct xdpw_screencast_instance *cast = data;

	logprint(TRACE, "wlroots: buffer event handler");
	cast->wlr_frame = frame;
	if (cast->simple_frame.scp_shm.width != width || cast->simple_frame.scp_shm.height != height || cast->simple_frame.scp_shm.stride != stride || cast->simple_frame.scp_shm.format != format) {
		logprint(TRACE, "wlroots: buffer properties changed");
		wlr_frame_buffer_chparam(cast, format, width, height, stride);
	}

	if (cast->simple_frame.scp_shm.buffer == NULL) {
		logprint(DEBUG, "wlroots: create shm buffer");
		cast->simple_frame.scp_shm.buffer = create_shm_buffer(cast, format, width, height,
			stride, &cast->simple_frame.scp_shm.data);
	} else {
		logprint(TRACE,"wlroots: shm buffer exists");
	}

	if (cast->simple_frame.scp_shm.buffer == NULL) {
		logprint(ERROR, "wlroots: failed to create buffer");
		abort();
	}

	zwlr_screencopy_frame_v1_copy_with_damage(frame, cast->simple_frame.scp_shm.buffer);
	logprint(TRACE, "wlroots: frame copied");
}

static void wlr_frame_flags(void *data, struct zwlr_screencopy_frame_v1 *frame,
		uint32_t flags) {
	struct xdpw_screencast_instance *cast = data;

	logprint(TRACE, "wlroots: flags event handler");
	cast->simple_frame.scp_shm.y_invert = flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT;
}

static void wlr_frame_ready(void *data, struct zwlr_screencopy_frame_v1 *frame,
		uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) {
	struct xdpw_screencast_instance *cast = data;

	logprint(TRACE, "wlroots: ready event handler");

	cast->simple_frame.scp_shm.tv_sec = ((((uint64_t)tv_sec_hi) << 32) | tv_sec_lo);
	cast->simple_frame.scp_shm.tv_nsec = tv_nsec;

	if (!cast->quit && !cast->err && cast->pwr_stream_state) {
		pw_loop_signal_event(cast->ctx->state->pw_loop, cast->event);
		return ;
	}

	xdpw_wlr_frame_free(cast);
}

static void wlr_frame_failed(void *data,
		struct zwlr_screencopy_frame_v1 *frame) {
	struct xdpw_screencast_instance *cast = data;

	logprint(TRACE, "wlroots: failed event handler");
	cast->err = true;

	xdpw_wlr_frame_free(cast);
}

static void wlr_frame_damage(void *data, struct zwlr_screencopy_frame_v1 *frame,
		uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
	struct xdpw_screencast_instance *cast = data;

	logprint(TRACE, "wlroots: damage event handler");

	cast->simple_frame.scp_shm.damage.x = x;
	cast->simple_frame.scp_shm.damage.y = y;
	cast->simple_frame.scp_shm.damage.width = width;
	cast->simple_frame.scp_shm.damage.height = height;
}

static const struct zwlr_screencopy_frame_v1_listener wlr_frame_listener = {
	.buffer = wlr_frame_buffer,
	.flags = wlr_frame_flags,
	.ready = wlr_frame_ready,
	.failed = wlr_frame_failed,
	.damage = wlr_frame_damage,
};

void xdpw_wlr_register_cb_scp_shm(struct xdpw_screencast_instance *cast) {
	cast->frame_callback = zwlr_screencopy_manager_v1_capture_output(
		cast->ctx->screencopy_manager, cast->with_cursor, cast->target_output->output);

	zwlr_screencopy_frame_v1_add_listener(cast->frame_callback,
		&wlr_frame_listener, cast);
}
