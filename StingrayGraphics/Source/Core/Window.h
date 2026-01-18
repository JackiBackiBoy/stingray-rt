#pragma once

#include "Core/Types.h"
#include "Core/StringTypes.h"
#include "Data/ArenaAllocator.h"

struct SRWindow;

typedef int SRWindowFlags;
typedef void (*SRWindow_OnResizeCallback)(SRWindow* window, u32 new_width, u32 new_height);

enum SRWindowFlags_ {
	SRWindowFlags_None = 0,
	SRWindowFlags_Centered = 1 << 0,
	SRWindowFlags_SizeIsClientArea = 1 << 1
};

SRWindow* SRWindow_Create(SRArena* arena, Str8 name, u32 width, u32 height, SRWindowFlags flags);
void      SRWindow_Destroy(SRWindow* window);
bool      SRWindow_PollEvents(SRWindow* window);
void      SRWindow_Show(SRWindow* window);
void*     SRWindow_GetInternalHandle(SRWindow* window);
void      SRWindow_GetClientSize(SRWindow* window, u32* width, u32* height);
f32       SRWindow_GetClientAspectRatio(SRWindow* window);

void      SRWindow_SetOnResizeCallback(SRWindow* window, SRWindow_OnResizeCallback callback);
