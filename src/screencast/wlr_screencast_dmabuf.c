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

static void wlr_dmabuf_frame_frame(void *data,
		struct zwlr_export_dmabuf_frame_v1 *dmabuf_frame, uint32_t width,
		uint32_t height, uint32_t offset_x, uint32_t offset_y,
		uint32_t buffer_flags, uint32_t flags, uint32_t format,
		uint32_t mod_high, uint32_t mod_low, uint32_t num_objects) {
	struct xdpw_screencast_instance *cast = data;

	wlr_frame_buffer_chparam(cast, 0, width, height, stride);
	cast->simple_frame.offset_x = offset_x;
	cast->simple_frame.offset_y = offset_y;
	cast->simple_frame.buffer_flags = buffer_flags;
	cast->simple_frame.flags = flags;
	cast->simple_frame.dmabuf_format = format;
	cast->simple_frame.mod_high = mod_high;
	cast->simple_frame.mod_low = mod_low;
	cast->simple_frame.num_objects = num_objects;
}

static void wlr_dmabuf_frame_object(void *data,
		struct zwlr_export_dmabuf_frame_v1 *dmabuf_frame, uint32_t index,
		int32_t fd, uint32_t size, uint32_t offset, uint32_t stride,
		uint32_t plane_index) {
	struct xdpw_screencast_instance *cast = data;
}

static void wlr_dmabuf_frame_ready(void *data,
		struct zwlr_export_dmabuf_frame_v1 *dmabuf_frame, uint32_t tv_sec_hi,
		uint32_t tv_sec_lo, uint32_t tv_nsec) {
	struct xdpw_screencast_instance *cast = data;
}

static void wlr_dmabuf_frame_cancel(void *data,
		struct zwlr_export_dmabuf_frame_v1 *dmabuf_frame, uint32_t reason) {
	struct xdpw_screencast_instance *cast = data;
}

static const struct zwlr_export_dmabuf_frame_v1_listener wlr_dmabuf_frame_listener = {
	.frame = wlr_dmabuf_frame_frame,
	.object = wlr_dmabuf_frame_object,
	.ready = wlr_dmabuf_frame_ready,
	.cancel = wlr_dmabuf_frame_cancel,
};
