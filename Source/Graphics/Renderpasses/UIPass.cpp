#include "UIPass.h"
#include "Core/Platform.h"

#include <algorithm>
#include <format>
#include <iostream>

namespace SR {
	// UI Event
	UIEvent::UIEvent(UIEventType type) {
		set_type(type);
	}

	void UIEvent::set_type(UIEventType type) {
		const uint32_t numType = static_cast<uint32_t>(type);

		if (type == m_Type) {
			return;
		}

		m_Type = type;

		if ((numType & m_EventMask) != 0) {
			return;
		}

		if ((numType & MOUSE_EVENT_MASK) != 0) { // mouse-related type
			m_EventMask = MOUSE_EVENT_MASK;
			m_Data = std::make_shared<MouseEventData>();
		}
		else if ((numType & KEYBOARD_EVENT_MASK) != 0) { // keyboard-related type
			m_EventMask = KEYBOARD_EVENT_MASK;
			m_Data = std::make_shared<KeyboardEventData>();
		}
		else if ((numType & NON_PURE_KEYBOARD_EVENT_MASK) != 0) { // non-pure keyboard-related type
			m_EventMask = NON_PURE_KEYBOARD_EVENT_MASK;
			m_Data = std::make_shared<KeyboardCharData>();
		}
	}

	// UI Pass
	UIPass::UIPass(GraphicsDevice& gfxDevice, Window& window) :
		m_GfxDevice(gfxDevice), m_Window(window) {

		//// GLFW objects
		//m_DefaultCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		//m_TextCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
		//m_CurrentCursor = m_DefaultCursor;

		// Graphics
		m_GfxDevice.create_shader(ShaderStage::VERTEX, "shaders/vulkan/ui.vert.spv", m_VertexShader);
		m_GfxDevice.create_shader(ShaderStage::PIXEL, "shaders/vulkan/ui.frag.spv", m_PixelShader);

		const PipelineInfo pipelineInfo = {
			.vertexShader = &m_VertexShader,
			.pixelShader = &m_PixelShader,
			.blendState = {
				.alphaToCoverage = false,
				.independentBlend = false,
				.renderTargetBlendStates = {
					BlendState::RenderTargetBlendState {
						.blendEnable = true,
						.srcBlend = Blend::SRC_ALPHA,
						.dstBlend = Blend::INV_SRC_ALPHA,
						.blendOp = BlendOp::ADD,
						.srcBlendAlpha = Blend::ONE,
						.dstBlendAlpha = Blend::ONE,
						.blendOpAlpha = BlendOp::ADD
					}
				}
			},
			.numRenderTargets = 1,
			.renderTargetFormats = { Format::R8G8B8A8_UNORM }
		};

		m_GfxDevice.create_pipeline(pipelineInfo, m_Pipeline);

		const BufferInfo uiParamsBufferInfo = {
			.size = MAX_UI_PARAMS * sizeof(UIParams),
			.stride = sizeof(UIParams),
			.usage = Usage::UPLOAD,
			.bindFlags = BindFlag::SHADER_RESOURCE,
			.miscFlags = MiscFlag::BUFFER_STRUCTURED,
			.persistentMap = true
		};

		// Load resources
		m_DefaultFont = AssetManager::load_font_from_file("fonts/SegoeUI.ttf", 14);
		m_DefaultBoldFont = AssetManager::load_font_from_file("Fonts/SegoeUIBold.ttf", 14);
		AssetManager::load_from_file(m_RightArrowIcon, "textures/right_arrow.png");
		AssetManager::load_from_file(m_CheckIcon, "textures/check.png");
		AssetManager::load_from_file(m_WindowIcon, "Textures/StingrayIcon24x24.png");
		AssetManager::load_from_file(m_MinimizeIcon, "textures/minimize.png");
		AssetManager::load_from_file(m_MaximizeIcon, "textures/maximize.png");
		AssetManager::load_from_file(m_CloseIcon, "textures/close.png");

		for (size_t i = 0; i < GraphicsDevice::FRAMES_IN_FLIGHT; ++i) {
			m_GfxDevice.create_buffer(uiParamsBufferInfo, m_UIParamsBuffers[i], m_UIParamsData.data());
		}
	}

	UIPass::~UIPass() {
		//delete m_DefaultCursor;
		//m_DefaultCursor = nullptr;

		//delete m_TextCursor;
		//m_TextCursor = nullptr;
	}

	void UIPass::execute(PassExecuteInfo& executeInfo) {
		const CommandList& cmdList = *executeInfo.cmdList;
		const FrameInfo& frameInfo = *executeInfo.frameInfo;

		m_PushConstant.projectionMatrix = glm::ortho(0.0f, (float)frameInfo.width, (float)frameInfo.height, 0.0f);
		m_PushConstant.uiParamsBufferIndex = m_GfxDevice.get_descriptor_index(m_UIParamsBuffers[m_GfxDevice.get_frame_index()], SubresourceType::SRV);

		// Update
		if (m_ActiveWidgetID != 0 && m_WidgetStateMap[m_ActiveWidgetID].type == WidgetType::INPUT_TEXT) {
			m_CaretTimer += frameInfo.dt;
		}
		else {
			m_CaretTimer = 0.0f;
		}

		// Titlebar
		const int clientWidth = m_Window.get_client_width();
		draw_rect({ 0, 0 }, clientWidth, 31, UI_PRIMARY_BACKGROUND_COL);
		draw_rect({ 0, 31 }, clientWidth, 1, UI_PRIMARY_BORDER_COL);

		const Texture* iconTex = m_WindowIcon.get_texture();
		draw_rect({ 8, 31 / 2 }, iconTex->info.width, iconTex->info.height, glm::vec4(1.0f), UIPosFlag::VCENTER, iconTex);
		draw_text({ clientWidth / 2, 31 / 2 }, "Stingray (Vulkan)", UIPosFlag::HCENTER | UIPosFlag::VCENTER);
		draw_rect({ clientWidth - 44 * 3, 0 }, 44, 31, glm::vec4(1.0f), UIPosFlag::NONE, m_MinimizeIcon.get_texture());
		draw_rect({ clientWidth - 44 * 2, 0 }, 44, 31, glm::vec4(1.0f), UIPosFlag::NONE, m_MaximizeIcon.get_texture());
		draw_rect({ clientWidth - 44, 0 }, 44, 31, glm::vec4(1.0f), UIPosFlag::NONE, m_CloseIcon.get_texture());

		// Sort the UI params based on zOrder
		std::stable_sort(
			m_UIParamsData.begin(), m_UIParamsData.end(),
			[](const UIParams& p1, const UIParams& p2) {
				return p1.zOrder < p2.zOrder;
			}
		);

		memcpy(m_UIParamsBuffers[m_GfxDevice.get_frame_index()].mappedData, m_UIParamsData.data(), m_UIParamsData.size() * sizeof(UIParams));

		// Rendering
		m_GfxDevice.bind_pipeline(m_Pipeline, cmdList);
		m_GfxDevice.push_constants(&m_PushConstant, sizeof(m_PushConstant), cmdList);

		m_GfxDevice.draw_instanced(6, static_cast<uint32_t>(m_UIParamsData.size()), 0, 0, cmdList);

		m_UIParamsData.clear();
		m_LastMenuDimensions = m_MenuDimensions;
		m_MenuDimensions.clear();

		m_CursorOrigin = m_DefaultCursorOrigin;
		m_LastCursorOriginDelta = { 0.0f, 0.0f };
		m_SameLineIsActive = false;
		m_SameLineWasActive = false;

		if (m_ActiveWidgetID != 0) {
			UIWidgetState& activeState = m_WidgetStateMap[m_ActiveWidgetID];

			if (has_flag(activeState.actions, WidgetAction::CLICKED)) {
				activeState.actions &= ~WidgetAction::CLICKED;

				if (activeState.type != WidgetType::INPUT_TEXT && activeState.type != WidgetType::MENU) {
					m_ActiveWidgetID = 0;
				}
			}
		}

		// Reset events
		m_CurrentKeyboardEvent = UIEvent(UIEventType::None);
		m_CurrentKeyboardCharEvent = UIEvent(UIEventType::None);
	}

	void UIPass::begin_menu_bar(int width) {
		m_CursorOrigin = { 42, 0 };

		m_MainMenuActive = true;
	}

	bool UIPass::begin_menu(const std::string& text) {
		assert(m_MainMenuActive);
		calc_cursor_origin();

		// Widget ID hash
		const uint64_t idHash = std::hash<std::string>{}(text);
		const uint64_t typeHash = std::hash<WidgetType>{}(WidgetType::MENU);
		const uint64_t id = widget_hash_combine(idHash, typeHash);

		const int width = m_DefaultFont->calc_text_width(text) + UI_PADDING * 2;
		const int height = m_DefaultFont->boundingBoxHeight + UI_PADDING * 2;

		UIWidgetState state = {};
		state.position = m_CursorOrigin;
		state.width = width;
		state.height = height;
		state.id = id;
		state.parentID = m_LastBegunMenuID;
		state.text = text;
		state.type = WidgetType::MENU;

		// Create menu dimension instance for this object
		if (state.parentID != 0) {
			m_MenuDimensions[state.id] = { 0, 0 };
		}

		// Calculate parent menu dimensions if needed
		if (state.parentID != 0) {
			if (state.width > m_MenuDimensions[state.parentID].x) {
				m_MenuDimensions[state.parentID].x = state.width;
			}

			m_MenuDimensions[state.parentID].y += state.height;
		}

		m_LastBegunMenuID = id;

		const auto& search = m_WidgetStateMap.find(id);
		const auto& activeSearch = m_WidgetStateMap.find(m_ActiveWidgetID);

		if (search == m_WidgetStateMap.end()) {
			m_WidgetStateMap.insert({ id, state });
			m_WidgetStateMapIndices.push_back(id);
		}
		else {
			UIWidgetState& internalState = search->second;
			internalState.position = state.position;

			if (state.parentID != 0 && m_LastMenuDimensions.find(state.parentID) != m_LastMenuDimensions.end()) {
				internalState.width = UI_PADDING + m_LastMenuDimensions[state.parentID].x + UI_MENU_RIGHT_PADDING;
			}

			state = search->second;
		}

		bool ret = false;

		if (state.parentID != 0) {
			bool thisOwnsLastHovered = false;
			uint64_t id = m_LastHoveredWidgetID;

			while (id != 0) {
				if (id == state.id) {
					thisOwnsLastHovered = true;
					break;
				}

				id = m_WidgetStateMap[id].parentID;
			}

			if (thisOwnsLastHovered) {
				ret = true;
			}
		}
		else {
			bool thisIsParentToActive = false;
			uint64_t id = activeSearch != m_WidgetStateMap.end() ? activeSearch->second.id : 0;

			while (id != 0) {
				if (id == state.id) {
					thisIsParentToActive = true;
					break;
				}

				id = m_WidgetStateMap[id].parentID;
			}

			if (thisIsParentToActive) {
				ret = true;
			}
		}

		glm::vec4 color = UI_WIDGET_PRIMARY_COL;

		if (has_flag(state.actions, WidgetAction::PRESSED)) {
			color = UI_WIDGET_PRIMARY_COL_PRESSED;
		}
		else if (has_flag(state.actions, WidgetAction::HOVERED)) {
			color = UI_WIDGET_PRIMARY_COL_HOV;
		}

		if (state.parentID != 0) {
			draw_rect(m_CursorOrigin + glm::vec2(1, 1), width - 2, height - 2, color, UIPosFlag::NONE, nullptr, 25);

			// Draw menu right-arrow (if it is a submenu)
			const Texture* arrowTex = m_RightArrowIcon.get_texture();
			int arrowTexWidth = static_cast<int>(arrowTex->info.width);
			int arrowTexHeight = static_cast<int>(arrowTex->info.height);

			draw_rect(
				state.position + glm::vec2(state.width - arrowTexWidth - UI_PADDING,
					state.height / 2
				),
				arrowTexWidth,
				arrowTexHeight,
				glm::vec4(1.0f),
				UIPosFlag::VCENTER,
				arrowTex,
				50
			);
		}
		else {
			draw_rect(m_CursorOrigin, width, height, color, UIPosFlag::NONE, nullptr, 25);
		}

		draw_text(m_CursorOrigin + glm::vec2(width / 2, height / 2), text, UIPosFlag::HCENTER | UIPosFlag::VCENTER, nullptr, 30);

		if (state.parentID != 0) {
			m_CursorOrigin.x += state.width;
			m_LastCursorOriginDelta.y = 0;
		}
		else {
			m_LastCursorOriginDelta.x = 0;
			m_LastCursorOriginDelta.y = state.height;
		}

		return ret;
	}

	bool UIPass::menu_item(const std::string& text) {
		assert(m_MainMenuActive);
		assert(m_LastBegunMenuID != 0);

		calc_cursor_origin();

		const auto parentSearch = m_WidgetStateMap.find(m_LastBegunMenuID);

		if (parentSearch == m_WidgetStateMap.end()) {
			return false;
		}

		UIWidgetState parentState = parentSearch->second;

		// Widget ID hash
		const uint64_t textHash = std::hash<std::string>{}(text);
		const uint64_t typeHash = std::hash<WidgetType>{}(WidgetType::MENU_ITEM);
		const uint64_t id = widget_hash_combine(textHash, typeHash);

		UIWidgetState state = {};
		state.position = m_CursorOrigin;
		state.width = m_DefaultFont->calc_text_width(text) + UI_PADDING * 2;
		state.height = m_DefaultFont->boundingBoxHeight + UI_PADDING * 2;
		state.id = id;
		state.parentID = m_LastBegunMenuID;
		state.text = text;
		state.type = WidgetType::MENU_ITEM;

		// Calculate parent-menu dimensions
		if (state.parentID != 0) {
			if (state.width > m_MenuDimensions[state.parentID].x) {
				m_MenuDimensions[state.parentID].x = state.width;
			}

			m_MenuDimensions[state.parentID].y += state.height;
		}

		const auto& search = m_WidgetStateMap.find(id);

		// This menu item hasn't been created before
		if (search == m_WidgetStateMap.end()) {
			m_WidgetStateMap.insert({ id, state });
			m_WidgetStateMapIndices.push_back(id);
		}
		else {
			UIWidgetState& internalState = search->second;

			if (state.parentID != 0 && m_LastMenuDimensions.find(state.parentID) != m_LastMenuDimensions.end()) {
				internalState.width = UI_PADDING + m_LastMenuDimensions[state.parentID].x + UI_MENU_RIGHT_PADDING;
			}

			state = search->second;
		}

		bool ret = has_flag(state.actions, WidgetAction::CLICKED);

		if (ret) {

		}

		draw_text(m_CursorOrigin + glm::vec2(UI_PADDING, state.height / 2), text, UIPosFlag::VCENTER, nullptr, 20);

		m_LastCursorOriginDelta.x = 0.0f;
		m_LastCursorOriginDelta.y = state.height;

		return ret;
	}

	void UIPass::end_menu() {
		assert(m_MainMenuActive);

		const auto search = m_WidgetStateMap.find(m_LastBegunMenuID);
		assert(search != m_WidgetStateMap.end());

		UIWidgetState state = search->second;

		if (state.parentID == 0) {
			m_CursorOrigin.x += state.width;
			m_CursorOrigin.y = 0;
			m_LastCursorOriginDelta.y = 0;
		}
		else {
			m_LastCursorOriginDelta.x = 0;
			m_LastCursorOriginDelta.y = 0;
			m_CursorOrigin.x = state.position.x;
			m_CursorOrigin.y = state.position.y + state.height;
		}

		UIWidgetState activeState = {};
		const auto& activeSearch = m_WidgetStateMap.find(m_ActiveWidgetID);

		if (activeSearch != m_WidgetStateMap.end()) {
			activeState = activeSearch->second;
		}

		// Find top parent menu
		uint64_t id = m_ActiveWidgetID;
		bool thisIsParentToActive = false;

		while (id != 0) {
			if (id == state.id) {
				thisIsParentToActive = true;
				break;
			}

			id = m_WidgetStateMap[id].parentID;
		}

		id = state.id;
		bool hasActiveParent = false;

		while (id != 0) {
			if (id == m_ActiveWidgetID) {
				hasActiveParent = true;
				break;
			}

			id = m_WidgetStateMap[id].parentID;
		}

		if (thisIsParentToActive || hasActiveParent) {

			// Menu background border
			draw_rect(
				state.position + (state.parentID != 0 ? glm::vec2(state.width, 0) : glm::vec2(0, state.height)),
				UI_PADDING + m_MenuDimensions[m_LastBegunMenuID].x + UI_MENU_RIGHT_PADDING,
				std::max(0.0f, m_MenuDimensions[m_LastBegunMenuID].y),
				UI_WIDGET_SECONDARY_COL,
				UIPosFlag::NONE,
				nullptr,
				10
			);

			// Menu background
			draw_rect(
				state.position + glm::vec2(1, 1) + (state.parentID != 0 ? glm::vec2(state.width, 0) : glm::vec2(0, state.height)),
				UI_PADDING + m_MenuDimensions[m_LastBegunMenuID].x + UI_MENU_RIGHT_PADDING - 2,
				std::max(0.0f, m_MenuDimensions[m_LastBegunMenuID].y - 2),
				UI_WIDGET_PRIMARY_COL,
				UIPosFlag::NONE,
				nullptr,
				10
			);

			// Draw the hovered menu item (if one is hovered)
			// Make sure the hovered widget is a menu item:
			const auto& hoveredSearch = m_WidgetStateMap.find(m_HoveredWidgetID);

			if (hoveredSearch == m_WidgetStateMap.end()) {
				m_LastBegunMenuID = state.parentID;
				return;
			}

			UIWidgetState& hoveredState = hoveredSearch->second;

			// Find top parent menu
			uint64_t id = hoveredState.id;
			bool hasActiveParent = false;

			while (id != 0) {
				if (m_ActiveWidgetID == id) {
					hasActiveParent = true;
					break;
				}

				id = m_WidgetStateMap[id].parentID;
			}

			if ((hoveredState.type != WidgetType::MENU_ITEM && hoveredState.type != WidgetType::MENU) || !hasActiveParent || hoveredState.parentID == 0) {
				m_LastBegunMenuID = state.parentID;
				return;
			}

			if (has_flag(hoveredState.actions, WidgetAction::HOVERED) || has_flag(hoveredState.actions, WidgetAction::PRESSED)) {
				//hoveredState.width = UI_PADDING + m_MenuDimensions[m_LastBegunMenuID].x + UI_MENU_RIGHT_PADDING;

				draw_rect(
					hoveredState.position,
					hoveredState.width,
					hoveredState.height,
					UI_WIDGET_PRIMARY_COL_HOV,
					UIPosFlag::NONE,
					nullptr,
					11
				);
			}
		}

		// If the menu has a parent menu, then parentID will be non-zero
		m_LastBegunMenuID = state.parentID;
	}

	void UIPass::end_menu_bar() {
		assert(m_MainMenuActive);

		m_LastCursorOriginDelta.x = 0;
		m_LastCursorOriginDelta.y = 0;
		m_CursorOrigin.x = UI_PADDING;
		m_CursorOrigin.y += m_DefaultFont->boundingBoxHeight + UI_PADDING * 3;

		m_MainMenuActive = false;
	}

	void UIPass::begin_split(const std::string& text) {
		calc_cursor_origin();

		const uint64_t textHash = std::hash<std::string>{}(text);
		const uint64_t typeHash = std::hash<WidgetType>{}(WidgetType::SPLIT);
		const uint64_t id = widget_hash_combine(textHash, typeHash);

		int width;
		int height;

		if (m_ActiveSplitIDs.empty()) {
			width = m_Window.get_client_width();
			height = m_Window.get_client_height() - UI_TITLEBAR_HEIGHT;
		}
		else {
			const uint64_t currSplitID = m_ActiveSplitIDs.back();
			const UIWidgetState& currSplitWidget = m_WidgetStateMap[currSplitID];
			width = 0; // TODO: Fix
			height = 0; // TODO: Fix
		}

		UIWidgetState state = {};
		state.position = m_CursorOrigin;
		state.id = id;
		state.width = width;
		state.height = height;
		state.text = text;
		state.type = WidgetType::SPLIT;

		const auto& search = m_WidgetStateMap.find(id);

		if (search == m_WidgetStateMap.end()) {
			m_WidgetStateMap.insert({ id, state });
			m_WidgetStateMapIndices.push_back(id);
		}
		else {
			UIWidgetState& internalState = search->second;
			internalState.position = state.position;
			internalState.width = width;
			internalState.height = height;
			state = search->second;
		}

		m_ActiveSplitIDs.push_back(id);
	}

	void UIPass::begin_panel(const std::string& text, float percentage) {
		assert(!m_ActiveSplitIDs.empty());

		const uint64_t currSplitID = m_ActiveSplitIDs.back();
		const UIWidgetState& currSplitWidget = m_WidgetStateMap[currSplitID];

		calc_cursor_origin();
		const uint64_t textHash = std::hash<std::string>{}(text);
		const uint64_t typeHash = std::hash<WidgetType>{}(WidgetType::PANEL);
		const uint64_t id = widget_hash_combine(textHash, typeHash);
		m_ActivePanelID = id;

		UIWidgetState state = {};
		state.position = m_CursorOrigin;
		state.id = id;
		state.width = static_cast<int>(percentage * currSplitWidget.width);
		state.height = currSplitWidget.height; // TODO: Fix
		state.text = text;
		state.type = WidgetType::PANEL;

		const auto& search = m_WidgetStateMap.find(id);

		if (search == m_WidgetStateMap.end()) {
			m_WidgetStateMap.insert({ id, state });
			m_WidgetStateMapIndices.push_back(id);
		}
		else {
			UIWidgetState& internalState = search->second;
			internalState.position = state.position;
			internalState.width = static_cast<int>(percentage * currSplitWidget.width);
			internalState.height = currSplitWidget.height; // TODO: Fix
			state = search->second;
		}

		// Drawing
		draw_rect(m_CursorOrigin - glm::vec2(UI_PADDING), state.width - 1, state.height, UI_PRIMARY_BACKGROUND_COL);
		draw_rect(m_CursorOrigin - glm::vec2(UI_PADDING) + glm::vec2(state.width - 1, 0), 1, state.height, UI_PRIMARY_BORDER_COL);
		draw_text(m_CursorOrigin, text, UIPosFlag::NONE, m_DefaultBoldFont);

		m_LastCursorOriginDelta.x = 0;
		m_LastCursorOriginDelta.y = m_DefaultFont->boundingBoxHeight + UI_PADDING;
	}

	void UIPass::end_panel() {
		assert(m_ActivePanelID != 0);
		m_LastCursorOriginDelta.x = 0;
		m_LastCursorOriginDelta.y = 0;
		m_CursorOrigin.x += m_WidgetStateMap[m_ActivePanelID].width;
		m_CursorOrigin.y = m_DefaultCursorOrigin.y;
		m_ActivePanelID = 0;
	}

	void UIPass::end_split() {
		assert(!m_ActiveSplitIDs.empty());
		m_ActiveSplitIDs.pop_back();
	}

	void UIPass::widget_text(const std::string& text, int fixedWidth) {
		calc_cursor_origin();

		draw_text(m_CursorOrigin, text);

		m_LastCursorOriginDelta.x = fixedWidth > 0 ? fixedWidth : m_DefaultFont->calc_text_width(text) + UI_PADDING * 2;
		m_LastCursorOriginDelta.y = m_DefaultFont->boundingBoxHeight + UI_PADDING;
	}

	bool UIPass::widget_button(const std::string& text) {
		//std::cout << "Num Widgets: " << m_WidgetStateMap.size() << '\n';
		calc_cursor_origin();

		// Widget ID hash
		const uint64_t textHash = std::hash<std::string>{}(text);
		const uint64_t typeHash = std::hash<WidgetType>{}(WidgetType::BUTTON);
		const uint64_t id = widget_hash_combine(textHash, typeHash);

		const int width = m_DefaultFont->calc_text_width(text) + UI_PADDING * 2;
		const int height = m_DefaultFont->boundingBoxHeight + UI_PADDING * 2;

		UIWidgetState state = {};
		state.position = m_CursorOrigin;
		state.width = width;
		state.height = height;
		state.id = id;
		state.text = text;
		state.type = WidgetType::BUTTON;

		const auto& search = m_WidgetStateMap.find(id);

		if (search == m_WidgetStateMap.end()) {
			m_WidgetStateMap.insert({ id, state });
			m_WidgetStateMapIndices.push_back(id);
		}
		else {
			UIWidgetState& internalState = search->second;
			internalState.position = state.position;
			state = search->second;
		}

		bool ret = has_flag(state.actions, WidgetAction::CLICKED);
		glm::vec4 color = UI_WIDGET_PRIMARY_COL;

		if (has_flag(state.actions, WidgetAction::PRESSED)) {
			color = UI_WIDGET_PRIMARY_COL_PRESSED;
		}
		else if (has_flag(state.actions, WidgetAction::HOVERED)) {
			color = UI_WIDGET_PRIMARY_COL_HOV;
		}

		draw_rect(m_CursorOrigin, width, height, color);
		draw_text(m_CursorOrigin + glm::vec2(width / 2, height / 2), text, UIPosFlag::HCENTER | UIPosFlag::VCENTER);

		m_LastCursorOriginDelta.x = width + UI_PADDING;
		m_LastCursorOriginDelta.y = height + UI_PADDING;

		return ret;
	}

	bool UIPass::widget_checkbox(const std::string& text, bool* value) {
		assert(value != nullptr);

		const uint64_t textHash = std::hash<std::string>{}(text);
		const uint64_t typeHash = std::hash<WidgetType>{}(WidgetType::CHECKBOX);
		const uint64_t id = widget_hash_combine(textHash, typeHash);

		calc_cursor_origin();

		UIWidgetState state = {};
		state.position = m_CursorOrigin;
		state.width = UI_WIDGET_CHECKBOX_SIZE;
		state.height = UI_WIDGET_CHECKBOX_SIZE;
		state.id = id;
		state.text = text;
		state.type = WidgetType::CHECKBOX;

		const auto search = m_WidgetStateMap.find(id);
		if (search == m_WidgetStateMap.end()) {
			m_WidgetStateMap.insert({ id, state });
			m_WidgetStateMapIndices.push_back(id);
		}
		else {
			state = search->second;
		}

		bool ret = false;
		glm::vec4 backgroundColor = UI_WIDGET_PRIMARY_COL;

		if (has_flag(state.actions, WidgetAction::CLICKED)) {
			ret = true;
			*value = !*value;
		}
		else if (has_flag(state.actions, WidgetAction::PRESSED)) {
			backgroundColor = UI_WIDGET_PRIMARY_COL_PRESSED;
		}
		else if (has_flag(state.actions, WidgetAction::HOVERED)) {
			backgroundColor = UI_WIDGET_PRIMARY_COL_HOV;
		}

		// Checkbox background
		draw_rect(
			m_CursorOrigin,
			UI_WIDGET_CHECKBOX_SIZE,
			UI_WIDGET_CHECKBOX_SIZE,
			backgroundColor
		);

		// Checkbox icon
		if (*value) {
			draw_rect(
				m_CursorOrigin,
				UI_WIDGET_CHECKBOX_SIZE,
				UI_WIDGET_CHECKBOX_SIZE,
				UI_WIDGET_ACCENT_COL,
				UIPosFlag::NONE,
				m_CheckIcon.get_texture()
			);
		}

		draw_text(
			m_CursorOrigin + glm::vec2(UI_WIDGET_CHECKBOX_SIZE + UI_PADDING, UI_WIDGET_CHECKBOX_SIZE / 2),
			text,
			UIPosFlag::VCENTER
		);

		m_LastCursorOriginDelta.x = state.width + UI_PADDING;
		m_LastCursorOriginDelta.y = state.height + UI_PADDING;

		return ret;
	}

	bool UIPass::widget_slider_float(const std::string& text, float* value, float min, float max) {
		assert(value != nullptr);
		assert(min < max);

		bool ret = false;
		calc_cursor_origin();

		const int trackHeight = m_DefaultFont->boundingBoxHeight + UI_PADDING * 2;
		const int handleHeight = trackHeight - 2;
		const int innerTrackWidth = UI_WIDGET_SLIDER_WIDTH - 2;

		const uint64_t textHash = std::hash<std::string>{}(text);
		const uint64_t typeHash = std::hash<WidgetType>{}(WidgetType::SLIDER_FLOAT);
		const uint64_t id = widget_hash_combine(textHash, typeHash);

		UIWidgetState state = {};
		state.position = m_CursorOrigin;
		state.width = UI_WIDGET_SLIDER_WIDTH;
		state.height = trackHeight;
		state.id = id;
		state.text = text;
		state.type = WidgetType::SLIDER_FLOAT;

		const auto search = m_WidgetStateMap.find(id);

		if (search == m_WidgetStateMap.end()) {
			m_WidgetStateMap.insert({ id, state });
			m_WidgetStateMapIndices.push_back(id);
		}
		else {
			state = search->second;
		}

		if (has_flag(state.actions, WidgetAction::PRESSED)) {
			if (m_CurrentMouseEvent.is_mouse_event()) {
				const MouseEventData& mouse = m_CurrentMouseEvent.get_mouse_data();

				const int innerTrackRelMouseX = std::min(mouse.position.x - m_CursorOrigin.x - 1 - UI_WIDGET_SLIDER_HANDLE_WIDTH / 2, m_CursorOrigin.x + 1 + innerTrackWidth - UI_WIDGET_SLIDER_HANDLE_WIDTH / 2);
				const float percentage = std::clamp((float)innerTrackRelMouseX / (innerTrackWidth - UI_WIDGET_SLIDER_HANDLE_WIDTH), 0.0f, 1.0f);
				const float newValue = min + fabsf(max - min) * percentage;

				if (newValue != *value) {
					ret = true; // indicate that variable has changed
				}

				*value = newValue;
			}
		}

		int tempMin = min;
		min -= tempMin;
		max -= tempMin;
		const float percentage = fabsf(*value - tempMin) / max;

		draw_rect(m_CursorOrigin, UI_WIDGET_SLIDER_WIDTH, trackHeight, UI_WIDGET_PRIMARY_COL);
		draw_rect({ m_CursorOrigin.x + 1 + std::clamp((innerTrackWidth - UI_WIDGET_SLIDER_HANDLE_WIDTH) * percentage, 0.0f, (float)innerTrackWidth - UI_WIDGET_SLIDER_HANDLE_WIDTH),
					m_CursorOrigin.y + 1 }, UI_WIDGET_SLIDER_HANDLE_WIDTH, handleHeight, UI_WIDGET_ACCENT_COL);
		draw_text(m_CursorOrigin + glm::vec2(UI_WIDGET_SLIDER_WIDTH / 2, trackHeight / 2), std::to_string(*value), UIPosFlag::HCENTER | UIPosFlag::VCENTER);

		// Label
		draw_text(m_CursorOrigin + glm::vec2(UI_WIDGET_SLIDER_WIDTH + UI_PADDING, trackHeight / 2), text, UIPosFlag::VCENTER);

		m_LastCursorOriginDelta.x = state.width; + UI_PADDING;
		m_LastCursorOriginDelta.y = state.height + UI_PADDING;

		return ret;
	}

	void UIPass::widget_text_input(const std::string& label, std::string& buffer) {
		calc_cursor_origin();

		const int boxWidth = UI_WIDGET_TEXT_INPUT_WIDTH;
		const int boxHeight = m_DefaultFont->boundingBoxHeight + UI_PADDING * 2;
		const int caretWidth = 1;
		const int caretHeight = boxHeight - UI_PADDING;
		const glm::vec2 textOrigin = m_CursorOrigin + glm::vec2(UI_PADDING, boxHeight / 2);

		const uint64_t textHash = std::hash<std::string>{}(label);
		const uint64_t typeHash = std::hash<WidgetType>{}(WidgetType::INPUT_TEXT);
		const uint64_t id = widget_hash_combine(textHash, typeHash);

		UIWidgetState state = {};
		state.position = m_CursorOrigin;
		state.width = boxWidth;
		state.height = boxHeight;
		state.id = id;
		state.text = buffer;
		state.type = WidgetType::INPUT_TEXT;

		const auto search = m_WidgetStateMap.find(id);

		if (search == m_WidgetStateMap.end()) {
			m_WidgetStateMap.insert({ id, state });
			m_WidgetStateMapIndices.push_back(id);
		}
		else {
			state = search->second;
		}

		// Background
		draw_rect(m_CursorOrigin, boxWidth, boxHeight, UI_WIDGET_PRIMARY_COL);

		LOCAL_PERSIST glm::vec2 downCaretPos = textOrigin;
		LOCAL_PERSIST glm::vec2 currCaretPos = textOrigin;
		LOCAL_PERSIST size_t downCaretIndex = 0;
		LOCAL_PERSIST size_t currCaretIndex = 0;
		LOCAL_PERSIST bool isMouseDownOnce = false;
		LOCAL_PERSIST bool isHighlighting = false;

		downCaretPos.y = textOrigin.y;
		currCaretPos.y = textOrigin.y;

		// Caret
		if (m_ActiveWidgetID == id) {
			if (m_CurrentMouseEvent.is_mouse_event()) {
				const MouseEventData& mouse = m_CurrentMouseEvent.get_mouse_data();

				// Calculate caret position based on mouse press position
				if (has_flag(state.actions, WidgetAction::PRESSED)) {
					m_CaretTimer = 0.0f;

					const glm::vec2 relMousePos = mouse.position - textOrigin;
					float relCaretPosX = 0.0f;
					size_t caretIndex = 0;

					for (size_t i = 0; i < buffer.length(); ++i) {
						const GlyphData& glyph = m_DefaultFont->glyphs[buffer[i]];
						float glyphWidth = glyph.advanceX;

						if (i == 0) {
							glyphWidth -= glyph.bearingX;
						}

						if (relCaretPosX + glyphWidth / 2 >= relMousePos.x) {
							break;
						}

						relCaretPosX += glyphWidth;
						++caretIndex;
					}

					const float newCaretPosX = textOrigin.x + relCaretPosX;

					if (!isMouseDownOnce) {
						downCaretPos.x = newCaretPosX;
						downCaretIndex = caretIndex;
						isMouseDownOnce = true;
					}

					currCaretPos.x = newCaretPosX;
					currCaretIndex = caretIndex;

					if (downCaretPos != currCaretPos) {
						isHighlighting = true;
					}
					else {
						isHighlighting = false;
					}
				}
				else {
					isMouseDownOnce = false;
				}
			}

			if (m_CurrentKeyboardEvent.is_keyboard_event()) {
				const KeyboardEventData& keyEventData = m_CurrentKeyboardEvent.get_keyboard_data();

				//if (keyEventData.key == GLFW_KEY_LEFT && (keyEventData.action == GLFW_PRESS || keyEventData.action == GLFW_REPEAT)) {
				//	m_CaretTimer = 0.0f;

				//	if (currCaretIndex > 0) {
				//		if (keyEventData.mods == GLFW_MOD_SHIFT) {
				//			--currCaretIndex;

				//			if (currCaretIndex == downCaretIndex) {
				//				isHighlighting = false;
				//			}
				//			else {
				//				isHighlighting = true;
				//				currCaretPos = textOrigin + glm::vec2(m_DefaultFont->calc_text_width(
				//					buffer.substr(0, currCaretIndex)
				//				), 0);
				//			}
				//		}
				//		else if (keyEventData.mods == 0) {
				//			--currCaretIndex;
				//			downCaretIndex = currCaretIndex;
				//			downCaretPos = textOrigin + glm::vec2(m_DefaultFont->calc_text_width(buffer.substr(0, currCaretIndex)), 0);
				//			currCaretPos = downCaretPos;
				//		}
				//	}
				//}
				//else if (keyEventData.key == GLFW_KEY_RIGHT && (keyEventData.action == GLFW_PRESS || keyEventData.action == GLFW_REPEAT)) {
				//	m_CaretTimer = 0.0f;

				//	if (currCaretIndex < buffer.length()) {
				//		if (keyEventData.mods == GLFW_MOD_SHIFT) {
				//			++currCaretIndex;

				//			if (currCaretIndex == downCaretIndex) {
				//				isHighlighting = false;
				//			}
				//			else {
				//				isHighlighting = true;
				//				currCaretPos = textOrigin + glm::vec2(m_DefaultFont->calc_text_width(
				//					buffer.substr(0, currCaretIndex)
				//				), 0);
				//			}
				//		}
				//		else if (keyEventData.mods == 0) {

				//			++currCaretIndex;
				//			downCaretIndex = currCaretIndex;
				//			downCaretPos = textOrigin + glm::vec2(m_DefaultFont->calc_text_width(buffer.substr(0, currCaretIndex)), 0);
				//			currCaretPos = downCaretPos;
				//		}
				//	}
				//}

				//if (!buffer.empty() && keyEventData.key == GLFW_KEY_BACKSPACE && (keyEventData.action == GLFW_PRESS || keyEventData.action == GLFW_REPEAT)) {
				//	const size_t firstIndex = std::min(currCaretIndex, downCaretIndex);
				//	const size_t lastIndex = std::max(currCaretIndex, downCaretIndex);

				//	if (firstIndex != lastIndex) { // highlight active
				//		buffer = buffer.erase(firstIndex, lastIndex - firstIndex);

				//		downCaretPos = textOrigin + glm::vec2(m_DefaultFont->calc_text_width(buffer.substr(0, firstIndex)), 0);
				//		currCaretPos = downCaretPos;
				//		downCaretIndex = firstIndex;
				//		currCaretIndex = downCaretIndex;
				//	}
				//	else if (firstIndex > 0) {
				//		buffer = buffer.erase(firstIndex - 1, 1);

				//		downCaretPos = textOrigin + glm::vec2(m_DefaultFont->calc_text_width(buffer.substr(0, firstIndex - 1)), 0);
				//		currCaretPos = downCaretPos;
				//		downCaretIndex = firstIndex - 1;
				//		currCaretIndex = downCaretIndex;
				//	}

				//	m_CaretTimer = 0.0f;
				//	isHighlighting = false;
				//}
			}

			if (m_CurrentKeyboardCharEvent.is_keyboard_char_event()) {
				const size_t firstIndex = std::min(currCaretIndex, downCaretIndex);
				const size_t lastIndex = std::max(currCaretIndex, downCaretIndex);

				const KeyboardCharData& keyboardChar = m_CurrentKeyboardCharEvent.get_keyboard_char_data();

				// TODO: Add UTF-8 support
				if (firstIndex != lastIndex) { // highlight active
					buffer.erase(buffer.begin() + firstIndex, buffer.begin() + lastIndex);
				}

				buffer.insert(firstIndex, 1, (char)keyboardChar.codePoint);

				downCaretPos = textOrigin + glm::vec2(m_DefaultFont->calc_text_width(buffer.substr(0, firstIndex + 1)), 0);
				currCaretPos = downCaretPos;
				downCaretIndex = firstIndex + 1;
				currCaretIndex = downCaretIndex;

				m_CaretTimer = 0.0f;
				isHighlighting = false;
			}

			if (isHighlighting) {
				const float minCaretX = std::min(currCaretPos.x, downCaretPos.x);
				const float maxCaretX = std::max(currCaretPos.x, downCaretPos.x);

				draw_rect({ minCaretX, downCaretPos.y }, maxCaretX - minCaretX, caretHeight, UI_WIDGET_HIGHLIGHT_COL, UIPosFlag::VCENTER);
			}
			else {
				if (m_CaretTimer <= UI_WIDGET_TEXT_INPUT_CARET_BLINK_RATE) {
					draw_rect(downCaretPos, caretWidth, caretHeight, UI_WIDGET_ACCENT_COL, UIPosFlag::VCENTER);
				}
				else if (m_CaretTimer >= UI_WIDGET_TEXT_INPUT_CARET_BLINK_RATE * 2) {
					m_CaretTimer = 0.0f;
				}
			}
		}

		// Text
		draw_text(textOrigin, buffer, UIPosFlag::VCENTER);

		m_LastCursorOriginDelta.x = state.width + UI_PADDING;
		m_LastCursorOriginDelta.y = state.height + UI_PADDING;
	}

	void UIPass::widget_input_scalar(const std::string& label, void* scalar, UIDataType type) {
		assert(scalar != nullptr);

		const uint64_t textHash = std::hash<std::string>{}(label);
		const uint64_t typeHash = std::hash<WidgetType>{}(WidgetType::CHECKBOX);
		const uint64_t id = widget_hash_combine(textHash, typeHash);

		calc_cursor_origin();

		UIWidgetState state = {};
		state.position = m_CursorOrigin;
		state.width = UI_WIDGET_CHECKBOX_SIZE;
		state.height = UI_WIDGET_CHECKBOX_SIZE;
		state.id = id;
		state.text = label;
		state.type = WidgetType::CHECKBOX;

		const auto search = m_WidgetStateMap.find(id);
		if (search == m_WidgetStateMap.end()) {
			m_WidgetStateMap.insert({ id, state });
			m_WidgetStateMapIndices.push_back(id);
		}
		else {
			state = search->second;
		}

		bool ret = false;

		// TODO: Implement

		m_LastCursorOriginDelta.x = state.width + UI_PADDING;
		m_LastCursorOriginDelta.y = state.height + UI_PADDING;
	}

	void UIPass::widget_image(const Texture& texture, int width, int height) {
		assert(has_flag(texture.info.bindFlags, BindFlag::SHADER_RESOURCE));

		calc_cursor_origin();

		const uint64_t idHash = std::hash<const Texture*>{}(&texture); // TODO: Might break
		const uint64_t typeHash = std::hash<WidgetType>{}(WidgetType::IMAGE);
		const uint64_t id = widget_hash_combine(idHash, typeHash);

		UIWidgetState state = {};
		state.position = m_CursorOrigin;
		state.width = width;
		state.height = height;
		state.id = id;
		state.type = WidgetType::IMAGE;

		const auto search = m_WidgetStateMap.find(id);

		if (search == m_WidgetStateMap.end()) {
			m_WidgetStateMap.insert({ id, state });
			m_WidgetStateMapIndices.push_back(id);
		}
		else {
			state = search->second;
		}

		draw_rect(m_CursorOrigin, width, height, glm::vec4(1.0f), UIPosFlag::NONE, &texture);

		m_LastCursorOriginDelta.x = state.width + UI_PADDING;
		m_LastCursorOriginDelta.y = state.height + UI_PADDING;
	}

	void UIPass::widget_same_line() {
		m_SameLineIsActive = true;
		m_SameLineWasActive = false;

		//m_CursorOrigin.y -= m_L
	}

	void UIPass::process_event(const UIEvent& event) {
		if (event.is_mouse_event()) {
			m_LastMouseEvent = m_CurrentMouseEvent;
			m_CurrentMouseEvent = event;
		}
		else if (event.is_keyboard_event()) {
			m_CurrentKeyboardEvent = event;
		}
		else if (event.is_keyboard_char_event()) {
			m_CurrentKeyboardCharEvent = event;
		}

		switch (event.get_type()) {
		case UIEventType::MouseMove:
			{
				const MouseEventData& mouse = event.get_mouse_data();
				bool hitAny = false;

				// NOTE: The indices are sorted by z-order, so the elements with
				// the highest z-order will be hit-tested first.
				for (size_t i = 0; i < m_WidgetStateMapIndices.size(); ++i) {
					UIWidgetState& state = m_WidgetStateMap[m_WidgetStateMapIndices[i]];

					if (state.hit_test(mouse.position)) {
						if ((state.type == WidgetType::MENU_ITEM || state.type == WidgetType::MENU) && state.parentID != 0) {
							// Find top parent menu
							uint64_t id = state.id;
							bool foundActiveMenu = false;

							while (id != 0) {
								if (m_ActiveWidgetID == id) {
									foundActiveMenu = true;
									break;
								}

								id = m_WidgetStateMap[id].parentID;
							}

							if (!foundActiveMenu) {
								continue;
							}

							if (state.type == WidgetType::MENU && state.parentID != 0) {
								m_LastHoveredNonRootMenuID = state.id;
							}
						}
						else if (m_ActiveWidgetID != 0 &&
								(m_WidgetStateMap[m_ActiveWidgetID].type == WidgetType::MENU ||
								m_WidgetStateMap[m_ActiveWidgetID].type == WidgetType::MENU_ITEM)) {
							continue;
						}

						hitAny = true;
						state.actions |= WidgetAction::HOVERED;
						m_HoveredWidgetID = state.id;
						m_LastHoveredWidgetID = m_HoveredWidgetID;
					}
					else {
						state.actions &= ~WidgetAction::HOVERED;
					}
				}

				if (!hitAny) {
					m_HoveredWidgetID = 0;
				}

				//if (m_CurrentCursor != m_DefaultCursor && m_HoveredWidgetID == 0) {
				//	m_CurrentCursor = m_DefaultCursor;
				//	glfwSetCursor(m_Window, m_DefaultCursor);
				//}
				//else if (m_CurrentCursor != m_TextCursor && m_WidgetStateMap[m_HoveredWidgetID].type == WidgetType::INPUT_TEXT) {
				//	m_CurrentCursor = m_TextCursor;
				//	glfwSetCursor(m_Window, m_TextCursor);
				//}
			}
			break;
		case UIEventType::MouseDown:
			{
				if (m_HoveredWidgetID != 0) {
					m_ActiveWidgetID = m_HoveredWidgetID;
					m_WidgetStateMap[m_ActiveWidgetID].actions |= WidgetAction::PRESSED;
				}
				else {
					m_ActiveWidgetID = 0;
				}
			}
			break;
		case UIEventType::MouseUp:
			{
				if (m_ActiveWidgetID == 0) {
					return;
				}

				UIWidgetState& activeState = m_WidgetStateMap[m_ActiveWidgetID];

				if (has_flag(activeState.actions, WidgetAction::HOVERED) &&
					has_flag(activeState.actions, WidgetAction::PRESSED)) {
					if (activeState.parentID != 0 &&
						(activeState.type == WidgetType::MENU ||
						activeState.type == WidgetType::MENU_ITEM)) {

						m_HoveredWidgetID = 0;
					}

					activeState.actions |= WidgetAction::CLICKED;
					activeState.actions &= ~WidgetAction::PRESSED;
				}
				else {
					activeState.actions &= ~WidgetAction::PRESSED;
				}
			}
			break;
		}
	}

	void UIPass::draw_text(const glm::vec2& pos, const std::string& text, UIPosFlag posFlags, const Font* font, uint32_t zOrder) {
		if (font == nullptr) {
			font = m_DefaultFont;
		}

		float textPosX = pos.x;
		float textPosY = pos.y;

		const float textPosOriginX = textPosX;
		const float textPosOriginY = textPosY;

		if (has_flag(posFlags, UIPosFlag::HCENTER)) {
			textPosX -= (float)(font->calc_text_width(text) / 2);
		}
		if (has_flag(posFlags, UIPosFlag::VCENTER)) {
			textPosY -= (float)(font->maxBearingY / 2);
		}

		for (size_t i = 0; i < text.length(); ++i) {
			const char character = text[i];
			const GlyphData& glyphData = font->glyphs[character];

			if (character == ' ') {
				textPosX += glyphData.advanceX;
				continue;
			}

			if (character == '\n') {
				textPosX = textPosOriginX;
				textPosY += font->lineSpacing;
			}

			UIParams textParams = {};
			textParams.color = { 1.0f, 1.0f, 1.0f, 1.0f };
			textParams.position.x = i == 0 ? textPosX : textPosX + glyphData.bearingX; // Only use bearing if it's not the first character
			textParams.position.y = textPosY + (font->maxBearingY - glyphData.bearingY);
			textParams.size = { glyphData.width, glyphData.height };
			textParams.texCoords[0] = glyphData.texCoords[0];
			textParams.texCoords[1] = glyphData.texCoords[1];
			textParams.texCoords[2] = glyphData.texCoords[2];
			textParams.texCoords[3] = glyphData.texCoords[3];
			textParams.texIndex = m_GfxDevice.get_descriptor_index(font->atlasTexture, SubresourceType::SRV);
			textParams.uiType = UIType::TEXT;
			textParams.zOrder = zOrder;

			m_UIParamsData.push_back(textParams);

			textPosX += glyphData.advanceX;
		}
	}

	void UIPass::draw_rect(const glm::vec2& pos, int width, int height, const glm::vec4& col, UIPosFlag posFlags, const Texture* texture, uint32_t zOrder) {
		if (width == 0 || height == 0) {
			return;
		}

		uint32_t texIndex = texture != nullptr ? m_GfxDevice.get_descriptor_index(*texture, SubresourceType::SRV) : 0;

		glm::vec2 rectPos = pos;

		if (has_flag(posFlags, UIPosFlag::HCENTER)) {
			rectPos.x -= width / 2;
		}

		if (has_flag(posFlags, UIPosFlag::VCENTER)) {
			rectPos.y -= height / 2;
		}

		UIParams rectParams = {};
		rectParams.color = col;
		rectParams.position.x = rectPos.x;
		rectParams.position.y = rectPos.y;
		rectParams.size = { width, height };
		rectParams.texCoords[0] = { 0.0f, 0.0f };
		rectParams.texCoords[1] = { 1.0f, 0.0f };
		rectParams.texCoords[2] = { 1.0f, 1.0f };
		rectParams.texCoords[3] = { 0.0f, 1.0f };
		rectParams.texIndex = texIndex;
		rectParams.uiType = UIType::RECTANGLE;
		rectParams.zOrder = zOrder;

		m_UIParamsData.push_back(rectParams);
	}

	void UIPass::calc_cursor_origin() {
		if (m_SameLineWasActive) {
			m_CursorOrigin.x = m_SameLineCursorOrigin.x;
			m_LastCursorOriginDelta.y = m_SameLineYIncrement;
			m_SameLineWasActive = false;
		}

		if (m_SameLineIsActive) {
			m_CursorOrigin.x += m_LastCursorOriginDelta.x;
			m_SameLineYIncrement = std::max(m_SameLineYIncrement, (int)m_LastCursorOriginDelta.y);

			m_SameLineIsActive = false;
			m_SameLineWasActive = true;
		}
		else {
			m_CursorOrigin.y += m_LastCursorOriginDelta.y;
		}

		if (!m_SameLineIsActive && !m_SameLineWasActive) {
			m_SameLineCursorOrigin = m_CursorOrigin;
			m_SameLineYIncrement = 0;
		}
	}
}
