//
// Created by arthur on 10/05/2026.
//

#ifndef CONCERTO_GRAPHICS_RHI_QUERYPOOL_HPP
#define CONCERTO_GRAPHICS_RHI_QUERYPOOL_HPP

#include <span>

#include "Concerto/Graphics/RHI/Defines.hpp"

namespace cct::gfx::rhi
{
	// A fixed number of GPU timestamp queries for command buffers of the graphics queue, created by
	// Device::CreateQueryPool. Queries are recorded through CommandBuffer::ResetQueries and CommandBuffer::WriteTimestamp,
	// so a command buffer that wraps another one forwards them like any other command. A query pool must only be used
	// with command buffers of the device that created it.
	//
	// A query must be reset before each write. A written value becomes readable once the submission that wrote it has
	// completed, that is once its fence has been waited. The caller must not reset or rewrite a query while a submission
	// that uses it is in flight.
	class CONCERTO_GRAPHICS_RHI_BASE_API QueryPool
	{
	public:
		virtual ~QueryPool() = default;

		QueryPool(const QueryPool&) = delete;
		QueryPool(QueryPool&&) = delete;
		QueryPool& operator=(const QueryPool&) = delete;
		QueryPool& operator=(QueryPool&&) = delete;

		[[nodiscard]] virtual UInt32 GetCapacity() const = 0;

		// Nanoseconds per timestamp tick: the difference of two timestamps of this pool times this period is a duration.
		[[nodiscard]] virtual double GetTimestampPeriod() const = 0;

		// Reads the timestamps [first, first + ticks.size()) in ticks, masked to the bits the queue writes. Returns
		// false, leaving ticks unspecified, when the range exceeds the capacity or one of the queries has no available
		// value: not written since its last reset, or written by a submission that has not completed.
		[[nodiscard]] virtual bool ReadTimestamps(UInt32 first, std::span<UInt64> ticks) = 0;

	protected:
		QueryPool() = default;
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_QUERYPOOL_HPP
