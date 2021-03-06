#pragma once
#include "vulkan/vulkan.hpp"

class SwapChain{
public:
	SwapChain() = default;
	void create(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface);

public:
	VkFormat					colorFormat;
	VkColorSpaceKHR				colorSpace;
	VkSwapchainKHR				swapchain;
	uint32_t					imageCount;
	std::vector<VkImage>		images;
	std::vector<VkImageView>	views;
};