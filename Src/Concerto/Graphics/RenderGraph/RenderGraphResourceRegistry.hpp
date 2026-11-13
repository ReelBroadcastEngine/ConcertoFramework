//
// Created by arthur on 11/05/2026.
//

#ifndef CONCERTO_GRAPHICS_RHI_RENDERGRAPH_RESOURCE_REGISTRY_HPP
#define CONCERTO_GRAPHICS_RHI_RENDERGRAPH_RESOURCE_REGISTRY_HPP

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

#include <Concerto/Core/Result/Result.hpp>

#include "Concerto/Graphics/RenderGraph/RenderGraphError.hpp"
#include "Concerto/Graphics/RenderGraph/RenderGraphResource.hpp"
#include "Concerto/Graphics/RenderGraph/RenderGraphStatistics.hpp"
#include "Concerto/Graphics/RHI/Buffer.hpp"
#include "Concerto/Graphics/RHI/Defines.hpp"
#include "Concerto/Graphics/RHI/Enums.hpp"
#include "Concerto/Graphics/RHI/Texture.hpp"

namespace cct::gfx::rhi
{
	class Device;

	class CONCERTO_GRAPHICS_RHI_BASE_API RenderGraphResourceRegistry
	{
	public:
		struct TextureTransientSnapshot
		{
			RGTextureDesc desc;
			std::shared_ptr<Texture> physical;
			ImageLayout lastLayout = ImageLayout::Undefined;
		};

		struct BufferTransientSnapshot
		{
			RGBufferDesc desc;
			std::shared_ptr<Buffer> physical;
			PipelineStageFlags lastStage;
			MemoryAccessFlags lastAccess;
		};

		RGTextureHandle RegisterTexture(const RGTextureDesc& desc);
		RGBufferHandle RegisterBuffer(const RGBufferDesc& desc);

		RGTextureHandle ImportTexture(const char* name,
									  std::shared_ptr<Texture> texture,
									  ImageLayout currentLayout,
									  PixelFormat format = PixelFormat::RGBA8_SRGB,
									  UInt32 width = 0,
									  UInt32 height = 0);
		RGBufferHandle ImportBuffer(const char* name,
									std::shared_ptr<Buffer> buffer,
									PipelineStageFlags initialStage = {},
									MemoryAccessFlags initialAccess = {});

		void UpdateImportedTexture(RGTextureHandle handle, std::shared_ptr<Texture> texture, ImageLayout currentLayout);

		// Assigns a physical resource to every transient without one, from the pool when it holds one of the same
		// description. Stops at the first transient whose description exceeds the device limits or whose creation fails;
		// the transients assigned before it keep their resource.
		[[nodiscard]] Result<void, RenderGraphError> Allocate(Device& device, RenderGraphStatistics& statistics);

		// Extract physical resources from transient entries into caller-owned snapshots
		// and clear the entries so the next Allocate() can re-populate them.
		void ExtractTransients(std::vector<TextureTransientSnapshot>& outTextures,
							   std::vector<BufferTransientSnapshot>& outBuffers);

		// Returns snapshots to the pool, recording lastUse, the serial of the last execution that used them. Only call it
		// once that execution has completed: a pooled resource is handed out again without synchronization.
		void ReturnTransientsToPool(std::vector<TextureTransientSnapshot>& textures,
									std::vector<BufferTransientSnapshot>& buffers,
									UInt64 lastUse);

		// Destroys the pooled resources whose last use is idleExecutions or more executions older than lastExecution.
		void TrimPool(UInt64 lastExecution, UInt64 idleExecutions);

		void Reset();

		// Clear all registered texture and buffer entries (imported and transient
		// descriptors) while keeping pool buckets for reuse on the next Allocate().
		void ClearEntries();

		[[nodiscard]] ImageLayout GetCurrentLayout(RGTextureHandle handle) const;
		void SetCurrentLayout(RGTextureHandle handle, ImageLayout layout);

		[[nodiscard]] PipelineStageFlags GetCurrentBufferStage(RGBufferHandle handle) const;
		[[nodiscard]] MemoryAccessFlags GetCurrentBufferAccess(RGBufferHandle handle) const;
		void SetCurrentBufferState(RGBufferHandle handle, PipelineStageFlags stage, MemoryAccessFlags access);

		// Declare the layout a texture must be in after the last render graph pass that writes it.
		// RenderGraph::Execute() emits the required barrier at the end of the frame.
		void SetExportLayout(RGTextureHandle handle, ImageLayout layout);
		[[nodiscard]] const std::unordered_map<UInt16, std::pair<RGTextureHandle, ImageLayout>>& ExportLayouts() const
		{
			return m_exportLayouts;
		}

		Texture& GetTexture(RGTextureHandle handle);
		// The shared owner of the physical texture behind handle; null for a transient before Allocate.
		[[nodiscard]] const std::shared_ptr<Texture>& GetTexturePointer(RGTextureHandle handle) const;
		Buffer& GetBuffer(RGBufferHandle handle);
		const RGTextureDesc& GetDesc(RGTextureHandle handle) const;
		bool IsImported(RGTextureHandle handle) const;
		bool IsImported(RGBufferHandle handle) const;

		[[nodiscard]] UInt32 TextureCount() const
		{
			return static_cast<UInt32>(m_textures.size());
		}
		[[nodiscard]] UInt32 BufferCount() const
		{
			return static_cast<UInt32>(m_buffers.size());
		}
		[[nodiscard]] std::size_t GetPooledTextureCount() const;
		[[nodiscard]] std::size_t GetPooledBufferCount() const;

	private:
		struct TextureEntry
		{
			RGTextureDesc desc;
			std::shared_ptr<Texture> physical;
			ImageLayout currentLayout = ImageLayout::Undefined;
			ImageLayout initialLayout = ImageLayout::Undefined;
			bool imported = false;
			UInt16 version = 0;
		};

		struct BufferEntry
		{
			RGBufferDesc desc;
			std::shared_ptr<Buffer> physical;
			PipelineStageFlags currentStage;
			MemoryAccessFlags currentAccess;
			bool imported = false;
			UInt16 version = 0;
		};

		// What makes two transients interchangeable; the name is not part of it.
		struct TextureKey
		{
			UInt32 m_width = 0;
			UInt32 m_height = 0;
			PixelFormat m_format = PixelFormat::RGBA8_SRGB;
			bool m_isDepth = false;
			bool m_storage = false;
			bool operator==(const TextureKey&) const = default;
		};
		struct TextureKeyHash
		{
			std::size_t operator()(const TextureKey& key) const;
		};
		struct BufferKey
		{
			UInt32 m_size = 0;
			UInt32 m_usage = 0;
			bool operator==(const BufferKey&) const = default;
		};
		struct BufferKeyHash
		{
			std::size_t operator()(const BufferKey& key) const;
		};

		struct PooledTexture
		{
			std::shared_ptr<Texture> m_physical;
			ImageLayout m_lastLayout = ImageLayout::Undefined;
			UInt64 m_lastUse = 0;
		};
		struct PooledBuffer
		{
			std::shared_ptr<Buffer> m_physical;
			PipelineStageFlags m_lastStage;
			MemoryAccessFlags m_lastAccess;
			UInt64 m_lastUse = 0;
		};

		static TextureKey KeyOf(const RGTextureDesc& desc);
		static BufferKey KeyOf(const RGBufferDesc& desc);

		std::vector<TextureEntry> m_textures;
		std::vector<BufferEntry> m_buffers;
		std::unordered_map<TextureKey, std::vector<PooledTexture>, TextureKeyHash> m_transientTexturePool;
		std::unordered_map<BufferKey, std::vector<PooledBuffer>, BufferKeyHash> m_transientBufferPool;
		std::unordered_map<UInt16, std::pair<RGTextureHandle, ImageLayout>> m_exportLayouts;
	};
} // namespace cct::gfx::rhi

#endif // CONCERTO_GRAPHICS_RHI_RENDERGRAPH_RESOURCE_REGISTRY_HPP
