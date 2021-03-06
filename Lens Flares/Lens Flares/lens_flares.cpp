#include "lens_flares.h"

#include <set>
#include <fstream>

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

void LensFlares::initWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(800, 800, "Triangle", 0, 0);
}

void LensFlares::prepareVulkan()
{
	createInstance();
	swapchain.create(physicalDevice, device, surfaceKHR);
	createCommandPool();
	createCommandBuffers();
	createSynchronizationPrimitives();
	createRenderPass();
	createPipelineCache();
	createFrameBuffers();
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
	frameBuffers.dft.width = width;
	frameBuffers.dft.height = height;
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
			.colorAttachmentCount = colorAttachments.size(),
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
			.dependencyCount = subpassDependencys.size(),
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
			.colorAttachmentCount = colorAttachments.size(),
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
			.dependencyCount = subpassDependencys.size(),
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
	
	// dft
	{
		createAttachment(&frameBuffers.dft.real, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height);
		createAttachment(&frameBuffers.dft.imaginary, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height);

		std::vector<VkAttachmentDescription> attachmentDescriptions(2);
		attachmentDescriptions[0] = {
			.flags = 0,
			.format = frameBuffers.dft.real.format,
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
			.format = frameBuffers.dft.imaginary.format,
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
			.colorAttachmentCount = colorAttachments.size(),
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
			.dependencyCount = subpassDependencys.size(),
			.pDependencies = subpassDependencys.data()
		};
		VkRenderPass renderPass;
		vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);

		std::vector<VkImageView> attachments(2);
		attachments[0] = frameBuffers.dft.real.view;
		attachments[1] = frameBuffers.dft.imaginary.view;

		VkFramebufferCreateInfo framebufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = renderPass,
			.attachmentCount = attachments.size(),
			.pAttachments = attachments.data(),
			.width = width,
			.height = height,
			.layers = 1
		};
		vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &frameBuffers.dft.framebuffer);
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
			.colorAttachmentCount = colorAttachments.size(),
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
			.dependencyCount = subpassDependencys.size(),
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
			.attachmentCount = attachments.size(),
			.pAttachments = attachments.data(),
			.width = width,
			.height = height,
			.layers = 1
		};
		vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &frameBuffers.idft.framebuffer);
	}

	// complexMultiplication
	{
		createAttachment(&frameBuffers.complexMultiplication.color, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height);

		std::vector<VkAttachmentDescription> attachmentDescriptions(1);
		attachmentDescriptions[0] = {
			.flags = 0,
			.format = frameBuffers.complexMultiplication.color.format,
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
			.colorAttachmentCount = colorAttachments.size(),
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
			.dependencyCount = subpassDependencys.size(),
			.pDependencies = subpassDependencys.data()
		};
		VkRenderPass renderPass;
		vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);

		std::vector<VkImageView> attachments(1);
		attachments[0] = frameBuffers.complexMultiplication.color.view;

		VkFramebufferCreateInfo framebufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = renderPass,
			.attachmentCount = attachments.size(),
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
			.colorAttachmentCount = colorAttachments.size(),
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
			.dependencyCount = subpassDependencys.size(),
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
			.attachmentCount = attachments.size(),
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
			.bindingCount = descriptorSetLayoutBindings.size(),
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
			},
			{
				.binding = 3,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			}
		};

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = descriptorSetLayoutBindings.size(),
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
			},
			{
				.binding = 3,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			}
		};

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = descriptorSetLayoutBindings.size(),
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
			.bindingCount = descriptorSetLayoutBindings.size(),
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
			.bindingCount = descriptorSetLayoutBindings.size(),
			.pBindings = descriptorSetLayoutBindings.data()
		};
		vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts.blend);
	}
}

void LensFlares::setupDescriptorSet()
{

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
		.extent = {width, height, 1.0f},
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