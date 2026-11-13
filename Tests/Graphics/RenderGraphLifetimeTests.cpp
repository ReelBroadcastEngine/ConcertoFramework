#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <span>

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

	struct Extent
	{
		cct::UInt32 m_width;
		cct::UInt32 m_height;
	};

	// A frame of the kind RBE declares every frame: two transient colour textures cleared by graphics passes (one
	// RGBA8, one RGBA16F), a third pass that writes the first texture again, and a side-effect transfer pass that reads
	// the first texture back into readback, which must hold readback->size bytes.
	void DeclareFrame(rhi::RenderGraph& graph, Extent extent, rhi::Buffer& readback, cct::gfx::PixelFormat secondFormat = cct::gfx::PixelFormat::RGBA16f)
	{
		const rhi::RGTextureHandle colour = graph.CreateTexture({.width = extent.m_width, .height = extent.m_height, .format = cct::gfx::PixelFormat::RGBA8uNorm, .isDepth = false, .name = "Colour"});
		const rhi::RGTextureHandle work = graph.CreateTexture({.width = extent.m_width, .height = extent.m_height, .format = secondFormat, .isDepth = false, .name = "Work"});
		graph.AddGraphicsPass("ClearWork", [work](rhi::RenderGraphBuilder& builder)
							  { builder.Write(work); }, [](rhi::RenderGraphContext&) {});
		graph.AddGraphicsPass("ClearColour", [colour, work](rhi::RenderGraphBuilder& builder)
							  {
								  builder.Read(work);
								  builder.Write(colour); }, [](rhi::RenderGraphContext&) {});
		graph.AddTransferPass("Readback", [colour](rhi::RenderGraphBuilder& builder)
							  {
								  builder.Read(colour);
								  builder.SetSideEffect(); }, [colour, &readback](rhi::RenderGraphContext& context)
							  {
								  context.GetCommandBuffer().Copy(context.GetTexture(colour), readback);
								  readback.RecordReadback(context.GetCommandBuffer()); });
	}

	// Declares and runs one frame; with explicitCompletion, tells the graph its execution completed.
	void RunFrame(const cct::tests::GraphicsTestDevice& context, rhi::RenderGraph& graph, Extent extent, rhi::Buffer& readback, bool explicitCompletion,
				  cct::gfx::PixelFormat secondFormat = cct::gfx::PixelFormat::RGBA16f)
	{
		graph.Reset();
		graph.Clear();
		DeclareFrame(graph, extent, readback, secondFormat);
		bool executed = false;
		context.SubmitAndWait([&](rhi::CommandBuffer& commands)
							  { executed = graph.Execute(commands, extent.m_width, extent.m_height).IsOk(); });
		REQUIRE(executed);
		if (explicitCompletion)
		{
			graph.MarkExecutionsComplete(graph.GetLastExecution());
		}
	}

	std::unique_ptr<rhi::Buffer> MakeReadback(rhi::Device& device, Extent extent)
	{
		const cct::UInt32 size = device.GetTextureCopyRowPitch(extent.m_width, 4) * cct::UInt32{extent.m_height};
		return device.CreateBuffer(static_cast<rhi::BufferUsageFlags>(rhi::BufferUsage::TransferDst) | rhi::BufferUsage::HostReadback, size, true);
	}

	constexpr Extent Small{.m_width = 64, .m_height = 32};
	constexpr Extent Large{.m_width = 96, .m_height = 48};
} // namespace

TEST_CASE("RenderGraph: an owner that reports completion keeps one generation of transients and creates nothing after the first frame", "[graphics][gpu][vulkan][rendergraph]")
{
	const cct::tests::GraphicsTestDevice context(rhi::Instance::Backend::Vulkan);
	if (context.GetDevice() == nullptr)
	{
		SKIP("No Vulkan device");
	}
	{
		rhi::RenderGraph graph(*context.GetDevice(), 1);
		const std::unique_ptr<rhi::Buffer> readback = MakeReadback(*context.GetDevice(), Small);

		RunFrame(context, graph, Small, *readback, true);
		const rhi::RenderGraphStatistics first = graph.GetStatistics();
		CHECK(first.m_texturesCreated == 2);
		for (int frame = 0; frame < 20; ++frame)
		{
			RunFrame(context, graph, Small, *readback, true);
		}
		const rhi::RenderGraphStatistics after = graph.GetStatistics();
		CHECK(after.m_texturesCreated == first.m_texturesCreated);
		CHECK(after.m_renderPassesCreated == first.m_renderPassesCreated);
		CHECK(after.m_frameBuffersCreated == first.m_frameBuffersCreated);
		CHECK(after.m_texturesReused == 40);

		const rhi::RenderGraphResidency residency = graph.GetResidency();
		CHECK(residency.m_assignedTextures == 2);
		CHECK(residency.m_retiredTextures == 0);
		CHECK(residency.m_pooledTextures == 0);
		CHECK(residency.m_cachedFrameBuffers == 2);
		CHECK(residency.m_retiredFrameBuffers == 0);
	}
	INFO("first error: " << context.GetFirstError());
	CHECK(context.GetErrorCount() == 0);
}

TEST_CASE("RenderGraph: without completion reports, retired transients wait maxFramesInFlight resets and stay bounded", "[graphics][gpu][vulkan][rendergraph]")
{
	const cct::tests::GraphicsTestDevice context(rhi::Instance::Backend::Vulkan);
	if (context.GetDevice() == nullptr)
	{
		SKIP("No Vulkan device");
	}
	{
		constexpr cct::UInt32 FramesInFlight = 2;
		rhi::RenderGraph graph(*context.GetDevice(), FramesInFlight);
		const std::unique_ptr<rhi::Buffer> readback = MakeReadback(*context.GetDevice(), Small);
		for (int frame = 0; frame < 20; ++frame)
		{
			RunFrame(context, graph, Small, *readback, false);
		}
		const rhi::RenderGraphResidency residency = graph.GetResidency();
		// The set assigned to the current declaration, and the sets of the previous executions still counted in flight.
		CHECK(residency.m_assignedTextures == 2);
		CHECK(residency.m_assignedTextures + residency.m_retiredTextures + residency.m_pooledTextures <= 2 * (FramesInFlight + 2));
		const cct::UInt64 created = graph.GetStatistics().m_texturesCreated;
		for (int frame = 0; frame < 20; ++frame)
		{
			RunFrame(context, graph, Small, *readback, false);
		}
		CHECK(graph.GetStatistics().m_texturesCreated == created);
	}
	CHECK(context.GetErrorCount() == 0);
}

TEST_CASE("RenderGraph: a size or format change creates new resources and releases the old ones once idle", "[graphics][gpu][vulkan][rendergraph]")
{
	const cct::tests::GraphicsTestDevice context(rhi::Instance::Backend::Vulkan);
	if (context.GetDevice() == nullptr)
	{
		SKIP("No Vulkan device");
	}
	{
		rhi::RenderGraph graph(*context.GetDevice(), 1);
		graph.SetIdleExecutionLimit(2);
		const std::unique_ptr<rhi::Buffer> smallReadback = MakeReadback(*context.GetDevice(), Small);
		const std::unique_ptr<rhi::Buffer> largeReadback = MakeReadback(*context.GetDevice(), Large);
		RunFrame(context, graph, Small, *smallReadback, true);
		const rhi::RenderGraphStatistics small = graph.GetStatistics();

		RunFrame(context, graph, Large, *largeReadback, true);
		CHECK(graph.GetStatistics().m_texturesCreated == small.m_texturesCreated + 2);
		CHECK(graph.GetStatistics().m_frameBuffersCreated == small.m_frameBuffersCreated + 2);
		CHECK(graph.GetStatistics().m_renderPassesCreated == small.m_renderPassesCreated);

		for (int frame = 0; frame < 4; ++frame)
		{
			RunFrame(context, graph, Large, *largeReadback, true);
		}
		rhi::RenderGraphResidency residency = graph.GetResidency();
		CHECK(residency.m_pooledTextures == 0);
		CHECK(residency.m_cachedFrameBuffers == 2);

		RunFrame(context, graph, Large, *largeReadback, true, cct::gfx::PixelFormat::RGBA8uNorm);
		// Work now has the attachment description of Colour: its render pass is reused, the RGBA16F one becomes idle.
		CHECK(graph.GetStatistics().m_renderPassesCreated == small.m_renderPassesCreated);
		for (int frame = 0; frame < 4; ++frame)
		{
			RunFrame(context, graph, Large, *largeReadback, true, cct::gfx::PixelFormat::RGBA8uNorm);
		}
		residency = graph.GetResidency();
		CHECK(residency.m_cachedRenderPasses == 1);
		CHECK(residency.m_retiredRenderPasses == 0);
	}
	CHECK(context.GetErrorCount() == 0);
}

TEST_CASE("RenderGraph: alternating between two declarations reuses both sets without creating anything", "[graphics][gpu][vulkan][rendergraph]")
{
	const cct::tests::GraphicsTestDevice context(rhi::Instance::Backend::Vulkan);
	if (context.GetDevice() == nullptr)
	{
		SKIP("No Vulkan device");
	}
	{
		rhi::RenderGraph graph(*context.GetDevice(), 1);
		const std::unique_ptr<rhi::Buffer> smallReadback = MakeReadback(*context.GetDevice(), Small);
		const std::unique_ptr<rhi::Buffer> largeReadback = MakeReadback(*context.GetDevice(), Large);
		RunFrame(context, graph, Small, *smallReadback, true);
		RunFrame(context, graph, Large, *largeReadback, true);
		const rhi::RenderGraphStatistics warm = graph.GetStatistics();
		for (int frame = 0; frame < 10; ++frame)
		{
			RunFrame(context, graph, Small, *smallReadback, true);
			RunFrame(context, graph, Large, *largeReadback, true);
		}
		CHECK(graph.GetStatistics().m_texturesCreated == warm.m_texturesCreated);
		CHECK(graph.GetStatistics().m_frameBuffersCreated == warm.m_frameBuffersCreated);
	}
	CHECK(context.GetErrorCount() == 0);
}

TEST_CASE("RenderGraph: a transient beyond the device limits fails Execute, records nothing, and the next frame recovers", "[graphics][gpu][vulkan][rendergraph]")
{
	const cct::tests::GraphicsTestDevice context(rhi::Instance::Backend::Vulkan);
	if (context.GetDevice() == nullptr)
	{
		SKIP("No Vulkan device");
	}
	{
		rhi::Device& device = *context.GetDevice();
		const rhi::DeviceLimits limits = device.GetLimits();
		REQUIRE(limits.m_maxTextureDimension2D >= 4096);
		rhi::RenderGraph graph(device, 1);
		const std::unique_ptr<rhi::Buffer> readback = MakeReadback(device, Small);

		for (const Extent invalid : {Extent{.m_width = limits.m_maxTextureDimension2D + 1, .m_height = 1}, Extent{.m_width = 0, .m_height = 8}})
		{
			graph.Reset();
			graph.Clear();
			const rhi::RGTextureHandle texture = graph.CreateTexture({.width = invalid.m_width, .height = invalid.m_height, .format = cct::gfx::PixelFormat::RGBA8uNorm, .isDepth = false, .name = "Invalid"});
			bool executed = false;
			graph.AddGraphicsPass("Clear", [texture](rhi::RenderGraphBuilder& builder)
								  {
									  builder.Write(texture);
									  builder.SetSideEffect(); }, [&executed](rhi::RenderGraphContext&)
								  { executed = true; });
			cct::Result<void, rhi::RenderGraphError> result = cct::Result<void, rhi::RenderGraphError>{};
			context.SubmitAndWait([&](rhi::CommandBuffer& commands)
								  { result = graph.Execute(commands, 1, 1); });
			REQUIRE(result.IsError());
			CHECK(result.GetError() == rhi::RenderGraphError::ResourceLimitExceeded);
			CHECK_FALSE(executed);
			CHECK(graph.GetLastExecution() == 0);
		}

		RunFrame(context, graph, Small, *readback, true);
		CHECK(graph.GetLastExecution() == 1);
	}
	// The refusals are logged as errors; nothing else is.
	CHECK(context.GetErrorCount() == 2);
}

TEST_CASE("RenderGraph: reused transients are cleared and read back correctly frame after frame", "[graphics][gpu][vulkan][rendergraph]")
{
	const cct::tests::GraphicsTestDevice context(rhi::Instance::Backend::Vulkan);
	if (context.GetDevice() == nullptr)
	{
		SKIP("No Vulkan device");
	}
	{
		rhi::RenderGraph graph(*context.GetDevice(), 1);
		const std::unique_ptr<rhi::Buffer> readback = MakeReadback(*context.GetDevice(), Small);
		constexpr std::array<float, 3> Reds{0.25F, 0.5F, 1.0F};
		for (const float red : Reds)
		{
			graph.SetClearColor(cct::Vector3f(red, 0.F, 0.F));
			RunFrame(context, graph, Small, *readback, true);
			cct::Byte* mapped = nullptr;
			REQUIRE(readback->Map(&mapped));
			const auto expected = static_cast<int>(std::lround(red * 255.F));
			CHECK(std::abs(static_cast<int>(static_cast<unsigned char>(mapped[0])) - expected) <= 1);
			CHECK(static_cast<unsigned char>(mapped[1]) == 0);
			readback->UnMap();
		}
	}
	CHECK(context.GetErrorCount() == 0);
}

TEST_CASE("RenderGraph: graphics passes merged into one render pass create it once", "[graphics][gpu][vulkan][rendergraph]")
{
	const cct::tests::GraphicsTestDevice context(rhi::Instance::Backend::Vulkan);
	if (context.GetDevice() == nullptr)
	{
		SKIP("No Vulkan device");
	}
	{
		rhi::RenderGraph graph(*context.GetDevice(), 1);
		graph.SetIdleExecutionLimit(2);
		const std::unique_ptr<rhi::Buffer> readback = MakeReadback(*context.GetDevice(), Small);
		for (int frame = 0; frame < 12; ++frame)
		{
			graph.Reset();
			graph.Clear();
			const rhi::RGTextureHandle colour = graph.CreateTexture({.width = Small.m_width, .height = Small.m_height, .format = cct::gfx::PixelFormat::RGBA8uNorm, .isDepth = false, .name = "Colour"});
			graph.AddGraphicsPass("First", [colour](rhi::RenderGraphBuilder& builder)
								  { builder.Write(colour); }, [](rhi::RenderGraphContext&) {});
			graph.AddGraphicsPass("Second", [colour](rhi::RenderGraphBuilder& builder)
								  { builder.Write(colour); }, [](rhi::RenderGraphContext&) {});
			graph.AddTransferPass("Readback", [colour](rhi::RenderGraphBuilder& builder)
								  {
									  builder.Read(colour);
									  builder.SetSideEffect(); }, [colour, &readback](rhi::RenderGraphContext& renderContext)
								  { renderContext.GetCommandBuffer().Copy(renderContext.GetTexture(colour), *readback); });
			bool executed = false;
			context.SubmitAndWait([&](rhi::CommandBuffer& commands)
								  { executed = graph.Execute(commands, Small.m_width, Small.m_height).IsOk(); });
			REQUIRE(executed);
			graph.MarkExecutionsComplete(graph.GetLastExecution());
		}
		CHECK(graph.GetStatistics().m_renderPassesCreated == 1);
		CHECK(graph.GetStatistics().m_frameBuffersCreated == 1);
		CHECK(graph.GetResidency().m_cachedRenderPasses == 1);
	}
	CHECK(context.GetErrorCount() == 0);
}

TEST_CASE("RenderGraph: cached framebuffers do not keep imported attachments alive", "[graphics][gpu][vulkan][rendergraph]")
{
	const cct::tests::GraphicsTestDevice context(rhi::Instance::Backend::Vulkan);
	if (context.GetDevice() == nullptr)
	{
		SKIP("No Vulkan device");
	}
	{
		rhi::Device& device = *context.GetDevice();
		rhi::RenderGraph graph(device, 1);
		const auto runFrame = [&](const std::shared_ptr<rhi::Texture>& target)
		{
			graph.Reset();
			graph.Clear();
			const rhi::RGTextureHandle imported = graph.ImportTexture("Imported", target, rhi::ImageLayout::Undefined, cct::gfx::PixelFormat::RGBA8uNorm, Small.m_width, Small.m_height);
			graph.AddGraphicsPass("Clear", [imported](rhi::RenderGraphBuilder& builder)
								  {
									  builder.Write(imported);
									  builder.SetSideEffect(); }, [](rhi::RenderGraphContext&) {});
			bool executed = false;
			context.SubmitAndWait([&](rhi::CommandBuffer& commands)
								  { executed = graph.Execute(commands, Small.m_width, Small.m_height).IsOk(); });
			REQUIRE(executed);
			graph.MarkExecutionsComplete(graph.GetLastExecution());
		};

		std::shared_ptr<rhi::Texture> first = device.CreateTexture(cct::gfx::PixelFormat::RGBA8uNorm, static_cast<cct::Int32>(Small.m_width), static_cast<cct::Int32>(Small.m_height));
		runFrame(first);
		runFrame(first);
		CHECK(graph.GetStatistics().m_frameBuffersCreated == 1);
		graph.Reset();
		graph.Clear();
		CHECK(first.use_count() == 1);

		first.reset();
		const std::shared_ptr<rhi::Texture> second = device.CreateTexture(cct::gfx::PixelFormat::RGBA8uNorm, static_cast<cct::Int32>(Small.m_width), static_cast<cct::Int32>(Small.m_height));
		runFrame(second);
		CHECK(graph.GetStatistics().m_frameBuffersCreated == 2);
		runFrame(second);
		CHECK(graph.GetResidency().m_cachedFrameBuffers == 1);
	}
	CHECK(context.GetErrorCount() == 0);
}
// NOLINTEND(cppcoreguidelines-avoid-do-while)
