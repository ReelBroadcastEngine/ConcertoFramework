//
// Created by arthur on 10/05/2026.
//

#ifndef CONCERTO_GRAPHICS_RHI_VULKAN_VK_RHI_QUERYPOOL_HPP
#define CONCERTO_GRAPHICS_RHI_VULKAN_VK_RHI_QUERYPOOL_HPP

#include <span>
#include <vector>

#include "Concerto/Graphics/RHI/QueryPool.hpp"
#include <vulkan/vulkan.h>

namespace cct::gfx::rhi
{
	class VkRHIDevice;

	class CONCERTO_GRAPHICS_RHI_BASE_API VkRHIQueryPool final : public QueryPool
	{
	public:
		// Takes ownership of queryPool, a timestamp pool of capacity queries. timestampValidBits is the graphics queue
		// family's, timestampPeriod the device's, in nanoseconds per tick.
		VkRHIQueryPool(VkRHIDevice& device, VkQueryPool queryPool, UInt32 capacity, UInt32 timestampValidBits, double timestampPeriod);
		~VkRHIQueryPool() override;

		VkRHIQueryPool(const VkRHIQueryPool&) = delete;
		VkRHIQueryPool(VkRHIQueryPool&&) = delete;
		VkRHIQueryPool& operator=(const VkRHIQueryPool&) = delete;
		VkRHIQueryPool& operator=(VkRHIQueryPool&&) = delete;

		[[nodiscard]] UInt32 GetCapacity() const override;
		[[nodiscard]] double GetTimestampPeriod() const override;
		[[nodiscard]] bool ReadTimestamps(UInt32 first, std::span<UInt64> ticks) override;

		[[nodiscard]] VkQueryPool GetHandle() const;

	private:
		VkRHIDevice& m_device;
		VkQueryPool m_queryPool;
		UInt32 m_capacity;
		UInt64 m_validMask;
		double m_timestampPeriod;
		std::vector<UInt64> m_results;
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_VULKAN_VK_RHI_QUERYPOOL_HPP
