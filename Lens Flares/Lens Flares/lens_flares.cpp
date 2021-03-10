#include "lens_flares.h"

#include <set>
#include <fstream>

#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/opencv.hpp"

#include "stb_image.h"

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

LensFlares::LensFlares()
{
	initWindow();
	prepare();
}

LensFlares::~LensFlares()
{

}

void LensFlares::run()
{
	mainLoop();
}

void LensFlares::initWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(800, 800, "Triangle", 0, 0);
}

void LensFlares::prepare()
{
	createInstance();
	swapchain.create(physicalDevice, device, surfaceKHR);
	createCommandPool();
	createCommandBuffers();
	createSynchronizationPrimitives();
	createRenderPass();
	createPipelineCache();
	createFrameBuffers();

	loadResources();
	createFrameBuffers();
	createDescriptorPool();
	setupDescriptorSetLayout();
	setupDescriptorSet();
	createPipeline();
	buildCommandBuffers();
}

void LensFlares::mainLoop()
{
	while (!glfwWindowShouldClose(window))
	{
		uint32_t currentBuffer;
		vkAcquireNextImageKHR(device, swapchain.swapchain, UINT64_MAX, semaphore, (VkFence)nullptr, &currentBuffer);

		std::vector<VkSemaphore> semaphores = { semaphore };
		std::vector<VkSemaphore> renderSemaphores = { renderSemaphore };
		std::vector<VkPipelineStageFlags> pipelineStageFlags = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = nullptr,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = semaphores.data(),
			.pWaitDstStageMask = pipelineStageFlags.data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffers[currentBuffer],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = renderSemaphores.data()
		};
		vkQueueSubmit(graphicQueue, 1, &submitInfo, waitFences[currentBuffer]);

		VkPresentInfoKHR presentInfo = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = nullptr,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = renderSemaphores.data(),
			.swapchainCount = 1,
			.pSwapchains = &swapchain.swapchain,
			.pImageIndices = &currentBuffer
		};
		vkQueuePresentKHR(graphicQueue, &presentInfo);

		vkWaitForFences(device, 1, &waitFences[currentBuffer], VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &waitFences[currentBuffer]);

		glfwPollEvents();
	}
}

void LensFlares::createInstance()
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

#ifdef DEBUG
	auto extensions = getRequireExtensions();
	createInfo.enabledExtensionCount = extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
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

#ifdef DEBUG
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	for (const auto& extension : extensions)
		std::cout << extension.extensionName << std::endl;
#endif // DEBUG	
}

void LensFlares::setupDebugMessenger()
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

void LensFlares::pickPhysicalDevice()
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

void LensFlares::createSurface()
{
	if (glfwCreateWindowSurface(instance, window, nullptr, &surfaceKHR) != VK_SUCCESS)
		throw std::runtime_error("Failed to create window surface!");
}

void LensFlares::createLogicalDevice()
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

	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicQueue);
	vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void LensFlares::createSynchronizationPrimitives()
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

	waitFences.resize(commandBuffers.size());

	for (int i = 0; i < commandBuffers.size(); ++i)
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

void LensFlares::createRenderPass()
{
	// bright
	{
		VkAttachmentDescription colorAttachmentDescription = {
			.flags = 0,
			.format = VK_FORMAT_B8G8R8A8_UNORM,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
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

		std::vector<VkSubpassDependency> dependencies(2);
		dependencies[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		dependencies[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = &colorAttachmentDescription,
			.subpassCount = 1,
			.pSubpasses = &subpassDescription,
			.dependencyCount = (uint32_t)dependencies.size(),
			.pDependencies = dependencies.data()
		};

		if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &frameBuffers.bright.renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass!");
		}
	}
	
	// bright_dft
	{
		VkAttachmentDescription colorAttachmentDescription = {
			.flags = 0,
			.format = VK_FORMAT_B8G8R8A8_UNORM,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		};

		std::vector<VkAttachmentReference> colorAttachmentReferences = {
			{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
			{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
		};

		VkSubpassDescription subpassDescription = {
			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			colorAttachmentReferences.size(),
			&colorAttachmentReferences[0],
			nullptr,
			nullptr,
			0,
			nullptr
		};

		std::vector<VkSubpassDependency> dependencies(2);
		dependencies[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		dependencies[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = &colorAttachmentDescription,
			.subpassCount = 1,
			.pSubpasses = &subpassDescription,
			.dependencyCount = (uint32_t)dependencies.size(),
			.pDependencies = dependencies.data()
		};

		if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &frameBuffers.bright_dft.renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass!");
		}
	}

	// blur
	{
		VkAttachmentDescription colorAttachmentDescription = {
			.flags = 0,
			.format = VK_FORMAT_B8G8R8A8_UNORM,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
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

		std::vector<VkSubpassDependency> dependencies(2);
		dependencies[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		dependencies[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = &colorAttachmentDescription,
			.subpassCount = 1,
			.pSubpasses = &subpassDescription,
			.dependencyCount = (uint32_t)dependencies.size(),
			.pDependencies = dependencies.data()
		};

		if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &frameBuffers.blur.renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass!");
		}
	}

	// blur_dft
	{
		VkAttachmentDescription colorAttachmentDescription = {
			.flags = 0,
			.format = VK_FORMAT_B8G8R8A8_UNORM,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		};

		std::vector<VkAttachmentReference> colorAttachmentReferences = {
			{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
			{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
		};

		VkSubpassDescription subpassDescription = {
			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			colorAttachmentReferences.size(),
			&colorAttachmentReferences[0],
			nullptr,
			nullptr,
			0,
			nullptr
		};

		std::vector<VkSubpassDependency> dependencies(2);
		dependencies[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		dependencies[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = &colorAttachmentDescription,
			.subpassCount = 1,
			.pSubpasses = &subpassDescription,
			.dependencyCount = (uint32_t)dependencies.size(),
			.pDependencies = dependencies.data()
		};

		if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &frameBuffers.blur_dft.renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass!");
		}
	}

	// complexMultiplication
	{
		VkAttachmentDescription colorAttachmentDescription = {
			.flags = 0,
			.format = VK_FORMAT_B8G8R8A8_UNORM,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		};

		std::vector<VkAttachmentReference> colorAttachmentReferences = {
			{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
			{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
		};

		VkSubpassDescription subpassDescription = {
			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			colorAttachmentReferences.size(),
			colorAttachmentReferences.data(),
			nullptr,
			nullptr,
			0,
			nullptr
		};

		std::vector<VkSubpassDependency> dependencies(2);
		dependencies[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		dependencies[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = &colorAttachmentDescription,
			.subpassCount = 1,
			.pSubpasses = &subpassDescription,
			.dependencyCount = (uint32_t)dependencies.size(),
			.pDependencies = dependencies.data()
		};

		if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &frameBuffers.complexMultiplication.renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass!");
		}
	}

	// idft
	{
		VkAttachmentDescription colorAttachmentDescription = {
			.flags = 0,
			.format = VK_FORMAT_B8G8R8A8_UNORM,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
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

		std::vector<VkSubpassDependency> dependencies(2);
		dependencies[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		dependencies[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = &colorAttachmentDescription,
			.subpassCount = 1,
			.pSubpasses = &subpassDescription,
			.dependencyCount = (uint32_t)dependencies.size(),
			.pDependencies = dependencies.data()
		};

		if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &frameBuffers.idft.renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass!");
		}
	}

	// blend
	{
		VkAttachmentDescription colorAttachmentDescription = {
			.flags = 0,
			.format = VK_FORMAT_B8G8R8A8_UNORM,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
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

		std::vector<VkSubpassDependency> dependencies(2);
		dependencies[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		dependencies[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = &colorAttachmentDescription,
			.subpassCount = 1,
			.pSubpasses = &subpassDescription,
			.dependencyCount = (uint32_t)dependencies.size(),
			.pDependencies = dependencies.data()
		};

		if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &frameBuffers.blend.renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass!");
		}
	}

	// Shared sampler used for all color attachments
	VkSamplerCreateInfo sampler = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = VK_FILTER_NEAREST,
		.minFilter = VK_FILTER_NEAREST,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.mipLodBias = 0.0f,
		.maxAnisotropy = 1.0f,
		.minLod = 0.0f,
		.maxLod = 1.0f,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
	};
	vkCreateSampler(device, &sampler, nullptr, &colorSampler);
}

void LensFlares::createPipelineCache()
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

void LensFlares::createPipeline()
{
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

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			VK_NULL_HANDLE,
			"main",
			nullptr
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			VK_NULL_HANDLE,
			"main",
			nullptr
		}
	};

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		nullptr,
		0,
		2,
		&shaderStages[0],
		&pipelineVertexInputStageCreateInfo,
		&pipelineInputAssemblyStateCreateInfo,
		nullptr,
		&pipelineViewportStateCreateInfo,
		&pipelineRasterizationStateCreateInfo,
		&pipelineMultisampleStateCreateInfo,
		&pipelineDepthStencilStateCreateInfo,
		&pipelineColorBlendStateCreateInfo,
		&pipelineDynamicStateCreateInfo,
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
		0,
		VK_NULL_HANDLE,
		0
	};

	// bright
	{
		VkShaderModule vertex;
		VkShaderModule fragment;
		try{
			vertex = createShaderModule("./feature_extraction.vert");
			fragment = createShaderModule("./feature_extraction.frag");
		}catch (const std::exception& e){
			throw e;
		}
		shaderStages[0].module = vertex;
		shaderStages[1].module = fragment;
		graphicsPipelineCreateInfo.renderPass = frameBuffers.bright.renderPass;
		graphicsPipelineCreateInfo.layout = pipelineLayouts.bright;
			if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipelines.bright) != VK_SUCCESS)
				throw std::runtime_error("Failed to create graphics pipelines!");
	}
	
	// blur
	{
		VkShaderModule vertex;
		VkShaderModule fragment;
		try {
			vertex = createShaderModule("./feature_extraction.vert");
			fragment = createShaderModule("./blur.frag");
		}
		catch (const std::exception& e) {
			throw e;
		}
		shaderStages[0].module = vertex;
		shaderStages[1].module = fragment;
		graphicsPipelineCreateInfo.renderPass = frameBuffers.blur.renderPass;
		graphicsPipelineCreateInfo.layout = pipelineLayouts.blur;
		if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipelines.blur) != VK_SUCCESS)
			throw std::runtime_error("Failed to create graphics pipelines!");
	}

	// complexMultiplication
	{
		VkShaderModule vertex;
		VkShaderModule fragment;
		try {
			vertex = createShaderModule("./feature_extraction.vert");
			fragment = createShaderModule("./complexMultiplication.frag");
		}
		catch (const std::exception& e) {
			throw e;
		}
		shaderStages[0].module = vertex;
		shaderStages[1].module = fragment;
		graphicsPipelineCreateInfo.renderPass = frameBuffers.complexMultiplication.renderPass;
		graphicsPipelineCreateInfo.layout = pipelineLayouts.complexMultiplication;
		if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipelines.complexMultiplication) != VK_SUCCESS)
			throw std::runtime_error("Failed to create graphics pipelines!");
	}

	// idft
	{
		VkShaderModule vertex;
		VkShaderModule fragment;
		try {
			vertex = createShaderModule("./feature_extraction.vert");
			fragment = createShaderModule("./fft.frag");
		}
		catch (const std::exception& e) {
			throw e;
		}
		shaderStages[0].module = vertex;
		shaderStages[1].module = fragment;
		graphicsPipelineCreateInfo.renderPass = frameBuffers.idft.renderPass;
		graphicsPipelineCreateInfo.layout = pipelineLayouts.idft;
		if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipelines.idft) != VK_SUCCESS)
			throw std::runtime_error("Failed to create graphics pipelines!");
	}

	// blend
	{
		VkShaderModule vertex;
		VkShaderModule fragment;
		try {
			vertex = createShaderModule("./feature_extraction.vert");
			fragment = createShaderModule("./blend.frag");
		}
		catch (const std::exception& e) {
			throw e;
		}
		shaderStages[0].module = vertex;
		shaderStages[1].module = fragment;
		graphicsPipelineCreateInfo.renderPass = frameBuffers.blend.renderPass;
		graphicsPipelineCreateInfo.layout = pipelineLayouts.blend;
		if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipelines.blend) != VK_SUCCESS)
			throw std::runtime_error("Failed to create graphics pipelines!");
	}

	// dft
	{
		VkShaderModule vertex;
		VkShaderModule fragment;
		try {
			vertex = createShaderModule("./feature_extraction.vert");
			fragment = createShaderModule("./fft.frag");
		}
		catch (const std::exception& e) {
			throw e;
		}
		shaderStages[0].module = vertex;
		shaderStages[1].module = fragment;
		graphicsPipelineCreateInfo.renderPass = frameBuffers.bright_dft.renderPass;
		graphicsPipelineCreateInfo.layout = pipelineLayouts.dft;
		std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates(2);
		blendAttachmentStates[0] = { .blendEnable = false, .colorWriteMask = 0xf };
		blendAttachmentStates[1] = { .blendEnable = false, .colorWriteMask = 0xf };
		pipelineColorBlendStateCreateInfo.attachmentCount = blendAttachmentStates.size();
		pipelineColorBlendStateCreateInfo.pAttachments = blendAttachmentStates.data();
		if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipelines.bright_dft) != VK_SUCCESS)
			throw std::runtime_error("Failed to create graphics pipelines!");

		graphicsPipelineCreateInfo.renderPass = frameBuffers.blur_dft.renderPass;
		if(vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipelines.blur_dft) != VK_SUCCESS)
			throw std::runtime_error("Failed to create graphics pipelines!");
	}
}

void LensFlares::createCommandPool()
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

void LensFlares::createFrameBuffers()
{
	frameBuffers.bright.width = width;
	frameBuffers.bright.height = height;
	frameBuffers.blur.width = width;
	frameBuffers.blur.height = height;
	frameBuffers.blend.width = width;
	frameBuffers.blend.height = height;
	frameBuffers.bright_dft.width = width;
	frameBuffers.bright_dft.height = height;
	frameBuffers.blur_dft.width = width;
	frameBuffers.blur_dft.height = height;
	frameBuffers.idft.width = width;
	frameBuffers.idft.height = height;
	frameBuffers.complexMultiplication.width = width;
	frameBuffers.complexMultiplication.height = height;

	// bright
	{
		createAttachment(&frameBuffers.bright.color, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height);
		std::vector<VkAttachmentDescription> attachmentDescriptions(1);
		attachmentDescriptions[0] = {
			.flags = 0,
			.format = frameBuffers.bright.color.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		std::vector<VkAttachmentReference> colorAttachments = {
			{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
		};

		VkSubpassDescription subpass = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = (uint32_t)colorAttachments.size(),
			.pColorAttachments = colorAttachments.data()
		};

		std::vector<VkSubpassDependency> subpassDependencys(2);
		subpassDependencys[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		subpassDependencys[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = attachmentDescriptions.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = (uint32_t)subpassDependencys.size(),
			.pDependencies = subpassDependencys.data()
		};
		VkRenderPass renderPass;
		vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);

		std::vector<VkImageView> attachments(1);
		attachments[0] = frameBuffers.bright.color.view;

		VkFramebufferCreateInfo framebufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = renderPass,
			.attachmentCount = 1,
			.pAttachments = attachments.data(),
			.width = width,
			.height = height,
			.layers = 1
		};
		vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &frameBuffers.bright.framebuffer);
	}
	
	// blur
	{
		createAttachment(&frameBuffers.blur.color, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height);
		std::vector<VkAttachmentDescription> attachmentDescriptions(1);
		attachmentDescriptions[0] = {
			.flags = 0,
			.format = frameBuffers.blur.color.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		std::vector<VkAttachmentReference> colorAttachments = {
			{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
		};

		VkSubpassDescription subpass = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = (uint32_t)colorAttachments.size(),
			.pColorAttachments = colorAttachments.data()
		};

		std::vector<VkSubpassDependency> subpassDependencys(2);
		subpassDependencys[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		subpassDependencys[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = attachmentDescriptions.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = (uint32_t)subpassDependencys.size(),
			.pDependencies = subpassDependencys.data()
		};
		VkRenderPass renderPass;
		vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);

		std::vector<VkImageView> attachments(1);
		attachments[0] = frameBuffers.blur.color.view;

		VkFramebufferCreateInfo framebufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = renderPass,
			.attachmentCount = 1,
			.pAttachments = attachments.data(),
			.width = width,
			.height = height,
			.layers = 1
		};
		vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &frameBuffers.blur.framebuffer);
	}
	
	// bright_dft
	{
		createAttachment(&frameBuffers.bright_dft.real, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height);
		createAttachment(&frameBuffers.bright_dft.imaginary, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height);

		std::vector<VkAttachmentDescription> attachmentDescriptions(2);
		attachmentDescriptions[0] = {
			.flags = 0,
			.format = frameBuffers.bright_dft.real.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};
		attachmentDescriptions[1] = {
			.flags = 0,
			.format = frameBuffers.bright_dft.imaginary.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		std::vector<VkAttachmentReference> colorAttachments = {
			{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
			{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
		};

		VkSubpassDescription subpass = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = (uint32_t)colorAttachments.size(),
			.pColorAttachments = colorAttachments.data()
		};

		std::vector<VkSubpassDependency> subpassDependencys(2);
		subpassDependencys[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		subpassDependencys[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = attachmentDescriptions.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = (uint32_t)subpassDependencys.size(),
			.pDependencies = subpassDependencys.data()
		};
		VkRenderPass renderPass;
		vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);

		std::vector<VkImageView> attachments(2);
		attachments[0] = frameBuffers.bright_dft.real.view;
		attachments[1] = frameBuffers.bright_dft.imaginary.view;

		VkFramebufferCreateInfo framebufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = renderPass,
			.attachmentCount = (uint32_t)attachments.size(),
			.pAttachments = attachments.data(),
			.width = width,
			.height = height,
			.layers = 1
		};
		vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &frameBuffers.bright_dft.framebuffer);
	}

	// blur_dft
	{
		createAttachment(&frameBuffers.blur_dft.real, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height);
		createAttachment(&frameBuffers.blur_dft.imaginary, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height);

		std::vector<VkAttachmentDescription> attachmentDescriptions(2);
		attachmentDescriptions[0] = {
			.flags = 0,
			.format = frameBuffers.blur_dft.real.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};
		attachmentDescriptions[1] = {
			.flags = 0,
			.format = frameBuffers.blur_dft.imaginary.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		std::vector<VkAttachmentReference> colorAttachments = {
			{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
			{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
		};

		VkSubpassDescription subpass = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = (uint32_t)colorAttachments.size(),
			.pColorAttachments = colorAttachments.data()
		};

		std::vector<VkSubpassDependency> subpassDependencys(2);
		subpassDependencys[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		subpassDependencys[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = attachmentDescriptions.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = (uint32_t)subpassDependencys.size(),
			.pDependencies = subpassDependencys.data()
		};
		VkRenderPass renderPass;
		vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);

		std::vector<VkImageView> attachments(2);
		attachments[0] = frameBuffers.blur_dft.real.view;
		attachments[1] = frameBuffers.blur_dft.imaginary.view;

		VkFramebufferCreateInfo framebufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = renderPass,
			.attachmentCount = (uint32_t)attachments.size(),
			.pAttachments = attachments.data(),
			.width = width,
			.height = height,
			.layers = 1
		};
		vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &frameBuffers.blur_dft.framebuffer);
	}
	
	// idft
	{	
		createAttachment(&frameBuffers.idft.color, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height);

		std::vector<VkAttachmentDescription> attachmentDescriptions(1);
		attachmentDescriptions[0] = {
			.flags = 0,
			.format = frameBuffers.idft.color.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		std::vector<VkAttachmentReference> colorAttachments = {
			{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
		};

		VkSubpassDescription subpass = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = (uint32_t)colorAttachments.size(),
			.pColorAttachments = colorAttachments.data()
		};

		std::vector<VkSubpassDependency> subpassDependencys(2);
		subpassDependencys[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		subpassDependencys[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = attachmentDescriptions.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = (uint32_t)subpassDependencys.size(),
			.pDependencies = subpassDependencys.data()
		};
		VkRenderPass renderPass;
		vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);

		std::vector<VkImageView> attachments(1);
		attachments[0] = frameBuffers.idft.color.view;

		VkFramebufferCreateInfo framebufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = renderPass,
			.attachmentCount = (uint32_t)attachments.size(),
			.pAttachments = attachments.data(),
			.width = width,
			.height = height,
			.layers = 1
		};
		vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &frameBuffers.idft.framebuffer);
	}

	// complexMultiplication
	{
		createAttachment(&frameBuffers.complexMultiplication.real, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height);
		createAttachment(&frameBuffers.complexMultiplication.imaginary, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height);

		std::vector<VkAttachmentDescription> attachmentDescriptions(2);
		attachmentDescriptions[0] = {
			.flags = 0,
			.format = frameBuffers.complexMultiplication.real.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};
		attachmentDescriptions[0] = {
			.flags = 0,
			.format = frameBuffers.complexMultiplication.imaginary.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		std::vector<VkAttachmentReference> colorAttachments = {
			{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
			{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
		};

		VkSubpassDescription subpass = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = (uint32_t)colorAttachments.size(),
			.pColorAttachments = colorAttachments.data()
		};

		std::vector<VkSubpassDependency> subpassDependencys(2);
		subpassDependencys[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		subpassDependencys[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = attachmentDescriptions.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = (uint32_t)subpassDependencys.size(),
			.pDependencies = subpassDependencys.data()
		};
		VkRenderPass renderPass;
		vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);

		std::vector<VkImageView> attachments(2);
		attachments[0] = frameBuffers.complexMultiplication.real.view;
		attachments[1] = frameBuffers.complexMultiplication.imaginary.view;

		VkFramebufferCreateInfo framebufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = renderPass,
			.attachmentCount = (uint32_t)attachments.size(),
			.pAttachments = attachments.data(),
			.width = width,
			.height = height,
			.layers = 1
		};
		vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &frameBuffers.complexMultiplication.framebuffer);
	}
	
	// blend
	{
		createAttachment(&frameBuffers.blend.color, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height);

		std::vector<VkAttachmentDescription> attachmentDescriptions(1);
		attachmentDescriptions[0] = {
			.flags = 0,
			.format = frameBuffers.blend.color.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		std::vector<VkAttachmentReference> colorAttachments = {
			{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
		};

		VkSubpassDescription subpass = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = (uint32_t)colorAttachments.size(),
			.pColorAttachments = colorAttachments.data()
		};

		std::vector<VkSubpassDependency> subpassDependencys(2);
		subpassDependencys[0] = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		subpassDependencys[1] = {
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};

		VkRenderPassCreateInfo renderPassCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = attachmentDescriptions.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = (uint32_t)subpassDependencys.size(),
			.pDependencies = subpassDependencys.data()
		};
		VkRenderPass renderPass;
		vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);

		std::vector<VkImageView> attachments(1);
		attachments[0] = frameBuffers.blend.color.view;

		VkFramebufferCreateInfo framebufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = renderPass,
			.attachmentCount = (uint32_t)attachments.size(),
			.pAttachments = attachments.data(),
			.width = width,
			.height = height,
			.layers = 1
		};
		vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &frameBuffers.blend.framebuffer);
	}
}

void LensFlares::createCommandBuffers()
{
	commandBuffers.resize(swapchain.imageCount);
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = (uint32_t)commandBuffers.size()
	};
	vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffers.data());
}

void LensFlares::createDescriptorPool()
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

void LensFlares::setupDescriptorSetLayout()
{
	// bright
	{
		VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		};

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = 1,
			.pBindings = &descriptorSetLayoutBinding
		};
		vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts.bright);
	}

	//blur
	{
		std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings(2);
		descriptorSetLayoutBindings[0] = {
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		};
		descriptorSetLayoutBindings[1] = {
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		};

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = (uint32_t)descriptorSetLayoutBindings.size(),
			.pBindings = descriptorSetLayoutBindings.data()
		};
		vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts.blur);
	}

	// dft
	{
		std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings = {
			{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			},
			{
				.binding = 2,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			}
		};

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = (uint32_t)descriptorSetLayoutBindings.size(),
			.pBindings = descriptorSetLayoutBindings.data()
		};
		vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts.dft);
	}

	// idft
	{
		std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings = {
			{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			},
			{
				.binding = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			},
			{
				.binding = 2,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			}
		};

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = (uint32_t)descriptorSetLayoutBindings.size(),
			.pBindings = descriptorSetLayoutBindings.data()
		};
		vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts.idft);
	}

	// complexMultiplication
	{
		std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings = {
			{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			},
			{
				.binding = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			},
			{
				.binding = 2,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			},
			{
				.binding = 3,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			}
		};

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = (uint32_t)descriptorSetLayoutBindings.size(),
			.pBindings = descriptorSetLayoutBindings.data()
		};
		vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts.complexMultiplication);
	}

	// blend 
	{
		std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings = {
			{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			},
			{
				.binding = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			}
		};

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = (uint32_t)descriptorSetLayoutBindings.size(),
			.pBindings = descriptorSetLayoutBindings.data()
		};
		vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts.blend);
	}
}

void LensFlares::setupDescriptorSet()
{
	// bright
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &descriptorSetLayouts.bright,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr
		};
		vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.bright);

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 0,
			.pSetLayouts = &descriptorSetLayouts.bright
		};
		vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSets.bright);

		VkDescriptorImageInfo descriptorImageInfo = {
			.sampler = textureDescriptor.sampler,
			.imageView = textureDescriptor.view,
			.imageLayout = textureDescriptor.imageLayout
		};
		VkWriteDescriptorSet writeDescriptorSet = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.bright,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descriptorImageInfo
		};
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
	}

	// blur 
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &descriptorSetLayouts.blur,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr
		};
		vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.blur);

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 0,
			.pSetLayouts = &descriptorSetLayouts.blur
		};
		vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSets.blur);

		VkDescriptorImageInfo descriptorImageInfo = {
			.sampler = colorSampler,
			.imageView = frameBuffers.bright.color.view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		std::vector<VkWriteDescriptorSet> writeDescriptorSets(2);
		writeDescriptorSets[0] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.blur,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descriptorImageInfo
		};
		writeDescriptorSets[1] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.blur,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &uniformBuffers.blur.descriptor
		};
		vkUpdateDescriptorSets(device, 2, &writeDescriptorSets[0], 0, nullptr);
	}

	// blur_dft
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &descriptorSetLayouts.dft,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr
		};
		vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.dft);

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 0,
			.pSetLayouts = &descriptorSetLayouts.dft
		};
		vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSets.blur_dft);

		VkDescriptorImageInfo descriptorImageInfo = {
			.sampler = colorSampler,
			.imageView = frameBuffers.blur.color.view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		std::vector<VkWriteDescriptorSet> writeDescriptorSets(2);
		writeDescriptorSets[0] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.blur_dft,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descriptorImageInfo
		};
		writeDescriptorSets[1] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.blur_dft,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &uniformBuffers.dft.descriptor
		};
		vkUpdateDescriptorSets(device, 2, &writeDescriptorSets[0], 0, nullptr);
	}

	// bright_dft
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &descriptorSetLayouts.dft,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr
		};
		vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.dft);

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 0,
			.pSetLayouts = &descriptorSetLayouts.dft
		};
		vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSets.bright_dft);

		VkDescriptorImageInfo descriptorImageInfo = {
			.sampler = colorSampler,
			.imageView = frameBuffers.bright.color.view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		std::vector<VkWriteDescriptorSet> writeDescriptorSets(2);
		writeDescriptorSets[0] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.bright_dft,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descriptorImageInfo
		};
		writeDescriptorSets[1] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.bright_dft,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &uniformBuffers.dft.descriptor
		};
		vkUpdateDescriptorSets(device, 2, &writeDescriptorSets[0], 0, nullptr);
	}

	// complexMultiplication
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &descriptorSetLayouts.complexMultiplication,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr
		};
		vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.complexMultiplication);

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 0,
			.pSetLayouts = &descriptorSetLayouts.complexMultiplication
		};
		vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSets.complexMultiplication);

		std::vector<VkDescriptorImageInfo> descriptorImageInfos(4);
		descriptorImageInfos[0] = {
			.sampler = colorSampler,
			.imageView = frameBuffers.bright_dft.real.view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		descriptorImageInfos[1] = {
			.sampler = colorSampler,
			.imageView = frameBuffers.bright_dft.imaginary.view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		descriptorImageInfos[2] = {
			.sampler = colorSampler,
			.imageView = frameBuffers.blur_dft.real.view,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};
		descriptorImageInfos[3] = {
			.sampler = colorSampler,
			.imageView = frameBuffers.blur_dft.imaginary.view,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};
		std::vector<VkWriteDescriptorSet> writeDescriptorSets(4);
		writeDescriptorSets[0] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.complexMultiplication,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descriptorImageInfos[0]
		};
		writeDescriptorSets[1] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.complexMultiplication,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descriptorImageInfos[1]
		};
		writeDescriptorSets[2] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.complexMultiplication,
			.dstBinding = 2,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descriptorImageInfos[2]
		};
		writeDescriptorSets[3] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.complexMultiplication,
			.dstBinding = 3,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descriptorImageInfos[3]
		};
		vkUpdateDescriptorSets(device, 4, &writeDescriptorSets[0], 0, nullptr);
	}

	// idft
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &descriptorSetLayouts.idft,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr
		};
		vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.idft);

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 0,
			.pSetLayouts = &descriptorSetLayouts.idft
		};
		vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSets.idft);

		std::vector<VkDescriptorImageInfo> descriptorImageInfos(2);
		descriptorImageInfos[0] = {
			.sampler = colorSampler,
			.imageView = frameBuffers.complexMultiplication.real.view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		descriptorImageInfos[1] = {
			.sampler = colorSampler,
			.imageView = frameBuffers.complexMultiplication.imaginary.view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		std::vector<VkWriteDescriptorSet> writeDescriptorSets(3);
		writeDescriptorSets[0] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.idft,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descriptorImageInfos[0]
		};
		writeDescriptorSets[1] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.idft,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descriptorImageInfos[1]
		};
		writeDescriptorSets[2] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.idft,
			.dstBinding = 2,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &uniformBuffers.idft.descriptor
		};
		vkUpdateDescriptorSets(device, 3, &writeDescriptorSets[0], 0, nullptr);
	}

	// blend
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &descriptorSetLayouts.blend,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr
		};
		vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.blend);

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 0,
			.pSetLayouts = &descriptorSetLayouts.blend
		};
		vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSets.blend);

		std::vector<VkDescriptorImageInfo> descriptorImageInfos(2);
		descriptorImageInfos[0] = {
			.sampler = textureDescriptor.sampler,
			.imageView = textureDescriptor.view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		descriptorImageInfos[1] = {
			.sampler = colorSampler,
			.imageView = frameBuffers.idft.color.view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		std::vector<VkWriteDescriptorSet> writeDescriptorSets(2);
		writeDescriptorSets[0] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.blend,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descriptorImageInfos[0]
		};
		writeDescriptorSets[1] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = descriptorSets.blend,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &descriptorImageInfos[1]
		};
		vkUpdateDescriptorSets(device, 2, &writeDescriptorSets[0], 0, nullptr);
	}
}

void LensFlares::loadResources()
{
	int width, height, nchannels;
	unsigned char* textureData = stbi_load("./led.jpg", &width, &height, &nchannels, 0);

	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &formatProperties);

	assert(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

	VkImageCreateInfo imageCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.extent = {(uint32_t)width, (uint32_t)height, (uint32_t)1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VkImage image;
	vkCreateImage(device, &imageCreateInfo, nullptr, &image);
	VkMemoryRequirements memoryReq;
	vkGetImageMemoryRequirements(device, image, &memoryReq);
	VkMemoryAllocateInfo memoryAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = memoryReq.size,
		.memoryTypeIndex = getMemoryTypeIndex(memoryReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};
	VkDeviceMemory deviceMemory;
	vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &deviceMemory);
	vkBindImageMemory(device, image, deviceMemory, 0);
	void* data;
	VkImageSubresource subRes = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.mipLevel = 1
	};
	VkSubresourceLayout	subResLayout;
	vkGetImageSubresourceLayout(device, image, &subRes, &subResLayout);
	vkMapMemory(device, deviceMemory, 0, memoryReq.size, 0, &data);
	memcpy(data, textureData, memoryReq.size);
	vkUnmapMemory(device, deviceMemory);

	VkSamplerCreateInfo samplerCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0.0f,
		.compareOp = VK_COMPARE_OP_NEVER,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
	};
	vkCreateSampler(device, &samplerCreateInfo, nullptr, &textureDescriptor.sampler);

	VkImageViewCreateInfo imageViewCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
		.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
	};
	vkCreateImageView(device, &imageViewCreateInfo, nullptr, &textureDescriptor.view);
	textureDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void LensFlares::buildCommandBuffers()
{
	VkCommandBufferBeginInfo commandBufferBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pInheritanceInfo = nullptr
	};
	for (int i = 0; i < swapchain.imageCount; ++i)
	{
		vkBeginCommandBuffer(commandBuffers[i], &commandBufferBeginInfo);
		// bright
		{
			std::vector<VkClearValue> clearValues(1);
			clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };

			VkRenderPassBeginInfo renderPassBeginInfo = {
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.pNext = nullptr,
				.renderPass = frameBuffers.bright.renderPass,
				.framebuffer = frameBuffers.bright.framebuffer,
				.clearValueCount = 1,
				.pClearValues = clearValues.data()
			};
			renderPassBeginInfo.renderArea.extent = { width, height };

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = {
				.x = (float)0,
				.y = (float)0,
				.width = (float)width,
				.height = (float)height,
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			};
			vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);
			VkRect2D scissor = { 
				.offset = {0, 0},
				.extent = {width, height},
			};
			vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.bright);
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.bright,
				0, 1, &descriptorSets.bright, 0, 0);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
			vkCmdEndRenderPass(commandBuffers[i]);
		}
		
		// blur
		{
			std::vector<VkClearValue> clearValues(1);
			clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };

			VkRenderPassBeginInfo renderPassBeginInfo = {
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.pNext = nullptr,
				.renderPass = frameBuffers.blur.renderPass,
				.framebuffer = frameBuffers.blur.framebuffer,
				.clearValueCount = 1,
				.pClearValues = clearValues.data()
			};
			renderPassBeginInfo.renderArea.extent = { width, height };

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = {
				.x = (float)0,
				.y = (float)0,
				.width = (float)width,
				.height = (float)height,
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			};
			vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);
			VkRect2D scissor = {
				.offset = {0, 0},
				.extent = {width, height},
			};
			vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.blur);
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.blur,
				0, 1, &descriptorSets.blur, 0, 0);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
			vkCmdEndRenderPass(commandBuffers[i]);
		}

		// blur_dft
		{
			std::vector<VkClearValue> clearValues(2);
			clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
			clearValues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };

			VkRenderPassBeginInfo renderPassBeginInfo = {
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.pNext = nullptr,
				.renderPass = frameBuffers.blur_dft.renderPass,
				.framebuffer = frameBuffers.blur_dft.framebuffer,
				.clearValueCount = 2,
				.pClearValues = clearValues.data()
			};
			renderPassBeginInfo.renderArea.extent = { width, height };

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = {
				.x = (float)0,
				.y = (float)0,
				.width = (float)width,
				.height = (float)height,
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			};
			vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);
			VkRect2D scissor = {
				.offset = {0, 0},
				.extent = {width, height},
			};
			vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.blur_dft);
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.dft,
				0, 1, &descriptorSets.blur_dft, 0, 0);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
			vkCmdEndRenderPass(commandBuffers[i]);
		}

		// bright_dft
		{
			std::vector<VkClearValue> clearValues(2);
			clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
			clearValues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };

			VkRenderPassBeginInfo renderPassBeginInfo = {
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.pNext = nullptr,
				.renderPass = frameBuffers.bright_dft.renderPass,
				.framebuffer = frameBuffers.bright_dft.framebuffer,
				.clearValueCount = 2,
				.pClearValues = clearValues.data()
			};
			renderPassBeginInfo.renderArea.extent = { width, height };

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = {
				.x = (float)0,
				.y = (float)0,
				.width = (float)width,
				.height = (float)height,
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			};
			vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);
			VkRect2D scissor = {
				.offset = {0, 0},
				.extent = {width, height},
			};
			vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.bright_dft);
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.dft,
				0, 1, &descriptorSets.bright_dft, 0, 0);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
			vkCmdEndRenderPass(commandBuffers[i]);
		}

		// complexMultiplication
		{
			std::vector<VkClearValue> clearValues(2);
			clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
			clearValues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };

			VkRenderPassBeginInfo renderPassBeginInfo = {
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.pNext = nullptr,
				.renderPass = frameBuffers.complexMultiplication.renderPass,
				.framebuffer = frameBuffers.complexMultiplication.framebuffer,
				.clearValueCount = 2,
				.pClearValues = clearValues.data()
			};
			renderPassBeginInfo.renderArea.extent = { width, height };

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = {
				.x = (float)0,
				.y = (float)0,
				.width = (float)width,
				.height = (float)height,
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			};
			vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);
			VkRect2D scissor = {
				.offset = {0, 0},
				.extent = {width, height},
			};
			vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.complexMultiplication);
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.complexMultiplication,
				0, 1, &descriptorSets.complexMultiplication, 0, 0);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
			vkCmdEndRenderPass(commandBuffers[i]);
		}

		// blend
		{
			std::vector<VkClearValue> clearValues(1);
			clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };

			VkRenderPassBeginInfo renderPassBeginInfo = {
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.pNext = nullptr,
				.renderPass = frameBuffers.blend.renderPass,
				.framebuffer = frameBuffers.blend.framebuffer,
				.clearValueCount = 1,
				.pClearValues = clearValues.data()
			};
			renderPassBeginInfo.renderArea.extent = { width, height };

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = {
				.x = (float)0,
				.y = (float)0,
				.width = (float)width,
				.height = (float)height,
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			};
			vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);
			VkRect2D scissor = {
				.offset = {0, 0},
				.extent = {width, height},
			};
			vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.blend);
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.blend,
				0, 1, &descriptorSets.blend, 0, 0);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
			vkCmdEndRenderPass(commandBuffers[i]);
		}
		vkEndCommandBuffer(commandBuffers[i]);
	}
}

void LensFlares::createUniformBuffers()
{
	VkBufferCreateInfo bufferCreateInfo = {
	.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.size = sizeof(blurUboVS) + sizeof(fftUboVS) * 2,
	.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	vkCreateBuffer(device, &bufferCreateInfo, nullptr, &uniformBuffers.buffer);

	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(device, uniformBuffers.buffer, &memReq);
	VkMemoryAllocateInfo memoryAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = memReq.size,
		.memoryTypeIndex = getMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};
	vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &uniformBuffers.memory);
	vkBindBufferMemory(device, uniformBuffers.buffer, uniformBuffers.memory, 0);
	uniformBuffers.blur.descriptor = {
		.buffer = uniformBuffers.buffer,
		.offset = 0,
		.range = sizeof(blurUboVS)
	};
	uniformBuffers.dft.descriptor = {
		.buffer = uniformBuffers.buffer,
		.offset = sizeof(blurUboVS),
		.range = sizeof(fftUboVS)
	};
	uniformBuffers.idft.descriptor = {
		.buffer = uniformBuffers.buffer,
		.offset = sizeof(blurUboVS) + sizeof(fftUboVS),
		.range = sizeof(fftUboVS)
	};

	void* data;
	blurUboVS = { 1.0f / width, 1.0f / height };
	vkMapMemory(device, uniformBuffers.memory, 0, sizeof(blurUboVS), 0, &data);
	memcpy(data, &blurUboVS, sizeof(blurUboVS));
	vkUnmapMemory(device, uniformBuffers.memory);
	fftUboVS = { glm::vec2(1.0f / width, 1.0f / height), false };
	vkMapMemory(device, uniformBuffers.memory, sizeof(blurUboVS), sizeof(fftUboVS), 0, &data);
	memcpy(data, &fftUboVS, sizeof(fftUboVS));
	vkUnmapMemory(device, uniformBuffers.memory);
	vkMapMemory(device, uniformBuffers.memory, sizeof(blurUboVS) + sizeof(fftUboVS), sizeof(fftUboVS), 0, &data);
	fftUboVS.isInverse = true;
	memcpy(data, &fftUboVS, sizeof(fftUboVS));
	vkUnmapMemory(device, uniformBuffers.memory);
}

bool LensFlares::isDeviceSuitable(VkPhysicalDevice device)
{
	return true;
}

QueueFamilyIndices LensFlares::findQueueFamilys(VkPhysicalDevice physicalDevice)
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

uint32_t LensFlares::getMemoryTypeIndex(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties)
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

VkCommandBuffer LensFlares::getCommandBuffer(bool begin)
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

void LensFlares::createAttachment(FrameBufferAttachment* attachment, VkFormat format, VkImageUsageFlags usage,
	float width, float height)
{
	attachment->format = format;

	VkImageAspectFlags aspectFlag;
	if (usage & VK_IMAGE_ASPECT_COLOR_BIT)
		aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

	VkImageCreateInfo imageCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = {(uint32_t)width, (uint32_t)height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	vkCreateImage(device, &imageCreateInfo, nullptr, &attachment->image);
	VkMemoryRequirements memreq;
	vkGetImageMemoryRequirements(device, attachment->image, &memreq);
	VkMemoryAllocateInfo memAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = memreq.size,
		.memoryTypeIndex = getMemoryTypeIndex(memreq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	vkAllocateMemory(device, &memAllocInfo, nullptr, &attachment->memory);
	vkBindImageMemory(device, attachment->image, attachment->memory, 0);

	VkImageViewCreateInfo imageViewCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = attachment->image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
		.subresourceRange = {
			.aspectMask = aspectFlag,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};
	vkCreateImageView(device, &imageViewCreateInfo, nullptr, &attachment->view);
}

VkShaderModule LensFlares::createShaderModule(const std::string& filepath)
{
	std::ifstream ifs(filepath.c_str(), std::ios::binary);
	ifs.seekg(0, ifs.end);
	uint32_t codeSize = ifs.tellg();
	ifs.seekg(0, ifs.beg);
	char* codeBuffer = (char*)malloc(codeSize);
	ifs.read(codeBuffer, codeSize);
	VkShaderModule shaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		codeSize,
		(uint32_t*)codeBuffer
	};
	if (vkCreateShaderModule(device, &vertexShaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		free(codeBuffer);
		throw std::runtime_error("Failed to create vertex shader module!");
	}
	return shaderModule;
}