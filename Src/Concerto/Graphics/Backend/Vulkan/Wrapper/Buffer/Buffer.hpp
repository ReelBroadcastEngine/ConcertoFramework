//
// Created by arthur on 09/06/22.
//

#ifndef CONCERTO_GRAPHICS_BUFFER_HPP
#define CONCERTO_GRAPHICS_BUFFER_HPP

#include <cstddef>
#include <functional>
#include <vk_mem_alloc.h>

#include <Concerto/Core/Assert.hpp>

#include "Concerto/Graphics/Backend/Vulkan/Wrapper/Object/Object.hpp"

namespace cct::gfx::vk
{
	class Allocator;

	class CONCERTO_GRAPHICS_VULKAN_BACKEND_API Buffer : public Object<VkBuffer>
	{
	public:
		Buffer();
		// How the host accesses a buffer's memory, which selects the memory type. Mapped buffers stay persistently mapped.
		enum class HostAccess : UInt8
		{
			// Not mapped: device-local memory.
			None,
			// Host writes, device reads (staging, uniforms, vertices): written sequentially, never read back, so
			// write-combined memory is acceptable. UnMap flushes non-coherent memory.
			SequentialWrite,
			// Device writes, host reads (readback): host-cached memory when the device has it. Map invalidates
			// non-coherent memory.
			Random
		};

		Buffer(Allocator& allocator, std::size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, HostAccess hostAccess);
		~Buffer() override;

		Buffer(Buffer&&) noexcept;
		Buffer(const Buffer&) = delete;

		Buffer& operator=(Buffer&&) noexcept;
		Buffer& operator=(const Buffer&) = delete;

		// Returns the failure of the allocation and leaves the buffer invalid, without asserting.
		VkResult Create(Allocator& allocator, std::size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, HostAccess hostAccess);

		template<typename T>
		void Copy(T& object, std::size_t padding = 0);

		void Copy(const void* object, std::size_t size, std::size_t padding = 0);

		template<typename DestBuffer, typename SrcObj>
		void Copy(std::vector<SrcObj>& objects, std::function<void(DestBuffer& destBuffer, SrcObj& srcObj)>&& copyFunc, std::size_t padding = 0);

		bool Map(Byte** data);

		template<typename T>
		T* Map();

		void UnMap();

		[[nodiscard]] VmaAllocation GetAllocation() const;
		[[nodiscard]] Allocator& GetAllocator() const;
		[[nodiscard]] std::size_t GetAllocatedSize() const;
		[[nodiscard]] VkBufferUsageFlags GetUsage() const;
		[[nodiscard]] VkMemoryPropertyFlags GetMemoryPropertyFlags() const;
		[[nodiscard]] HostAccess GetHostAccess() const;

	private:
		UInt32 m_mapCount = 0;
		std::size_t m_allocatedSize;
		Allocator* m_allocator;
		VmaAllocation m_allocation{VK_NULL_HANDLE};
		VkBufferUsageFlags m_usage;
		HostAccess m_hostAccess = HostAccess::None;
		bool m_hostCoherent = true;
	};

} // namespace cct::gfx::vk

#include "Concerto/Graphics/Backend/Vulkan/Wrapper/Buffer/Buffer.inl"

#endif // CONCERTO_GRAPHICS_BUFFER_HPP
