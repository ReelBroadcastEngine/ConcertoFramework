#include <array>
#include <cstddef>
#include <memory>
#include <span>

#include <Concerto/Graphics/RHI/Buffer.hpp>
#include <Concerto/Graphics/RHI/CommandBuffer.hpp>
#include <Concerto/Graphics/RHI/Device.hpp>
#include <Concerto/Graphics/RHI/Texture.hpp>

#include "GraphicsTestDevice/GraphicsTestDevice.hpp"
#include <catch2/catch_test_macros.hpp>

// NOLINTBEGIN(cppcoreguidelines-avoid-do-while): Catch2's assertion macros expand to do { } while (false).
namespace
{
	namespace rhi = cct::gfx::rhi;

	constexpr cct::UInt32 BytesPerTexel = 4;

	std::byte PatternByte(std::size_t x, std::size_t y)
	{
		return static_cast<std::byte>(((x * 13) + (y * 7) + 1) & 0xFFU);
	}

	struct Extent
	{
		cct::UInt32 m_width;
		cct::UInt32 m_height;
	};

	constexpr std::array Extents{Extent{.m_width = 1, .m_height = 1}, Extent{.m_width = 3, .m_height = 5}, Extent{.m_width = 641, .m_height = 361}, Extent{.m_width = 1920, .m_height = 1080},
								 Extent{.m_width = 1080, .m_height = 1920}};

	void CheckRoundTrip(const cct::tests::GraphicsTestDevice& context, Extent extent)
	{
		rhi::Device& device = *context.GetDevice();
		const cct::UInt32 rowPitch = device.GetTextureCopyRowPitch(extent.m_width, BytesPerTexel);
		const std::size_t rowBytes = std::size_t{extent.m_width} * BytesPerTexel;
		const std::size_t bufferBytes = std::size_t{rowPitch} * extent.m_height;
		REQUIRE(rowPitch >= rowBytes);

		const std::shared_ptr<rhi::Texture> texture = device.CreateTexture(cct::gfx::PixelFormat::RGBA8uNorm, static_cast<cct::Int32>(extent.m_width), static_cast<cct::Int32>(extent.m_height));
		const std::unique_ptr<rhi::Buffer> upload = device.CreateBuffer(static_cast<rhi::BufferUsageFlags>(rhi::BufferUsage::TransferSrc), static_cast<cct::UInt32>(bufferBytes), true);
		const std::unique_ptr<rhi::Buffer> readback =
			device.CreateBuffer(static_cast<rhi::BufferUsageFlags>(rhi::BufferUsage::TransferDst) | rhi::BufferUsage::HostReadback, static_cast<cct::UInt32>(bufferBytes), true);
		REQUIRE(texture != nullptr);
		REQUIRE(upload != nullptr);
		REQUIRE(readback != nullptr);
		CHECK(upload->GetMemoryProperties().m_hostVisible);
		CHECK(readback->GetMemoryProperties().m_hostVisible);

		cct::Byte* mapped = nullptr;
		REQUIRE(upload->Map(&mapped));
		const std::span<std::byte> uploadBytes(reinterpret_cast<std::byte*>(mapped), bufferBytes);
		for (std::size_t y = 0; y < extent.m_height; ++y)
		{
			for (std::size_t x = 0; x < rowBytes; ++x)
				uploadBytes.subspan((y * rowPitch) + x).front() = PatternByte(x, y);
		}
		upload->UnMap();

		context.SubmitAndWait(
			[&](rhi::CommandBuffer& commands)
			{
				commands.PipelineBarrier(*texture, rhi::ImageLayout::Undefined, rhi::ImageLayout::TransferDstOptimal, rhi::PipelineStage::Pipe, rhi::PipelineStage::Transfer, {},
										 rhi::MemoryAccess::TransferWrite);
				commands.Copy(*upload, *texture);
				commands.PipelineBarrier(*texture, rhi::ImageLayout::TransferDstOptimal, rhi::ImageLayout::TransferSrcOptimal, rhi::PipelineStage::Transfer, rhi::PipelineStage::Transfer,
										 rhi::MemoryAccess::TransferWrite, rhi::MemoryAccess::TransferRead);
				commands.Copy(*texture, *readback);
				readback->RecordReadback(commands);
			});

		REQUIRE(readback->Map(&mapped));
		const std::span<const std::byte> readBytes(reinterpret_cast<const std::byte*>(mapped), bufferBytes);
		std::size_t mismatches = 0;
		for (std::size_t y = 0; y < extent.m_height; ++y)
		{
			for (std::size_t x = 0; x < rowBytes; ++x)
			{
				if (readBytes.subspan((y * rowPitch) + x).front() != PatternByte(x, y))
					++mismatches;
			}
		}
		readback->UnMap();
		CHECK(mismatches == 0);
	}

	void CheckBackend(rhi::Instance::Backend backend)
	{
		const cct::tests::GraphicsTestDevice context(backend);
		if (context.GetDevice() == nullptr)
			SKIP("No device for this backend");
		for (const Extent extent : Extents)
		{
			INFO("extent " << extent.m_width << "x" << extent.m_height);
			CheckRoundTrip(context, extent);
		}
		INFO("first error: " << context.GetFirstError());
		CHECK(context.GetErrorCount() == 0);
	}
} // namespace

TEST_CASE("Vulkan: a texture read back through a HostReadback buffer holds what a host-written buffer uploaded", "[graphics][gpu][vulkan]")
{
	CheckBackend(rhi::Instance::Backend::Vulkan);
}

#ifdef CCT_PLATFORM_WINDOWS
TEST_CASE("Direct3D 12: a texture read back through a HostReadback buffer holds what a host-written buffer uploaded", "[graphics][gpu][dx12]")
{
	CheckBackend(rhi::Instance::Backend::DirectX12);
}
#endif

TEST_CASE("Vulkan: buffers are allocated for their host access direction", "[graphics][gpu][vulkan]")
{
	const cct::tests::GraphicsTestDevice context(rhi::Instance::Backend::Vulkan);
	if (context.GetDevice() == nullptr)
		SKIP("No Vulkan device");
	rhi::Device& device = *context.GetDevice();
	const std::unique_ptr<rhi::Buffer> readback = device.CreateBuffer(static_cast<rhi::BufferUsageFlags>(rhi::BufferUsage::TransferDst) | rhi::BufferUsage::HostReadback, 4096, true);
	const std::unique_ptr<rhi::Buffer> upload = device.CreateBuffer(static_cast<rhi::BufferUsageFlags>(rhi::BufferUsage::TransferSrc), 4096, true);
	const std::unique_ptr<rhi::Buffer> deviceOnly = device.CreateBuffer(static_cast<rhi::BufferUsageFlags>(rhi::BufferUsage::TransferDst), 4096, false);
	REQUIRE(readback != nullptr);
	REQUIRE(upload != nullptr);
	REQUIRE(deviceOnly != nullptr);
	CHECK(readback->GetMemoryProperties().m_hostVisible);
	CHECK(upload->GetMemoryProperties().m_hostVisible);
	CHECK(deviceOnly->GetMemoryProperties().m_deviceLocal);
	CHECK_FALSE(deviceOnly->GetMemoryProperties().m_hostVisible);
	// Vulkan does not guarantee a host-cached memory type, so only report a device that lacks one.
	if (!readback->GetMemoryProperties().m_hostCached)
		WARN("The readback buffer is not host-cached on this device: host reads from it are uncached");
	CHECK(context.GetErrorCount() == 0);
}
// NOLINTEND(cppcoreguidelines-avoid-do-while)
