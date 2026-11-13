//
// Created by arthur on 01/09/2025.
//

#include "Concerto/Graphics/Backend/Dx12/Wrapper/Fence/Fence.hpp"

#include <limits>

#include "Concerto/Graphics/Backend/Dx12/Wrapper/Device/Device.hpp"

namespace cct::gfx::dx12
{
	Fence::Fence(Device& device, D3D12_FENCE_FLAGS flags)
	{
		if (FAILED(Create(device, flags)))
			throw Dx12Exception(m_lastResult);
	}

	HRESULT Fence::Create(Device& device, D3D12_FENCE_FLAGS flags)
	{
		m_device = &device;

		m_lastResult = device->CreateFence(0, flags, IID_PPV_ARGS(&m_handle));
		return m_lastResult;
	}

	HRESULT Fence::Signal(ID3D12CommandQueue& queue) const
	{
		CCT_ASSERT(m_handle, "Invalid object state, 'Create' must be called");

		if (m_nextSignalValue == std::numeric_limits<UInt64>::max())
		{
			return E_FAIL;
		}
		m_lastResult = queue.Signal(m_handle.Get(), m_nextSignalValue);
		if (FAILED(m_lastResult))
		{
			return m_lastResult;
		}
		m_lastSignaledValue = m_nextSignalValue;
		++m_nextSignalValue;
		return m_lastResult;
	}

	HRESULT Fence::Wait() const
	{
		CCT_ASSERT(m_handle, "Invalid object state, 'Create' must be called");

		if (m_lastSignaledValue == 0)
		{
			return S_OK;
		}
		if (m_handle->GetCompletedValue() < m_lastSignaledValue)
		{
			m_lastResult = m_handle->SetEventOnCompletion(m_lastSignaledValue, nullptr);
			if (FAILED(m_lastResult))
			{
				return m_lastResult;
			}
		}
		const UInt64 completed = m_handle->GetCompletedValue();
		if (completed == std::numeric_limits<UInt64>::max())
		{
			return DXGI_ERROR_DEVICE_REMOVED;
		}
		return completed >= m_lastSignaledValue ? S_OK : E_FAIL;
	}

	UInt64 Fence::GetCompletedValue() const
	{
		CCT_ASSERT(m_handle, "Invalid object state, 'Create' must be called");
		return m_handle->GetCompletedValue();
	}
} // namespace cct::gfx::dx12