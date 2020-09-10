#ifndef SCREENCAST_SCP_DMABUF_H
#define SCREENCAST_SCP_DMABUF_H

#include <spa/param/video/format-utils.h> // required for datatype bool
#include <stdint.h>
#include <wayland-client-protocol.h> // required for datatypes
#include <gbm.h>

struct xdpw_frame_scp_dmabuf_damage {
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
};

struct xdpw_frame_scp_dmabuf {
	uint32_t width;
	uint32_t height;
	uint32_t size;
	uint32_t stride;
	bool y_invert;
	uint64_t tv_sec;
	uint32_t tv_nsec;
	uint32_t fourcc;
	struct xdpw_frame_scp_dmabuf_damage damage;
	struct wl_buffer *buffer;
	void *data;
	struct gbm_bo *bo;
	void *bo_map_handle;
};

enum spa_video_format xdpw_format_pw_from_linux_dmabuf(
	struct xdpw_frame_scp_dmabuf *frame);

#endif /* !SCREENCAST_SCP_DMABUF_H */
