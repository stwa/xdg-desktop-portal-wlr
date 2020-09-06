#ifndef WLR_SCREENCAST_SCP_SHM_H
#define WLR_SCREENCAST_SCP_SHM_H

#include "screencast_common.h"

void xdpw_wlr_frame_free_scp_shm(struct xdpw_screencast_instance *cast);
void xdpw_wlr_register_cb_scp_shm(struct xdpw_screencast_instance *cast);

#endif
