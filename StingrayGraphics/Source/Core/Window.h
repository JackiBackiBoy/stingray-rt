#pragma once

#include "Core/Types.h"

typedef int SRWindowFlags;

enum SRWindowFlags_ {
	SRWindowFlags_None = 0,
	SRWindowFlags_Centered = 1 << 0,
	SRWindowFlags_SizeIsClientArea = 1 << 1
};
struct SRWindow;

SRWindow* SRWindow_Create(const char* name, u32 width, u32 height, SRWindowFlags flags);
void      SRWindow_Destroy(SRWindow* window);
bool      SRWindow_PollEvents(SRWindow* window);
void      SRWindow_Show(SRWindow* window);
void*     SRWindow_GetInternalHandle(SRWindow* window);
void      SRWindow_GetClientSize(SRWindow* window, u32* width, u32* height);
f32       SRWindow_GetClientAspectRatio(SRWindow* window);
