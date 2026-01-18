#pragma once

#include "Core/EnumBitmaskOperators.h"
#include "Core/StringTypes.h"
#include "Core/Types.h"
#include "Data/Font.h"

#define MAX_UI_NODES 2048
#define MAX_UI_DRAW_INSTANCES 16384

typedef u32 UINodeID;

enum struct UINodeFlags : u32 {
	None = 0,
	Clickable = 1 << 0,
	DrawText = 1 << 1,
	DrawBackground = 1 << 2,
	DrawBorder = 1 << 3,
	HotAnimation = 1 << 4,
	ActiveAnimation = 1 << 5,

	DebugDrawBounds = 1u << 31
}; SR_ENABLE_BITMASK_OPERATORS(UINodeFlags);

enum struct UISizeType : u32 {
	None,
	Pixels,
	TextContent,
	PercentOfParent,
	ChildSum
};

enum UIAxis : u32 {
	UIAxis_X,
	UIAxis_Y,
	UIAxis_COUNT
};

struct UISize {
	UISizeType type;
	f32 value;
};

struct UINode {
	UINode* first_child;
	UINode* last_child;
	UINode* next_sibling;
	UINode* prev_sibling;
	UINode* parent;

	UINodeID id;
	UINodeFlags flags;
	Str8 str;
	UISize semantic_size[UIAxis_COUNT];
	UIAxis child_layout_axis;
	f32 padding;

	f32 computed_pos_rel[UIAxis_COUNT];
	f32 computed_size[UIAxis_COUNT];
};

// TODO: Improve font handling, right now we only use one font at a time
// TODO: Transition to exclusively use Arenas
struct UIContext;
UIContext* UI_CreateContext(const SRFont* font);
void       UI_DestroyContext(UIContext* ctx = nullptr);
void       UI_BeginFrame(f32 viewport_width, f32 viewport_height);
void       UI_EndFrame();

UINode*    UI_GetRootNode();
UINode*    UI_BeginRow(Str8 str, UISize size_x, UISize size_y);
void       UI_EndRow();
UINode*    UI_BeginCol(Str8 str, UISize size_x, UISize size_y);
void       UI_EndCol();
UINode*    UI_Button(Str8 str);

void       UI_PushParent(UINode* node);
void       UI_PushSemanticWidth(UISize size);
void       UI_PushSemanticHeight(UISize size);

void       UI_PopParent();
void       UI_PopSemanticWidth();
void       UI_PopSemanticHeight();
