#include "swapchain.h"

void SwapChain::create(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface)
{
	colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
	VkColorSpaceKHR	colorSpaceKHR;
	uint32_t formatCount = 0;
	std::vector<VkSurfaceFormatKHR> surfaceFormatKHRs;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	surfaceFormatKHRs.resize(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormatKHRs.data());
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
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilitesKHR);
	VkExtent2D extent = surfaceCapabilitesKHR.currentExtent;
	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModeKHRs(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModeKHRs.data());
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

	VkSwapchainKHR oldSwapChainKHR = swapchain;
	VkSwapchainCreateInfoKHR swapChainCreateInfo = {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		nullptr,
		0,
		surface,
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
	if (vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapchain) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
	std::vector<VkImage> images(imageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());

	views.resize(imageCount);
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
		if (vkCreateImageView(device, &imageViewCreateInfo, nullptr, &views[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image view!");
		}
	}
}