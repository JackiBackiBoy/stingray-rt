#include "ui_pass.h"
#include "../../platform.h"
#include "../../managers/asset_manager.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>

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
UIPass::UIPass(GFXDevice& gfxDevice, GLFWwindow* window) :
	m_GfxDevice(gfxDevice), m_Window(window) {

	// GLFW objects
	m_DefaultCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
	m_TextCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
	m_CurrentCursor = m_DefaultCursor;

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

	// Load font
	m_DefaultFont = assetmanager::load_font_from_file("resources/fonts/consola.ttf", 16);

	for (size_t i = 0; i < GFXDevice::FRAMES_IN_FLIGHT; ++i) {
		m_GfxDevice.create_buffer(uiParamsBufferInfo, m_UIParamsBuffers[i], m_UIParamsData.data());
	}
}

UIPass::~UIPass() {
	delete m_DefaultCursor;
	m_DefaultCursor = nullptr;

	delete m_TextCursor;
	m_TextCursor = nullptr;
}

void UIPass::execute(PassExecuteInfo& executeInfo) {
	const CommandList& cmdList = *executeInfo.cmdList;
	const FrameInfo& frameInfo = *executeInfo.frameInfo;

	m_PushConstant.projectionMatrix = glm::ortho(0.0f, (float)frameInfo.width, (float)frameInfo.height, 0.0f);
	m_PushConstant.uiParamsBufferIndex = m_GfxDevice.get_descriptor_index(m_UIParamsBuffers[m_GfxDevice.get_frame_index()]);

	// Update
	static float x = 0.5f;
	static std::string text = "Hello World!";
	static std::string text2 = "Testing!";

	if (m_ActiveWidgetID != 0 && m_WidgetStateMap[m_ActiveWidgetID].type == WidgetType::INPUT_TEXT) {
		m_CaretTimer += frameInfo.dt;
	}
	else {
		m_CaretTimer = 0.0f;
	}

	memcpy(m_UIParamsBuffers[m_GfxDevice.get_frame_index()].mappedData, m_UIParamsData.data(), m_UIParamsData.size() * sizeof(UIParams));

	// Rendering
	m_GfxDevice.bind_pipeline(m_Pipeline, cmdList);
	m_GfxDevice.push_constants(&m_PushConstant, sizeof(m_PushConstant), cmdList);

	m_GfxDevice.draw_instanced(6, static_cast<uint32_t>(m_UIParamsData.size()), 0, 0, cmdList);

	m_UIParamsData.clear();

	m_CursorOrigin = m_DefaultCursorOrigin;
	m_LastCursorOriginDelta = { 0.0f, 0.0f };
	m_SameLineIsActive = false;
	m_SameLineWasActive = false;

	if (m_ActiveWidgetID != 0) {
		UIWidgetState& activeState = m_WidgetStateMap[m_ActiveWidgetID];

		if (has_flag(activeState.actions, WidgetAction::CLICKED)) {
			activeState.actions &= ~WidgetAction::CLICKED;

			if (activeState.type != WidgetType::INPUT_TEXT) {
				m_ActiveWidgetID = 0;
			}
		}
	}

	// Reset events
	m_CurrentKeyboardEvent = UIEvent(UIEventType::None);
	m_CurrentKeyboardCharEvent = UIEvent(UIEventType::None);
}

void UIPass::widget_text(const std::string& text) {
	calc_cursor_origin();

	draw_text(m_CursorOrigin, text);

	m_LastCursorOriginDelta.x = m_DefaultFont->calc_text_width(text) + UI_PADDING * 2;
	m_LastCursorOriginDelta.y = m_DefaultFont->boundingBoxHeight + UI_PADDING;
}

bool UIPass::widget_button(const std::string& text) {
	calc_cursor_origin();

	// Widget ID hash
	const uint64_t idHash = std::hash<std::string>{}(text);
	const uint64_t typeHash = std::hash<WidgetType>{}(WidgetType::BUTTON);
	const uint64_t id = widget_hash_combine(idHash, typeHash);

	const int width = m_DefaultFont->calc_text_width(text) + UI_PADDING * 2;
	const int height = m_DefaultFont->boundingBoxHeight + UI_PADDING * 2;

	UIWidgetState state = {};
	state.position = m_CursorOrigin;
	state.width = width;
	state.height = height;
	state.id = id;
	state.text = text;
	state.type = WidgetType::BUTTON;

	const auto search = m_WidgetStateMap.find(id);

	if (search == m_WidgetStateMap.end()) {
		m_WidgetStateMap.insert({ id, state });
	}
	else {
		state = search->second;
	}

	bool ret = has_flag(state.actions, WidgetAction::CLICKED);
	glm::vec4 color = UI_WIDGET_PRIMARY_COL;

	if (has_flag(state.actions, WidgetAction::PRESSED)) {
		color = UI_WIDGET_PRIMARY_COL_CLK;
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

bool UIPass::widget_slider_float(const std::string& text, float* value, float min, float max) {
	assert(value != nullptr);
	assert(min < max);

	calc_cursor_origin();

	const int trackHeight = m_DefaultFont->boundingBoxHeight + UI_PADDING * 2;
	const int handleHeight = trackHeight - 2;
	const int innerTrackWidth = UI_WIDGET_SLIDER_WIDTH - 2;

	const uint64_t textHash = std::hash<std::string>{}(text);
	const uint64_t typeHash = std::hash<WidgetType>{}(WidgetType::SLIDER_FLOAT);
	const uint64_t pValHash = std::hash<float*>{}(value);
	const uint64_t id = widget_hash_combine(widget_hash_combine(textHash, typeHash), pValHash);

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
	}
	else {
		state = search->second;
	}

	if (has_flag(state.actions, WidgetAction::PRESSED)) {
		if (m_CurrentMouseEvent.is_mouse_event()) {
			const MouseEventData& mouse = m_CurrentMouseEvent.get_mouse_data();

			const int innerTrackRelMouseX = std::min(mouse.position.x - m_CursorOrigin.x - 1 - UI_WIDGET_SLIDER_HANDLE_WIDTH / 2, m_CursorOrigin.x + 1 + innerTrackWidth - UI_WIDGET_SLIDER_HANDLE_WIDTH / 2);
			const float percentage = std::clamp((float)innerTrackRelMouseX / (innerTrackWidth - UI_WIDGET_SLIDER_HANDLE_WIDTH), 0.0f, 1.0f);
			*value = min + fabsf(max - min) * percentage;
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

	m_LastCursorOriginDelta.x = state.width; + UI_PADDING;
	m_LastCursorOriginDelta.y = state.height + UI_PADDING;

	return false;
}

void UIPass::widget_text_input(const std::string& label, std::string& buffer) {
	calc_cursor_origin();

	const int boxWidth = UI_WIDGET_TEXT_INPUT_WIDTH;
	const int boxHeight = m_DefaultFont->boundingBoxHeight + UI_PADDING * 2;
	const int caretWidth = 1;
	const int caretHeight = boxHeight - UI_PADDING;
	const glm::vec2 textOrigin = m_CursorOrigin + glm::vec2(UI_PADDING, boxHeight / 2);

	const uint64_t idHash = std::hash<std::string>{}(label);
	const uint64_t typeHash = std::hash<WidgetType>{}(WidgetType::INPUT_TEXT);
	const uint64_t id = widget_hash_combine(idHash, typeHash);

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

			if (keyEventData.key == GLFW_KEY_LEFT && (keyEventData.action == GLFW_PRESS || keyEventData.action == GLFW_REPEAT)) {
				m_CaretTimer = 0.0f;

				if (currCaretIndex > 0) {
					if (keyEventData.mods == GLFW_MOD_SHIFT) {
						--currCaretIndex;

						if (currCaretIndex == downCaretIndex) {
							isHighlighting = false;
						}
						else {
							isHighlighting = true;
							currCaretPos = textOrigin + glm::vec2(m_DefaultFont->calc_text_width(
								buffer.substr(0, currCaretIndex)
							), 0);
						}
					}
					else if (keyEventData.mods == 0) {
						--currCaretIndex;
						downCaretIndex = currCaretIndex;
						downCaretPos = textOrigin + glm::vec2(m_DefaultFont->calc_text_width(buffer.substr(0, currCaretIndex)), 0);
						currCaretPos = downCaretPos;
					}
				}
			}
			else if (keyEventData.key == GLFW_KEY_RIGHT && (keyEventData.action == GLFW_PRESS || keyEventData.action == GLFW_REPEAT)) {
				m_CaretTimer = 0.0f;

				if (currCaretIndex < buffer.length()) {
					if (keyEventData.mods == GLFW_MOD_SHIFT) {
						++currCaretIndex;

						if (currCaretIndex == downCaretIndex) {
							isHighlighting = false;
						}
						else {
							isHighlighting = true;
							currCaretPos = textOrigin + glm::vec2(m_DefaultFont->calc_text_width(
								buffer.substr(0, currCaretIndex)
							), 0);
						}
					}
					else if (keyEventData.mods == 0) {

						++currCaretIndex;
						downCaretIndex = currCaretIndex;
						downCaretPos = textOrigin + glm::vec2(m_DefaultFont->calc_text_width(buffer.substr(0, currCaretIndex)), 0);
						currCaretPos = downCaretPos;
					}
				}
			}

			if (!buffer.empty() && keyEventData.key == GLFW_KEY_BACKSPACE && (keyEventData.action == GLFW_PRESS || keyEventData.action == GLFW_REPEAT)) {
				const size_t firstIndex = std::min(currCaretIndex, downCaretIndex);
				const size_t lastIndex = std::max(currCaretIndex, downCaretIndex);

				if (firstIndex != lastIndex) { // highlight active
					buffer = buffer.erase(firstIndex, lastIndex - firstIndex);

					downCaretPos = textOrigin + glm::vec2(m_DefaultFont->calc_text_width(buffer.substr(0, firstIndex)), 0);
					currCaretPos = downCaretPos;
					downCaretIndex = firstIndex;
					currCaretIndex = downCaretIndex;
				}
				else if (firstIndex > 0) {
					buffer = buffer.erase(firstIndex - 1, 1);

					downCaretPos = textOrigin + glm::vec2(m_DefaultFont->calc_text_width(buffer.substr(0, firstIndex - 1)), 0);
					currCaretPos = downCaretPos;
					downCaretIndex = firstIndex - 1;
					currCaretIndex = downCaretIndex;
				}

				m_CaretTimer = 0.0f;
				isHighlighting = false;
			}
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
			for (auto& [id, state] : m_WidgetStateMap) {
				if (state.hit_test(mouse.position)) {
					hitAny = true;
					state.actions |= WidgetAction::HOVERED;
					m_HoveredWidgetID = id;
				}
				else {
					state.actions &= ~WidgetAction::HOVERED;
				}
			}

			if (!hitAny) {
				m_HoveredWidgetID = 0;
			}

			if (m_CurrentCursor != m_DefaultCursor && m_HoveredWidgetID == 0) {
				m_CurrentCursor = m_DefaultCursor;
				glfwSetCursor(m_Window, m_DefaultCursor);
			}
			else if (m_CurrentCursor != m_TextCursor && m_WidgetStateMap[m_HoveredWidgetID].type == WidgetType::INPUT_TEXT) {
				m_CurrentCursor = m_TextCursor;
				glfwSetCursor(m_Window, m_TextCursor);
			}
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
			if (m_ActiveWidgetID != 0) {
				if (has_flag(m_WidgetStateMap[m_ActiveWidgetID].actions, WidgetAction::HOVERED) &&
					has_flag(m_WidgetStateMap[m_ActiveWidgetID].actions, WidgetAction::PRESSED)) {
					m_WidgetStateMap[m_ActiveWidgetID].actions |= WidgetAction::CLICKED;
					m_WidgetStateMap[m_ActiveWidgetID].actions &= ~WidgetAction::PRESSED;
				}
				else {
					m_WidgetStateMap[m_ActiveWidgetID].actions &= ~WidgetAction::PRESSED;
				}
			}
		}
		break;
	}
}

void UIPass::draw_text(const glm::vec2& pos, const std::string& text, UIPosFlag posFlags) {
	float textPosX = pos.x;
	float textPosY = pos.y;

	const float textPosOriginX = textPosX;
	const float textPosOriginY = textPosY;

	if (has_flag(posFlags, UIPosFlag::HCENTER)) {
		textPosX -= (float)(m_DefaultFont->calc_text_width(text) / 2);
	}
	if (has_flag(posFlags, UIPosFlag::VCENTER)) {
		textPosY -= (float)(m_DefaultFont->maxBearingY / 2);
	}

	for (size_t i = 0; i < text.length(); ++i) {
		const char character = text[i];
		const GlyphData& glyphData = m_DefaultFont->glyphs[character];

		if (character == ' ') {
			textPosX += glyphData.advanceX;
			continue;
		}

		if (character == '\n') {
			textPosX = textPosOriginX;
			textPosY += m_DefaultFont->lineSpacing;
		}

		UIParams textParams = {};
		textParams.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		textParams.position.x = i == 0 ? textPosX : textPosX + glyphData.bearingX; // Only use bearing if it's not the first character
		textParams.position.y = textPosY + (m_DefaultFont->maxBearingY - glyphData.bearingY);
		textParams.size = { glyphData.width, glyphData.height };
		textParams.texCoords[0] = glyphData.texCoords[0];
		textParams.texCoords[1] = glyphData.texCoords[1];
		textParams.texCoords[2] = glyphData.texCoords[2];
		textParams.texCoords[3] = glyphData.texCoords[3];
		textParams.texIndex = m_GfxDevice.get_descriptor_index(m_DefaultFont->atlasTexture);
		textParams.uiType = UIType::TEXT;
		m_UIParamsData.push_back(textParams);

		textPosX += glyphData.advanceX;
	}
}

void UIPass::draw_rect(const glm::vec2& pos, int width, int height, const glm::vec4& col, UIPosFlag posFlags, const Texture* texture) {
	uint32_t texIndex = texture != nullptr ? m_GfxDevice.get_descriptor_index(*texture) : 0;

	glm::vec2 rectPos = pos;

	if (has_flag(posFlags, UIPosFlag::HCENTER)) {
		rectPos.x -= (float)width / 2;
	}

	if (has_flag(posFlags, UIPosFlag::VCENTER)) {
		rectPos.y -= (float)height / 2;
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

	m_UIParamsData.push_back(rectParams);
}

void UIPass::calc_cursor_origin() {
	if (m_SameLineWasActive) {
		m_CursorOrigin.x = m_SameLineCursorOrigin.x;

		m_SameLineWasActive = false;
	}

	if (m_SameLineIsActive) {
		m_CursorOrigin.x += m_LastCursorOriginDelta.x;

		m_SameLineIsActive = false;
		m_SameLineWasActive = true;
	}
	else {
		m_CursorOrigin.y += m_LastCursorOriginDelta.y;
	}

	if (!m_SameLineIsActive && !m_SameLineWasActive) {
		m_SameLineCursorOrigin = m_CursorOrigin;
	}
}
