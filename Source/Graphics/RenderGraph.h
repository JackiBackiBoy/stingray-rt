#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Core/FrameInfo.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

class RenderGraph;

enum class AttachmentType : uint8_t {
	RENDER_TARGET,
	DEPTH_STENCIL,
	RW_TEXTURE
};

struct AttachmentInfo {
	uint32_t width = 1;
	uint32_t height = 1;
	AttachmentType type = AttachmentType::RENDER_TARGET;
	Format format = Format::UNKNOWN;
};

struct RenderPassAttachment {
	AttachmentInfo info = {};
	Texture texture = {};
	std::string name = {};
	ResourceState currentState = ResourceState::UNDEFINED;

	std::vector<uint32_t> readInPasses = {};
	std::vector<uint32_t> writtenInPasses = {};
};

struct PassExecuteInfo {
	RenderGraph* renderGraph;
	GFXDevice* gfxDevice;
	const CommandList* cmdList;
	const FrameInfo* frameInfo;
};

class RenderPass {
public:
	RenderPass(RenderGraph& graph, uint32_t index, const std::string& name) :
		m_Graph(graph), m_Index(index), m_Name(name) {}
	~RenderPass() {}

	void add_input_attachment(const std::string& name);
	void add_output_attachment(const std::string& name, const AttachmentInfo& info);
	void execute(GFXDevice& gfxDevice, const CommandList& cmdList, const FrameInfo& frameInfo);

	inline std::vector<RenderPassAttachment*>& get_input_attachments() { return m_InputAttachments; }
	inline std::vector<RenderPassAttachment*>& get_output_attachments() { return m_OutputAttachments; }
	inline void set_execute_callback(std::function<void(PassExecuteInfo& executeInfo)> callback) { m_ExecuteCallback = std::move(callback); }

private:
	RenderGraph& m_Graph;
	uint32_t m_Index;
	std::string m_Name;
	std::function<void(PassExecuteInfo& executeInfo)> m_ExecuteCallback;

	std::vector<RenderPassAttachment*> m_InputAttachments = {};
	std::vector<RenderPassAttachment*> m_OutputAttachments = {};
};

class RenderGraph {
public:
	RenderGraph(GFXDevice& gfxDevice) : m_GfxDevice(gfxDevice) {}
	~RenderGraph() {}

	RenderPass* add_pass(const std::string& name);
	RenderPassAttachment* get_attachment(const std::string& name);

	void build();
	void execute(const SwapChain& swapChain, const CommandList& cmdList, const FrameInfo& frameInfo);

private:
	void recurse_build(uint32_t index);

	GFXDevice& m_GfxDevice;

	std::vector<std::unique_ptr<RenderPass>> m_Passes = {};
	std::vector<std::unique_ptr<RenderPassAttachment>> m_Attachments = {};
	std::unordered_map<std::string, size_t> m_PassIndexLUT = {};
	std::unordered_map<std::string, size_t> m_AttachmentIndexLUT = {};
};