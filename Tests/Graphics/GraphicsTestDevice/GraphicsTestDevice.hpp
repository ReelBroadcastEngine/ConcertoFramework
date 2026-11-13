#ifndef CONCERTO_TESTS_GRAPHICS_GRAPHICSTESTDEVICE_HPP
#define CONCERTO_TESTS_GRAPHICS_GRAPHICSTESTDEVICE_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include <Concerto/Core/Logger/Logger.hpp>
#include <Concerto/Graphics/RHI/Instance/Instance.hpp>

namespace cct::gfx::rhi
{
	class CommandBuffer;
	class CommandPool;
	class Device;
} // namespace cct::gfx::rhi

namespace cct::tests
{
	// A headless device of one backend, with Vulkan standard and synchronization validation. Every message the RHI
	// logs at error level or above while it lives is counted, validation errors included.
	class GraphicsTestDevice
	{
	public:
		explicit GraphicsTestDevice(gfx::rhi::Instance::Backend backend);
		~GraphicsTestDevice();

		GraphicsTestDevice(const GraphicsTestDevice&) = delete;
		GraphicsTestDevice(GraphicsTestDevice&&) = delete;
		GraphicsTestDevice& operator=(const GraphicsTestDevice&) = delete;
		GraphicsTestDevice& operator=(GraphicsTestDevice&&) = delete;

		// nullptr when the backend has no device on this machine.
		[[nodiscard]] gfx::rhi::Device* GetDevice() const;
		[[nodiscard]] std::size_t GetErrorCount() const;
		[[nodiscard]] std::string GetFirstError() const;

		// Records into a new command buffer of the graphics queue, submits it and waits for its fence.
		void SubmitAndWait(const std::function<void(gfx::rhi::CommandBuffer&)>& record) const;

	private:
		Logger m_logger;
		mutable std::mutex m_mutex;
		std::size_t m_errorCount = 0;
		std::string m_firstError;
		std::unique_ptr<gfx::rhi::Instance> m_instance;
		std::unique_ptr<gfx::rhi::Device> m_device;
		std::unique_ptr<gfx::rhi::CommandPool> m_commandPool;
	};
} // namespace cct::tests

#endif // CONCERTO_TESTS_GRAPHICS_GRAPHICSTESTDEVICE_HPP
