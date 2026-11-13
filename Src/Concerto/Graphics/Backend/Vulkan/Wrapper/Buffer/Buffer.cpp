//
// Created by arthur on 21/06/2022.
//

#include "Concerto/Graphics/Backend/Vulkan/Wrapper/Buffer/Buffer.hpp"

#include <stdexcept>
#include <utility>
#include <vk_mem_alloc.h>

#include "Concerto/Graphics/Backend/Vulkan/VkException.hpp"
#include "Concerto/Graphics/Backend/Vulkan/Wrapper/Allocator/Allocator.hpp"
#include "Concerto/Graphics/Backend/Vulkan/Wrapper/Device/Device.hpp"

namespace cct::gfx::vk
{
	Buffer::Buffer() :
		m_allocatedSize(0),
		m_allocator(nullptr),
		m_usage(0)
	{
	}

	Buffer::Buffer(Allocator& allocator, std::size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, HostAccess hostAccess) :
		Object(*allocator.GetDevice()),
		m_allocatedSize(allocSize),
		m_allocator(&allocator),
		m_usage(usage)
	{
		if (Create(allocator, allocSize, usage, memoryUsage, hostAccess) != VK_SUCCESS)
		{
			throw VkException(GetLastResult());
		}
	}

	Buffer::Buffer(Buffer&& other) noexcept :
		Object(std::move(other)),
		m_mapCount(std::exchange(other.m_mapCount, 0)),
		m_allocatedSize(std::exchange(other.m_allocatedSize, 0)),
		m_allocator(std::exchange(other.m_allocator, nullptr)),
		m_allocation(std::exchange(other.m_allocation, nullptr)),
		m_usage(std::exchange(other.m_usage, 0)),
		m_hostAccess(std::exchange(other.m_hostAccess, HostAccess::None)),
		m_hostCoherent(std::exchange(other.m_hostCoherent, true))
	{
	}

	Buffer& Buffer::operator=(Buffer&& other) noexcept
	{
		std::swap(m_mapCount, other.m_mapCount);
		std::swap(m_allocatedSize, other.m_allocatedSize);
		std::swap(m_allocator, other.m_allocator);
		std::swap(m_allocation, other.m_allocation);
		std::swap(m_usage, other.m_usage);
		std::swap(m_hostAccess, other.m_hostAccess);
		std::swap(m_hostCoherent, other.m_hostCoherent);

		Object::operator=(std::move(other));

		return *this;
	}

	VkResult Buffer::Create(Allocator& allocator, std::size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, HostAccess hostAccess)
	{
		m_device = allocator.GetDevice();

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext = nullptr;

		bufferInfo.size = allocSize;
		bufferInfo.usage = usage;

		VmaAllocationCreateInfo vmaAllocInfo = {};
		vmaAllocInfo.usage = memoryUsage;
		switch (hostAccess)
		{
			case HostAccess::None:
				vmaAllocInfo.flags = 0;
				break;
			case HostAccess::SequentialWrite:
				vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
				break;
			case HostAccess::Random:
				vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
				break;
		}

		m_lastResult = vmaCreateBuffer(*allocator.Get(), &bufferInfo, &vmaAllocInfo, &m_handle, &m_allocation, nullptr);
		if (m_lastResult != VK_SUCCESS)
		{
			m_handle = VK_NULL_HANDLE;
			m_allocation = VK_NULL_HANDLE;
			return m_lastResult;
		}
		m_hostAccess = hostAccess;
		VkMemoryPropertyFlags memoryFlags = 0;
		vmaGetAllocationMemoryProperties(*allocator.Get(), m_allocation, &memoryFlags);
		m_hostCoherent = (memoryFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

		return m_lastResult;
	}

	Buffer::~Buffer()
	{
		if (!IsValid())
			return;
		if (m_mapCount != 0)
		{
			CCT_ASSERT_FALSE("ConcertoGraphics: Trying to destroy a buffer that is mapped");
			return;
		}
		// m_allocator->GetDevice()->WaitIdle();
		vmaDestroyBuffer(*m_allocator->Get(), m_handle, m_allocation);
	}

	void Buffer::Copy(const void* object, std::size_t size, std::size_t padding)
	{
		CCT_ASSERT(IsValid(), "Invalid object state, 'Create' must be called");

		Byte* data = nullptr;
		if (Map(&data) == false)
			return;
		data += padding;
		std::memcpy(data, object, size);
		UnMap();
	}

	bool Buffer::Map(Byte** data)
	{
		CCT_ASSERT(IsValid(), "Invalid object state, 'Create' must be called");

		if (vmaMapMemory(*m_allocator->Get(), m_allocation, reinterpret_cast<void**>(data)) != VK_SUCCESS)
		{
			return false;
		}
		if (m_hostAccess == HostAccess::Random && !m_hostCoherent &&
			vmaInvalidateAllocation(*m_allocator->Get(), m_allocation, 0, VK_WHOLE_SIZE) != VK_SUCCESS)
		{
			vmaUnmapMemory(*m_allocator->Get(), m_allocation);
			return false;
		}
		m_mapCount++;
		return true;
	}

	void Buffer::UnMap()
	{
		CCT_ASSERT(IsValid(), "Invalid object state, 'Create' must be called");

		if (m_mapCount == 0)
		{
			CCT_ASSERT_FALSE("ConcertoGraphics: Trying to destroy a buffer that is mapped");
			return;
		}
		m_mapCount--;
		if (m_hostAccess == HostAccess::SequentialWrite && !m_hostCoherent)
		{
			const VkResult flushed = vmaFlushAllocation(*m_allocator->Get(), m_allocation, 0, VK_WHOLE_SIZE);
			if (flushed != VK_SUCCESS)
			{
				CCT_GFX_LOG_ERROR("Vulkan", "vmaFlushAllocation failed (VkResult {}): host writes may not reach the device", static_cast<int>(flushed));
			}
		}
		vmaUnmapMemory(*m_allocator->Get(), m_allocation);
	}

	VmaAllocation Buffer::GetAllocation() const
	{
		return m_allocation;
	}

	Allocator& Buffer::GetAllocator() const
	{
		CCT_ASSERT(m_allocator, "ConcertoGraphics: allocator is null");
		return *m_allocator;
	}

	std::size_t Buffer::GetAllocatedSize() const
	{
		return m_allocatedSize;
	}

	VkBufferUsageFlags Buffer::GetUsage() const
	{
		return m_usage;
	}

	Buffer::HostAccess Buffer::GetHostAccess() const
	{
		return m_hostAccess;
	}

	VkMemoryPropertyFlags Buffer::GetMemoryPropertyFlags() const
	{
		CCT_ASSERT(IsValid(), "Invalid object state, 'Create' must be called");

		VkMemoryPropertyFlags flags = 0;
		vmaGetAllocationMemoryProperties(*m_allocator->Get(), m_allocation, &flags);
		return flags;
	}
} // namespace cct::gfx::vk
