//
// Created by arthur on 11/05/2026.
//

#ifndef CONCERTO_GRAPHICS_RHI_RENDERGRAPH_COMPILER_HPP
#define CONCERTO_GRAPHICS_RHI_RENDERGRAPH_COMPILER_HPP

#include <cstddef>
#include <memory>
#include <span>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Concerto/Graphics/RenderGraph/RenderGraphPass.hpp"
#include "Concerto/Graphics/RenderGraph/RenderGraphResource.hpp"
#include "Concerto/Graphics/RenderGraph/RenderGraphStatistics.hpp"
#include "Concerto/Graphics/RHI/Defines.hpp"
#include "Concerto/Graphics/RHI/Enums.hpp"
#include "Concerto/Graphics/RHI/RenderPass.hpp"

namespace cct::gfx::rhi
{
	class Device;
	class RenderGraphResourceRegistry;

	class CONCERTO_GRAPHICS_RHI_BASE_API RenderGraphCompiler
	{
	public:
		RenderGraphCompiler() = default;
		~RenderGraphCompiler() = default;
		RenderGraphCompiler(const RenderGraphCompiler&) = delete;
		RenderGraphCompiler(RenderGraphCompiler&&) noexcept = default;
		RenderGraphCompiler& operator=(const RenderGraphCompiler&) = delete;
		RenderGraphCompiler& operator=(RenderGraphCompiler&&) noexcept = default;

		std::vector<RGCompiledPass> Compile(
			std::vector<RGPass>& passes,
			RenderGraphResourceRegistry& registry,
			Device& device,
			RGTextureHandle finalOutput,
			RenderGraphStatistics& statistics);

		// Returns the required ImageLayout for a given texture usage. A Compute pass writes
		// storage images and reads them in ImageLayout::General; a sampled (non-storage)
		// texture read by a Compute pass stays in ImageLayout::ShaderReadOnlyOptimal.
		static ImageLayout RequiredLayout(const RGTextureUsage& usage, RGPassType passType, bool isStorage);

		// Returns (srcStage, dstStage, srcAccess, dstAccess) for a layout transition
		static std::tuple<PipelineStageFlags, PipelineStageFlags, MemoryAccessFlags, MemoryAccessFlags>
		InferBarrierParams(ImageLayout oldLayout, ImageLayout newLayout);

		static std::pair<PipelineStageFlags, MemoryAccessFlags>
		BufferAccessParams(RGPassType type, RGResourceAccess access);

		// Render passes outlive compilations: a compiled pass refers to a cached one, created once per distinct attachment
		// description. Records that execution began renderPass.
		void MarkRenderPassUsed(const RenderPass* renderPass, UInt64 execution);
		// Moves into retired the cached render passes that no pass of inUse refers to and that no execution used in the
		// idleExecutions executions up to lastExecution. Returns the serial of the last execution that used one of them.
		UInt64 RetireIdleRenderPasses(UInt64 lastExecution, UInt64 idleExecutions, std::span<const RGCompiledPass> inUse, std::vector<std::unique_ptr<RenderPass>>& retired);
		[[nodiscard]] std::size_t GetCachedRenderPassCount() const;

	private:
		struct CachedRenderPass
		{
			std::vector<RenderPass::Attachment> m_attachments;
			bool m_hasDepth = false;
			std::unique_ptr<RenderPass> m_renderPass;
			UInt64 m_lastUse = 0;
		};

		RenderPass* AcquireRenderPass(std::vector<RenderPass::Attachment>&& attachments, bool hasDepth, Device& device, RenderGraphStatistics& statistics);

		struct PassDep
		{
			std::vector<UInt32> incoming; // passes this pass depends on
		};

		void BuildDependencies(std::vector<RGPass>& passes);
		void CullPasses(std::vector<RGPass>& passes, RGTextureHandle finalOutput);
		std::vector<UInt32> TopologicalSort(const std::vector<RGPass>& passes);
		static std::vector<RenderPass::Attachment> DescribeAttachments(const RGPass& pass, const std::unordered_set<UInt16>& alreadyWritten, const RenderGraphResourceRegistry& registry);

		std::unordered_map<UInt16, UInt32> m_textureLastWriter; // texIdx → last writer passIdx
		std::unordered_map<UInt16, UInt32> m_bufferLastWriter;
		std::vector<PassDep> m_deps;
		std::vector<std::vector<UInt32>> m_adjOut; // outgoing edges: adjOut[A] = {B,...} A→B
		std::vector<CachedRenderPass> m_renderPassCache;
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_RENDERGRAPH_COMPILER_HPP
