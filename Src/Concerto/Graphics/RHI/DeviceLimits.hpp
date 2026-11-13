#ifndef CONCERTO_GRAPHICS_RHI_DEVICELIMITS_HPP
#define CONCERTO_GRAPHICS_RHI_DEVICELIMITS_HPP

#include "Concerto/Graphics/RHI/Defines.hpp"

namespace cct::gfx::rhi
{
	// Limits of a device that callers must respect: creating a resource beyond them is invalid usage in the backend
	// APIs, not a reported failure.
	struct DeviceLimits
	{
		UInt32 m_maxTextureDimension2D = 0;
		UInt32 m_maxFrameBufferWidth = 0;
		UInt32 m_maxFrameBufferHeight = 0;
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_DEVICELIMITS_HPP
