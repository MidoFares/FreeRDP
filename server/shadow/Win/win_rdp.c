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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/addin.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/channels/channels.h>

#include "win_rdp.h"

void shw_OnChannelConnectedEventHandler(rdpContext* context, ChannelConnectedEventArgs* e)
{
	shwContext* shw = (shwContext*) context;

	printf("OnChannelConnected: %s\n", e->name);
}

void shw_OnChannelDisconnectedEventHandler(rdpContext* context, ChannelDisconnectedEventArgs* e)
{
	shwContext* shw = (shwContext*) context;

	printf("OnChannelDisconnected: %s\n", e->name);
}

void shw_begin_paint(rdpContext* context)
{
	shwContext* shw;
	rdpGdi* gdi = context->gdi;

	shw = (shwContext*) context;

	gdi->primary->hdc->hwnd->invalid->null = 1;
	gdi->primary->hdc->hwnd->ninvalid = 0;
}

void shw_end_paint(rdpContext* context)
{
	INT32 x, y;
	UINT32 w, h;
	shwContext* shw;
	HGDI_RGN invalid;
	HGDI_RGN cinvalid;
	rdpSettings* settings;
	rdpGdi* gdi = context->gdi;

	shw = (shwContext*) context;
	settings = context->settings;

	invalid = gdi->primary->hdc->hwnd->invalid;
	cinvalid = gdi->primary->hdc->hwnd->cinvalid;

	x = invalid->x;
	y = invalid->y;
	w = invalid->w;
	h = invalid->h;

	if (x < 0)
		x = 0;

	if (x > settings->DesktopWidth - 1)
		x = settings->DesktopWidth - 1;

	w += x % 16;
	x -= x % 16;

	w += w % 16;

	if (x + w > settings->DesktopWidth)
		w = settings->DesktopWidth - x;

	if (y < 0)
		y = 0;

	if (y > settings->DesktopHeight - 1)
		y = settings->DesktopHeight - 1;

	h += y % 16;
	y -= y % 16;

	h += h % 16;

	if (h > settings->DesktopHeight)
		h = settings->DesktopHeight;

	if (y + h > settings->DesktopHeight)
		h = settings->DesktopHeight - y;

	if (w * h < 1)
		return;
}

void shw_desktop_resize(rdpContext* context)
{

}

void shw_surface_frame_marker(rdpContext* context, SURFACE_FRAME_MARKER* surfaceFrameMarker)
{
	shwContext* shw = (shwContext*) context;
}

void shw_OnConnectionResultEventHandler(rdpContext* context, ConnectionResultEventArgs* e)
{
	shwContext* shw = (shwContext*) context;
	printf("OnConnectionResult: %d\n", e->result);
}

BOOL shw_pre_connect(freerdp* instance)
{
	shwContext* shw;
	rdpContext* context = instance->context;

	shw = (shwContext*) context;

	PubSub_SubscribeConnectionResult(context->pubSub,
			(pConnectionResultEventHandler) shw_OnConnectionResultEventHandler);

	PubSub_SubscribeChannelConnected(context->pubSub,
			(pChannelConnectedEventHandler) shw_OnChannelConnectedEventHandler);

	PubSub_SubscribeChannelDisconnected(context->pubSub,
			(pChannelDisconnectedEventHandler) shw_OnChannelDisconnectedEventHandler);

	freerdp_client_load_addins(context->channels, instance->settings);

	freerdp_channels_pre_connect(context->channels, instance);

	return TRUE;
}

BOOL shw_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	shwContext* shw;
	rdpSettings* settings;

	shw = (shwContext*) instance->context;
	settings = instance->settings;

	gdi_init(instance, CLRBUF_32BPP, NULL);
	gdi = instance->context->gdi;

	instance->update->BeginPaint = shw_begin_paint;
	instance->update->EndPaint = shw_end_paint;
	instance->update->DesktopResize = shw_desktop_resize;
	instance->update->SurfaceFrameMarker = shw_surface_frame_marker;

	freerdp_channels_post_connect(instance->context->channels, instance);

	return TRUE;
}

void* shw_client_thread(void* arg)
{
	int index;
	int rcount;
	int wcount;
	BOOL bSuccess;
	void* rfds[32];
	void* wfds[32];
	int fds_count;
	HANDLE fds[64];
	shwContext* shw;
	rdpContext* context;
	rdpChannels* channels;
	freerdp* instance = (freerdp*) arg;

	ZeroMemory(rfds, sizeof(rfds));
	ZeroMemory(wfds, sizeof(wfds));

	context = (rdpContext*) instance->context;
	shw = (shwContext*) context;

	bSuccess = freerdp_connect(instance);

	printf("freerdp_connect: %d\n", bSuccess);

	if (!freerdp_connect(instance))
	{
		ExitThread(0);
		return NULL;
	}

	channels = instance->context->channels;

	while (1)
	{
		rcount = 0;
		wcount = 0;

		if (!freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount))
		{
			fprintf(stderr, "Failed to get FreeRDP file descriptor\n");
			break;
		}

		if (!freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount))
		{
			fprintf(stderr, "Failed to get channels file descriptor\n");
			break;
		}

		fds_count = 0;
		
		for (index = 0; index < rcount; index++)
			fds[fds_count++] = rfds[index];

		for (index = 0; index < wcount; index++)
			fds[fds_count++] = wfds[index];

		if (MsgWaitForMultipleObjects(fds_count, fds, FALSE, 1000, QS_ALLINPUT) == WAIT_FAILED)
		{
			fprintf(stderr, "MsgWaitForMultipleObjects failure: 0x%08X", GetLastError());
			break;
		}

		if (!freerdp_check_fds(instance))
		{
			fprintf(stderr, "Failed to check FreeRDP file descriptor\n");
			break;
		}

		if (freerdp_shall_disconnect(instance))	
		{
			break;
		}

		if (!freerdp_channels_check_fds(channels, instance))
		{
			fprintf(stderr, "Failed to check channels file descriptor\n");
			break;
		}
	}

	freerdp_free(instance);

	ExitThread(0);
	return NULL;
}

/**
 * Client Interface
 */

void shw_freerdp_client_global_init(void)
{

}

void shw_freerdp_client_global_uninit(void)
{

}

int shw_freerdp_client_start(rdpContext* context)
{
	shwContext* shw;
	freerdp* instance = context->instance;

	shw = (shwContext*) context;

	shw->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) shw_client_thread,
			instance, 0, NULL);

	return 0;
}

int shw_freerdp_client_stop(rdpContext* context)
{
	shwContext* shw = (shwContext*) context;

	SetEvent(shw->StopEvent);

	return 0;
}

int shw_freerdp_client_new(freerdp* instance, rdpContext* context)
{
	shwContext* shw;
	rdpSettings* settings;

	shw = (shwContext*) instance->context;

	instance->PreConnect = shw_pre_connect;
	instance->PostConnect = shw_post_connect;

	context->channels = freerdp_channels_new();

	shw->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	settings = instance->settings;
	shw->settings = instance->context->settings;

	settings->SoftwareGdi = TRUE;
	settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_INDEX] = settings->BitmapCacheEnabled;
	settings->OrderSupport[NEG_MEM3BLT_INDEX] = (settings->SoftwareGdi) ? TRUE : FALSE;
	settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = settings->BitmapCacheEnabled;
	settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = (settings->SoftwareGdi) ? FALSE : TRUE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = (settings->SoftwareGdi) ? FALSE : TRUE;
	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;

	settings->AsyncTransport = FALSE;
	settings->AsyncChannels = FALSE;
	settings->AsyncUpdate = FALSE;
	settings->AsyncInput = FALSE;

	settings->IgnoreCertificate = TRUE;

	settings->RdpSecurity = TRUE;
	settings->TlsSecurity = TRUE;
	settings->NlaSecurity = FALSE;

	settings->ColorDepth = 32;

	settings->CompressionEnabled = TRUE;

	settings->AutoReconnectionEnabled = FALSE;

	return 0;
}

void shw_freerdp_client_free(freerdp* instance, rdpContext* context)
{
	shwContext* shw = (shwContext*) instance->context;
}

int shw_RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	pEntryPoints->Version = 1;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);

	pEntryPoints->settings = NULL;

	pEntryPoints->ContextSize = sizeof(shwContext);
	pEntryPoints->GlobalInit = shw_freerdp_client_global_init;
	pEntryPoints->GlobalUninit = shw_freerdp_client_global_uninit;
	pEntryPoints->ClientNew = shw_freerdp_client_new;
	pEntryPoints->ClientFree = shw_freerdp_client_free;
	pEntryPoints->ClientStart = shw_freerdp_client_start;
	pEntryPoints->ClientStop = shw_freerdp_client_stop;

	return 0;
}

int win_shadow_rdp_init(winShadowSubsystem* subsystem)
{
	rdpContext* context;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;

	ZeroMemory(&clientEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;

	shw_RdpClientEntry(&clientEntryPoints);

	context = freerdp_client_context_new(&clientEntryPoints);

	subsystem->shw = (shwContext*) context;
	subsystem->shw->settings = context->settings;

	return 1;
}

int win_shadow_rdp_start(winShadowSubsystem* subsystem)
{
	int status;
	shwContext* shw = subsystem->shw;
	rdpContext* context = (rdpContext*) shw;

	status = freerdp_client_start(context);

	return status;
}

int win_shadow_rdp_stop(winShadowSubsystem* subsystem)
{
	int status;
	shwContext* shw = subsystem->shw;
	rdpContext* context = (rdpContext*) shw;

	status = freerdp_client_stop(context);

	return status;
}

int win_shadow_rdp_uninit(winShadowSubsystem* subsystem)
{
	win_shadow_rdp_stop(subsystem);

	return 1;
}
