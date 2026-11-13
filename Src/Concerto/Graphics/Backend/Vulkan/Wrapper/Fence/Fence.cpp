//
// Created by arthur on 15/06/22.
//

#include "Concerto/Graphics/Backend/Vulkan/Wrapper/Fence/Fence.hpp"

#include <cassert>
#include <stdexcept>

#include <Concerto/Core/Assert.hpp>

#include "Concerto/Graphics/Backend/Vulkan/VkException.hpp"
#include "Concerto/Graphics/Backend/Vulkan/Wrapper/Device/Device.hpp"

namespace cct::gfx::vk
{
	Fence::Fence(Device& device, bool signaled) :
		SimpleObject(device)
	{
		if (Create(device, signaled) != VK_SUCCESS)
			throw VkException(GetLastResult());
	}

	Fence::~Fence() = default;

	VkResult Fence::Create(Device& device, bool signaled)
	{
		m_device = &device;

		VkFenceCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		info.pNext = nullptr;
		info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

		m_lastResult = m_device->vkCreateFence(*m_device->Get(), &info, nullptr, &m_handle);

		return m_lastResult;
	}

	VkResult Fence::Wait(UInt64 timeout) const
	{
		CCT_ASSERT(IsValid(), "Invalid object state, 'Create' must be called");
		CCT_AUTO_PROFILER_SCOPE();

		return m_device->vkWaitForFences(*m_device->Get(), 1, &m_handle, VK_TRUE, timeout);
	}

	VkResult Fence::Reset() const
	{
		CCT_ASSERT(IsValid(), "Invalid object state, 'Create' must be called");
		return m_device->vkResetFences(*m_device->Get(), 1, &m_handle);
	}
} // namespace cct::gfx::vk