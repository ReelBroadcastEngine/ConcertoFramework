//
// Created by arthur on 03/09/2024.
//

#ifndef CONCERTO_GRAPHICS_RHI_HPP
#define CONCERTO_GRAPHICS_RHI_HPP

#include <span>
#include <type_traits>

#include <Concerto/Core/FunctionRef/FunctionRef.hpp>

#include "Concerto/Graphics/RHI/BufferMemoryProperties.hpp"
#include "Concerto/Graphics/RHI/Defines.hpp"

namespace cct::gfx::rhi
{
	class CommandBuffer;

	class CONCERTO_GRAPHICS_RHI_BASE_API Buffer
	{
	public:
		virtual ~Buffer() = default;

		// Mappable buffers (Device::CreateBuffer with allowBufferMapping) are for one transfer direction, set by their usage:
		// with BufferUsage::HostReadback the device writes and the host reads, otherwise the host writes and the device
		// reads. Map makes completed device writes visible to the host (after RecordReadback), UnMap makes host writes
		// visible to work submitted afterwards. Returns false when the buffer cannot be mapped.
		virtual bool Map(Byte** data) = 0;
		virtual void UnMap() = 0;
		[[nodiscard]] virtual BufferMemoryProperties GetMemoryProperties() const = 0;
		// Records, after the last device write of a HostReadback buffer in cmd, what the host needs to read those writes once
		// the submission completed: a device-to-host barrier, or a copy into host-readable memory.
		virtual void RecordReadback(CommandBuffer& /*cmd*/)
		{
		}

		template<typename T>
			requires std::is_trivially_copyable_v<T>
		void Write(T& object, UInt32 padding = 0);

		template<typename DestBuffer, typename SrcObj>
		void Write(std::span<SrcObj> objects, FunctionRef<void(DestBuffer& destBuffer, SrcObj& srcObj)>&& copyFunc, std::size_t padding = 0);
	};
} // namespace cct::gfx::rhi

#include "Concerto/Graphics/RHI/Buffer.inl"

#endif // CONCERTO_GRAPHICS_RHI_HPP