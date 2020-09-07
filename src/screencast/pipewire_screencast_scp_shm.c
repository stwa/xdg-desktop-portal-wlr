#include "pipewire_screencast.h"
#include "pipewire_screencast_scp_shm.h"

#include <pipewire/pipewire.h>
#include <spa/utils/result.h>
#include <spa/param/props.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/format-utils.h>

#include "screencast_common.h"
#include "spa/pod/builder.h"
#include "wlr_screencast.h"
#include "xdpw.h"
#include "logger.h"

static void writeFrameData(void *pwFramePointer, void *wlrFramePointer,
		uint32_t height, uint32_t stride, bool inverted) {
	if (!inverted) {
		memcpy(pwFramePointer, wlrFramePointer, height * stride);
		return;
	}

	for (size_t i = 0; i < (size_t)height; ++i) {
		void *flippedWlrRowPointer = wlrFramePointer + ((height - i - 1) * stride);
		void *pwRowPointer = pwFramePointer + (i * stride);
		memcpy(pwRowPointer, flippedWlrRowPointer, stride);
	}

	return;
}

void pwr_copydata_scp_shm(struct xdpw_screencast_instance *cast, struct spa_data *d) {
	d[0].type = SPA_DATA_MemPtr;
	d[0].maxsize = cast->simple_frame.scp_shm.size;
	d[0].mapoffset = 0;
	d[0].chunk->size = cast->simple_frame.scp_shm.size;
	d[0].chunk->stride = cast->simple_frame.scp_shm.stride;
	d[0].chunk->offset = 0;
	d[0].flags = 0;
	d[0].fd = -1;

	writeFrameData(d[0].data, cast->simple_frame.scp_shm.data, cast->simple_frame.scp_shm.height,
		cast->simple_frame.scp_shm.stride, cast->simple_frame.scp_shm.y_invert);

	logprint(TRACE, "pipewire: pointer %p", d[0].data);
	logprint(TRACE, "pipewire: size %d", d[0].maxsize);
	logprint(TRACE, "pipewire: stride %d", d[0].chunk->stride);
	logprint(TRACE, "pipewire: width %d", cast->simple_frame.scp_shm.width);
	logprint(TRACE, "pipewire: height %d", cast->simple_frame.scp_shm.height);
	logprint(TRACE, "pipewire: y_invert %d", cast->simple_frame.scp_shm.y_invert);
}
