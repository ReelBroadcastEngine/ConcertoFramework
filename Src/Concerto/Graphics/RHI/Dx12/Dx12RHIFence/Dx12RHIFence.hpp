//
// Created by arthur on 09/04/2026.
//

#ifndef CONCERTO_GRAPHICS_RHI_DX12_DX12_RHI_FENCE_HPP
#define CONCERTO_GRAPHICS_RHI_DX12_DX12_RHI_FENCE_HPP

#include "Concerto/Graphics/Backend/Dx12/Wrapper/Fence/Fence.hpp"
#include "Concerto/Graphics/RHI/Fence.hpp"

namespace cct::gfx::rhi
{
	class Dx12RHIDevice;

	class CONCERTO_GRAPHICS_RHI_BASE_API Dx12RHIFence : public rhi::Fence
	{
	public:
		explicit Dx12RHIFence(Dx12RHIDevice& device);
		~Dx12RHIFence() override = default;

		[[nodiscard]] Result<void, QueueError> Wait() override;
		[[nodiscard]] Result<void, QueueError> Reset() override;

		[[nodiscard]] bool IsSignalPending() const;
		[[nodiscard]] HRESULT Signal(ID3D12CommandQueue& queue);

		dx12::Fence& GetFence()
		{
			return m_fence;
		}

	private:
		Dx12RHIDevice* m_device;
		dx12::Fence m_fence;
		bool m_signalPending = false;
		bool m_completed = false;
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_DX12_DX12_RHI_FENCE_HPP
