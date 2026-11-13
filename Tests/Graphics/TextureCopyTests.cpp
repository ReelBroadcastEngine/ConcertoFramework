#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>

#include <Concerto/Graphics/RenderGraph/RenderGraph.hpp>
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

	struct Extent
	{
		cct::UInt32 m_width;
		cct::UInt32 m_height;
	};

	constexpr std::array Extents{Extent{.m_width = 1, .m_height = 1}, Extent{.m_width = 3, .m_height = 5}, Extent{.m_width = 641, .m_height = 361}, Extent{.m_width = 1920, .m_height = 1080},
								 Extent{.m_width = 1080, .m_height = 1920}};

	std::byte PatternByte(std::size_t x, std::size_t y, cct::UInt32 seed)
	{
		return static_cast<std::byte>(((x * 13) + (y * 7) + seed) & 0xFFU);
	}

	// A texture of extent in format holding PatternByte(seed) in every byte, left in TransferSrcOptimal.
	std::shared_ptr<rhi::Texture> CreatePatternTexture(const cct::tests::GraphicsTestDevice& context, Extent extent, cct::gfx::PixelFormat format, cct::UInt32 seed)
	{
		rhi::Device& device = *context.GetDevice();
		const cct::UInt32 rowPitch = device.GetTextureCopyRowPitch(extent.m_width, BytesPerTexel);
		const std::size_t rowBytes = std::size_t{extent.m_width} * BytesPerTexel;
		const std::size_t bufferBytes = std::size_t{rowPitch} * extent.m_height;
		std::shared_ptr<rhi::Texture> texture = device.CreateTexture(format, static_cast<cct::Int32>(extent.m_width), static_cast<cct::Int32>(extent.m_height));
		const std::unique_ptr<rhi::Buffer> upload = device.CreateBuffer(static_cast<rhi::BufferUsageFlags>(rhi::BufferUsage::TransferSrc), static_cast<cct::UInt32>(bufferBytes), true);
		REQUIRE(texture != nullptr);
		REQUIRE(upload != nullptr);

		cct::Byte* mapped = nullptr;
		REQUIRE(upload->Map(&mapped));
		const std::span<std::byte> uploadBytes(reinterpret_cast<std::byte*>(mapped), bufferBytes);
		for (std::size_t y = 0; y < extent.m_height; ++y)
		{
			for (std::size_t x = 0; x < rowBytes; ++x)
			{
				uploadBytes.subspan((y * rowPitch) + x).front() = PatternByte(x, y, seed);
			}
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
			});
		return texture;
	}

	std::unique_ptr<rhi::Buffer> CreateReadbackBuffer(rhi::Device& device, Extent extent)
	{
		const cct::UInt32 rowPitch = device.GetTextureCopyRowPitch(extent.m_width, BytesPerTexel);
		std::unique_ptr<rhi::Buffer> readback = device.CreateBuffer(static_cast<rhi::BufferUsageFlags>(rhi::BufferUsage::TransferDst) | rhi::BufferUsage::HostReadback,
																	rowPitch * extent.m_height, true);
		REQUIRE(readback != nullptr);
		return readback;
	}

	// The bytes of readback that differ from PatternByte(seed).
	std::size_t CountMismatches(rhi::Device& device, rhi::Buffer& readback, Extent extent, cct::UInt32 seed)
	{
		const cct::UInt32 rowPitch = device.GetTextureCopyRowPitch(extent.m_width, BytesPerTexel);
		const std::size_t rowBytes = std::size_t{extent.m_width} * BytesPerTexel;
		cct::Byte* mapped = nullptr;
		REQUIRE(readback.Map(&mapped));
		const std::span<const std::byte> readBytes(reinterpret_cast<const std::byte*>(mapped), std::size_t{rowPitch} * extent.m_height);
		std::size_t mismatches = 0;
		for (std::size_t y = 0; y < extent.m_height; ++y)
		{
			for (std::size_t x = 0; x < rowBytes; ++x)
			{
				mismatches += readBytes.subspan((y * rowPitch) + x).front() == PatternByte(x, y, seed) ? 0 : 1;
			}
		}
		readback.UnMap();
		return mismatches;
	}

	void CheckRecordedCopy(const cct::tests::GraphicsTestDevice& context, Extent extent, cct::gfx::PixelFormat format)
	{
		rhi::Device& device = *context.GetDevice();
		const std::shared_ptr<rhi::Texture> source = CreatePatternTexture(context, extent, format, 1);
		const std::shared_ptr<rhi::Texture> destination = device.CreateTexture(format, static_cast<cct::Int32>(extent.m_width), static_cast<cct::Int32>(extent.m_height));
		REQUIRE(destination != nullptr);
		const std::unique_ptr<rhi::Buffer> readback = CreateReadbackBuffer(device, extent);

		context.SubmitAndWait(
			[&](rhi::CommandBuffer& commands)
			{
				commands.PipelineBarrier(*destination, rhi::ImageLayout::Undefined, rhi::ImageLayout::TransferDstOptimal, rhi::PipelineStage::Pipe, rhi::PipelineStage::Transfer, {},
										 rhi::MemoryAccess::TransferWrite);
				commands.Copy(*source, *destination);
				commands.PipelineBarrier(*destination, rhi::ImageLayout::TransferDstOptimal, rhi::ImageLayout::TransferSrcOptimal, rhi::PipelineStage::Transfer, rhi::PipelineStage::Transfer,
										 rhi::MemoryAccess::TransferWrite, rhi::MemoryAccess::TransferRead);
				commands.Copy(*destination, *readback);
				readback->RecordReadback(commands);
			});
		CHECK(CountMismatches(device, *readback, extent, 1) == 0);
	}

	void CheckRecordedCopies(rhi::Instance::Backend backend)
	{
		const cct::tests::GraphicsTestDevice context(backend);
		if (context.GetDevice() == nullptr)
		{
			SKIP("No device for this backend");
		}
		const auto source = context.GetDevice()->CreateTexture(cct::gfx::PixelFormat::RGBA8uNorm, 4, 4);
		const auto differentSize = context.GetDevice()->CreateTexture(cct::gfx::PixelFormat::RGBA8uNorm, 2, 4);
		const auto differentFormat = context.GetDevice()->CreateTexture(cct::gfx::PixelFormat::RGBA8_SRGB, 4, 4);
		REQUIRE(source != nullptr);
		REQUIRE(differentSize != nullptr);
		REQUIRE(differentFormat != nullptr);
		context.SubmitAndWait([&](rhi::CommandBuffer& commands)
							  {
								  CHECK_THROWS_AS(commands.Copy(*source, *source), std::invalid_argument);
								  CHECK_THROWS_AS(commands.Copy(*source, *differentSize), std::invalid_argument);
								  CHECK_THROWS_AS(commands.Copy(*source, *differentFormat), std::invalid_argument); });
		for (const cct::gfx::PixelFormat format : {cct::gfx::PixelFormat::RGBA8uNorm, cct::gfx::PixelFormat::RGBA8_SRGB})
		{
			for (const Extent extent : Extents)
			{
				INFO("format " << static_cast<int>(format) << ", extent " << extent.m_width << "x" << extent.m_height);
				CheckRecordedCopy(context, extent, format);
			}
		}
		INFO("first error: " << context.GetFirstError());
		CHECK(context.GetErrorCount() == 0);
	}

	// The RenderGraph pattern of a persistent texture refreshed every frame: the destination is imported as Undefined, its
	// old content discarded, written by a side-effect transfer pass and exported in ShaderReadOnlyOptimal; a later
	// submission reads it from that layout. The second frame rewrites the texture the first frame's reader used.
	void CheckGraphCopies(rhi::Instance::Backend backend)
	{
		const cct::tests::GraphicsTestDevice context(backend);
		if (context.GetDevice() == nullptr)
		{
			SKIP("No device for this backend");
		}
		rhi::Device& device = *context.GetDevice();
		constexpr Extent CopyExtent{.m_width = 641, .m_height = 361};
		constexpr cct::gfx::PixelFormat Format = cct::gfx::PixelFormat::RGBA8_SRGB;
		const std::shared_ptr<rhi::Texture> destination = device.CreateTexture(Format, static_cast<cct::Int32>(CopyExtent.m_width), static_cast<cct::Int32>(CopyExtent.m_height));
		REQUIRE(destination != nullptr);
		const std::unique_ptr<rhi::Buffer> readback = CreateReadbackBuffer(device, CopyExtent);
		rhi::RenderGraph writer(device, 1);
		rhi::RenderGraph reader(device, 1);

		for (const cct::UInt32 seed : {cct::UInt32{3}, cct::UInt32{200}})
		{
			INFO("seed " << seed);
			const std::shared_ptr<rhi::Texture> source = CreatePatternTexture(context, CopyExtent, Format, seed);

			writer.Reset();
			writer.Clear();
			const rhi::RGTextureHandle sourceHandle = writer.ImportTexture("Source", source, rhi::ImageLayout::TransferSrcOptimal, Format, CopyExtent.m_width, CopyExtent.m_height);
			const rhi::RGTextureHandle destinationHandle = writer.ImportTexture("Destination", destination, rhi::ImageLayout::Undefined, Format, CopyExtent.m_width, CopyExtent.m_height);
			writer.AddTransferPass(
				"Copy",
				[sourceHandle, destinationHandle](rhi::RenderGraphBuilder& builder)
				{
					builder.Read(sourceHandle);
					builder.Write(destinationHandle);
					builder.SetSideEffect();
				},
				[sourceHandle, destinationHandle](rhi::RenderGraphContext& graphContext)
				{ graphContext.GetCommandBuffer().Copy(graphContext.GetTexture(sourceHandle), graphContext.GetTexture(destinationHandle)); });
			writer.SetExportLayout(destinationHandle, rhi::ImageLayout::ShaderReadOnlyOptimal);
			bool executed = false;
			context.SubmitAndWait([&](rhi::CommandBuffer& commands)
								  { executed = writer.Execute(commands, CopyExtent.m_width, CopyExtent.m_height).IsOk(); });
			REQUIRE(executed);
			writer.MarkExecutionsComplete(writer.GetLastExecution());

			reader.Reset();
			reader.Clear();
			const rhi::RGTextureHandle readHandle = reader.ImportTexture("Published", destination, rhi::ImageLayout::ShaderReadOnlyOptimal, Format, CopyExtent.m_width, CopyExtent.m_height);
			reader.AddTransferPass(
				"Readback",
				[readHandle](rhi::RenderGraphBuilder& builder)
				{
					builder.Read(readHandle);
					builder.SetSideEffect();
				},
				[readHandle, &readback](rhi::RenderGraphContext& graphContext)
				{
					graphContext.GetCommandBuffer().Copy(graphContext.GetTexture(readHandle), *readback);
					readback->RecordReadback(graphContext.GetCommandBuffer());
				});
			reader.SetExportLayout(readHandle, rhi::ImageLayout::ShaderReadOnlyOptimal);
			executed = false;
			context.SubmitAndWait([&](rhi::CommandBuffer& commands)
								  { executed = reader.Execute(commands, CopyExtent.m_width, CopyExtent.m_height).IsOk(); });
			REQUIRE(executed);
			reader.MarkExecutionsComplete(reader.GetLastExecution());

			CHECK(CountMismatches(device, *readback, CopyExtent, seed) == 0);
		}
		INFO("first error: " << context.GetFirstError());
		CHECK(context.GetErrorCount() == 0);
	}
} // namespace

TEST_CASE("Vulkan: a texture copied into another texture of the same size and format holds the same bytes", "[graphics][gpu][vulkan]")
{
	CheckRecordedCopies(rhi::Instance::Backend::Vulkan);
}

TEST_CASE("Vulkan: a RenderGraph transfer pass refreshes a persistent texture by copy, imported as Undefined and exported for sampling", "[graphics][gpu][vulkan][rendergraph]")
{
	CheckGraphCopies(rhi::Instance::Backend::Vulkan);
}

#ifdef CCT_PLATFORM_WINDOWS
TEST_CASE("Direct3D 12: a texture copied into another texture of the same size and format holds the same bytes", "[graphics][gpu][dx12]")
{
	CheckRecordedCopies(rhi::Instance::Backend::DirectX12);
}

TEST_CASE("Direct3D 12: a RenderGraph transfer pass refreshes a persistent texture by copy, imported as Undefined and exported for sampling", "[graphics][gpu][dx12][rendergraph]")
{
	CheckGraphCopies(rhi::Instance::Backend::DirectX12);
}
#endif
// NOLINTEND(cppcoreguidelines-avoid-do-while)
