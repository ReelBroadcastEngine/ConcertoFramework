//
// Created by arthur on 03/09/2024.
//

#include "Concerto/Graphics/RHI/Vulkan/VkRHIBuffer/VkRHIBuffer.hpp"

#include "Concerto/Graphics/RHI/CommandBuffer.hpp"
#include "Concerto/Graphics/RHI/Vulkan/Utils/Utils.hpp"

namespace cct::gfx::rhi
{
	namespace
	{
		vk::Buffer::HostAccess SelectHostAccess(rhi::BufferUsageFlags usage, bool allowBufferMapping)
		{
			if (!allowBufferMapping)
				return vk::Buffer::HostAccess::None;
			return usage.Contains(BufferUsage::HostReadback) ? vk::Buffer::HostAccess::Random : vk::Buffer::HostAccess::SequentialWrite;
		}
	} // namespace

	VkRHIBuffer::VkRHIBuffer(VkRHIDevice& device, rhi::BufferUsageFlags usage, UInt32 allocationSize, bool allowBufferMapping) :
		vk::Buffer(device.GetAllocator(), allocationSize, Converters::ToVulkan(usage), VMA_MEMORY_USAGE_AUTO, SelectHostAccess(usage, allowBufferMapping)),
		m_device(device),
		m_allowBufferMapping(allowBufferMapping)
	{
	}

	bool VkRHIBuffer::Map(Byte** data)
	{
		if (!m_allowBufferMapping)
		{
			CCT_ASSERT_FALSE("ConcertoGraphics: buffer mapping is not enabled");
			return false;
		}
		return vk::Buffer::Map(data);
	}

	void VkRHIBuffer::UnMap()
	{
		if (!m_allowBufferMapping)
		{
			CCT_ASSERT_FALSE("ConcertoGraphics: buffer mapping is not enabled");
			return;
		}
		vk::Buffer::UnMap();
	}

	void VkRHIBuffer::RecordReadback(CommandBuffer& cmd)
	{
		cmd.PipelineBarrier(*this, PipelineStage::AllCommands, PipelineStage::Host, MemoryAccess::MemoryWrite, MemoryAccess::HostRead);
	}

	BufferMemoryProperties VkRHIBuffer::GetMemoryProperties() const
	{
		const VkMemoryPropertyFlags flags = GetMemoryPropertyFlags();
		return BufferMemoryProperties{
			.m_deviceLocal = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0,
			.m_hostVisible = (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0,
			.m_hostCoherent = (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0,
			.m_hostCached = (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0,
		};
	}
} // namespace cct::gfx::rhi
