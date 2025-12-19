#pragma once

#include "Core/EnumBitmaskOperators.hpp"
#include "Graphics/GraphicsDevice.hpp"
#include "Graphics/FrameInfo.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

enum class SRPassType : uint8_t {
	Graphics,
	Compute
};

enum SRAccessFlag : uint8_t {
	None  = 0,
	Read  = 1 << 0,
	Write = 1 << 1
};
SR_ENABLE_BITMASK_OPERATORS(SRAccessFlag);

enum class SRSizeClass : uint8_t {
	None,
	SwapchainRelative
};

struct SRRenderPassAttachmentSubresource {
	SRResourceState state = SRResourceState::Undefined;
	SRAccessMask lastBarrierAccess = SRAccessMask::None;
	SRPipelineStage lastBarrierStage = SRPipelineStage::None;
};

struct SRRenderPassAttachment {
	inline bool is_read_in_pass(uint32_t passIdx) const {
		for (uint32_t i = 0; i < readInPasses.size(); ++i) {
			if (readInPasses[i] == passIdx) {
				return true;
			}
		}

		return false;
	}
	inline bool is_written_in_pass(uint32_t passIdx) const {
		for (uint32_t i = 0; i < writtenInPasses.size(); ++i) {
			if (writtenInPasses[i] == passIdx) {
				return true;
			}
		}

		return false;
	}

	SRTexture texture;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t mipLevels = 1;
	float depthClearValue = 0.0f; // NOTE: Only used for depth attachment
	SRFormat format = SRFormat::Unknown;
	SRSizeClass sizeClass = SRSizeClass::SwapchainRelative;
	enum class Type : uint8_t {
		RenderTarget,
		DepthStencil,
		ReadWriteTexture
	} type = Type::RenderTarget;
	std::vector<SRRenderPassAttachmentSubresource> subresourceStates;
	std::string name;

	std::vector<uint32_t> readInPasses = {};
	std::vector<uint32_t> writtenInPasses = {};
};

struct SRRenderPassAttachmentInput {
	SRRenderPassAttachment* attachment = nullptr;
	SRResourceState targetState = SRResourceState::Undefined;
	SRAccessFlag accessFlags = SRAccessFlag::None;
	SRSubresourceRange subresources = SRSubresourceRange::All();
};

// NOTE: The only purpose of a PrefabPass is to store pass data, whose life-
// time is bound to the render graph. The pass itself has no impact on the
// render graph execution order, and is only used for utility.
class PrefabPass {
public:
	PrefabPass(const std::string& name) : m_Name(name) {}
	~PrefabPass() {}

	template <typename T>
	T& allocate_pass_data() {
		m_PassData = std::make_shared<T>();
		return *static_cast<T*>(m_PassData.get());
	}

	template <typename T>
	T* get_pass_data() const {
		return static_cast<T*>(m_PassData.get());
	}

private:
	std::string m_Name;
	std::shared_ptr<void> m_PassData = nullptr;
};

class SRRenderGraph;
class SRRenderPass {
public:
	SRRenderPass(SRRenderGraph& frameGraph, uint32_t index, const std::string& name, SRPassType type) :
		m_RenderGraph(frameGraph), m_Index(index), m_Name(name), m_Type(type) {
	}
	~SRRenderPass() {}

	// ------------------------------ Inputs -------------------------------
	SRRenderPass& add_color_input(const std::string& name, SRAccessFlag accessFlags, SRSubresourceRange subrange = SRSubresourceRange::All());
	SRRenderPass& add_depth_input(const std::string& name); // NOTE: DSV only

	// ------------------------------ Outputs ------------------------------
	// TODO: Use an AttachmentInfo struct instead of this long signature
	SRRenderPass& add_color_output(const std::string& name, int width, int height, SRFormat format, int mipLevels = 1, SRSizeClass sizeClass = SRSizeClass::SwapchainRelative);
	SRRenderPass& add_depth_output(const std::string& name, int width, int height, SRFormat format, float clearValue = 0.0f, SRSizeClass sizeClass = SRSizeClass::SwapchainRelative);
	SRRenderPass& add_rw_texture_output(const std::string& name, int width, int height, SRFormat format, int mipLevels = 1, SRSizeClass sizeClass = SRSizeClass::SwapchainRelative);
	SRRenderPass& set_execute_callback(std::function<void(SRRenderPass& self, SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo)> callback);

	// --------------------------- Miscellaneous ---------------------------
	void execute(SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo);

	template <typename T>
	T& allocate_pass_data() {
		m_PassData = std::make_shared<T>();
		return *static_cast<T*>(m_PassData.get());
	}

	template <typename T>
	T* get_pass_data() const {
		return static_cast<T*>(m_PassData.get());
	}

	inline std::string get_name() const { return m_Name; }
	inline uint32_t get_index() const { return m_Index; }
	inline SRPassType get_type() const { return m_Type; }
	SRRenderPassAttachment* get_attachment(const std::string& name);
	const std::vector<SRRenderPassAttachment*>& get_output_attachments() const;
	const std::vector<SRRenderPassAttachmentInput>& get_input_attachments() const;

private:
	SRRenderGraph& m_RenderGraph;
	std::string m_Name;
	uint32_t m_Index;
	SRPassType m_Type;
	std::vector<SRRenderPassAttachmentInput> m_InputAttachments;
	std::vector<SRRenderPassAttachment*> m_OutputAttachments;
	std::function<void(SRRenderPass& self, SRGraphicsDevice& gfxDevice, const SRCmdList& cmdList, const SRFrameInfo& frameInfo)> m_ExecuteCallback;
	std::shared_ptr<void> m_PassData = nullptr;
};

class SRRenderGraph {
public:
	SRRenderGraph() {}
	~SRRenderGraph() {}

	SRRenderPass& add_render_pass(const std::string& name, SRPassType type);
	PrefabPass& add_prefab_pass(const std::string& name);

	void build(SRGraphicsDevice& gfxDevice);
	void execute(SRGraphicsDevice& gfxDevice, const SRSwapchain& swapchain, const SRCmdList& cmdList, const SRFrameInfo& frameInfo);
	void notify_swapchain_resize(SRGraphicsDevice& gfxDevice, int newWidth, int newHeight);

	SRRenderPassAttachment* get_attachment(const std::string& name);
	// TODO: Improve this garbage
	inline std::vector<SRRenderPass*> get_all_passes() {
		std::vector<SRRenderPass*> passes;

		for (const auto& pass : m_RenderPasses) {
			passes.push_back(pass.get());
		}

		return passes;
	}

private:
	std::vector<std::unique_ptr<SRRenderPass>> m_RenderPasses;
	std::vector<std::unique_ptr<SRRenderPassAttachment>> m_Attachments;
	std::unordered_map<std::string, size_t> m_PassIndexLUT;
	std::unordered_map<std::string, size_t> m_AttachmentIndexLUT;

	std::vector<std::unique_ptr<PrefabPass>> m_PrefabPasses;
	std::unordered_map<std::string, size_t> m_PrefabPassIndexLUT;
};
