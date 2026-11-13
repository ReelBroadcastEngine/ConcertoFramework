//
// Created by arthur on 09/04/2026.
//

#ifndef CONCERTO_GRAPHICS_RHI_VULKAN_VK_RHI_FENCE_HPP
#define CONCERTO_GRAPHICS_RHI_VULKAN_VK_RHI_FENCE_HPP

#include "Concerto/Graphics/Backend/Vulkan/Wrapper/Fence/Fence.hpp"
#include "Concerto/Graphics/RHI/Fence.hpp"

namespace cct::gfx::rhi
{
	class VkRHIDevice;

	class CONCERTO_GRAPHICS_RHI_BASE_API VkRHIFence : public rhi::Fence
	{
	public:
		explicit VkRHIFence(VkRHIDevice& device);
		~VkRHIFence() override = default;

		[[nodiscard]] Result<void, QueueError> Wait() override;
		[[nodiscard]] Result<void, QueueError> Reset() override;

		[[nodiscard]] bool IsSignalPending() const;
		void MarkSignalPending();

		vk::Fence& GetFence()
		{
			return m_fence;
		}

	private:
		vk::Fence m_fence;
		bool m_signalPending = false;
		bool m_completed = false;
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_VULKAN_VK_RHI_FENCE_HPP
