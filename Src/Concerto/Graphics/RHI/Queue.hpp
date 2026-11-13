//
// Created by arthur on 09/04/2026.
//

#ifndef CONCERTO_GRAPHICS_RHI_QUEUE_HPP
#define CONCERTO_GRAPHICS_RHI_QUEUE_HPP

#include <Concerto/Core/Result/Result.hpp>

#include "Concerto/Graphics/RHI/Defines.hpp"
#include "Concerto/Graphics/RHI/QueueError.hpp"

namespace cct::gfx::rhi
{
	class CommandBuffer;
	class Fence;

	class CONCERTO_GRAPHICS_RHI_BASE_API Queue
	{
	public:
		virtual ~Queue() = default;

		// Success means the command buffer and the signal of the fence were both enqueued. A fence already submitted since
		// its last Reset is refused with NotSubmitted, so that Fence::Wait always refers to this submission.
		[[nodiscard]] virtual Result<void, QueueError> Submit(CommandBuffer& cmdBuf, Fence* fence = nullptr) = 0;
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_QUEUE_HPP
