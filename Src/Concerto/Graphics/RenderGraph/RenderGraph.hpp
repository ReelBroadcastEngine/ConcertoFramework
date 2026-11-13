//
// Created by arthur on 11/05/2026.
//

#ifndef CONCERTO_GRAPHICS_RHI_RENDERGRAPH_HPP
#define CONCERTO_GRAPHICS_RHI_RENDERGRAPH_HPP

#include <deque>
#include <functional>
#include <iosfwd>
#include <memory>
#include <vector>

#include <Concerto/Core/Result/Result.hpp>

#include "Concerto/Graphics/RenderGraph/RenderGraphBuilder.hpp"
#include "Concerto/Graphics/RenderGraph/RenderGraphCompiler.hpp"
#include "Concerto/Graphics/RenderGraph/RenderGraphContext.hpp"
#include "Concerto/Graphics/RenderGraph/RenderGraphError.hpp"
#include "Concerto/Graphics/RenderGraph/RenderGraphPass.hpp"
#include "Concerto/Graphics/RenderGraph/RenderGraphResource.hpp"
#include "Concerto/Graphics/RenderGraph/RenderGraphResourceRegistry.hpp"
#include "Concerto/Graphics/RenderGraph/RenderGraphStatistics.hpp"
#include "Concerto/Graphics/RHI/CommandBuffer.hpp"
#include "Concerto/Graphics/RHI/Defines.hpp"
#include "Concerto/Graphics/RHI/Device.hpp"
#include "Concerto/Graphics/RHI/FrameBuffer.hpp"
#include "Concerto/Graphics/RHI/QueryPool.hpp"

namespace cct::gfx::rhi
{
	// Resource lifetime: the graph keeps every resource an execution may still use until it knows that execution has
	// completed on the GPU. Each Execute has a serial (GetLastExecution). An owner that waits for its submissions
	// calls MarkExecutionsComplete after the wait, and what the graph retired since (transients released by Clear,
	// idle render passes and framebuffers) is released at once, so the next declaration reuses the same transients.
	// Without it, a retired set is released after maxFramesInFlight further calls to Reset. That rule also applies with
	// completion reports, as an upper bound: an owner must never have more than maxFramesInFlight executions in flight
	// when it calls Reset.
	//
	// Render passes and framebuffers are cached across Clear: a declaration that compiles to the same attachments
	// reuses them. A cached render pass or framebuffer, or a pooled transient, that no execution used for the idle
	// execution limit is released through the same completion rule. Only executions age the caches: a graph that is not
	// executed keeps them.
	class CONCERTO_GRAPHICS_RHI_BASE_API RenderGraph
	{
	public:
		static constexpr UInt32 DefaultIdleExecutionLimit = 8;

		explicit RenderGraph(Device& device, UInt32 maxFramesInFlight = 3);
		RenderGraph(const RenderGraph&) = delete;
		RenderGraph& operator=(const RenderGraph&) = delete;
		RenderGraph(RenderGraph&&) = default;
		RenderGraph& operator=(RenderGraph&&) = delete;

		// Resource declaration
		RGTextureHandle CreateTexture(const RGTextureDesc& desc);
		RGBufferHandle CreateBuffer(const RGBufferDesc& desc);

		// Import external resources (swapchain backbuffer, persistent textures)
		RGTextureHandle ImportTexture(const char* name,
									  std::shared_ptr<Texture> texture,
									  ImageLayout currentLayout = ImageLayout::Undefined,
									  PixelFormat format = PixelFormat::RGBA8_SRGB,
									  UInt32 width = 0,
									  UInt32 height = 0);
		RGBufferHandle ImportBuffer(const char* name,
									std::shared_ptr<Buffer> buffer,
									PipelineStageFlags initialStage = {},
									MemoryAccessFlags initialAccess = {});

		void UpdateImportedTexture(RGTextureHandle handle, std::shared_ptr<Texture> texture, ImageLayout currentLayout);

		// Pass declaration
		void AddPass(const char* name, RGPassType type,
					 std::function<void(RenderGraphBuilder&)> setup,
					 std::function<void(RenderGraphContext&)> execute);

		void AddGraphicsPass(const char* name,
							 std::function<void(RenderGraphBuilder&)> setup,
							 std::function<void(RenderGraphContext&)> execute);
		void AddComputePass(const char* name,
							std::function<void(RenderGraphBuilder&)> setup,
							std::function<void(RenderGraphContext&)> execute);
		void AddTransferPass(const char* name,
							 std::function<void(RenderGraphBuilder&)> setup,
							 std::function<void(RenderGraphContext&)> execute);

		void SetFinalOutput(RGTextureHandle handle);
		// Color used when a pass is the first writer of a color attachment (load op Clear).
		void SetClearColor(const Vector3f& color);

		void Compile();

		// Compile (if dirty) and allocate physical resources for all declared transients.
		// Idempotent; called by Execute(). Call it explicitly to access transient textures
		// (GetTexture) before the first Execute — e.g. to bind descriptor sets once at build time.
		// Fails when a transient exceeds the device limits or cannot be created; the transients allocated before it keep
		// their resource.
		[[nodiscard]] Result<void, RenderGraphError> AllocateResources();

		// Records the graph into cmd. When AllocateResources fails, records nothing and returns its error.
		[[nodiscard]] Result<void, RenderGraphError> Execute(CommandBuffer& cmd, UInt32 frameWidth, UInt32 frameHeight);
		void Reset();

		// Physical texture behind a handle. Transients require AllocateResources() first.
		// The assignment is stable until Clear(): descriptor sets bound to this texture
		// stay valid across frames.
		[[nodiscard]] Texture& GetTexture(RGTextureHandle handle);

		// Render pass compiled for the named graphics pass (or the group it was merged
		// into). Requires Compile() to have run. Useful to create pipelines ahead of the
		// first Execute() against a render pass that is format/attachment-compatible with
		// the one the graph will actually use at runtime.
		[[nodiscard]] const RenderPass& GetPassRenderPass(const char* name) const;

		// Update the tracked layout of an imported texture after an external pipeline barrier.
		// Call this when you manually emit a barrier on a texture that bypasses the graph's tracking.
		void SetTextureLayout(RGTextureHandle handle, ImageLayout layout);

		// Declare the layout a texture must be in after the last pass that writes it.
		// Execute() emits the required barrier automatically at the end of the frame.
		void SetExportLayout(RGTextureHandle handle, ImageLayout layout);

		// Query the layout currently tracked by the registry for an imported texture.
		// Useful after Execute() to determine which textures need a post-pass barrier.
		[[nodiscard]] ImageLayout GetCurrentLayout(RGTextureHandle handle) const;

		// Force recompilation on next Execute (e.g. after window resize or pass topology change).
		void MarkDirty();

		// Clear all passes and resource registrations while preserving pending frame resources
		// (framebuffers, transients) so in-flight GPU work remains safe.
		// Call after Reset(), before re-declaring a new set of passes.
		void Clear();

		// Debug: write graph as text to the given stream.
		void DumpGraph(std::ostream& out) const;

		[[nodiscard]] const RenderGraphStatistics& GetStatistics() const;
		[[nodiscard]] RenderGraphResidency GetResidency() const;

		// The serial of the last successful Execute, counted from 1; 0 before the first.
		[[nodiscard]] UInt64 GetLastExecution() const;
		// Declares that the submissions of every Execute up to execution have completed on the GPU.
		void MarkExecutionsComplete(UInt64 execution);
		void SetIdleExecutionLimit(UInt32 executions);

		// Measures the GPU time of the passes: while pool is set, Execute resets queries [0, GetTimestampQueryCount()) of
		// pool and writes a timestamp before the first pass and after each pass, unless pool has fewer queries. Pass
		// nullptr to stop. pool must outlive its use by the graph and its submissions.
		void SetTimestampQueryPool(QueryPool* pool);
		// The queries the last compilation needs: one per compiled pass, plus one.
		[[nodiscard]] UInt32 GetTimestampQueryCount() const;
		// Once the submission that executed the last Execute has completed and before Clear: adds the GPU time of its
		// passes to the statistics, by pass type. Returns false, adding nothing, when that Execute wrote no timestamps or
		// they are not available.
		bool ResolveTimestamps();

	private:
		// Members are destroyed in reverse declaration order: the framebuffer before the views it references, the views
		// before the textures they were created from.
		struct CachedFrameBuffer
		{
			const RenderPass* m_renderPass = nullptr;
			UInt32 m_width = 0;
			UInt32 m_height = 0;
			// Identifies the attachments: an expired entry never matches, so a destroyed texture whose address is reused
			// cannot alias it.
			std::vector<std::weak_ptr<Texture>> m_attachments;
			// Transient attachments are owned, so that the pool cannot destroy one while the entry exists. Imported ones
			// (swapchain images) are not, so that their owner can release them, for example to resize a swapchain.
			std::vector<std::shared_ptr<Texture>> m_ownedAttachments;
			std::vector<std::unique_ptr<TextureView>> m_views;
			std::unique_ptr<FrameBuffer> m_frameBuffer;
			UInt64 m_lastUse = 0;
		};

		// Resources the GPU may still use, released once execution lastUse has completed. Framebuffers are destroyed
		// first, render passes last.
		struct RetiredResources
		{
			UInt64 m_lastUse = 0;
			std::vector<std::unique_ptr<RenderPass>> m_renderPasses;
			std::vector<RenderGraphResourceRegistry::BufferTransientSnapshot> m_transientBuffers;
			std::vector<RenderGraphResourceRegistry::TextureTransientSnapshot> m_transientTextures;
			std::vector<CachedFrameBuffer> m_frameBuffers;
		};

		// nullptr when the device cannot create the framebuffer.
		FrameBuffer* AcquireFrameBuffer(const RGPass& pass, const RenderPass& renderPass, UInt32 frameWidth, UInt32 frameHeight, UInt64 execution);
		void RetireIdleObjects();
		void ReleaseCompletedResources();
		void ReleaseRetired(RetiredResources& retired);

		Device& m_device;
		std::vector<RGPass> m_passes;
		RenderGraphResourceRegistry m_registry;
		RenderGraphCompiler m_compiler;
		std::vector<RGCompiledPass> m_compiledPasses;
		std::vector<CachedFrameBuffer> m_frameBufferCache;
		RetiredResources m_retired; // staged by Clear() and idle retirement, moved into m_pendingFrames by Reset()
		std::deque<RetiredResources> m_pendingFrames;
		UInt64 m_lastExecution = 0;
		UInt64 m_completedExecution = 0;
		UInt32 m_idleExecutionLimit = DefaultIdleExecutionLimit;
		UInt32 m_maxFramesInFlight;
		RGTextureHandle m_finalOutput;
		Vector3f m_clearColor{0.F, 0.F, 0.F};
		bool m_dirty = true;
		RenderGraphStatistics m_statistics;
		QueryPool* m_timestampPool = nullptr;
		// The passes timed by the last Execute, in recording order.
		std::vector<RGPassType> m_timedPassTypes;
		std::vector<UInt64> m_timestampTicks;
		// The framebuffer of each compiled pass that opens a render pass, acquired by Execute before it records.
		std::vector<FrameBuffer*> m_passFrameBuffers;
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_RENDERGRAPH_HPP
