/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_SHADOW_SERVER_WIN_RDP_H
#define FREERDP_SHADOW_SERVER_WIN_RDP_H

typedef struct shw_context shwContext;

#include "win_shadow.h"

struct shw_context
{
	rdpContext context;
	DEFINE_RDP_CLIENT_COMMON();

	HANDLE StopEvent;
	freerdp* instance;
	rdpSettings* settings;
};

#ifdef __cplusplus
extern "C" {
#endif

int win_shadow_rdp_init(winShadowSubsystem* subsystem);
int win_shadow_rdp_uninit(winShadowSubsystem* subsystem);

int win_shadow_rdp_start(winShadowSubsystem* subsystem);
int win_shadow_rdp_stop(winShadowSubsystem* subsystem);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SHADOW_SERVER_WIN_RDP_H */
