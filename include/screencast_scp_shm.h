#ifndef SCREENCAST_SCP_H
#define SCREENCAST_SCP_H

#include <spa/param/video/format-utils.h> // required for datatype bool
#include <wayland-client-protocol.h> // required for datatypes

struct xdpw_frame_scp_shm_damage {
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
};

struct xdpw_frame_scp_shm {
	uint32_t width;
	uint32_t height;
	uint32_t size;
	uint32_t stride;
	bool y_invert;
	uint64_t tv_sec;
	uint32_t tv_nsec;
	enum wl_shm_format format;
	struct xdpw_frame_scp_shm_damage damage;
	struct wl_buffer *buffer;
	void *data;
};



#endif /* !SCREENCAST_SCP_H */
