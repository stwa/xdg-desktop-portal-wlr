#include "pipewire_screencast.h"

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

void pwr_param_buffer_scp_shm(const struct spa_pod *param, struct spa_pod_builder *b, struct xdpw_screencast_instance *cast) {
	param = spa_pod_builder_add_object(b,
		SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
		SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(BUFFERS, 1, 32),
		SPA_PARAM_BUFFERS_blocks,  SPA_POD_Int(1),
		SPA_PARAM_BUFFERS_size,    SPA_POD_Int(cast->simple_frame.scp_shm.size),
		SPA_PARAM_BUFFERS_stride,  SPA_POD_Int(cast->simple_frame.scp_shm.stride),
		SPA_PARAM_BUFFERS_align,   SPA_POD_Int(ALIGN));
}

void pwr_param_meta_scp_shm(const struct spa_pod *param, struct spa_pod_builder *b, struct xdpw_screencast_instance *cast) {
	param = spa_pod_builder_add_object(b,
		SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
		SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
		SPA_PARAM_META_size, SPA_POD_Int(sizeof(struct spa_meta_header)));
}

void pwr_param_format_scp_shm(const struct spa_pod *param, struct spa_pod_builder *b, struct xdpw_screencast_instance *cast) {
	enum spa_video_format format = xdpw_format_pw(cast);
	enum spa_video_format format_without_alpha =
		xdpw_format_pw_strip_alpha(format);
	uint32_t n_formats = 1;
	if (format_without_alpha != SPA_VIDEO_FORMAT_UNKNOWN) {
		n_formats++;
	}

	param = spa_pod_builder_add_object(b,
		SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
		SPA_FORMAT_mediaType,       SPA_POD_Id(SPA_MEDIA_TYPE_video),
		SPA_FORMAT_mediaSubtype,    SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
		SPA_FORMAT_VIDEO_format,    SPA_POD_CHOICE_ENUM_Id(n_formats + 1,
			format, format, format_without_alpha),
		SPA_FORMAT_VIDEO_size,      SPA_POD_CHOICE_RANGE_Rectangle(
			&SPA_RECTANGLE(cast->simple_frame.scp_shm.width, cast->simple_frame.scp_shm.height),
			&SPA_RECTANGLE(1, 1),
			&SPA_RECTANGLE(4096, 4096)),
		// variable framerate
		SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&SPA_FRACTION(0, 1)),
		SPA_FORMAT_VIDEO_maxFramerate, SPA_POD_CHOICE_RANGE_Fraction(
			&SPA_FRACTION(cast->framerate, 1),
			&SPA_FRACTION(1, 1),
			&SPA_FRACTION(cast->framerate, 1)));
}
