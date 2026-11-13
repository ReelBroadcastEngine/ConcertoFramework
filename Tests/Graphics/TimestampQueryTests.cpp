#include <array>
#include <memory>
#include <span>

#include <Concerto/Graphics/RHI/CommandBuffer.hpp>
#include <Concerto/Graphics/RHI/Device.hpp>
#include <Concerto/Graphics/RHI/QueryPool.hpp>

#include "GraphicsTestDevice/GraphicsTestDevice.hpp"
#include <catch2/catch_test_macros.hpp>

// NOLINTBEGIN(cppcoreguidelines-avoid-do-while): Catch2's assertion macros expand to do { } while (false).
namespace
{
	namespace rhi = cct::gfx::rhi;

	void CheckTimestamps(rhi::Instance::Backend backend)
	{
		const cct::tests::GraphicsTestDevice context(backend);
		if (context.GetDevice() == nullptr)
			SKIP("No device for this backend");
		rhi::Device& device = *context.GetDevice();
		CHECK(device.CreateQueryPool(0) == nullptr);
		const std::unique_ptr<rhi::QueryPool> pool = device.CreateQueryPool(4);
		if (pool == nullptr)
			SKIP("The graphics queue has no timestamp support");
		CHECK(pool->GetCapacity() == 4);
		CHECK(pool->GetTimestampPeriod() > 0.0);

		context.SubmitAndWait(
			[&](rhi::CommandBuffer& commands)
			{
				commands.ResetQueries(*pool, 0, 4);
				commands.WriteTimestamp(*pool, 0);
				commands.WriteTimestamp(*pool, 1);
			});
		std::array<cct::UInt64, 2> ticks{};
		REQUIRE(pool->ReadTimestamps(0, ticks));
		CHECK(ticks.back() >= ticks.front());

		// Queries 2 and 3 were reset and never written; ranges past the capacity are refused.
		CHECK_FALSE(pool->ReadTimestamps(2, ticks));
		CHECK_FALSE(pool->ReadTimestamps(1, ticks));
		CHECK_FALSE(pool->ReadTimestamps(3, ticks));
		CHECK_FALSE(pool->ReadTimestamps(5, std::span<cct::UInt64>{}));
		CHECK(pool->ReadTimestamps(4, std::span<cct::UInt64>{}));

		context.SubmitAndWait([&](rhi::CommandBuffer& commands)
							  { commands.ResetQueries(*pool, 0, 2); });
		CHECK_FALSE(pool->ReadTimestamps(0, ticks));

		INFO("first error: " << context.GetFirstError());
		CHECK(context.GetErrorCount() == 0);
	}
} // namespace

TEST_CASE("Vulkan: timestamp queries are available once written and their submission completed", "[graphics][gpu][vulkan]")
{
	CheckTimestamps(rhi::Instance::Backend::Vulkan);
}

#ifdef CCT_PLATFORM_WINDOWS
TEST_CASE("Direct3D 12: timestamp queries are available once written and their submission completed", "[graphics][gpu][dx12]")
{
	CheckTimestamps(rhi::Instance::Backend::DirectX12);
}
#endif
// NOLINTEND(cppcoreguidelines-avoid-do-while)
