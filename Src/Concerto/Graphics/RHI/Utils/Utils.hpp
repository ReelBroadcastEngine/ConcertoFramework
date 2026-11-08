//
// Created by arthur on 06/09/2024.
//

#ifndef CONCERTO_GRAPHICS_UTILS_HPP
#define CONCERTO_GRAPHICS_UTILS_HPP

#include <Concerto/Core/Types/Types.hpp>

namespace cct::gfx
{
	struct Rect2D
	{
		Int32 x;
		Int32 y;
		UInt32 width;
		UInt32 height;
	};

	struct Viewport
	{
		float x;
		float y;
		float width;
		float height;
		float minDepth;
		float maxDepth;
	};

	// Copies a rectangle of tightly or explicitly pitched texels from a buffer into a texture.
	// m_bufferRowLength is in texels (0 = tightly packed, i.e. equal to m_width). On DX12 the
	// resulting row pitch must respect Device::GetTextureCopyRowPitch and m_bufferOffset must be a
	// multiple of Device::GetTextureCopyPlacementAlignment.
	struct BufferTextureCopyRegion
	{
		UInt64 m_bufferOffset = 0;
		UInt32 m_bufferRowLength = 0;
		Int32 m_x = 0;
		Int32 m_y = 0;
		UInt32 m_width = 0;
		UInt32 m_height = 0;
	};
} // namespace cct::gfx

#endif // CONCERTO_GRAPHICS_UTILS_HPP
