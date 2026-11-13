//
// Created by arthur on 09/04/2026.
//

#include "Concerto/Graphics/RHI/Dx12/Dx12RHIFence/Dx12RHIFence.hpp"

#include "Concerto/Graphics/RHI/Dx12/Dx12RHIDevice/Dx12RHIDevice.hpp"

namespace cct::gfx::rhi
{
	Dx12RHIFence::Dx12RHIFence(Dx12RHIDevice& device) :
		m_device(&device),
		m_fence(device)
	{
	}

	Result<void, QueueError> Dx12RHIFence::Wait()
	{
		if (!m_signalPending)
		{
			return QueueError::NoPendingSubmission;
		}

		const HRESULT waited = m_fence.Wait();
		// A removed device reports every fence value as completed, so a finished wait proves nothing until the device is
		// known to be intact.
		if (const HRESULT removed = m_device->GetRemovedReason(); FAILED(removed))
		{
			CCT_RHI_LOG_ERROR("Dx12RHIFence: device removed while waiting for a submission (HRESULT {:#x})", static_cast<unsigned>(removed));
			return QueueError::DeviceLost;
		}
		if (waited == DXGI_ERROR_DEVICE_REMOVED)
		{
			return QueueError::DeviceLost;
		}
		if (FAILED(waited))
		{
			CCT_RHI_LOG_ERROR("Dx12RHIFence: waiting for a submission failed (HRESULT {:#x})", static_cast<unsigned>(waited));
			return QueueError::CompletionUnknown;
		}
		m_completed = true;
		return {};
	}

	Result<void, QueueError> Dx12RHIFence::Reset()
	{
		if (m_signalPending && !m_completed)
		{
			return QueueError::FenceNotReset;
		}
		m_signalPending = false;
		m_completed = false;
		return {};
	}

	bool Dx12RHIFence::IsSignalPending() const
	{
		return m_signalPending;
	}

	HRESULT Dx12RHIFence::Signal(ID3D12CommandQueue& queue)
	{
		const HRESULT signalled = m_fence.Signal(queue);
		m_signalPending = SUCCEEDED(signalled);
		return signalled;
	}
} // namespace cct::gfx::rhi
