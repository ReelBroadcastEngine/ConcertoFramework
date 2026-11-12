//
// Created by arthur on 10/05/2026.
//

#ifndef CONCERTO_GRAPHICS_RHI_DX12_DX12_RHI_QUERYPOOL_HPP
#define CONCERTO_GRAPHICS_RHI_DX12_DX12_RHI_QUERYPOOL_HPP

#include <d3d12.h>
#include <span>
#include <vector>

#include "Concerto/Graphics/RHI/QueryPool.hpp"
#include <wrl/client.h>

namespace cct::gfx::rhi
{
	// Direct3D 12 has no query availability: a query counts as available once a WriteTimestamp was recorded for it after
	// its last ResetQueries, so reading it before the submission completed is the caller's error (QueryPool contract).
	class CONCERTO_GRAPHICS_RHI_BASE_API Dx12RHIQueryPool final : public QueryPool
	{
	public:
		// mappedReadback is readbackBuffer mapped for reading, capacity 64-bit values. timestampPeriod is in nanoseconds
		// per tick.
		Dx12RHIQueryPool(Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap, Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer, const UInt64* mappedReadback, UInt32 capacity,
						 double timestampPeriod);
		~Dx12RHIQueryPool() override;

		Dx12RHIQueryPool(const Dx12RHIQueryPool&) = delete;
		Dx12RHIQueryPool(Dx12RHIQueryPool&&) = delete;
		Dx12RHIQueryPool& operator=(const Dx12RHIQueryPool&) = delete;
		Dx12RHIQueryPool& operator=(Dx12RHIQueryPool&&) = delete;

		[[nodiscard]] UInt32 GetCapacity() const override;
		[[nodiscard]] double GetTimestampPeriod() const override;
		[[nodiscard]] bool ReadTimestamps(UInt32 first, std::span<UInt64> ticks) override;

		[[nodiscard]] ID3D12QueryHeap* GetQueryHeap() const;
		[[nodiscard]] ID3D12Resource* GetReadbackBuffer() const;
		void MarkReset(UInt32 first, UInt32 count);
		void MarkWritten(UInt32 index);

	private:
		Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_queryHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_readbackBuffer;
		std::span<const UInt64> m_readback;
		double m_timestampPeriod;
		std::vector<UInt8> m_written;
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_DX12_DX12_RHI_QUERYPOOL_HPP
