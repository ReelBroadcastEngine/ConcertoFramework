//
// Created by arthur on 09/04/2026.
//

#include "Concerto/Graphics/RHI/Dx12/Dx12RHIQueue/Dx12RHIQueue.hpp"

#include <Concerto/Core/Cast.hpp>

#include "Concerto/Graphics/RHI/Dx12/Dx12RHICommandBuffer/Dx12RHICommandBuffer.hpp"
#include "Concerto/Graphics/RHI/Dx12/Dx12RHIDevice/Dx12RHIDevice.hpp"
#include "Concerto/Graphics/RHI/Dx12/Dx12RHIFence/Dx12RHIFence.hpp"

namespace cct::gfx::rhi
{
	namespace
	{
		bool LogIfDeviceRemoved(const Dx12RHIDevice& device, const char* where)
		{
			const HRESULT reason = device.GetRemovedReason();
			if (SUCCEEDED(reason))
				return false;
			CCT_RHI_LOG_ERROR("Dx12RHIQueue: device removed at {} (HRESULT {:#x})", where, static_cast<unsigned>(reason));
			return true;
		}
	} // namespace

	Dx12RHIQueue::Dx12RHIQueue(ID3D12CommandQueue* queue, Dx12RHIDevice& device) :
		m_queue(queue),
		m_device(&device)
	{
	}

	Result<void, QueueError> Dx12RHIQueue::Submit(CommandBuffer& cmdBuf, Fence* fence)
	{
		if (!m_queue)
		{
			CCT_RHI_LOG_ERROR("Dx12RHIQueue: no command queue to submit to");
			return QueueError::NotSubmitted;
		}

		Dx12RHIFence* dx12RHIFence = fence != nullptr ? &Cast<Dx12RHIFence&>(*fence) : nullptr;
		if (dx12RHIFence != nullptr && dx12RHIFence->IsSignalPending())
		{
			CCT_RHI_LOG_ERROR("Dx12RHIQueue: the fence was submitted again before being reset");
			return QueueError::NotSubmitted;
		}

		if (LogIfDeviceRemoved(*m_device, "Submit (before ExecuteCommandLists)"))
		{
			return QueueError::DeviceLost;
		}

		const auto& dx12CmdBuf = Cast<const Dx12RHICommandBuffer&>(cmdBuf);

		ID3D12CommandList* cmdLists[] = {dx12CmdBuf.Get()};
		m_queue->ExecuteCommandLists(1, cmdLists);

		if (LogIfDeviceRemoved(*m_device, "Submit (after ExecuteCommandLists)"))
		{
			return QueueError::DeviceLost;
		}

		if (dx12RHIFence != nullptr)
		{
			if (const HRESULT signalled = dx12RHIFence->Signal(*m_queue); FAILED(signalled))
			{
				if (LogIfDeviceRemoved(*m_device, "Submit (Signal)"))
				{
					return QueueError::DeviceLost;
				}
				CCT_RHI_LOG_ERROR("Dx12RHIQueue: the fence signal was not enqueued after the command list (HRESULT {:#x})", static_cast<unsigned>(signalled));
				return QueueError::CompletionUnknown;
			}
		}
		return {};
	}

	void Dx12RHIQueue::SetNativeQueue(ID3D12CommandQueue* queue)
	{
		m_queue = queue;
	}
} // namespace cct::gfx::rhi
