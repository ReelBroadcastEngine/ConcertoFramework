//
// Created by arthur on 01/09/2025.
//

#ifndef CONCERTO_GRAPHICS_BACKEND_D3D12_FENCE_HPP
#define CONCERTO_GRAPHICS_BACKEND_D3D12_FENCE_HPP

#include "Concerto/Graphics/Backend/Dx12/Wrapper/Object/Object.hpp"

namespace cct::gfx::dx12
{
	class CONCERTO_GRAPHICS_DX12_BACKEND_API Fence : public Object<ID3D12Fence>
	{
	public:
		Fence() = default;
		Fence(Device& device, D3D12_FENCE_FLAGS flags = D3D12_FENCE_FLAG_NONE);
		~Fence() override = default;

		Fence(const Fence&) = delete;
		Fence(Fence&&) = default;

		Fence& operator=(const Fence&) = delete;
		Fence& operator=(Fence&&) = default;

		HRESULT Create(Device& device, D3D12_FENCE_FLAGS flags = D3D12_FENCE_FLAG_NONE);

		// The value to wait for advances only when the signal was enqueued.
		[[nodiscard]] HRESULT Signal(ID3D12CommandQueue& queue) const;
		// Waits for the last successfully signalled value and rejects the device-removal sentinel.
		[[nodiscard]] HRESULT Wait() const;
		[[nodiscard]] UInt64 GetCompletedValue() const;

	private:
		mutable UInt64 m_nextSignalValue = 1;
		mutable UInt64 m_lastSignaledValue = 0;
	};
} // namespace cct::gfx::dx12

#endif // CONCERTO_GRAPHICS_BACKEND_D3D12_FENCE_HPP
