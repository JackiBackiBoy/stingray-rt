#include "UICore.h"
#include "Data/ArenaAllocator.h"
#include "Data/HashMap.h"
#include "Graphics/RenderGraph.h"
#include "Math/MathFunctions.h"

#include <stdlib.h>

struct UIContext {
	const SRFont* font;

	UINode* root_node;
	SRVector<UINode> nodes;
	SRHashMap<UINodeID, u64> id_to_node_index_map;
	UINodeID active_id;
	UINodeID hot_id;

	// Stack
	SRVector<UINode*> stack_parents;
	SRVector<UISize> stack_semantic_widths;
	SRVector<UISize> stack_semantic_heights;
};

global UIContext* g_ctx;

internal u64 Hash_U32(const UINodeID* id) {
	u32 x = *id;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return (u64)x;
}

internal UINodeID UINodeID_FromStr8(Str8 str) {
	UINodeID hash = 2166136261u; // FNV offset basis
	for (u64 i = 0; i < str.size; ++i) {
		hash ^= str.data[i];
		hash *= 16777619u; // FNV prime
	}
	return hash;
}

internal UINode* UI_GetCurrentParent() {
	if (g_ctx->stack_parents.size > 0) {
		return g_ctx->stack_parents[g_ctx->stack_parents.size - 1];
	}

	return nullptr;
}

internal UINode* UINode_CreateOrGet(UINodeFlags flags, Str8 str) {
	UINodeID node_id = UINodeID_FromStr8(str);
	u64* node_index = SRHashMap_Get(&g_ctx->id_to_node_index_map, node_id);
	UINode* node;

	if (node_index == nullptr) {
		SRVector_PushBack(&g_ctx->nodes, {
			.id = node_id,
			.flags = flags,
			.str = str
			});
		SRHashMap_Put(&g_ctx->id_to_node_index_map, node_id, g_ctx->nodes.size - 1);
		node = &g_ctx->nodes.data[g_ctx->nodes.size - 1];
	}
	else {
		node = &g_ctx->nodes.data[*node_index];
	}

	// Reset per-frame state
	node->first_child = nullptr;
	node->last_child = nullptr;
	node->next_sibling = nullptr;
	node->prev_sibling = nullptr;
	node->parent = nullptr;
	node->computed_pos_rel[UIAxis_X] = 0.0f;
	node->computed_pos_rel[UIAxis_Y] = 0.0f;
	node->computed_size[UIAxis_X] = 0.0f;
	node->computed_size[UIAxis_Y] = 0.0f;

	node->parent = UI_GetCurrentParent();
	if (g_ctx->stack_semantic_widths.size > 0) { node->semantic_size[UIAxis_X] = SRVector_GetBack(&g_ctx->stack_semantic_widths); }
	if (g_ctx->stack_semantic_heights.size > 0) { node->semantic_size[UIAxis_Y] = SRVector_GetBack(&g_ctx->stack_semantic_heights); }

	if (node->parent) {
		for (u32 axis = 0; axis < UIAxis_COUNT; ++axis) {
			assert(!(node->parent->semantic_size[axis].type == UISizeType::ChildSum && node->semantic_size[axis].type == UISizeType::PercentOfParent));
		}

		node->prev_sibling = node->parent->last_child;
		if (node->prev_sibling) {
			node->prev_sibling->next_sibling = node;
		}

		if (!node->parent->first_child) {
			node->parent->first_child = node;
		}
		node->parent->last_child = node;
	}

	return node;
}

internal void UI_LayoutPass_ComputeStandaloneSizes(UINode* node) {
	assert(node);

	UISize semantic_size_x = node->semantic_size[UIAxis_X];
	UISize semantic_size_y = node->semantic_size[UIAxis_Y];

	// NOTE: It doesn't matter if we use pre-order or post-order traversal here
	switch (semantic_size_x.type) {
	case UISizeType::Pixels: { node->computed_size[UIAxis_X] = semantic_size_x.value; } break;
	case UISizeType::TextContent: { node->computed_size[UIAxis_X] = SRFont_CalcTextWidth(g_ctx->font, node->str); } break;
	default: break;
	}
	switch (semantic_size_y.type) {
	case UISizeType::Pixels: { node->computed_size[UIAxis_Y] = semantic_size_y.value; } break;
	case UISizeType::TextContent: { node->computed_size[UIAxis_Y] = (f32)g_ctx->font->bbox_ymax; } break;
	default: break;
	}

	UINode* child = node->first_child;
	while (child != nullptr) {
		UI_LayoutPass_ComputeStandaloneSizes(child);
		child = child->next_sibling;
	}
}

internal void UI_LayoutPass_ComputeAncestorDependentSizes(UINode* node) {
	assert(node);

	UISize semantic_size_x = node->semantic_size[UIAxis_X];
	UISize semantic_size_y = node->semantic_size[UIAxis_Y];

	// NOTE: Pre-order traversal required
	switch (semantic_size_x.type) {
	case UISizeType::PercentOfParent: { assert(node->parent); node->computed_size[UIAxis_X] = semantic_size_x.value * node->parent->computed_size[UIAxis_X]; } break;
	default: break;
	}
	switch (semantic_size_y.type) {
	case UISizeType::PercentOfParent: { assert(node->parent); node->computed_size[UIAxis_Y] = semantic_size_y.value * node->parent->computed_size[UIAxis_Y]; } break;
	default: break;
	}

	UINode* child = node->first_child;
	while (child != nullptr) {
		UI_LayoutPass_ComputeAncestorDependentSizes(child);
		child = child->next_sibling;
	}
}

internal void UI_LayoutPass_ComputeDescendantDependentSizes(UINode* node) {
	assert(node);

	UINode* child = node->first_child;
	while (child != nullptr) {
		UI_LayoutPass_ComputeDescendantDependentSizes(child);
		child = child->next_sibling;
	}

	UISize semantic_size_x = node->semantic_size[UIAxis_X];
	UISize semantic_size_y = node->semantic_size[UIAxis_Y];

	// NOTE: Post-order traversal required
	switch (semantic_size_x.type) {
	case UISizeType::ChildSum: {
		UINode* child = node->first_child;
		f32 width_sum = 0.0f;
		f32 width_max = 0.0f;

		while (child != nullptr) {
			width_sum += child->computed_size[UIAxis_X];
			if (child->computed_size[UIAxis_X] > width_max) {
				width_max = child->computed_size[UIAxis_X];
			}

			child = child->next_sibling;
		}

		if (node->child_layout_axis == UIAxis_X) {
			node->computed_size[UIAxis_X] = width_sum;
		}
		else {
			// NOTE: In the case that the node uses a child layout axis that is inconsistent with the semantic size axis
			node->computed_size[UIAxis_X] = width_max;
		}
	} break;
	default: break;
	}

	switch (semantic_size_y.type) {
	case UISizeType::ChildSum: {
		UINode* child = node->first_child;
		f32 height_sum = 0.0f;
		f32 height_max = 0.0f;

		while (child != nullptr) {
			height_sum += child->computed_size[UIAxis_Y];
			if (child->computed_size[UIAxis_Y] > height_max) {
				height_max = child->computed_size[UIAxis_Y];
			}

			child = child->next_sibling;
		}

		if (node->child_layout_axis == UIAxis_Y) {
			node->computed_size[UIAxis_Y] = height_sum;
		}
		else {
			// NOTE: In the case that the node uses a child layout axis that is inconsistent with the semantic size axis
			node->computed_size[UIAxis_Y] = height_max;
		}
	} break;
	default: break;
	}
}

internal void UI_LayoutPass_ComputeFinalScreenCoordinates(UINode* node) {
	assert(node);

	f32 pos_x = node->computed_pos_rel[UIAxis_X];
	f32 pos_y = node->computed_pos_rel[UIAxis_Y];

	UINode* child = node->first_child;
	while (child != nullptr) {
		child->computed_pos_rel[UIAxis_X] = pos_x;
		child->computed_pos_rel[UIAxis_Y] = pos_y;

		if (node->child_layout_axis == UIAxis_X) {
			pos_x += child->computed_size[UIAxis_X];
		}
		else if (node->child_layout_axis == UIAxis_Y) {
			pos_y += child->computed_size[UIAxis_Y];
		}

		UI_LayoutPass_ComputeFinalScreenCoordinates(child);
		child = child->next_sibling;
	}
}

// --- Public API ---------------------------------------------------------------------------------
UIContext* UI_CreateContext(const SRFont* font) {
	UIContext* ctx = (UIContext*)calloc(1, sizeof(UIContext));
	ctx->font = font;
	g_ctx = ctx;

	SRVector_Create(&ctx->nodes, MAX_UI_NODES);
	SRHashMap_Create(&ctx->id_to_node_index_map, Hash_U32);

	SRVector_Create(&ctx->stack_parents);
	SRVector_Create(&ctx->stack_semantic_widths);
	SRVector_Create(&ctx->stack_semantic_heights);

	return ctx;
}

void UI_DestroyContext(UIContext* ctx) {
	if (!ctx) {
		ctx = g_ctx;
	}

	SRVector_Destroy(&ctx->nodes);
	SRHashMap_Destroy(&ctx->id_to_node_index_map);

	SRVector_Destroy(&ctx->stack_parents);
	SRVector_Destroy(&ctx->stack_semantic_widths);
	SRVector_Destroy(&ctx->stack_semantic_heights);

	free(ctx);
}

void UI_BeginFrame(f32 viewport_width, f32 viewport_height) {
	g_ctx->root_node = UINode_CreateOrGet(UINodeFlags::None, Str8_Literal("ROOT"));
	g_ctx->root_node->semantic_size[UIAxis_X] = { UISizeType::Pixels, viewport_width };
	g_ctx->root_node->semantic_size[UIAxis_Y] = { UISizeType::Pixels, viewport_height };
	g_ctx->root_node->child_layout_axis = UIAxis_Y;

	UI_PushParent(g_ctx->root_node);
}

void UI_EndFrame() {
	UI_PopParent();

	UINode* root = UI_GetRootNode();

	UI_LayoutPass_ComputeStandaloneSizes(root);
	UI_LayoutPass_ComputeAncestorDependentSizes(root);
	UI_LayoutPass_ComputeDescendantDependentSizes(root);
	//UI_LayputPass_ComputeViolations(); // TODO
	UI_LayoutPass_ComputeFinalScreenCoordinates(root);
}

UINode* UI_GetRootNode() {
	return g_ctx->root_node;
}

UINode* UI_BeginRow(Str8 str) {
	UINode* node = UINode_CreateOrGet(UINodeFlags::DrawBackground, str);
	node->child_layout_axis = UIAxis_X;

	UI_PushParent(node);
	return node;
}

void UI_EndRow() {
	UI_PopParent();
}

UINode* UI_BeginCol(Str8 str) {
	UINode* node = UINode_CreateOrGet(UINodeFlags::DrawBackground, str);
	node->child_layout_axis = UIAxis_Y;

	UI_PushParent(node);
	return node;
}

void UI_EndCol() {
	UI_PopParent();
}

UINode* UI_Button(Str8 str) {
	// TODO: We need to implement #-symboling to indicate extra information to be hashed WITHOUT
	// being visible in the final string.
	UINode* node = UINode_CreateOrGet(
		UINodeFlags::Clickable | UINodeFlags::DrawText | UINodeFlags::DrawBackground | UINodeFlags::HotAnimation,
		str
	);
	node->semantic_size[UIAxis_X] = { UISizeType::TextContent, 0 };
	node->semantic_size[UIAxis_Y] = { UISizeType::TextContent, 0 };

	return node;
}

void UI_PushParent(UINode* node)        { SRVector_PushBack(&g_ctx->stack_parents, node); }
void UI_PushSemanticWidth(UISize size)  { SRVector_PushBack(&g_ctx->stack_semantic_widths, size); }
void UI_PushSemanticHeight(UISize size) { SRVector_PushBack(&g_ctx->stack_semantic_heights, size); }

void UI_PushSemanticSize(UIAxis axis, UISize size) {
	switch (axis) {
		case UIAxis_X: { UI_PushSemanticWidth(size); } break;
		case UIAxis_Y: { UI_PushSemanticHeight(size); } break;
		default: break;
	}
}

void UI_PopParent() {
	assert(g_ctx->stack_parents.size > 0);
	SRVector_PopBack(&g_ctx->stack_parents);
}

void UI_PopSemanticWidth() {
	assert(g_ctx->stack_semantic_widths.size > 0);
	SRVector_PopBack(&g_ctx->stack_semantic_widths);
}

void UI_PopSemanticHeight() {
	assert(g_ctx->stack_semantic_heights.size > 0);
	SRVector_PopBack(&g_ctx->stack_semantic_heights);
}

void UI_PopSemanticSize(UIAxis axis) {
	switch (axis) {
		case UIAxis_X: { UI_PopSemanticWidth(); } break;
		case UIAxis_Y: { UI_PopSemanticHeight(); } break;
		default: break;
	}
}

UISize UISize_Pixels(f32 pixels) { return { UISizeType::Pixels, pixels }; }
UISize UISize_TextContent()      { return { UISizeType::TextContent, 0.0f }; }
UISize UISize_PctParent(f32 pct) { return { UISizeType::PercentOfParent, pct }; }
UISize UISize_ChildSum()         { return { UISizeType::ChildSum, 0.0f }; }
