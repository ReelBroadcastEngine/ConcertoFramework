//
// Created by arthur on 11/05/2026.
//

#include "Concerto/Graphics/RenderGraph/RenderGraphResourceRegistry.hpp"

#include <algorithm>
#include <functional>

#include <Concerto/Core/Assert.hpp>

#include "Concerto/Graphics/RHI/Device.hpp"

namespace cct::gfx::rhi
{
	namespace
	{
		void HashCombine(std::size_t& seed, std::size_t value)
		{
			seed ^= value + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
		}

		bool FitsLimits(const RGTextureDesc& desc, const DeviceLimits& limits)
		{
			if (desc.width == 0 || desc.height == 0)
				return false;
			const UInt32 maxWidth = std::min(limits.m_maxTextureDimension2D, limits.m_maxFrameBufferWidth);
			const UInt32 maxHeight = std::min(limits.m_maxTextureDimension2D, limits.m_maxFrameBufferHeight);
			return desc.width <= maxWidth && desc.height <= maxHeight;
		}
	} // namespace

	std::size_t RenderGraphResourceRegistry::TextureKeyHash::operator()(const TextureKey& key) const
	{
		std::size_t seed = std::hash<UInt32>{}(key.width);
		HashCombine(seed, std::hash<UInt32>{}(key.height));
		HashCombine(seed, std::hash<UInt8>{}(static_cast<UInt8>(key.format)));
		HashCombine(seed, std::hash<bool>{}(key.isDepth));
		HashCombine(seed, std::hash<bool>{}(key.storage));
		return seed;
	}

	std::size_t RenderGraphResourceRegistry::BufferKeyHash::operator()(const BufferKey& key) const
	{
		std::size_t seed = std::hash<UInt32>{}(key.size);
		HashCombine(seed, std::hash<UInt32>{}(key.usage));
		return seed;
	}

	RenderGraphResourceRegistry::TextureKey RenderGraphResourceRegistry::KeyOf(const RGTextureDesc& desc)
	{
		return TextureKey{.width = desc.width, .height = desc.height, .format = desc.format, .isDepth = desc.isDepth, .storage = desc.storage};
	}

	RenderGraphResourceRegistry::BufferKey RenderGraphResourceRegistry::KeyOf(const RGBufferDesc& desc)
	{
		return BufferKey{.size = desc.size, .usage = desc.usage.Value()};
	}

	RGTextureHandle RenderGraphResourceRegistry::RegisterTexture(const RGTextureDesc& desc)
	{
		const UInt16 idx = static_cast<UInt16>(m_textures.size());
		m_textures.push_back({desc, nullptr, ImageLayout::Undefined, ImageLayout::Undefined, false, 0});
		return {idx, 0};
	}

	RGBufferHandle RenderGraphResourceRegistry::RegisterBuffer(const RGBufferDesc& desc)
	{
		const UInt16 idx = static_cast<UInt16>(m_buffers.size());
		m_buffers.push_back({desc, nullptr, {}, {}, false, 0});
		return {idx, 0};
	}

	RGTextureHandle RenderGraphResourceRegistry::ImportTexture(const char* name,
															   std::shared_ptr<Texture> texture,
															   ImageLayout currentLayout,
															   PixelFormat format,
															   UInt32 width,
															   UInt32 height)
	{
		const UInt16 idx = static_cast<UInt16>(m_textures.size());
		RGTextureDesc desc;
		desc.name = name;
		desc.format = format;
		desc.width = width;
		desc.height = height;
		m_textures.push_back({desc, std::move(texture), currentLayout, currentLayout, true, 0});
		return {idx, 0};
	}

	RGBufferHandle RenderGraphResourceRegistry::ImportBuffer(const char* name,
															 std::shared_ptr<Buffer> buffer,
															 PipelineStageFlags initialStage,
															 MemoryAccessFlags initialAccess)
	{
		const UInt16 idx = static_cast<UInt16>(m_buffers.size());
		RGBufferDesc desc;
		desc.name = name;
		m_buffers.push_back({desc, std::move(buffer), initialStage, initialAccess, true, 0});
		return {idx, 0};
	}

	void RenderGraphResourceRegistry::UpdateImportedTexture(RGTextureHandle handle, std::shared_ptr<Texture> texture, ImageLayout currentLayout)
	{
		CCT_ASSERT(handle.index < m_textures.size() && m_textures[handle.index].imported,
				   "RenderGraphResourceRegistry::UpdateImportedTexture: handle {} is not an imported texture", handle.index);
		TextureEntry& entry = m_textures[handle.index];
		entry.physical = std::move(texture);
		entry.currentLayout = currentLayout;
		entry.initialLayout = currentLayout;
	}

	Result<void, RenderGraphError> RenderGraphResourceRegistry::Allocate(Device& device, RenderGraphStatistics& statistics)
	{
		const DeviceLimits limits = device.GetLimits();
		for (auto& entry : m_textures)
		{
			if (entry.imported || entry.physical)
				continue;

			auto pool = m_transientTexturePool.find(KeyOf(entry.desc));
			if (pool != m_transientTexturePool.end() && !pool->second.empty())
			{
				entry.physical = std::move(pool->second.back().physical);
				entry.currentLayout = pool->second.back().lastLayout;
				pool->second.pop_back();
				++statistics.m_texturesReused;
				continue;
			}

			if (!FitsLimits(entry.desc, limits))
			{
				CCT_GFX_LOG_ERROR("RenderGraph", "transient '{}' of {}x{} exceeds the device limits", entry.desc.name, entry.desc.width, entry.desc.height);
				return RenderGraphError::ResourceLimitExceeded;
			}
			std::shared_ptr<Texture> created = entry.desc.storage
												   ? device.CreateStorageTexture(entry.desc.format, static_cast<Int32>(entry.desc.width), static_cast<Int32>(entry.desc.height))
												   : device.CreateTexture(entry.desc.format, static_cast<Int32>(entry.desc.width), static_cast<Int32>(entry.desc.height));
			if (created == nullptr)
			{
				CCT_GFX_LOG_ERROR("RenderGraph", "transient '{}' of {}x{} could not be created", entry.desc.name, entry.desc.width, entry.desc.height);
				return RenderGraphError::ResourceCreationFailed;
			}
			entry.physical = std::move(created);
			entry.currentLayout = ImageLayout::Undefined;
			++statistics.m_texturesCreated;
		}

		for (auto& entry : m_buffers)
		{
			if (entry.imported || entry.physical)
				continue;

			auto pool = m_transientBufferPool.find(KeyOf(entry.desc));
			if (pool != m_transientBufferPool.end() && !pool->second.empty())
			{
				entry.physical = std::move(pool->second.back().physical);
				entry.currentStage = pool->second.back().lastStage;
				entry.currentAccess = pool->second.back().lastAccess;
				pool->second.pop_back();
				++statistics.m_buffersReused;
				continue;
			}

			std::unique_ptr<Buffer> created = entry.desc.size == 0 ? nullptr : device.CreateBuffer(entry.desc.usage, entry.desc.size, false);
			if (created == nullptr)
			{
				CCT_GFX_LOG_ERROR("RenderGraph", "transient buffer '{}' of {} bytes could not be created", entry.desc.name, entry.desc.size);
				return RenderGraphError::ResourceCreationFailed;
			}
			entry.physical = std::shared_ptr<Buffer>(std::move(created));
			++statistics.m_buffersCreated;
		}
		return {};
	}

	void RenderGraphResourceRegistry::ExtractTransients(
		std::vector<RenderGraphResourceRegistry::TextureTransientSnapshot>& outTextures,
		std::vector<RenderGraphResourceRegistry::BufferTransientSnapshot>& outBuffers)
	{
		for (auto& entry : m_textures)
		{
			if (!entry.imported && entry.physical)
			{
				outTextures.push_back({entry.desc, std::move(entry.physical), entry.currentLayout});
				entry.currentLayout = ImageLayout::Undefined;
			}
		}

		for (auto& entry : m_buffers)
		{
			if (!entry.imported && entry.physical)
			{
				outBuffers.push_back({entry.desc, std::move(entry.physical), entry.currentStage, entry.currentAccess});
				entry.currentStage = {};
				entry.currentAccess = {};
			}
		}
	}

	void RenderGraphResourceRegistry::ReturnTransientsToPool(
		std::vector<RenderGraphResourceRegistry::TextureTransientSnapshot>& textures,
		std::vector<RenderGraphResourceRegistry::BufferTransientSnapshot>& buffers,
		UInt64 lastUse)
	{
		// Bounded per description, on top of TrimPool: a resource beyond the bound is destroyed at once, which is safe
		// because returned resources are no longer used by the GPU.
		constexpr std::size_t MaxPooledPerDesc = 8;

		for (auto& snap : textures)
		{
			auto& bucket = m_transientTexturePool[KeyOf(snap.desc)];
			if (bucket.size() < MaxPooledPerDesc)
				bucket.push_back({std::move(snap.physical), snap.lastLayout, lastUse});
		}
		for (auto& snap : buffers)
		{
			auto& bucket = m_transientBufferPool[KeyOf(snap.desc)];
			if (bucket.size() < MaxPooledPerDesc)
				bucket.push_back({std::move(snap.physical), snap.lastStage, snap.lastAccess, lastUse});
		}
		textures.clear();
		buffers.clear();
	}

	void RenderGraphResourceRegistry::TrimPool(UInt64 lastExecution, UInt64 idleExecutions)
	{
		const auto idle = [lastExecution, idleExecutions](UInt64 lastUse)
		{ return lastExecution >= lastUse + idleExecutions; };
		for (auto& [key, bucket] : m_transientTexturePool)
			std::erase_if(bucket, [&](const PooledTexture& pooled)
						  { return idle(pooled.lastUse); });
		for (auto& [key, bucket] : m_transientBufferPool)
			std::erase_if(bucket, [&](const PooledBuffer& pooled)
						  { return idle(pooled.lastUse); });
		std::erase_if(m_transientTexturePool, [](const auto& item)
					  { return item.second.empty(); });
		std::erase_if(m_transientBufferPool, [](const auto& item)
					  { return item.second.empty(); });
	}

	void RenderGraphResourceRegistry::ClearEntries()
	{
		m_textures.clear();
		m_buffers.clear();
		m_exportLayouts.clear();
	}

	void RenderGraphResourceRegistry::SetExportLayout(RGTextureHandle handle, ImageLayout layout)
	{
		m_exportLayouts[handle.index] = {handle, layout};
	}

	void RenderGraphResourceRegistry::Reset()
	{
		// Transient entries keep their physical resource and tracked layout across frames:
		// the handle->physical assignment is stable until ClearEntries(), so descriptor sets
		// bound to a transient texture stay valid between rebuilds.
		for (auto& entry : m_textures)
		{
			if (entry.imported)
				entry.currentLayout = entry.initialLayout;
		}
	}

	ImageLayout RenderGraphResourceRegistry::GetCurrentLayout(RGTextureHandle handle) const
	{
		CCT_ASSERT(handle.index < m_textures.size(),
				   "RenderGraphResourceRegistry::GetCurrentLayout: invalid handle index {}", handle.index);
		return m_textures[handle.index].currentLayout;
	}

	void RenderGraphResourceRegistry::SetCurrentLayout(RGTextureHandle handle, ImageLayout layout)
	{
		CCT_ASSERT(handle.index < m_textures.size(),
				   "RenderGraphResourceRegistry::SetCurrentLayout: invalid handle index {}", handle.index);
		m_textures[handle.index].currentLayout = layout;
	}

	PipelineStageFlags RenderGraphResourceRegistry::GetCurrentBufferStage(RGBufferHandle handle) const
	{
		CCT_ASSERT(handle.index < m_buffers.size(),
				   "RenderGraphResourceRegistry::GetCurrentBufferStage: invalid handle index {}", handle.index);
		return m_buffers[handle.index].currentStage;
	}

	MemoryAccessFlags RenderGraphResourceRegistry::GetCurrentBufferAccess(RGBufferHandle handle) const
	{
		CCT_ASSERT(handle.index < m_buffers.size(),
				   "RenderGraphResourceRegistry::GetCurrentBufferAccess: invalid handle index {}", handle.index);
		return m_buffers[handle.index].currentAccess;
	}

	void RenderGraphResourceRegistry::SetCurrentBufferState(RGBufferHandle handle, PipelineStageFlags stage, MemoryAccessFlags access)
	{
		CCT_ASSERT(handle.index < m_buffers.size(),
				   "RenderGraphResourceRegistry::SetCurrentBufferState: invalid handle index {}", handle.index);
		m_buffers[handle.index].currentStage = stage;
		m_buffers[handle.index].currentAccess = access;
	}

	Texture& RenderGraphResourceRegistry::GetTexture(RGTextureHandle handle)
	{
		CCT_ASSERT(handle.index < m_textures.size() && m_textures[handle.index].physical != nullptr,
				   "RenderGraphResourceRegistry::GetTexture: invalid or unallocated handle index {}", handle.index);
		return *m_textures[handle.index].physical;
	}

	const std::shared_ptr<Texture>& RenderGraphResourceRegistry::GetTexturePointer(RGTextureHandle handle) const
	{
		CCT_ASSERT(handle.index < m_textures.size(),
				   "RenderGraphResourceRegistry::GetTexturePointer: invalid handle index {}", handle.index);
		return m_textures[handle.index].physical;
	}

	Buffer& RenderGraphResourceRegistry::GetBuffer(RGBufferHandle handle)
	{
		CCT_ASSERT(handle.index < m_buffers.size() && m_buffers[handle.index].physical != nullptr,
				   "RenderGraphResourceRegistry::GetBuffer: invalid or unallocated handle index {}", handle.index);
		return *m_buffers[handle.index].physical;
	}

	const RGTextureDesc& RenderGraphResourceRegistry::GetDesc(RGTextureHandle handle) const
	{
		CCT_ASSERT(handle.index < m_textures.size(),
				   "RenderGraphResourceRegistry::GetDesc: invalid handle index {}", handle.index);
		return m_textures[handle.index].desc;
	}

	bool RenderGraphResourceRegistry::IsImported(RGTextureHandle handle) const
	{
		CCT_ASSERT(handle.index < m_textures.size(),
				   "RenderGraphResourceRegistry::IsImported: invalid handle index {}", handle.index);
		return m_textures[handle.index].imported;
	}

	bool RenderGraphResourceRegistry::IsImported(RGBufferHandle handle) const
	{
		CCT_ASSERT(handle.index < m_buffers.size(),
				   "RenderGraphResourceRegistry::IsImported: invalid handle index {}", handle.index);
		return m_buffers[handle.index].imported;
	}

	std::size_t RenderGraphResourceRegistry::GetPooledTextureCount() const
	{
		std::size_t count = 0;
		for (const auto& [key, bucket] : m_transientTexturePool)
			count += bucket.size();
		return count;
	}

	std::size_t RenderGraphResourceRegistry::GetPooledBufferCount() const
	{
		std::size_t count = 0;
		for (const auto& [key, bucket] : m_transientBufferPool)
			count += bucket.size();
		return count;
	}
} // namespace cct::gfx::rhi
