//
// Created by arthur on 09/04/2026.
//

#ifndef CONCERTO_GRAPHICS_RHI_FENCE_HPP
#define CONCERTO_GRAPHICS_RHI_FENCE_HPP

#include <Concerto/Core/Result/Result.hpp>

#include "Concerto/Graphics/RHI/Defines.hpp"
#include "Concerto/Graphics/RHI/QueueError.hpp"

namespace cct::gfx::rhi
{
	class CONCERTO_GRAPHICS_RHI_BASE_API Fence
	{
	public:
		virtual ~Fence() = default;

		// Succeeds only once the submission that signalled this fence since its last Reset has completed.
		[[nodiscard]] virtual Result<void, QueueError> Wait() = 0;
		// Valid once Wait succeeded; the fence can then be submitted again.
		[[nodiscard]] virtual Result<void, QueueError> Reset() = 0;
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_FENCE_HPP
