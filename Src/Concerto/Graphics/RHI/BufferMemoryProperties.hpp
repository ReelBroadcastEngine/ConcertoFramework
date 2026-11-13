#ifndef CONCERTO_GRAPHICS_RHI_BUFFERMEMORYPROPERTIES_HPP
#define CONCERTO_GRAPHICS_RHI_BUFFERMEMORYPROPERTIES_HPP

namespace cct::gfx::rhi
{
	// Where the memory of a buffer lives and how the host sees it, as allocated by the backend.
	struct BufferMemoryProperties
	{
		bool m_deviceLocal = false;
		bool m_hostVisible = false;
		// Host writes and device writes are visible to the other side without an explicit flush or invalidation.
		bool m_hostCoherent = false;
		// Host reads go through the CPU caches; uncached host-visible memory is write-combined and slow to read.
		bool m_hostCached = false;
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_BUFFERMEMORYPROPERTIES_HPP
