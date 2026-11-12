//
// Created by arthur on 10/05/2026.
//

#include "Concerto/Graphics/RHI/Vulkan/VkRHIQueryPool/VkRHIQueryPool.hpp"

#include <limits>

#include "Concerto/Graphics/Backend/Vulkan/Wrapper/Device/Device.hpp"
#include "Concerto/Graphics/RHI/Vulkan/VkRHIDevice/VkRHIDevice.hpp"

namespace cct::gfx::rhi
{
	VkRHIQueryPool::VkRHIQueryPool(VkRHIDevice& device, VkQueryPool queryPool, UInt32 capacity, UInt32 timestampValidBits, double timestampPeriod) :
		m_device(device),
		m_queryPool(queryPool),
		m_capacity(capacity),
		m_validMask(timestampValidBits >= 64 ? std::numeric_limits<UInt64>::max() : (UInt64{1} << timestampValidBits) - 1),
		m_timestampPeriod(timestampPeriod)
	{
	}

	VkRHIQueryPool::~VkRHIQueryPool()
	{
		auto& vkDevice = static_cast<vk::Device&>(m_device);
		vkDevice.vkDestroyQueryPool(*vkDevice.Get(), m_queryPool, nullptr);
	}

	UInt32 VkRHIQueryPool::GetCapacity() const
	{
		return m_capacity;
	}

	double VkRHIQueryPool::GetTimestampPeriod() const
	{
		return m_timestampPeriod;
	}

	bool VkRHIQueryPool::ReadTimestamps(UInt32 first, std::span<UInt64> ticks)
	{
		if (first > m_capacity || ticks.size() > m_capacity - first)
			return false;
		if (ticks.empty())
			return true;

		// Each query yields its value followed by its availability word.
		constexpr std::size_t WordsPerQuery = 2;
		m_results.assign(ticks.size() * WordsPerQuery, 0);
		auto& vkDevice = static_cast<vk::Device&>(m_device);
		const VkResult result = vkDevice.vkGetQueryPoolResults(*vkDevice.Get(), m_queryPool, first, static_cast<UInt32>(ticks.size()), m_results.size() * sizeof(UInt64), m_results.data(),
															   WordsPerQuery * sizeof(UInt64), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		if (result != VK_SUCCESS)
			return false;

		std::span<const UInt64> words = m_results;
		for (UInt64& tick : ticks)
		{
			if (words.subspan(1).front() == 0)
				return false;
			tick = words.front() & m_validMask;
			words = words.subspan(WordsPerQuery);
		}
		return true;
	}

	VkQueryPool VkRHIQueryPool::GetHandle() const
	{
		return m_queryPool;
	}
} // namespace cct::gfx::rhi
