//
// Created by arthur on 01/09/2025.
//

#include "Concerto/Graphics/RHI/Dx12/Dx12RHIBuffer/Dx12RHIBuffer.hpp"

#include <Concerto/Core/Cast.hpp>

#include "Concerto/Graphics/RHI/Dx12/Dx12RHICommandBuffer/Dx12RHICommandBuffer.hpp"
#include "Concerto/Graphics/RHI/Dx12/Dx12RHIDevice/Dx12RHIDevice.hpp"

namespace cct::gfx::rhi
{
	Dx12RHIBuffer::Dx12RHIBuffer(Dx12RHIDevice& device, rhi::BufferUsageFlags usage, UInt32 size, bool allowMapping) :
		m_device(&device),
		m_size(size),
		m_allowMapping(allowMapping)
	{
		// CBV requires 256-byte aligned size
		bool isUniform = usage.Contains(BufferUsage::Uniform);
		UInt32 allocSize = isUniform ? (size + 255) & ~255u : size;
		m_size = allocSize;

		const bool isStorage = usage.Contains(BufferUsage::Storage);
		const bool needsHostReadback = usage.Contains(BufferUsage::HostReadback);
		BufferUsageFlags copyDestinationUsage = BufferUsage::TransferDst;
		copyDestinationUsage |= BufferUsage::HostReadback;
		// A readback heap resource can only be a copy destination, so a buffer read by the host that the GPU also uses
		// otherwise is written in a default heap and copied into a readback shadow by RecordReadback.
		const bool directReadback = allowMapping && needsHostReadback && (usage & ~copyDestinationUsage).None();
		const bool needsReadbackShadow = allowMapping && needsHostReadback && !directReadback;
		const bool wantsUav = isStorage && (!allowMapping || needsHostReadback);

		D3D12_HEAP_PROPERTIES heapProperties = {};
		if (directReadback)
		{
			heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
		}
		else if (allowMapping && !needsReadbackShadow)
		{
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		}
		else
		{
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		}
		m_hostReadback = allowMapping && needsHostReadback;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = allocSize;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = wantsUav
								 ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
								 : D3D12_RESOURCE_FLAG_NONE;

		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
		if (heapProperties.Type == D3D12_HEAP_TYPE_UPLOAD)
		{
			initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
		}
		else if (heapProperties.Type == D3D12_HEAP_TYPE_READBACK)
		{
			initialState = D3D12_RESOURCE_STATE_COPY_DEST;
		}
		m_currentState = initialState;

		HRESULT hr = device.Get()->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			initialState,
			nullptr,
			IID_PPV_ARGS(&m_resource));

		if (FAILED(hr))
		{
			CCT_RHI_LOG_WARN("Dx12RHIBuffer: buffer of {} bytes not created (HRESULT {:#x})", allocSize, static_cast<unsigned>(hr));
			m_resource.Reset();
		}

		if (SUCCEEDED(hr))
			m_gpuAddress = m_resource->GetGPUVirtualAddress();

		if (needsReadbackShadow && SUCCEEDED(hr))
		{
			D3D12_HEAP_PROPERTIES readbackHeapProperties = {};
			readbackHeapProperties.Type = D3D12_HEAP_TYPE_READBACK;
			readbackHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			readbackHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			D3D12_RESOURCE_DESC readbackDesc = resourceDesc;
			readbackDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			HRESULT readbackHr = device.Get()->CreateCommittedResource(
				&readbackHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&readbackDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&m_readbackResource));

			if (FAILED(readbackHr))
			{
				CCT_RHI_LOG_WARN("Dx12RHIBuffer: readback shadow of {} bytes not created (HRESULT {:#x})", allocSize, static_cast<unsigned>(readbackHr));
				m_resource.Reset();
			}
		}
	}

	void Dx12RHIBuffer::RecordReadback(CommandBuffer& cmd)
	{
		if (!m_readbackResource)
			return;

		auto* cmdList = Cast<Dx12RHICommandBuffer&>(cmd).Get();

		const D3D12_RESOURCE_STATES priorState = m_currentState;

		if (priorState != D3D12_RESOURCE_STATE_COPY_SOURCE)
		{
			D3D12_RESOURCE_BARRIER toCopySrc{};
			toCopySrc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			toCopySrc.Transition.pResource = m_resource.Get();
			toCopySrc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			toCopySrc.Transition.StateBefore = priorState;
			toCopySrc.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
			cmdList->ResourceBarrier(1, &toCopySrc);
		}

		cmdList->CopyBufferRegion(m_readbackResource.Get(), 0, m_resource.Get(), 0, m_size);

		if (priorState != D3D12_RESOURCE_STATE_COPY_SOURCE)
		{
			D3D12_RESOURCE_BARRIER backToPrior{};
			backToPrior.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			backToPrior.Transition.pResource = m_resource.Get();
			backToPrior.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			backToPrior.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
			backToPrior.Transition.StateAfter = priorState;
			cmdList->ResourceBarrier(1, &backToPrior);
		}
		m_currentState = priorState;
	}

	bool Dx12RHIBuffer::Map(Byte** data)
	{
		if (!m_allowMapping || !m_resource)
			return false;

		auto* target = m_readbackResource ? m_readbackResource.Get() : m_resource.Get();
		const D3D12_RANGE readRange = m_hostReadback ? D3D12_RANGE{.Begin = 0, .End = m_size} : D3D12_RANGE{.Begin = 0, .End = 0};
		return SUCCEEDED(target->Map(0, &readRange, reinterpret_cast<void**>(data)));
	}

	BufferMemoryProperties Dx12RHIBuffer::GetMemoryProperties() const
	{
		ID3D12Resource* mapped = m_readbackResource != nullptr ? m_readbackResource.Get() : m_resource.Get();
		D3D12_HEAP_PROPERTIES heap{.Type = D3D12_HEAP_TYPE_DEFAULT};
		D3D12_HEAP_FLAGS heapFlags{};
		if (mapped == nullptr || FAILED(mapped->GetHeapProperties(&heap, &heapFlags)))
		{
			return {};
		}
		if (heap.Type != D3D12_HEAP_TYPE_CUSTOM)
		{
			heap = m_device->Get()->GetCustomHeapProperties(0, heap.Type);
		}
		const bool hostVisible = heap.CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
		return BufferMemoryProperties{
			.m_deviceLocal = heap.MemoryPoolPreference == D3D12_MEMORY_POOL_L1 || !hostVisible,
			.m_hostVisible = hostVisible,
			.m_hostCoherent = hostVisible,
			.m_hostCached = heap.CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
		};
	}

	void Dx12RHIBuffer::UnMap()
	{
		if (!m_allowMapping || !m_resource)
			return;

		auto* target = m_readbackResource ? m_readbackResource.Get() : m_resource.Get();
		const D3D12_RANGE nothingWritten{.Begin = 0, .End = 0};
		target->Unmap(0, m_hostReadback ? &nothingWritten : nullptr);
	}
} // namespace cct::gfx::rhi
