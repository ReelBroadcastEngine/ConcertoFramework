#ifndef CONCERTO_GRAPHICS_RHI_QUEUEERROR_HPP
#define CONCERTO_GRAPHICS_RHI_QUEUEERROR_HPP

#include "Concerto/Graphics/RHI/Defines.hpp"

namespace cct::gfx::rhi
{
	enum class QueueError : UInt8
	{
		// Proven before the work reached the queue: nothing of it executes and the fence is not signalled for it, so the
		// resources it uses can be reused or released.
		NotSubmitted,
		// The work may have reached the queue but its completion cannot be observed. The resources it uses must stay alive
		// and unmodified until the device is idle.
		CompletionUnknown,
		// The device was lost or removed. Nothing submitted to it can be shown to have completed, and it must be recreated.
		DeviceLost,
		// Fence::Wait was called without a submission signalling the fence since its last Reset.
		NoPendingSubmission,
		// Fence::Reset failed, or the fence still has a submission whose completion was not observed.
		FenceNotReset
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_QUEUEERROR_HPP
