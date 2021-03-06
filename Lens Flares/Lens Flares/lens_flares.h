#pragma once
#include <optional>

#include "vulkan/vulkan.hpp"
#include "glm/glm.hpp"
#include "GLFW/glfw3.h"

#include "swapchain.h"

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

class LensFlares
{
public:

private:
	void initWindow();
	void prepareVulkan();

private:
	void createInstance();
	void setupDebugMessenger();
	void pickPhysicalDevice();
	void createSynchronizationPrimitives();
	void createSurface();
	void createLogicalDevice();
	void createRenderPass();
	void createPipelineCache();
	void createPipeline();
	void createCommandPool();
	void createFrameBuffers();
	void createCommandBuffers();
	void createDescriptorPool();
	void setupDescriptorSetLayout();
	void setupDescriptorSet();

private:
	bool isDeviceSuitable(VkPhysicalDevice device);
	QueueFamilyIndices findQueueFamilys(VkPhysicalDevice physicalDevice);
	uint32_t getMemoryTypeIndex(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties);
	VkCommandBuffer getCommandBuffer(bool begin);
	struct FrameBufferAttachment;
	void createAttachment(FrameBufferAttachment *attachment, VkFormat format, VkImageUsageFlags usage,
		float width, float height);

private:
	GLFWwindow*						window;
	VkInstance						instance;
	VkPhysicalDevice				physicalDevice;
	VkSurfaceKHR					surfaceKHR;
	QueueFamilyIndices				indices;
	VkQueue							graphicQueue;
	VkQueue							presentQueue;
	VkDevice						device;
	SwapChain						swapchain;
	VkRenderPass					renderPass;
	VkPipelineCache					pipelineCache;
	VkPipeline						pipeline;
	VkPipelineLayout				pipelineLayout;
	VkCommandPool					commandPool;
	std::vector<VkCommandBuffer>	commandBuffers;
	VkDescriptorPool				descriptorPool;

	VkSemaphore						semaphore;
	VkSemaphore						renderSemaphore;
	std::vector<VkFence>			waitFences;

	uint32_t						width;
	uint32_t						height;
	
	struct { 
		VkBuffer buffer; 
		VkDeviceMemory memory; 
	} deviceVertex;
	struct { 
		VkDeviceMemory memory; 
		VkBuffer buffer; 
		VkDescriptorBufferInfo descriptor; 
	} uniformBufferVS;
	struct { 
		glm::mat4 model; 
		glm::mat4 view; 
		glm::mat4 porjection; 
	} uboVS;
	struct {
		VkPipeline	bright;
		VkPipeline	blur;
		VkPipeline	dft;
		VkPipeline	idft;
		VkPipeline	complexMultiplication;
		VkPipeline	blend;
	} pipelines;
	struct {
		VkPipelineLayout	bright;
		VkPipelineLayout	blur;
		VkPipelineLayout	dft;
		VkPipelineLayout	idft;
		VkPipelineLayout	complexMultiplication;
		VkPipelineLayout	blend;
	} pipelineLayouts;
	struct {
		VkDescriptorSet		bright;
		VkDescriptorSet		blur;
		VkDescriptorSet		dft;
		VkDescriptorSet		idft;
		VkDescriptorSet		complexMultiplication;
		VkDescriptorSet		blend;
	} descriptorSets;
	struct {
		VkDescriptorSetLayout	bright;
		VkDescriptorSetLayout	blur;
		VkDescriptorSetLayout	dft;
		VkDescriptorSetLayout	idft;
		VkDescriptorSetLayout	complexMultiplication;
		VkDescriptorSetLayout	blend;
	} descriptorSetLayouts;
	struct FrameBufferAttachment {
		VkImage			image;
		VkDeviceMemory	memory;
		VkImageView		view;
		VkFormat		format;
	};
	struct FrameBuffer {
		uint32_t	width, height;
		VkFramebuffer	framebuffer;
		VkRenderPass	renderPass;
	};
	struct {
		struct : public FrameBuffer {
			FrameBufferAttachment color;
		} bright, blur, idft, complexMultiplication, blend;
		struct : public FrameBuffer {
			FrameBufferAttachment real, imaginary;
		} dft;
	} frameBuffers;
};