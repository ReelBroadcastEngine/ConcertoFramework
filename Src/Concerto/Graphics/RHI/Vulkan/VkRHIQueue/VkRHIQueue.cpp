//
// Created by arthur on 09/04/2026.
//

#include "Concerto/Graphics/RHI/Vulkan/VkRHIQueue/VkRHIQueue.hpp"

#include <Concerto/Core/Cast.hpp>

#include "Concerto/Graphics/RHI/Vulkan/VkRHICommandBuffer/VkRHICommandBuffer.hpp"
#include "Concerto/Graphics/RHI/Vulkan/VkRHIDevice/VkRHIDevice.hpp"
#include "Concerto/Graphics/RHI/Vulkan/VkRHIFence/VkRHIFence.hpp"

namespace cct::gfx::rhi
{
	VkRHIQueue::VkRHIQueue(vk::Queue& queue, VkRHIDevice& device) :
		m_queue(&queue),
		m_device(&device)
	{
	}

	Result<void, QueueError> VkRHIQueue::Submit(CommandBuffer& cmdBuf, Fence* fence)
	{
		const auto& vkCmdBuf = Cast<const VkRHICommandBuffer&>(cmdBuf);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		VkCommandBuffer cmdBufHandle = *static_cast<const vk::CommandBuffer&>(vkCmdBuf).Get();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBufHandle;

		VkRHIFence* vkRHIFence = nullptr;
		VkFence vkFence = VK_NULL_HANDLE;
		if (fence != nullptr)
		{
			vkRHIFence = &Cast<VkRHIFence&>(*fence);
			if (vkRHIFence->IsSignalPending())
			{
				CCT_RHI_LOG_ERROR("VkRHIQueue: the fence was submitted again before being reset");
				return QueueError::NotSubmitted;
			}
			vkFence = *vkRHIFence->GetFence().Get();
		}

		const VkResult result = m_device->vkQueueSubmit(*m_queue->Get(), 1, &submitInfo, vkFence);
		switch (result)
		{
			case VK_SUCCESS:
				if (vkRHIFence != nullptr)
				{
					vkRHIFence->MarkSignalPending();
				}
				return {};
			// The specification guarantees that a submission failing with an out-of-memory error leaves every resource
			// and synchronization primitive unaffected; a failure that cannot keep that guarantee is VK_ERROR_DEVICE_LOST.
			case VK_ERROR_OUT_OF_HOST_MEMORY:
			case VK_ERROR_OUT_OF_DEVICE_MEMORY:
				CCT_RHI_LOG_ERROR("VkRHIQueue: vkQueueSubmit failed, nothing was submitted (VkResult {})", static_cast<int>(result));
				return QueueError::NotSubmitted;
			case VK_ERROR_DEVICE_LOST:
				CCT_RHI_LOG_ERROR("VkRHIQueue: device lost at vkQueueSubmit");
				return QueueError::DeviceLost;
			default:
				CCT_RHI_LOG_ERROR("VkRHIQueue: vkQueueSubmit failed (VkResult {})", static_cast<int>(result));
				return QueueError::CompletionUnknown;
		}
	}
} // namespace cct::gfx::rhi
