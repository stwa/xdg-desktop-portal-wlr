#ifndef PIPEWIRE_SCREENCAST_SCP_SHM_H
#define PIPEWIRE_SCREENCAST_SCP_SHM_H

#include "screencast_common.h"

#include <pipewire/pipewire.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/format-utils.h>


void pwr_copydata_scp_shm(struct xdpw_screencast_instance *cast, struct spa_data *d);

void pwr_param_buffer_scp_shm(const struct spa_pod *param, struct spa_pod_builder *b, struct xdpw_screencast_instance *cast);

void pwr_param_meta_scp_shm(const struct spa_pod *param, struct spa_pod_builder *b, struct xdpw_screencast_instance *cast);

void pwr_param_format_scp_shm(const struct spa_pod *param, struct spa_pod_builder *b, struct xdpw_screencast_instance *cast);

#endif /* !PIPEWIRE_SCREENCAST_SCP_SHM_H */
