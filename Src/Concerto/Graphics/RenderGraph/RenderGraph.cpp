//
// Created by arthur on 11/05/2026.
//

#include "Concerto/Graphics/RenderGraph/RenderGraph.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <ostream>
#include <span>
#include <stdexcept>

#include <Concerto/Core/Assert.hpp>

#include "Concerto/Graphics/RenderGraph/RenderGraphCompiler.hpp"

namespace cct::gfx::rhi
{
	namespace
	{
		using Clock = std::chrono::steady_clock;
	} // namespace

	RenderGraph::RenderGraph(Device& device, UInt32 maxFramesInFlight) :
		m_device(device),
		m_maxFramesInFlight(maxFramesInFlight)
	{
	}

	RGTextureHandle RenderGraph::CreateTexture(const RGTextureDesc& desc)
	{
		MarkDirty();
		return m_registry.RegisterTexture(desc);
	}

	RGBufferHandle RenderGraph::CreateBuffer(const RGBufferDesc& desc)
	{
		MarkDirty();
		return m_registry.RegisterBuffer(desc);
	}

	RGTextureHandle RenderGraph::ImportTexture(const char* name,
											   std::shared_ptr<Texture> texture,
											   ImageLayout currentLayout,
											   PixelFormat format,
											   UInt32 width,
											   UInt32 height)
	{
		MarkDirty();
		return m_registry.ImportTexture(name, std::move(texture), currentLayout, format, width, height);
	}

	RGBufferHandle RenderGraph::ImportBuffer(const char* name,
											 std::shared_ptr<Buffer> buffer,
											 PipelineStageFlags initialStage,
											 MemoryAccessFlags initialAccess)
	{
		MarkDirty();
		return m_registry.ImportBuffer(name, std::move(buffer), initialStage, initialAccess);
	}

	void RenderGraph::UpdateImportedTexture(RGTextureHandle handle, std::shared_ptr<Texture> texture, ImageLayout currentLayout)
	{
		m_registry.UpdateImportedTexture(handle, std::move(texture), currentLayout);
	}

	void RenderGraph::AddPass(const char* name, RGPassType type,
							  std::function<void(RenderGraphBuilder&)> setup,
							  std::function<void(RenderGraphContext&)> execute)
	{
		MarkDirty();

		RGPass pass;
		pass.name = name;
		pass.type = type;
		pass.execute = std::move(execute);

		RenderGraphBuilder builder(pass);
		setup(builder);

		m_passes.push_back(std::move(pass));
	}

	void RenderGraph::AddGraphicsPass(const char* name,
									  std::function<void(RenderGraphBuilder&)> setup,
									  std::function<void(RenderGraphContext&)> execute)
	{
		AddPass(name, RGPassType::Graphics, std::move(setup), std::move(execute));
	}

	void RenderGraph::AddComputePass(const char* name,
									 std::function<void(RenderGraphBuilder&)> setup,
									 std::function<void(RenderGraphContext&)> execute)
	{
		AddPass(name, RGPassType::Compute, std::move(setup), std::move(execute));
	}

	void RenderGraph::AddTransferPass(const char* name,
									  std::function<void(RenderGraphBuilder&)> setup,
									  std::function<void(RenderGraphContext&)> execute)
	{
		AddPass(name, RGPassType::Transfer, std::move(setup), std::move(execute));
	}

	void RenderGraph::SetFinalOutput(RGTextureHandle handle)
	{
		m_finalOutput = handle;
		MarkDirty();
	}

	void RenderGraph::SetClearColor(const Vector3f& color)
	{
		m_clearColor = color;
	}

	void RenderGraph::Compile()
	{
		const Clock::time_point start = Clock::now();
		m_compiledPasses = m_compiler.Compile(m_passes, m_registry, m_device, m_finalOutput, m_statistics);
		m_dirty = false;
		++m_statistics.m_compilations;
		m_statistics.m_compileTime += Clock::now() - start;
	}

	Result<void, RenderGraphError> RenderGraph::AllocateResources()
	{
		if (m_dirty)
			Compile();

		const Clock::time_point start = Clock::now();
		Result<void, RenderGraphError> allocated = m_registry.Allocate(m_device, m_statistics);
		m_statistics.m_allocationTime += Clock::now() - start;
		return allocated;
	}

	Texture& RenderGraph::GetTexture(RGTextureHandle handle)
	{
		return m_registry.GetTexture(handle);
	}

	const RenderPass& RenderGraph::GetPassRenderPass(const char* name) const
	{
		for (std::size_t i = 0; i < m_compiledPasses.size(); ++i)
		{
			if (m_passes[m_compiledPasses[i].passIndex].name != name)
				continue;

			for (std::size_t j = i + 1; j-- > 0;)
			{
				if (m_compiledPasses[j].renderPass)
					return *m_compiledPasses[j].renderPass;
			}
		}

		throw std::runtime_error(std::format("RenderGraph::GetPassRenderPass: no compiled render pass for '{}' (culled, never declared, or Compile() not called)", name));
	}

	Result<void, RenderGraphError> RenderGraph::Execute(CommandBuffer& cmd, UInt32 frameWidth, UInt32 frameHeight)
	{
		m_timedPassTypes.clear();
		Result<void, RenderGraphError> allocated = AllocateResources();
		if (allocated.IsError())
		{
			return allocated;
		}
		const Clock::time_point recordStart = Clock::now();
		const UInt64 execution = m_lastExecution + 1;
		m_passFrameBuffers.assign(m_compiledPasses.size(), nullptr);
		for (std::size_t index = 0; index < m_compiledPasses.size(); ++index)
		{
			const RGCompiledPass& compiled = m_compiledPasses.at(index);
			if (compiled.renderPass == nullptr)
			{
				continue;
			}
			m_compiler.MarkRenderPassUsed(compiled.renderPass, execution);
			FrameBuffer* frameBuffer = AcquireFrameBuffer(m_passes.at(compiled.passIndex), *compiled.renderPass, frameWidth, frameHeight, execution);
			if (frameBuffer == nullptr)
			{
				return RenderGraphError::ResourceCreationFailed;
			}
			m_passFrameBuffers.at(index) = frameBuffer;
		}
		m_lastExecution = execution;
		++m_statistics.m_executions;
		const UInt32 timestampCount = GetTimestampQueryCount();
		const bool timed = m_timestampPool != nullptr && m_timestampPool->GetCapacity() >= timestampCount;
		if (timed)
		{
			cmd.ResetQueries(*m_timestampPool, 0, timestampCount);
			cmd.WriteTimestamp(*m_timestampPool, 0);
		}

		RenderPass* activeRenderPass = nullptr;
		FrameBuffer* currentFrameBuffer = nullptr;

		struct BufferAccessState
		{
			PipelineStageFlags stage;
			MemoryAccessFlags access;
			bool isWrite;
		};
		std::unordered_map<UInt16, BufferAccessState> bufferStates;
		const auto bufferAccessWasWrite = [](MemoryAccessFlags access)
		{
			return access.Contains(MemoryAccess::ShaderWrite) || access.Contains(MemoryAccess::TransferWrite);
		};

		// Emit layout-transition barriers for all texture usages in a pass.
		// Must only be called when no render pass is active.
		const auto emitBarriersForPass = [&](const RGPass& p)
		{
			std::vector<TextureBarrier> textureBarriers;
			textureBarriers.reserve(p.textureUsages.size());
			for (const RGTextureUsage& usage : p.textureUsages)
			{
				const ImageLayout required = RenderGraphCompiler::RequiredLayout(usage, p.type, m_registry.GetDesc(usage.handle).storage);
				const ImageLayout current = m_registry.GetCurrentLayout(usage.handle);
				if (current == required)
					continue;
				auto [srcStage, dstStage, srcAccess, dstAccess] =
					RenderGraphCompiler::InferBarrierParams(current, required);
				textureBarriers.push_back(TextureBarrier{
					&m_registry.GetTexture(usage.handle), current, required, srcStage, dstStage, srcAccess, dstAccess});
				m_registry.SetCurrentLayout(usage.handle, required);
			}
			cmd.PipelineBarrier(std::span<const TextureBarrier>(textureBarriers));

			for (const RGBufferUsage& usage : p.bufferUsages)
			{
				auto [stage, access] = RenderGraphCompiler::BufferAccessParams(p.type, usage.access);
				const bool isWrite = (usage.access == RGResourceAccess::Write);

				auto it = bufferStates.find(usage.handle.index);
				if (it == bufferStates.end())
				{
					const PipelineStageFlags prevStage = m_registry.GetCurrentBufferStage(usage.handle);
					const MemoryAccessFlags prevAccess = m_registry.GetCurrentBufferAccess(usage.handle);
					it = bufferStates.emplace(usage.handle.index,
											  BufferAccessState{prevStage, prevAccess, bufferAccessWasWrite(prevAccess)})
							 .first;
				}

				BufferAccessState& prev = it->second;

				if (!prev.isWrite && !isWrite)
				{
					prev.stage |= stage;
					prev.access |= access;
					m_registry.SetCurrentBufferState(usage.handle, prev.stage, prev.access);
					continue;
				}

				cmd.PipelineBarrier(m_registry.GetBuffer(usage.handle),
									prev.stage, stage, prev.access, access);
				m_registry.SetCurrentBufferState(usage.handle, stage, access);
				prev = BufferAccessState{stage, access, isWrite};
			}
		};

		for (std::size_t i = 0; i < m_compiledPasses.size(); ++i)
		{
			RGCompiledPass& compiled = m_compiledPasses[i];
			const RGPass& pass = m_passes[compiled.passIndex];

			const bool isMergedGraphics = (pass.type == RGPassType::Graphics && !compiled.renderPass && activeRenderPass != nullptr);

			if (!isMergedGraphics)
				emitBarriersForPass(pass);

			if (pass.type == RGPassType::Graphics && compiled.renderPass)
			{
				for (std::size_t j = i + 1; j < m_compiledPasses.size(); ++j)
				{
					if (m_compiledPasses[j].renderPass)
						break; // next independent render-pass group
					const RGPass& mp = m_passes[m_compiledPasses[j].passIndex];
					if (mp.type != RGPassType::Graphics)
						break;
					emitBarriersForPass(mp);
				}

				currentFrameBuffer = m_passFrameBuffers.at(i);
				activeRenderPass = compiled.renderPass;
				cmd.BeginRenderPass(*compiled.renderPass, *currentFrameBuffer, m_clearColor);
			}

			RenderGraphContext ctx(cmd, currentFrameBuffer, activeRenderPass, m_registry, frameWidth, frameHeight);
			cmd.BeginDebugLabel(pass.name.c_str());
			pass.execute(ctx);
			cmd.EndDebugLabel();

			if (pass.type == RGPassType::Graphics && compiled.closesRenderPass && currentFrameBuffer != nullptr)
			{
				cmd.EndRenderPass();
				currentFrameBuffer = nullptr;
				activeRenderPass = nullptr;
			}

			if (timed)
			{
				m_timedPassTypes.push_back(pass.type);
				cmd.WriteTimestamp(*m_timestampPool, static_cast<UInt32>(m_timedPassTypes.size()));
			}
		}

		for (const auto& [_, pair] : m_registry.ExportLayouts())
		{
			const auto& [handle, exportLayout] = pair;
			const ImageLayout current = m_registry.GetCurrentLayout(handle);
			if (current == exportLayout)
				continue;
			auto [srcStage, dstStage, srcAccess, dstAccess] =
				RenderGraphCompiler::InferBarrierParams(current, exportLayout);
			Texture& tex = m_registry.GetTexture(handle);
			cmd.PipelineBarrier(tex, current, exportLayout, srcStage, dstStage, srcAccess, dstAccess);
			m_registry.SetCurrentLayout(handle, exportLayout);
		}
		m_statistics.m_recordTime += Clock::now() - recordStart;
		return {};
	}

	void RenderGraph::Reset()
	{
		const Clock::time_point start = Clock::now();
		m_pendingFrames.push_back(std::move(m_retired));
		m_retired = {};
		while (m_pendingFrames.size() > m_maxFramesInFlight)
		{
			ReleaseRetired(m_pendingFrames.front());
			m_pendingFrames.pop_front();
		}
		RetireIdleObjects();
		ReleaseCompletedResources();

		m_registry.Reset();
		m_statistics.m_resetTime += Clock::now() - start;
	}

	void RenderGraph::SetTextureLayout(RGTextureHandle handle, ImageLayout layout)
	{
		m_registry.SetCurrentLayout(handle, layout);
	}

	void RenderGraph::SetExportLayout(RGTextureHandle handle, ImageLayout layout)
	{
		m_registry.SetExportLayout(handle, layout);
	}

	ImageLayout RenderGraph::GetCurrentLayout(RGTextureHandle handle) const
	{
		return m_registry.GetCurrentLayout(handle);
	}

	void RenderGraph::MarkDirty()
	{
		m_dirty = true;
	}

	void RenderGraph::Clear()
	{
		const Clock::time_point start = Clock::now();
		const std::size_t retiredTextures = m_retired.m_transientTextures.size();
		const std::size_t retiredBuffers = m_retired.m_transientBuffers.size();
		m_registry.ExtractTransients(m_retired.m_transientTextures, m_retired.m_transientBuffers);
		if (m_retired.m_transientTextures.size() != retiredTextures || m_retired.m_transientBuffers.size() != retiredBuffers)
		{
			m_retired.m_lastUse = std::max(m_retired.m_lastUse, m_lastExecution);
		}
		m_passes.clear();
		m_compiledPasses.clear();
		m_registry.ClearEntries();
		m_finalOutput = {};
		m_dirty = true;
		RetireIdleObjects();
		ReleaseCompletedResources();
		m_statistics.m_clearTime += Clock::now() - start;
	}

	const RenderGraphStatistics& RenderGraph::GetStatistics() const
	{
		return m_statistics;
	}

	RenderGraphResidency RenderGraph::GetResidency() const
	{
		RenderGraphResidency residency;
		for (UInt32 texture = 0; texture < m_registry.TextureCount(); ++texture)
		{
			const RGTextureHandle handle{.index = static_cast<UInt16>(texture), .version = 0};
			if (!m_registry.IsImported(handle) && m_registry.GetTexturePointer(handle) != nullptr)
			{
				++residency.m_assignedTextures;
			}
		}
		const auto countRetired = [&residency](const RetiredResources& retired)
		{
			residency.m_retiredTextures += retired.m_transientTextures.size();
			residency.m_retiredFrameBuffers += retired.m_frameBuffers.size();
			residency.m_retiredRenderPasses += retired.m_renderPasses.size();
		};
		countRetired(m_retired);
		for (const RetiredResources& pending : m_pendingFrames)
		{
			countRetired(pending);
		}
		residency.m_pooledTextures = m_registry.GetPooledTextureCount();
		residency.m_cachedFrameBuffers = m_frameBufferCache.size();
		residency.m_cachedRenderPasses = m_compiler.GetCachedRenderPassCount();
		return residency;
	}

	UInt64 RenderGraph::GetLastExecution() const
	{
		return m_lastExecution;
	}

	void RenderGraph::MarkExecutionsComplete(UInt64 execution)
	{
		m_completedExecution = std::max(m_completedExecution, std::min(execution, m_lastExecution));
		ReleaseCompletedResources();
	}

	void RenderGraph::SetIdleExecutionLimit(UInt32 executions)
	{
		m_idleExecutionLimit = std::max<UInt32>(executions, 1);
	}

	FrameBuffer* RenderGraph::AcquireFrameBuffer(const RGPass& pass, const RenderPass& renderPass, UInt32 frameWidth, UInt32 frameHeight, UInt64 execution)
	{
		std::vector<RGTextureHandle> attachmentHandles = pass.colorAttachments;
		if (pass.depthAttachment.has_value())
		{
			attachmentHandles.push_back(*pass.depthAttachment);
		}

		UInt32 width = frameWidth;
		UInt32 height = frameHeight;
		if (!attachmentHandles.empty())
		{
			const RGTextureDesc& desc = m_registry.GetDesc(attachmentHandles.front());
			if (desc.width > 0 && desc.height > 0)
			{
				width = desc.width;
				height = desc.height;
			}
		}

		const auto matches = [&](const CachedFrameBuffer& cached)
		{
			if (cached.m_renderPass != &renderPass || cached.m_width != width || cached.m_height != height || cached.m_attachments.size() != attachmentHandles.size())
			{
				return false;
			}
			return std::ranges::equal(cached.m_attachments, attachmentHandles, [this](const std::weak_ptr<Texture>& texture, RGTextureHandle handle)
									  {
										  const std::shared_ptr<Texture> alive = texture.lock();
										  return alive != nullptr && alive == m_registry.GetTexturePointer(handle); });
		};
		const auto found = std::ranges::find_if(m_frameBufferCache, matches);
		if (found != m_frameBufferCache.end())
		{
			found->m_lastUse = execution;
			return found->m_frameBuffer.get();
		}

		const Clock::time_point start = Clock::now();
		CachedFrameBuffer entry;
		entry.m_renderPass = &renderPass;
		entry.m_width = width;
		entry.m_height = height;
		entry.m_lastUse = execution;
		for (const RGTextureHandle handle : attachmentHandles)
		{
			const std::shared_ptr<Texture>& texture = m_registry.GetTexturePointer(handle);
			entry.m_attachments.emplace_back(texture);
			if (!m_registry.IsImported(handle))
			{
				entry.m_ownedAttachments.push_back(texture);
			}
			entry.m_views.push_back(texture->CreateView());
		}
		entry.m_frameBuffer = m_device.CreateFrameBuffer(width, height, renderPass, entry.m_views);
		m_statistics.m_frameBufferCreationTime += Clock::now() - start;
		if (entry.m_frameBuffer == nullptr)
		{
			return nullptr;
		}
		++m_statistics.m_frameBuffersCreated;
		m_frameBufferCache.push_back(std::move(entry));
		return m_frameBufferCache.back().m_frameBuffer.get();
	}

	void RenderGraph::RetireIdleObjects()
	{
		const UInt64 limit = m_idleExecutionLimit;
		const auto isIdle = [this, limit](UInt64 lastUse)
		{ return m_lastExecution >= lastUse + limit; };
		std::erase_if(m_frameBufferCache, [&](CachedFrameBuffer& cached)
					  {
						  const bool expired = std::ranges::any_of(cached.m_attachments, [](const std::weak_ptr<Texture>& texture) { return texture.expired(); });
						  if (!expired && !isIdle(cached.m_lastUse))
						  {
							  return false;
						  }
						  m_retired.m_lastUse = std::max(m_retired.m_lastUse, cached.m_lastUse);
						  m_retired.m_frameBuffers.push_back(std::move(cached));
						  return true; });
		const std::size_t retiredRenderPasses = m_retired.m_renderPasses.size();
		const UInt64 renderPassLastUse = m_compiler.RetireIdleRenderPasses(m_lastExecution, limit, m_compiledPasses, m_retired.m_renderPasses);
		if (m_retired.m_renderPasses.size() != retiredRenderPasses)
		{
			m_retired.m_lastUse = std::max(m_retired.m_lastUse, renderPassLastUse);
		}
		m_registry.TrimPool(m_lastExecution, limit);
	}

	void RenderGraph::ReleaseCompletedResources()
	{
		if (m_retired.m_lastUse <= m_completedExecution)
		{
			ReleaseRetired(m_retired);
		}
		std::erase_if(m_pendingFrames, [this](RetiredResources& pending)
					  {
						  if (pending.m_lastUse > m_completedExecution)
						  {
							  return false;
						  }
						  ReleaseRetired(pending);
						  return true; });
	}

	void RenderGraph::ReleaseRetired(RetiredResources& retired)
	{
		retired.m_frameBuffers.clear();
		m_registry.ReturnTransientsToPool(retired.m_transientTextures, retired.m_transientBuffers, retired.m_lastUse);
		retired.m_renderPasses.clear();
		retired.m_lastUse = 0;
	}

	void RenderGraph::SetTimestampQueryPool(QueryPool* pool)
	{
		m_timestampPool = pool;
		m_timedPassTypes.clear();
	}

	UInt32 RenderGraph::GetTimestampQueryCount() const
	{
		return static_cast<UInt32>(m_compiledPasses.size()) + 1;
	}

	bool RenderGraph::ResolveTimestamps()
	{
		if (m_timestampPool == nullptr || m_timedPassTypes.empty())
		{
			return false;
		}
		m_timestampTicks.resize(m_timedPassTypes.size() + 1);
		const bool available = m_timestampPool->ReadTimestamps(0, m_timestampTicks);
		const std::vector<RGPassType> passTypes = std::move(m_timedPassTypes);
		m_timedPassTypes.clear();
		if (!available)
		{
			return false;
		}

		const double period = m_timestampPool->GetTimestampPeriod();
		const auto toDuration = [period](UInt64 begin, UInt64 end)
		{
			return std::chrono::nanoseconds{end > begin ? static_cast<std::chrono::nanoseconds::rep>(static_cast<double>(end - begin) * period) : 0};
		};
		std::span<const UInt64> ticks = m_timestampTicks;
		m_statistics.m_gpuTime += toDuration(ticks.front(), ticks.back());
		for (const RGPassType type : passTypes)
		{
			const std::chrono::nanoseconds passTime = toDuration(ticks.front(), ticks.subspan(1).front());
			ticks = ticks.subspan(1);
			switch (type)
			{
				case RGPassType::Graphics:
					m_statistics.m_gpuGraphicsTime += passTime;
					break;
				case RGPassType::Compute:
					m_statistics.m_gpuComputeTime += passTime;
					break;
				case RGPassType::Transfer:
					m_statistics.m_gpuTransferTime += passTime;
					break;
			}
		}
		++m_statistics.m_gpuTimedExecutions;
		return true;
	}

	void RenderGraph::DumpGraph(std::ostream& out) const
	{
		out << "RenderGraph (" << m_passes.size() << " declared, "
			<< m_compiledPasses.size() << " compiled)\n";

		for (const RGCompiledPass& cp : m_compiledPasses)
		{
			const RGPass& pass = m_passes[cp.passIndex];
			const char* typeStr =
				pass.type == RGPassType::Graphics ? "Graphics" : pass.type == RGPassType::Compute ? "Compute"
																								  : "Transfer";

			out << "  [" << typeStr << "] " << pass.name << "\n";

			for (const RGTextureUsage& usage : pass.textureUsages)
			{
				out << "    Texture[" << usage.handle.index << "] "
					<< (usage.access == RGResourceAccess::Read ? "READ" : "WRITE")
					<< (usage.isDepth ? " (depth)" : "") << "\n";
			}

			for (const RGBufferUsage& usage : pass.bufferUsages)
			{
				out << "    Buffer[" << usage.handle.index << "] "
					<< (usage.access == RGResourceAccess::Read ? "READ" : "WRITE") << "\n";
			}
		}

		for (UInt32 i = 0; i < static_cast<UInt32>(m_passes.size()); ++i)
		{
			if (m_passes[i].culled)
				out << "  [CULLED] " << m_passes[i].name << "\n";
		}
	}
} // namespace cct::gfx::rhi
