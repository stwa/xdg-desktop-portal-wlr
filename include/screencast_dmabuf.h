#ifndef SCREENCAST_DMABUF_H
#define SCREENCAST_DMABUF_H

#include <wayland-client-protocol.h>

struct xdpw_frame_dmabuf {
	uint32_t offset_x;
	uint32_t offset_y;
	uint32_t buffer_flags;
	uint32_t flags;
	uint32_t mod_high;
	uint32_t mod_low;
	uint32_t num_objects;
	uint32_t format;
};

#endif /* SCREENCAST_DMABUF_H */
