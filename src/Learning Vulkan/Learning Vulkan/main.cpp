#include <GL/glew.h>
#define GLFW_INCLUDE_VULKAN
#define GLFW_WINDOW_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <windows.h>
#include <vulkan/vulkan_win32.h>

#include <iostream>
#include <vector>
#include <optional>
#include <fstream>
#include <stdexcept>
#include <set>

#define DEBUG

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT*
	pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if (func != nullptr)
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT debugMessenger)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
		return func(instance, debugMessenger, pAllocator);
}

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

class TriangleApplication
{
public:
	TriangleApplication(const float width, const float height)
		: physicalDevice(VK_NULL_HANDLE)
		, swapChainKHR(VK_NULL_HANDLE)
		, width(width)
		, height(height)
	{

	}

	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanUp();
	}

private:
	void initWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		window = glfwCreateWindow(800, 800, "Triangle", 0, 0);
	}
	
	void initVulkan()
	{
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();

		createSemaphore();
	
		createSwapChain();
		createCommandPool();
		createCommandBuffers();
		createSynchornizationPrimitives();
		createRenderPass();
		createPipelineCache();
		createFrameBuffer();
		
		initScene();
		createUniformBuffer();
		setupDescriptorSetLayout();
		createPipeline();
		createDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			uint32_t currentBuffer;
			if (vkAcquireNextImageKHR(device, swapChainKHR, UINT64_MAX, semaphore, (VkFence)nullptr, &currentBuffer) != VK_SUCCESS)
				throw std::runtime_error("Failed to get next image KHR!");

			std::vector<VkSemaphore> semaphores = { semaphore };
			std::vector<VkSemaphore> renderSemaphores = { renderSemaphore };
			std::vector<VkPipelineStageFlags> stageFlags = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

			VkSubmitInfo submitInfo = {
				VK_STRUCTURE_TYPE_SUBMIT_INFO,
				nullptr,
				1,
				semaphores.data(),
				stageFlags.data(),
				1,
				&commandBuffers[currentBuffer],
				1,
				renderSemaphores.data()
			};

			vkQueueSubmit(graphicQueues, 1, &submitInfo, waitFences[currentBuffer]);

			VkPresentInfoKHR presentInfoKHR = {
				VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				nullptr,
				1,
				renderSemaphores.data(),
				1,
				&swapChainKHR,
				&currentBuffer,
				nullptr
			};

			if (vkQueuePresentKHR(graphicQueues, &presentInfoKHR) != VK_SUCCESS)
				throw std::runtime_error("Failed to present!");

			vkWaitForFences(device, 1, &waitFences[currentBuffer], VK_TRUE, UINT64_MAX);
			vkResetFences(device, 1, &waitFences[currentBuffer]);

			glfwPollEvents();
		}
	}

	void cleanUp()
	{
		vkDestroyInstance(instance, nullptr);
		glfwDestroyWindow(window);
		glfwTerminate();
	}

private:
	void createInstance()
	{
#ifdef DEBUG
		if (!checkValidationLayersSupport())
			std::cerr << "Validation layers requested, but not available!" << std::endl;
#endif // DEBUG

		VkApplicationInfo appInfo;
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pNext = nullptr;
		appInfo.pApplicationName = "Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.flags = 0;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = getRequireExtensions();
		createInfo.enabledExtensionCount = extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
#ifdef DEBUG
		createInfo.enabledLayerCount = validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();

		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
#endif // DEBUG

		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create instance!");
		}

		{
			uint32_t extensionCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
			std::vector<VkExtensionProperties> extensions(extensionCount);
			vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

			for (const auto& extension : extensions)
				std::cout << extension.extensionName << std::endl;
		}
	}

	void setupDebugMessenger()
	{
#ifdef DEBUG
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to set up debug messenger!");
		}
#endif // DEBUG
	}

	void pickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		if (deviceCount == 0)
			throw std::runtime_error("Failed to find GPUs with Vulkan support!");

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (isDeviceSuitable(device))
			{
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE)
			throw std::runtime_error("Failed to find a suitable GPU!");
	}

	void createSurface()
	{
		if (glfwCreateWindowSurface(instance, window, nullptr, &surfaceKHR) != VK_SUCCESS)
			throw std::runtime_error("Failed to create window surface!");
	}

	void createLogicalDevice()
	{
		indices = findQueueFamilys(physicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos; 
		std::set<uint32_t> uniqueQueueFamilies = {
			indices.graphicsFamily.value(), indices.presentFamily.value()
		};

		float queuePripority = 1.0f;
		for (auto queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo createInfo = {
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				nullptr,
				0,
				queueFamily,
				1,
				&queuePripority
			};
			queueCreateInfos.push_back(createInfo);
		}

		VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
		VkDeviceCreateInfo deviceCreateInfo = {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			nullptr,
			0,
			queueCreateInfos.size(),
			queueCreateInfos.data(),
			0,
			nullptr,
			deviceExtensions.size(),
			deviceExtensions.data(),
			&physicalDeviceFeatures
		};

#ifdef DEBUG
		deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
#else
		deviceCreateInfo.enabledLayerCount = 0;
#endif // DEBUG

		if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create device!");
		}

		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicQueues);
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueues);
	}

	void createSemaphore()
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo = {
			VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			nullptr,
			0
		};

		if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore) != VK_SUCCESS)
			throw std::runtime_error("Failed to create semaphore!");

		if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderSemaphore) != VK_SUCCESS)
			throw std::runtime_error("Failed to create semaphore!");
	}

	void createSwapChain()
	{
		VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
		VkColorSpaceKHR	colorSpaceKHR;
		uint32_t formatCount = 0;
		std::vector<VkSurfaceFormatKHR> surfaceFormatKHRs;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surfaceKHR, &formatCount, nullptr);
		surfaceFormatKHRs.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surfaceKHR, &formatCount, surfaceFormatKHRs.data());
		bool mark = false;
		if (formatCount == 1 && surfaceFormatKHRs[0].format == VK_FORMAT_UNDEFINED)
		{
			colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
			colorSpaceKHR = surfaceFormatKHRs[0].colorSpace;
		}
		else {
			for (const auto& surfaceFormatKHR : surfaceFormatKHRs)
			{
				if (surfaceFormatKHR.format == VK_FORMAT_B8G8R8A8_UNORM)
				{
					colorSpaceKHR = surfaceFormatKHR.colorSpace;
					mark = true;
					break;
				}
			}

			if (!mark)
			{
				colorFormat = surfaceFormatKHRs[0].format;
				colorSpaceKHR = surfaceFormatKHRs[0].colorSpace;
			}
		}

		VkSurfaceCapabilitiesKHR surfaceCapabilitesKHR;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surfaceKHR, &surfaceCapabilitesKHR);
		VkExtent2D extent = surfaceCapabilitesKHR.currentExtent;
		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surfaceKHR, &presentModeCount, nullptr);
		std::vector<VkPresentModeKHR> presentModeKHRs(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surfaceKHR, &presentModeCount, presentModeKHRs.data());
		VkPresentModeKHR presentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
		for (int i = 0; i < presentModeCount; ++i)
		{
			if (presentModeKHRs[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				presentModeKHR = presentModeKHRs[i];
				break;
			}
			if (presentModeKHR != VK_PRESENT_MODE_MAILBOX_KHR &&
				presentModeKHRs[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
				presentModeKHR = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}

		uint32_t desiredNumberOfSwapchainImages = surfaceCapabilitesKHR.minImageCount + 1;
		if (surfaceCapabilitesKHR.maxImageCount > 0 && desiredNumberOfSwapchainImages > surfaceCapabilitesKHR.maxImageCount)
			desiredNumberOfSwapchainImages = surfaceCapabilitesKHR.maxImageCount;

		VkSurfaceTransformFlagsKHR presentTransform;
		if (surfaceCapabilitesKHR.currentTransform & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
			presentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		else
			presentTransform = surfaceCapabilitesKHR.currentTransform;

		VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
				VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
				VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
				VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		};
		for (auto& compositeAlphaFlag : compositeAlphaFlags) {
			if (surfaceCapabilitesKHR.supportedCompositeAlpha & compositeAlphaFlag) {
				compositeAlpha = compositeAlphaFlag;
				break;
			};
		}

		VkSwapchainKHR oldSwapChainKHR = swapChainKHR;
		VkSwapchainCreateInfoKHR swapChainCreateInfo = {
			VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			nullptr,
			0,
			surfaceKHR,
			desiredNumberOfSwapchainImages,
			colorFormat,
			colorSpaceKHR,
			extent,
			1,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0,
			nullptr,
			(VkSurfaceTransformFlagBitsKHR)presentTransform,
			compositeAlpha,
			presentModeKHR,
			VK_TRUE,
			VK_NULL_HANDLE
		};
		if (vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapChainKHR) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(device, swapChainKHR, &imageCount, nullptr);
		std::vector<VkImage> images(imageCount);
		vkGetSwapchainImagesKHR(device, swapChainKHR, &imageCount, images.data());

		imageViews.resize(imageCount);
		for (int i = 0; i < imageCount; ++i)
		{
			VkImageViewCreateInfo imageViewCreateInfo = {
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				nullptr,
				0,
				images[i],
				VK_IMAGE_VIEW_TYPE_2D,
				VK_FORMAT_B8G8R8A8_UNORM,
				{VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
				{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
			};
			if (vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageViews[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create image view!");
			}
		}
	}

	void createRenderPass()
	{
		VkAttachmentDescription colorAttachmentDescription = {
			0,
			VK_FORMAT_B8G8R8A8_UNORM,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		};

		VkAttachmentReference colorAttachmentReference = {
			0,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		VkSubpassDescription subpassDescription = {
			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			1,
			&colorAttachmentReference,
			nullptr,
			nullptr,
			0,
			nullptr
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			nullptr,
			0,
			1,
			&colorAttachmentDescription,
			1,
			&subpassDescription,
			0,
			nullptr
		};

		if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass!");
		}
	}

	void createPipelineCache()
	{
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
			nullptr,
			0,
			0,
			nullptr
		};
		if (vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache) != VK_SUCCESS)
			throw std::runtime_error("Failed to create pipeline cache!");
	}

	void createPipeline()
	{
		std::ifstream ifs("./triangle.vert.spv", std::ios::binary);
		ifs.seekg(0, ifs.end);
		uint32_t codeSize = ifs.tellg();
		ifs.seekg(0, ifs.beg);
		char* vertexCodeBuffer = (char*)malloc(codeSize);
		ifs.read(vertexCodeBuffer, codeSize);
		VkShaderModule vertexShaderModule;
		VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {
			VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			nullptr,
			0,
			codeSize,
			(uint32_t*)vertexCodeBuffer
		};
		if (vkCreateShaderModule(device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule) != VK_SUCCESS)
			throw std::runtime_error("Failed to create vertex shader module!");
		
		ifs.close();
		ifs.open("./triangle.frag.spv", std::ios::binary);
		ifs.seekg(0, ifs.end);
		codeSize = ifs.tellg();
		ifs.seekg(0, ifs.beg);
		char* fragCodeBuffer = (char*)malloc(codeSize);
		ifs.read(fragCodeBuffer, codeSize);
		VkShaderModule fragShaderModule;
		VkShaderModuleCreateInfo fragShaderModuleCreateInfo = {
			VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			nullptr,
			0,
			codeSize,
			(uint32_t*)fragCodeBuffer
		};
		if (vkCreateShaderModule(device, &fragShaderModuleCreateInfo, nullptr, &fragShaderModule) != VK_SUCCESS)
			throw std::runtime_error("Failed to create frag shader module!");

		free(vertexCodeBuffer);
		free(fragCodeBuffer);

		VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			vertexShaderModule,
			"main",
			nullptr
		};
		VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			fragShaderModule,
			"main",
			nullptr
		};
		VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfos[] = {
			vertexShaderStageCreateInfo, fragShaderStageCreateInfo
		};
		
		VkVertexInputBindingDescription vertexInputBindingDescription = {
			0,
			5 * sizeof(float),
			VK_VERTEX_INPUT_RATE_VERTEX
		};
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions = {
			{0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
			{1, 0, VK_FORMAT_R32G32B32_SFLOAT, 2 * sizeof(float)}
		};
		VkPipelineVertexInputStateCreateInfo pipelineVertexInputStageCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			nullptr,
			0,
			1,
			&vertexInputBindingDescription,
			2,
			vertexInputAttributeDescriptions.data()
		};

		VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			false
		};

		VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			nullptr,
			0,
			1,
			nullptr,
			1,
			nullptr
		};

		VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_FALSE,
			VK_TRUE,
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_BACK_BIT,
			VK_FRONT_FACE_CLOCKWISE,
			VK_TRUE,
			0.0,
			0.0,
			0.0,
			1
		};

		VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_SAMPLE_COUNT_1_BIT,
			VK_FALSE,
			1.0f,
			nullptr,
			VK_TRUE,
			VK_TRUE
		};

		VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_TRUE,
			VK_TRUE,
			VK_COMPARE_OP_LESS_OR_EQUAL,
			VK_FALSE,
			VK_FALSE,
			{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0},
			{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0},
			0.0f,
			0.0f
		};

		VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {
			VK_FALSE,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_OP_ADD,
			0xf
		};
		VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_FALSE,
			VK_LOGIC_OP_CLEAR,
			1,
			&pipelineColorBlendAttachmentState,
			0
		};

		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};
		VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			nullptr,
			0,
			3,
			dynamicStates.data()
		};

		VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			nullptr,
			0,
			2,
			pipelineShaderStageCreateInfos,
			&pipelineVertexInputStageCreateInfo,
			&pipelineInputAssemblyStateCreateInfo,
			nullptr,
			&pipelineViewportStateCreateInfo,
			&pipelineRasterizationStateCreateInfo,
			&pipelineMultisampleStateCreateInfo,
			&pipelineDepthStencilStateCreateInfo,
			&pipelineColorBlendStateCreateInfo,
			&pipelineDynamicStateCreateInfo,
			pipelineLayout,
			renderPass,
			0,
			VK_NULL_HANDLE,
			0
		};

		if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS)
			throw std::runtime_error("Failed to create graphics pipelines!");
	}

	void createCommandPool()
	{
		VkCommandPoolCreateInfo commandPoolCreateInfo = {
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			nullptr,
			0,
			indices.graphicsFamily.value()
		};
		if (vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create command pool!");
		}
	}

	void createFrameBuffer()
	{
		frameBuffers.resize(imageViews.size());
		for (int i = 0; i < imageViews.size(); ++i)
		{
			VkFramebufferCreateInfo frameBufferCreateInfo = {
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				nullptr,
				0,
				renderPass,
				1,
				&imageViews[i],
				width,
				height,
				1
			};
			if (vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create frameBuffer!");
			}
		}
	}

	void createCommandBuffers()
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			nullptr,
			commandPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			imageCount
		};
		commandBuffers.resize(imageCount);
		if (vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate command buffer!");
		}
	}

	void createSynchornizationPrimitives()
	{
		waitFences.resize(commandBuffers.size());

		for (int i = 0; i < commandBuffers.size();++i)
		{
			VkFenceCreateInfo fenceCreateInfo = {
				VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				nullptr,
				0
			};

			if (vkCreateFence(device, &fenceCreateInfo, nullptr, &waitFences[i]) != VK_SUCCESS)
				throw std::runtime_error(std::string("Failed to create fence!").c_str());
		}
	}

	void createDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}
		};
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			nullptr,
			0,
			1,
			1,
			descriptorPoolSizes.data()
		};
		if (vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS)
			throw std::runtime_error("Failed to create descriptor pool!");
	}

	void setupDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding = {
			{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
		};

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			nullptr,
			0,
			1,
			descriptorSetLayoutBinding.data()
		};

		if (vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
			throw std::runtime_error("Failed to create descriptor set layout!");

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			nullptr,
			0,
			1,
			&descriptorSetLayout,
			0,
			nullptr
		};
		if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
			throw std::runtime_error("Failed to create pipeline layout!");
	}

	void setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			nullptr,
			descriptorPool,
			1,
			&descriptorSetLayout
		};

		if (vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet) != VK_SUCCESS)
			throw std::runtime_error("Failed to allocate descriptor set!");

		VkWriteDescriptorSet writeDescriptorSet = {
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			nullptr,
			descriptorSet,
			0,
			0,
			1,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			nullptr,
			&uniformBufferVS.descriptor,
			nullptr,
		};

		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
	}

	void initScene()
	{
		float vertices[] = {
			 0.0f, -0.5f, 1.0f, 0.0f, 0.0f,
			 0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f
		};

		struct StagingBuffer {
			VkDeviceMemory memory;
			VkBuffer buffer;
		} vertex;

		// host
		VkBufferCreateInfo hostBufferCreateInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			nullptr,
			0,
			sizeof(vertices),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			1,
			&(indices.graphicsFamily.value())
		};
		if (vkCreateBuffer(device, &hostBufferCreateInfo, nullptr, &vertex.buffer) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create buffer!");
		}
		VkMemoryRequirements memoryReq = {};
		vkGetBufferMemoryRequirements(device, vertex.buffer, &memoryReq);

		VkMemoryAllocateInfo memoryAllocateInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			nullptr,
			memoryReq.size,
			getMemoryTypeIndex(memoryReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
		};
		if (vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &vertex.memory) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate memory!");
		}
		void* data;
		if (vkMapMemory(device, vertex.memory, 0, memoryAllocateInfo.allocationSize, 0, &data) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to map memory!");
		}
		memcpy(data, vertices, sizeof(vertices));
		vkUnmapMemory(device, vertex.memory);
		vkBindBufferMemory(device, vertex.buffer, vertex.memory, 0);

		//device
		VkBufferCreateInfo deviceBufferCreateInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			nullptr,
			0,
			sizeof(vertices),
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			1,
			&(indices.graphicsFamily.value())
		};
		if (vkCreateBuffer(device, &deviceBufferCreateInfo, nullptr, &deviceVertex.buffer) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create device buffer!");
		}
		vkGetBufferMemoryRequirements(device, deviceVertex.buffer, &memoryReq);
		memoryAllocateInfo.allocationSize = memoryReq.size;
		memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memoryReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &deviceVertex.memory) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate device memory!");
		}
		vkBindBufferMemory(device, deviceVertex.buffer, deviceVertex.memory, 0);
		VkBufferCopy copyRegion = { 0, 0, sizeof(vertices) };
		VkCommandBuffer commandBuffer = getCommandBuffer(true);
		vkCmdCopyBuffer(commandBuffer, vertex.buffer, deviceVertex.buffer, 1, &copyRegion);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo commandBufferBeginInfo = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			nullptr,
			0,
			nullptr
		};

		VkClearValue clearValue[2];
		clearValue[0].color = { 0.0f, 0.0f, 0.2f, 1.0f };
		clearValue[1].depthStencil = { 1, 0 };

		VkRect2D renderArea = {
			{0,0},
			{width, height}
		};

		for (int i = 0; i < commandBuffers.size(); ++i)
		{
			vkBeginCommandBuffer(commandBuffers[i], &commandBufferBeginInfo);

			VkRenderPassBeginInfo renderPassBeginInfo = {
				VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				nullptr,
				renderPass,
				frameBuffers[i],
				renderArea,
				2,
				clearValue
			};
			vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = {
				0,
				0,
				width,
				height,
				0.0,
				1.0
			};
			vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);

			VkRect2D scissors[1] = {
				{0,0,width, height}
			};
			vkCmdSetScissor(commandBuffers[i], 0, 1, scissors);

			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
				0, 1, &descriptorSet, 0, nullptr);

			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			VkDeviceSize offset[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &deviceVertex.buffer, offset);

			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

			vkCmdEndRenderPass(commandBuffers[i]);

			vkEndCommandBuffer(commandBuffers[i]);
		}
	}

	void createUniformBuffer()
	{
		VkBufferCreateInfo bufferCreateInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			nullptr,
			0,
			sizeof(uboVS),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0,
			nullptr
		};

		vkCreateBuffer(device, &bufferCreateInfo, nullptr, &uniformBufferVS.buffer);
		VkMemoryRequirements memreq;
		vkGetBufferMemoryRequirements(device, uniformBufferVS.buffer, &memreq);

		VkMemoryAllocateInfo memoryAllocateInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			nullptr,
			memreq.size,
			getMemoryTypeIndex(memreq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
		};
		vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &uniformBufferVS.memory);
		vkBindBufferMemory(device, uniformBufferVS.buffer, uniformBufferVS.memory, 0);

		uniformBufferVS.descriptor = {
			uniformBufferVS.buffer,
			0,
			sizeof(uboVS)
		};
	}

	void updateUniformBuffer()
	{
		uboVS.porjection = glm::perspective(glm::radians(45.0f), float(width) / height, 0.01f, 25.0f);
		uboVS.view = glm::lookAt(glm::vec3(0.0, 0.0, 3.0), glm::vec3(0.0), glm::vec3(0.0, 1.0, 0.0));
		uboVS.model = glm::mat4(1.0f);

		uint8_t* data;
		vkMapMemory(device, uniformBufferVS.memory, 0, sizeof(uboVS), 0, (void**)&data);
		memcpy(data, &uboVS, sizeof(uboVS));
		vkUnmapMemory(device, uniformBufferVS.memory);
	}

private:
	bool checkValidationLayersSupport()
	{
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers)
		{
			bool layerFound = false;
			for (const auto& layerProperty : availableLayers)
			{
				if (strcmp(layerName, layerProperty.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
				return false;
		}
		return true;
	}

	std::vector<const char*> getRequireExtensions()
	{
		uint32_t glfwExtensionsCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionsCount);
#ifdef DEBUG
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif // DEBUG

		return extensions;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			throw std::runtime_error(std::string("validattion layer: ") + pCallbackData->pMessage);
	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	bool isDeviceSuitable(VkPhysicalDevice device)
	{
		return true;
	}

	QueueFamilyIndices findQueueFamilys(VkPhysicalDevice physicalDevice)
	{
		QueueFamilyIndices queueFamilyIndice;
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

		for (int i = 0; i < queueFamilyCount; ++i)
		{
			if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				queueFamilyIndice.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surfaceKHR, &presentSupport);

			if (presentSupport)
				queueFamilyIndice.presentFamily = i;

			if (queueFamilyIndice.isComplete())
				break;
		}
		return queueFamilyIndice;
	}

	uint32_t getMemoryTypeIndex(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
		for (int i = 0; i < deviceMemoryProperties.memoryTypeCount; ++i)
		{
			if (memoryTypeBits & 1)
			{
				if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
					return i;
			}
		}
	}

	VkCommandBuffer getCommandBuffer(bool begin)
	{
		VkCommandBufferAllocateInfo allocateInfo = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			nullptr,
			commandPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			1
		};
		VkCommandBuffer commandBuffer;
		if (vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer) != VK_SUCCESS)
			throw std::runtime_error("Failed to allocate command buffers!");

		if (begin)
		{
			VkCommandBufferBeginInfo commandBufferBeginInfo = {};
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
				throw std::runtime_error("Failed to begin command buffer!");
		}
		return commandBuffer;
	}

private:
	float							width;
	float							height;
	GLFWwindow*						window;
	VkInstance						instance;
	VkDebugUtilsMessengerEXT		debugMessenger;
	VkPhysicalDevice				physicalDevice;
	VkSurfaceKHR					surfaceKHR;
	VkDevice						device;
	QueueFamilyIndices				indices;
	VkQueue							graphicQueues;
	VkQueue							presentQueues;
	VkCommandPool					commandPool;
	VkSwapchainKHR					swapChainKHR;
	uint32_t						imageCount;
	std::vector<VkImageView>		imageViews;
	std::vector<VkCommandBuffer>	commandBuffers;
	std::vector<VkFramebuffer>		frameBuffers;
	VkRenderPass					renderPass;
	VkPipelineCache					pipelineCache;
	VkPipeline						pipeline;
	VkPipelineLayout				pipelineLayout;
	VkDescriptorPool				descriptorPool;
	VkDescriptorSet					descriptorSet;
	VkDescriptorSetLayout			descriptorSetLayout;
	VkSemaphore						semaphore;
	VkSemaphore						renderSemaphore;
	std::vector<VkFence>			waitFences;
	struct { VkBuffer buffer; VkDeviceMemory memory; } deviceVertex;
	struct { VkDeviceMemory memory; VkBuffer buffer; VkDescriptorBufferInfo descriptor; }uniformBufferVS;
	struct { glm::mat4 model; glm::mat4 view; glm::mat4 porjection; } uboVS;
};

int main()
{
	TriangleApplication app(800, 800);
	try
	{
		app.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return 0;
}