#ifndef WLR_SCREENCAST_COMMON_H
#define WLR_SCREENCAST_COMMON_H

#include <screencast_common.h>

#include <gbm.h>

struct wl_buffer *wlr_create_shm_buffer(struct xdpw_screencast_instance *cast,
		enum wl_shm_format fmt, int width, int height, int stride,
		void **data_out);

struct gbm_device *create_gbm_device();
void destroy_gbm_device(struct gbm_device *gbm);

#endif /* !WLR_SCREENCAST_COMMON_H */
