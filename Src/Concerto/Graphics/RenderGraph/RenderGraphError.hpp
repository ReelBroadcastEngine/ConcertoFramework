#ifndef CONCERTO_GRAPHICS_RHI_RENDERGRAPH_ERROR_HPP
#define CONCERTO_GRAPHICS_RHI_RENDERGRAPH_ERROR_HPP

#include "Concerto/Graphics/RHI/Defines.hpp"

namespace cct::gfx::rhi
{
	enum class RenderGraphError : UInt8
	{
		// A transient's description exceeds the device limits (Device::GetLimits) or has a zero extent.
		ResourceLimitExceeded,
		// The device could not create a transient texture or buffer, for example out of memory.
		ResourceCreationFailed
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_RENDERGRAPH_ERROR_HPP
