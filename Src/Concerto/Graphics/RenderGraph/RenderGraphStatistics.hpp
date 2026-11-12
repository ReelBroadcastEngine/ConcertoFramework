#ifndef CONCERTO_GRAPHICS_RHI_RENDERGRAPH_STATISTICS_HPP
#define CONCERTO_GRAPHICS_RHI_RENDERGRAPH_STATISTICS_HPP

#include <chrono>

#include "Concerto/Graphics/RHI/Defines.hpp"

namespace cct::gfx::rhi
{
	// What a RenderGraph has done since it was created. Times are host time measured with std::chrono::steady_clock.
	struct RenderGraphStatistics
	{
		UInt64 m_compilations = 0;
		UInt64 m_executions = 0;
		// GPU objects the graph created.
		UInt64 m_renderPassesCreated = 0;
		UInt64 m_frameBuffersCreated = 0;
		UInt64 m_texturesCreated = 0;
		UInt64 m_buffersCreated = 0;
		// Transients assigned a pooled resource instead of a new one.
		UInt64 m_texturesReused = 0;
		UInt64 m_buffersReused = 0;

		std::chrono::nanoseconds m_resetTime{0};
		std::chrono::nanoseconds m_clearTime{0};
		// Compile, and its phases: dependency analysis, culling, ordering and render pass creation.
		std::chrono::nanoseconds m_compileTime{0};
		std::chrono::nanoseconds m_dependencyTime{0};
		std::chrono::nanoseconds m_cullingTime{0};
		std::chrono::nanoseconds m_sortTime{0};
		std::chrono::nanoseconds m_renderPassCreationTime{0};
		// Assigning physical resources to transients.
		std::chrono::nanoseconds m_allocationTime{0};
		// Execute without compilation and allocation: barriers, framebuffers and the passes' command recording. The
		// framebuffer and attachment view creation it includes is also counted on its own.
		std::chrono::nanoseconds m_recordTime{0};
		std::chrono::nanoseconds m_frameBufferCreationTime{0};

		// Executions whose GPU time was resolved (RenderGraph::ResolveTimestamps), their GPU time from the start of the
		// first pass to the end of the last one, and the part of it spent in each type of pass, barriers before a pass
		// included.
		UInt64 m_gpuTimedExecutions = 0;
		std::chrono::nanoseconds m_gpuTime{0};
		std::chrono::nanoseconds m_gpuGraphicsTime{0};
		std::chrono::nanoseconds m_gpuComputeTime{0};
		std::chrono::nanoseconds m_gpuTransferTime{0};
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_RENDERGRAPH_STATISTICS_HPP
