#include <memory>

#include <Concerto/Graphics/RHI/CommandBuffer.hpp>
#include <Concerto/Graphics/RHI/CommandPool.hpp>
#include <Concerto/Graphics/RHI/Device.hpp>
#include <Concerto/Graphics/RHI/Fence.hpp>
#include <Concerto/Graphics/RHI/Queue.hpp>

#include "GraphicsTestDevice/GraphicsTestDevice.hpp"
#include <catch2/catch_test_macros.hpp>

// NOLINTBEGIN(cppcoreguidelines-avoid-do-while): Catch2's assertion macros expand to do { } while (false).
namespace
{
	namespace rhi = cct::gfx::rhi;

	void CheckSubmissionContract(rhi::Instance::Backend backend)
	{
		const cct::tests::GraphicsTestDevice context(backend);
		if (context.GetDevice() == nullptr)
		{
			SKIP("No device for this backend");
		}
		rhi::Device& device = *context.GetDevice();
		const std::unique_ptr<rhi::CommandPool> pool = device.CreateCommandPool(rhi::QueueFamily::Graphics, rhi::CommandBufferUsage::Primary);
		const std::unique_ptr<rhi::CommandBuffer> commands = pool->AllocateCommandBuffer();
		const std::unique_ptr<rhi::Fence> fence = device.CreateFence();
		rhi::Queue& queue = device.GetQueue(rhi::QueueFamily::Graphics);

		const cct::Result<void, rhi::QueueError> waitedBeforeSubmission = fence->Wait();
		REQUIRE(waitedBeforeSubmission.IsError());
		CHECK(waitedBeforeSubmission.GetError() == rhi::QueueError::NoPendingSubmission);

		commands->Begin();
		commands->End();
		REQUIRE(queue.Submit(*commands, fence.get()).IsOk());

		const cct::Result<void, rhi::QueueError> resetBeforeWait = fence->Reset();
		REQUIRE(resetBeforeWait.IsError());
		CHECK(resetBeforeWait.GetError() == rhi::QueueError::FenceNotReset);
		const cct::Result<void, rhi::QueueError> resubmitted = queue.Submit(*commands, fence.get());
		REQUIRE(resubmitted.IsError());
		CHECK(resubmitted.GetError() == rhi::QueueError::NotSubmitted);

		CHECK(fence->Wait().IsOk());
		CHECK(fence->Wait().IsOk());
		REQUIRE(fence->Reset().IsOk());

		const cct::Result<void, rhi::QueueError> waitedAfterReset = fence->Wait();
		REQUIRE(waitedAfterReset.IsError());
		CHECK(waitedAfterReset.GetError() == rhi::QueueError::NoPendingSubmission);

		commands->Begin();
		commands->End();
		REQUIRE(queue.Submit(*commands, fence.get()).IsOk());
		CHECK(fence->Wait().IsOk());
		CHECK(fence->Reset().IsOk());

		commands->Begin();
		commands->End();
		CHECK(queue.Submit(*commands).IsOk());
		device.WaitIdle();

		INFO("first error: " << context.GetFirstError());
		CHECK(context.GetErrorCount() == 1);
	}
} // namespace

TEST_CASE("Vulkan: a fence wait succeeds only for the submission that signalled it since its last reset", "[graphics][gpu][vulkan]")
{
	CheckSubmissionContract(rhi::Instance::Backend::Vulkan);
}

#ifdef CCT_PLATFORM_WINDOWS
TEST_CASE("Direct3D 12: a fence wait succeeds only for the submission that signalled it since its last reset", "[graphics][gpu][dx12]")
{
	CheckSubmissionContract(rhi::Instance::Backend::DirectX12);
}
#endif
// NOLINTEND(cppcoreguidelines-avoid-do-while)
