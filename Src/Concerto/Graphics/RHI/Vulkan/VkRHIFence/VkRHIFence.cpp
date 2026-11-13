//
// Created by arthur on 09/04/2026.
//

#include "Concerto/Graphics/RHI/Vulkan/VkRHIFence/VkRHIFence.hpp"

#include "Concerto/Graphics/RHI/Vulkan/VkRHIDevice/VkRHIDevice.hpp"

namespace cct::gfx::rhi
{
	VkRHIFence::VkRHIFence(VkRHIDevice& device) :
		m_fence(device, false)
	{
	}

	Result<void, QueueError> VkRHIFence::Wait()
	{
		if (!m_signalPending)
		{
			return QueueError::NoPendingSubmission;
		}

		const VkResult result = m_fence.Wait(UINT64_MAX);
		if (result == VK_SUCCESS)
		{
			m_completed = true;
			return {};
		}
		if (result == VK_ERROR_DEVICE_LOST)
		{
			CCT_RHI_LOG_ERROR("VkRHIFence: device lost while waiting for a submission");
			return QueueError::DeviceLost;
		}
		CCT_RHI_LOG_ERROR("VkRHIFence: vkWaitForFences failed (VkResult {})", static_cast<int>(result));
		return QueueError::CompletionUnknown;
	}

	Result<void, QueueError> VkRHIFence::Reset()
	{
		if (m_signalPending && !m_completed)
		{
			return QueueError::FenceNotReset;
		}
		const VkResult result = m_fence.Reset();
		if (result != VK_SUCCESS)
		{
			CCT_RHI_LOG_ERROR("VkRHIFence: vkResetFences failed (VkResult {})", static_cast<int>(result));
			return result == VK_ERROR_DEVICE_LOST ? QueueError::DeviceLost : QueueError::FenceNotReset;
		}
		m_signalPending = false;
		m_completed = false;
		return {};
	}

	bool VkRHIFence::IsSignalPending() const
	{
		return m_signalPending;
	}

	void VkRHIFence::MarkSignalPending()
	{
		m_signalPending = true;
	}
} // namespace cct::gfx::rhi
