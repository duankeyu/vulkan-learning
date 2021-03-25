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
	LensFlares();
	~LensFlares();
	void run();

private:
	void initWindow();
	void prepare();
	void mainLoop();

private:
	void createInstance();
	void createSurface();
	void setupDebugMessenger();
	void pickPhysicalDevice();
	void createSynchronizationPrimitives();
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
	void loadResources();
	void buildCommandBuffers();
	void createUniformBuffers();

private:
	bool isDeviceSuitable(VkPhysicalDevice device);
	QueueFamilyIndices findQueueFamilys(VkPhysicalDevice physicalDevice);
	uint32_t getMemoryTypeIndex(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties);
	VkCommandBuffer getCommandBuffer(bool begin);
	struct FrameBufferAttachment;
	void createAttachment(FrameBufferAttachment *attachment, VkFormat format, VkImageUsageFlags usage,
		float width, float height);
	VkShaderModule	createShaderModule(const std::string& filepath);
	bool checkValidationLayersSupport();
	std::vector<const char*> getRequireExtensions();
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

private:
	GLFWwindow*						window;
	VkInstance						instance;
	VkDebugUtilsMessengerEXT		debugMessenger;
	VkPhysicalDevice				physicalDevice;
	VkSurfaceKHR					surfaceKHR;
	QueueFamilyIndices				indices;
	VkQueue							graphicQueue;
	VkQueue							presentQueue;
	VkDevice						device;
	SwapChain						swapchain;
	VkPipelineCache					pipelineCache;
	VkCommandPool					commandPool;
	std::vector<VkCommandBuffer>	commandBuffers;
	VkDescriptorPool				descriptorPool;
	VkSampler						colorSampler;

	VkSemaphore						semaphore;
	VkSemaphore						renderSemaphore;
	std::vector<VkFence>			waitFences;

	uint32_t						width;
	uint32_t						height;
	
	struct {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 porjection;
	} uboVS;
	struct {
		glm::vec2 uv;
		bool isInverse;
	} fftUboVS;
	struct {
		float u;
		float v;
	} blurUboVS;
	struct {
		struct {
			VkDescriptorBufferInfo descriptor;
		} blur, dft, idft;
		VkDeviceMemory memory;
		VkBuffer buffer;
	} uniformBuffers;
	struct {
		VkImageView view;
		VkSampler	sampler;
		VkImageLayout	imageLayout;
	} textureDescriptor;
	struct {
		VkPipeline	bright;
		VkPipeline	blur;
		VkPipeline	bright_dft;
		VkPipeline	blur_dft;
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
		VkDescriptorSet		bright_dft;
		VkDescriptorSet		blur_dft;
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
		} bright, blur, idft, blend;
		struct : public FrameBuffer {
			FrameBufferAttachment real, imaginary;
		} bright_dft, blur_dft, complexMultiplication;
	} frameBuffers;
};