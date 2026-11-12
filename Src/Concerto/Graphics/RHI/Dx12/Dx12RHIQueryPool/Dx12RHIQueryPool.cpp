//
// Created by arthur on 10/05/2026.
//

#include "Concerto/Graphics/RHI/Dx12/Dx12RHIQueryPool/Dx12RHIQueryPool.hpp"

#include <algorithm>
#include <utility>

namespace cct::gfx::rhi
{
	Dx12RHIQueryPool::Dx12RHIQueryPool(Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap, Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer, const UInt64* mappedReadback, UInt32 capacity,
									   double timestampPeriod) :
		m_queryHeap(std::move(queryHeap)),
		m_readbackBuffer(std::move(readbackBuffer)),
		m_readback(mappedReadback, capacity),
		m_timestampPeriod(timestampPeriod),
		m_written(capacity, 0)
	{
	}

	Dx12RHIQueryPool::~Dx12RHIQueryPool()
	{
		const D3D12_RANGE nothingWritten{0, 0};
		m_readbackBuffer->Unmap(0, &nothingWritten);
	}

	UInt32 Dx12RHIQueryPool::GetCapacity() const
	{
		return static_cast<UInt32>(m_written.size());
	}

	double Dx12RHIQueryPool::GetTimestampPeriod() const
	{
		return m_timestampPeriod;
	}

	bool Dx12RHIQueryPool::ReadTimestamps(UInt32 first, std::span<UInt64> ticks)
	{
		if (first > m_written.size() || ticks.size() > m_written.size() - first)
			return false;
		const std::span<const UInt8> written = std::span<const UInt8>(m_written).subspan(first, ticks.size());
		if (std::ranges::find(written, UInt8{0}) != written.end())
			return false;
		std::ranges::copy(m_readback.subspan(first, ticks.size()), ticks.begin());
		return true;
	}

	ID3D12QueryHeap* Dx12RHIQueryPool::GetQueryHeap() const
	{
		return m_queryHeap.Get();
	}

	ID3D12Resource* Dx12RHIQueryPool::GetReadbackBuffer() const
	{
		return m_readbackBuffer.Get();
	}

	void Dx12RHIQueryPool::MarkReset(UInt32 first, UInt32 count)
	{
		if (first >= m_written.size())
			return;
		std::ranges::fill(std::span<UInt8>(m_written).subspan(first, std::min<std::size_t>(count, m_written.size() - first)), UInt8{0});
	}

	void Dx12RHIQueryPool::MarkWritten(UInt32 index)
	{
		if (index < m_written.size())
			std::span<UInt8>(m_written).subspan(index).front() = 1;
	}
} // namespace cct::gfx::rhi
