#include "GraphicsTestDevice/GraphicsTestDevice.hpp"

#include <exception>
#include <stdexcept>
#include <string_view>

#include <Concerto/Graphics/RHI/CommandBuffer.hpp>
#include <Concerto/Graphics/RHI/CommandPool.hpp>
#include <Concerto/Graphics/RHI/Device.hpp>
#include <Concerto/Graphics/RHI/Fence.hpp>
#include <Concerto/Graphics/RHI/Queue.hpp>

namespace cct::tests
{
	GraphicsTestDevice::GraphicsTestDevice(gfx::rhi::Instance::Backend backend)
	{
		m_logger.AddListener([this](const char* /*category*/, const char* /*channel*/, LogLevel level, std::string_view message)
							 {
								 if (level < LogLevel::Error) {
									 return;
}
								 const std::scoped_lock lock(m_mutex);
								 if (m_errorCount == 0) {
									 m_firstError = message;
}
								 ++m_errorCount; });
		gfx::rhi::Instance::SetLogger(m_logger);
		const EnumFlags<gfx::rhi::ValidationFlags> validation = backend == gfx::rhi::Instance::Backend::Vulkan
																	? gfx::rhi::ValidationFlags::Standard | gfx::rhi::ValidationFlags::SyncValidation
																	: EnumFlags<gfx::rhi::ValidationFlags>{gfx::rhi::ValidationFlags::None};
		try
		{
			m_instance = std::make_unique<gfx::rhi::Instance>(backend, validation);
			m_device = m_instance->CreateDevice(0);
		}
		catch (const std::exception&)
		{
			m_device.reset();
			m_instance.reset();
			return;
		}
		if (m_device != nullptr)
		{
			m_commandPool = m_device->CreateCommandPool(gfx::rhi::QueueFamily::Graphics, gfx::rhi::CommandBufferUsage::Primary);
		}
	}

	GraphicsTestDevice::~GraphicsTestDevice()
	{
		if (m_device != nullptr)
		{
			m_device->WaitIdle();
		}
	}

	gfx::rhi::Device* GraphicsTestDevice::GetDevice() const
	{
		return m_device.get();
	}

	std::size_t GraphicsTestDevice::GetErrorCount() const
	{
		const std::scoped_lock lock(m_mutex);
		return m_errorCount;
	}

	std::string GraphicsTestDevice::GetFirstError() const
	{
		const std::scoped_lock lock(m_mutex);
		return m_firstError;
	}

	void GraphicsTestDevice::SubmitAndWait(const std::function<void(gfx::rhi::CommandBuffer&)>& record) const
	{
		const std::unique_ptr<gfx::rhi::CommandBuffer> commands = m_commandPool->AllocateCommandBuffer();
		const std::unique_ptr<gfx::rhi::Fence> fence = m_device->CreateFence();
		commands->Begin();
		record(*commands);
		commands->End();
		Result<void, gfx::rhi::QueueError> completed = m_device->GetQueue(gfx::rhi::QueueFamily::Graphics).Submit(*commands, fence.get());
		if (completed.IsOk())
		{
			completed = fence->Wait();
		}
		if (completed.IsError())
		{
			if (completed.GetError() != gfx::rhi::QueueError::NotSubmitted)
			{
				m_device->WaitIdle();
			}
			throw std::runtime_error("GraphicsTestDevice: the submission did not complete");
		}
	}
} // namespace cct::tests
