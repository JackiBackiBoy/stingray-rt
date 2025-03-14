#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Graphics/RenderGraph.h"
#include "Core/EnumFlags.h"
#include "Data/Font.h"
#include "Managers/AssetManager.h"

#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace SR {
	enum class UIPosFlag {
		NONE = 0,
		HCENTER = 1 << 0,
		VCENTER = 1 << 1,
	};

	enum class UIEventType : uint32_t {
		None = 0x0000,

		// Mouse
		MouseMove = 0x0001,
		MouseDrag = 0x0002,
		MouseDown = 0x0003,
		MouseUp = 0x0004,
		MouseWheel = 0x0005,
		MouseEnter = 0x0006,
		MouseExit = 0x0007,
		MouseExitWindow = 0x0008,

		// Pure keyboard events
		KeyboardDown = 0x0010,
		KeyboardUp = 0x0020,
		KeyboardEnter = 0x0030,
		KeyboardExit = 0x0040,

		// Non-pure keyboard events
		KeyboardChar = 0x0500,

		// Navigation and focus
		FocusLost = 0x1000,
	};

	enum class UIDataType {
		S8,       // signed char / char (with sensible compilers)
		U8,       // unsigned char
		S16,      // short
		U16,      // unsigned short
		S32,      // int
		U32,      // unsigned int
		S64,      // long long / __int64
		U64,      // unsigned long long / unsigned __int64
		DOUBLE,   // double
	};

	struct KeyboardEventData {
		int key;
		int action;
		int mods;
	};

	struct KeyboardCharData {
		uint32_t codePoint = 0;
	};

	struct MouseButtons {
		bool left = false;
		bool right = false;
		bool middle = false;
	};

	struct MouseEventData {
		glm::vec2 position = {};
		glm::vec2 wheelDelta = {};
		MouseButtons causeButtons = {};
		MouseButtons downButtons = {};
		int clickCount = 0;
	};

	enum class WidgetAction : uint8_t {
		NONE = 0,
		HOVERED = 1 << 0,
		PRESSED = 1 << 1,
		CLICKED = 1 << 2, // NOTE: Will only be active for ONE frame
	};

	template<>
	struct enable_bitmask_operators<UIPosFlag> { static constexpr bool enable = true; };
	template<>
	struct enable_bitmask_operators<WidgetAction> { static constexpr bool enable = true; };

	class UIEvent {
	public:
		UIEvent(UIEventType type);
		~UIEvent() = default;

		inline UIEventType get_type() const { return m_Type; }
		inline MouseEventData& get_mouse_data() const {
			assert(m_EventMask == MOUSE_EVENT_MASK && "ASSERTION FAILED: Can not acquire mouse data from non-mouse event!");
			return *std::static_pointer_cast<MouseEventData>(m_Data);
		}
		inline KeyboardEventData& get_keyboard_data() const {
			assert(m_EventMask == KEYBOARD_EVENT_MASK && "ASSERTION FAILED: Can not acquire keyboard data from non-keyboard event!");
			return *std::static_pointer_cast<KeyboardEventData>(m_Data);
		}
		inline KeyboardCharData& get_keyboard_char_data() const {
			assert(m_EventMask == NON_PURE_KEYBOARD_EVENT_MASK && "ASSERTION FAILED: Can not acquire keyboard-char from non-keyboard-char event!");
			return *std::static_pointer_cast<KeyboardCharData>(m_Data);
		}

		inline bool is_mouse_event() const { return ((uint32_t)m_EventMask & MOUSE_EVENT_MASK) != 0; }
		inline bool is_keyboard_event() const { return ((uint32_t)m_EventMask & KEYBOARD_EVENT_MASK) != 0; }
		inline bool is_keyboard_char_event() const { return ((uint32_t)m_EventMask & NON_PURE_KEYBOARD_EVENT_MASK) != 0; }

		void set_type(UIEventType type);

	private:
		UIEventType m_Type = UIEventType::None;
		std::shared_ptr<void> m_Data = {};
		uint32_t m_EventMask = 0x0000;

		static constexpr uint32_t MOUSE_EVENT_MASK = 0x000F;
		static constexpr uint32_t KEYBOARD_EVENT_MASK = 0x00F0;
		static constexpr uint32_t NON_PURE_KEYBOARD_EVENT_MASK = 0x0F00;
	};

	class UIPass {
	public:
		UIPass(GraphicsDevice& gfxDevice, Window& window);
		~UIPass();

		void execute(PassExecuteInfo& executeInfo);

		// Menu
		void begin_menu_bar(int width);
		bool begin_menu(const std::string& text);
		bool menu_item(const std::string& text);
		void end_menu();
		void end_menu_bar();

		void begin_split(const std::string& text);
		void begin_panel(const std::string& text, float percentage);
		void end_panel();
		void end_split();

		// Widgets
		void widget_text(const std::string& text, int fixedWidth = 0);
		bool widget_button(const std::string& text);
		bool widget_checkbox(const std::string& text, bool* value);
		bool widget_slider_float(const std::string& text, float* value, float min, float max);
		void widget_text_input(const std::string& label, std::string& buffer);
		void widget_input_scalar(const std::string& label, void* scalar, UIDataType type);
		void widget_image(const Texture& texture, int width, int height);
		void widget_same_line();

		void process_event(const UIEvent& event);

		static constexpr int UI_PADDING = 8;

	private:
		static inline glm::vec4 rgb_to_norm(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
			return (1.0f / 255.0f) * glm::vec4((float)r, (float)g, (float)b, (float)a);
		}

		static constexpr uint32_t MAX_UI_PARAMS = 2048;
		static constexpr int UI_WIDGET_SLIDER_WIDTH = 300; // TODO: Should be a percentage instead
		static constexpr int UI_WIDGET_SLIDER_HANDLE_WIDTH = 20;
		static constexpr int UI_WIDGET_TEXT_INPUT_WIDTH = 300;
		static constexpr float UI_WIDGET_TEXT_INPUT_CARET_BLINK_RATE = 0.5f;
		static constexpr int UI_TITLEBAR_HEIGHT = 31;
		static constexpr glm::vec4 UI_PRIMARY_BACKGROUND_COL = { 0.094f, 0.094f, 0.094f, 1.0f };
		static constexpr glm::vec4 UI_PRIMARY_BORDER_COL = { 0.168f, 0.168f, 0.168f, 1.0f };
		static constexpr glm::vec4 UI_WIDGET_ACCENT_COL = { 0.0f, 0.470f, 0.831f, 1.0f };
		static constexpr glm::vec4 UI_WIDGET_PRIMARY_COL = { 0.2f, 0.2f, 0.2f, 1.0f };
		static constexpr glm::vec4 UI_WIDGET_PRIMARY_COL_HOV = { 0.4f, 0.4f, 0.4f, 1.0f };
		static constexpr glm::vec4 UI_WIDGET_PRIMARY_COL_PRESSED = { 0.5f, 0.5f, 0.5f, 1.0f };
		static constexpr glm::vec4 UI_WIDGET_SECONDARY_COL = { 0.3f, 0.3f, 0.3f, 1.0f };
		static constexpr glm::vec4 UI_WIDGET_HIGHLIGHT_COL = { 0.0f, 0.470f, 0.831f, 1.0f };
		static constexpr int UI_MENU_RIGHT_PADDING = 32;
		static constexpr int UI_WIDGET_CHECKBOX_SIZE = 24;

		enum UIType : uint8_t {
			RECTANGLE = 0,
			TEXT = 1 << 0,
		};

		enum class WidgetType : uint8_t {
			BUTTON,
			CHECKBOX,
			SLIDER_FLOAT,
			IMAGE,
			INPUT_TEXT,
			MENU,
			MENU_ITEM,
			SPLIT,
			PANEL,
		};

		struct UIWidgetState { // NOTE: Only used for certain widgets
			WidgetType type;
			WidgetAction actions = WidgetAction::NONE;
			bool enabled = true;
			std::string text;
			glm::vec2 position;
			int width;
			int height;
			uint64_t id;
			uint64_t parentID = 0;

			inline bool hit_test(glm::vec2 point) {
				return (
					enabled &&
					point.x >= position.x && point.x < position.x + width &&
					point.y >= position.y && point.y < position.y + height
					);
			}
		};

		struct PushConstant {
			glm::mat4 projectionMatrix;
			uint32_t uiParamsBufferIndex;
		} m_PushConstant = {};

		struct UIParams {
			glm::vec4 color = {};
			glm::vec2 position = {};
			glm::vec2 size = {};
			glm::vec2 texCoords[4] = {};
			uint32_t texIndex = 0;
			uint32_t uiType = 0;
			uint32_t zOrder = 0; // NOTE: Not used by shader
			uint32_t pad3;
		};

		void draw_text(const glm::vec2& pos, const std::string& text, UIPosFlag posFlags = UIPosFlag::NONE, const Font* font = nullptr, uint32_t zOrder = 0);
		void draw_rect(const glm::vec2& pos, int width, int height, const glm::vec4& col, UIPosFlag posFlags = UIPosFlag::NONE, const Texture* texture = nullptr, uint32_t zOrder = 0);
		//void draw_triangle(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, const glm::vec4& col, uint32_t zOrder = 0);
		void calc_cursor_origin();
		inline uint64_t widget_hash_combine(uint64_t hash1, uint64_t hash2) {
			return hash1 ^ (hash2 + 0x9e3779b97f4a7c15ULL + (hash1 << 6) + (hash1 >> 2));
		}

		GraphicsDevice& m_GfxDevice;
		Shader m_VertexShader = {};
		Shader m_PixelShader = {};
		Pipeline m_Pipeline = {};
		Buffer m_UIParamsBuffers[GraphicsDevice::FRAMES_IN_FLIGHT] = {};
		Font* m_DefaultFont = {};
		Font* m_DefaultBoldFont = {};
		Asset m_RightArrowIcon = {};
		Asset m_CheckIcon = {};
		Asset m_WindowIcon = {};
		Asset m_MinimizeIcon = {};
		Asset m_MaximizeIcon = {};
		Asset m_CloseIcon = {};

		std::vector<UIParams> m_UIParamsData = {};
		glm::vec2 m_DefaultCursorOrigin = { UI_PADDING, UI_TITLEBAR_HEIGHT + UI_PADDING };
		glm::vec2 m_CursorOrigin = m_DefaultCursorOrigin;
		glm::vec2 m_LastCursorOriginDelta = { 0.0f, 0.0f };
		int m_SameLineYIncrement = 0;

		glm::vec2 m_SameLineCursorOrigin = m_DefaultCursorOrigin;
		bool m_SameLineIsActive = false;
		bool m_SameLineWasActive = false;
		bool m_MainMenuActive = false;

		MouseEventData m_CurrentMouseData = {};
		UIEvent m_LastMouseEvent = UIEvent(UIEventType::None);
		UIEvent m_CurrentMouseEvent = UIEvent(UIEventType::None);
		UIEvent m_CurrentKeyboardEvent = UIEvent(UIEventType::None);
		UIEvent m_CurrentKeyboardCharEvent = UIEvent(UIEventType::None);

		std::unordered_map<uint64_t, UIWidgetState> m_WidgetStateMap = {};
		std::vector<uint64_t> m_WidgetStateMapIndices = {};
		std::unordered_map<uint64_t, glm::vec2> m_LastMenuDimensions = {};
		std::unordered_map<uint64_t, glm::vec2> m_MenuDimensions = {};
		std::vector<uint64_t> m_ActiveSplitIDs = {};
		uint64_t m_LastBegunMenuID = 0;
		uint64_t m_ActiveWidgetID = 0;
		uint64_t m_HoveredWidgetID = 0;
		uint64_t m_LastHoveredWidgetID = 0;
		uint64_t m_LastHoveredNonRootMenuID = 0;
		uint64_t m_ActivePanelID = 0;
		float m_CaretTimer = 0.0f;

		Window& m_Window;
		//GLFWwindow* m_Window = nullptr;
		//GLFWcursor* m_DefaultCursor = nullptr;
		//GLFWcursor* m_TextCursor = nullptr;
		//GLFWcursor* m_CurrentCursor = nullptr;

		bool m_HitAnyWidget = false;
	};
}
