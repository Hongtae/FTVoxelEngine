#include "framework.h"
#include "UIRenderer.h"

#ifdef _WIN32
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT(*defaultWndProc)(HWND, UINT, WPARAM, LPARAM) = nullptr;
bool mouseLocked = false;

LRESULT forwardImGuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    if (ImGui::GetCurrentContext()) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            mouseLocked = true;
            return ::DefWindowProcW(hWnd, msg, wParam, lParam);
        }
    }
    mouseLocked = false;

    if (defaultWndProc)
        return (*defaultWndProc)(hWnd, msg, wParam, lParam);
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
#endif

UIRenderer::UIRenderer()
    : swapchain(nullptr)
    , fence(VK_NULL_HANDLE)
    , commandBuffer(VK_NULL_HANDLE)
    , commandPool(VK_NULL_HANDLE)
    , descriptorPool(VK_NULL_HANDLE) {

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
}

UIRenderer::~UIRenderer() {
#ifdef _WIN32
    ImGui_ImplWin32_Shutdown();
#endif
    ImGui::DestroyContext();
}

void UIRenderer::setWindow(Window* window) {
#ifdef _WIN32
    HWND hWnd = (HWND)window->platformHandle();
    ImGui_ImplWin32_Init(hWnd);

    defaultWndProc = decltype(defaultWndProc)(GetWindowLongPtrW(hWnd, GWLP_WNDPROC));
    SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)forwardImGuiWndProc);
#endif
}

void UIRenderer::setSwapChain(SwapChain* swapchain) {
    this->swapchain = swapchain;
}

void UIRenderer::initialize(std::shared_ptr<CommandQueue> queue) {

    this->cqueue = std::dynamic_pointer_cast<FV::Vulkan::CommandQueue>(queue);
    if (this->cqueue.get() == nullptr)
        throw std::runtime_error("Unable to get vulkan command queue!");
    this->gdevice = this->cqueue->gdevice;
    if (gdevice.get() == nullptr)
        throw std::runtime_error("Unable to get vulkan device!");

    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    };
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    descriptorPoolCreateInfo.pPoolSizes = pool_sizes;

    VkResult err = vkCreateDescriptorPool(gdevice->device, &descriptorPoolCreateInfo, nullptr, &this->descriptorPool);
    if (err != VK_SUCCESS)
        throw std::runtime_error("vkCreateDescriptorPool error!");

    VkCommandPoolCreateInfo cmdPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cmdPoolCreateInfo.queueFamilyIndex = cqueue->family->familyIndex;
    cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    err = vkCreateCommandPool(gdevice->device, &cmdPoolCreateInfo, gdevice->allocationCallbacks(), &this->commandPool);
    if (err != VK_SUCCESS)
        throw std::runtime_error("vkCreateCommandPool failed");

    VkCommandBufferAllocateInfo  bufferInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    bufferInfo.commandPool = this->commandPool;
    bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    bufferInfo.commandBufferCount = 1;
    err = vkAllocateCommandBuffers(gdevice->device, &bufferInfo, &this->commandBuffer);
    if (err != VK_SUCCESS)
        throw std::runtime_error("vkAllocateCommandBuffers failed");

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = gdevice->instance->instance;
    init_info.Device = gdevice->device;
    init_info.PhysicalDevice = gdevice->physicalDevice.device;
    init_info.Queue = cqueue->queue;
    init_info.QueueFamily = cqueue->family->familyIndex;
    init_info.MinImageCount = 2;
    init_info.ImageCount = uint32_t(swapchain->maximumBufferCount());
    init_info.UseDynamicRendering = true;
    init_info.DescriptorPool = this->descriptorPool;
    init_info.ColorAttachmentFormat = FV::Vulkan::getPixelFormat(swapchain->pixelFormat());

    ImGui_ImplVulkan_Init(&init_info, nullptr);

    err = vkResetCommandPool(gdevice->device, this->commandPool, 0);
    if (err != VK_SUCCESS)
        throw std::runtime_error("vkResetCommandPool failed");

    VkCommandBufferBeginInfo cbufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    cbufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(this->commandBuffer, &cbufferBeginInfo);
    if (err != VK_SUCCESS)
        throw std::runtime_error("vkBeginCommandBuffer failed!");

    ImGui_ImplVulkan_CreateFontsTexture(this->commandBuffer);

    err = vkEndCommandBuffer(this->commandBuffer);
    if (err != VK_SUCCESS)
        throw std::runtime_error("vkEndCommandBuffer failed!");

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &this->commandBuffer;
    err = vkQueueSubmit(cqueue->queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (err != VK_SUCCESS)
        throw std::runtime_error("vkQueueSubmit failed!");

    err = vkDeviceWaitIdle(gdevice->device);
    if (err != VK_SUCCESS)
        throw std::runtime_error("vkDeviceWaitIdle failed!");

    ImGui_ImplVulkan_DestroyFontUploadObjects();

    VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    err = vkCreateFence(gdevice->device, &fenceCreateInfo, gdevice->allocationCallbacks(), &this->fence);
    if (err != VK_SUCCESS)
        throw std::runtime_error("vkCreateFence failed!");
}

void UIRenderer::finalize() {
    VkResult err = vkDeviceWaitIdle(gdevice->device);
    if (err != VK_SUCCESS)
        throw std::runtime_error("vkDeviceWaitIdle failed.");

    ImGui_ImplVulkan_Shutdown();

    vkDestroyFence(gdevice->device, this->fence, gdevice->allocationCallbacks());
    vkFreeCommandBuffers(gdevice->device, this->commandPool, 1, &this->commandBuffer);
    vkDestroyCommandPool(gdevice->device, this->commandPool, gdevice->allocationCallbacks());
    vkDestroyDescriptorPool(gdevice->device, this->descriptorPool, gdevice->allocationCallbacks());

    this->fence = VK_NULL_HANDLE;
    this->commandBuffer = VK_NULL_HANDLE;
    this->commandPool = VK_NULL_HANDLE;
    this->descriptorPool = VK_NULL_HANDLE;
    this->swapchain = nullptr;

    cqueue.reset();
    gdevice.reset();
}

void UIRenderer::update(float delta) {
}

void UIRenderer::prepareScene(const RenderPassDescriptor&) {
    ImGui_ImplVulkan_NewFrame();

#ifdef _WIN32
    ImGui_ImplWin32_NewFrame();
#endif
}

void UIRenderer::render(const RenderPassDescriptor& rp, const Rect& frame, CommandQueue*) {
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (!is_minimized) {
#if FVCORE_ENABLE_VULKAN
        VkResult err = vkWaitForFences(gdevice->device, 1, &this->fence, VK_TRUE, UINT64_MAX);
        if (err != VK_SUCCESS)
            throw std::runtime_error("vkWaitForFences failed.");

        err = vkResetFences(gdevice->device, 1, &this->fence);
        if (err != VK_SUCCESS)
            throw std::runtime_error("vkResetFences failed.");

        err = vkResetCommandPool(gdevice->device, this->commandPool, 0);
        if (err != VK_SUCCESS)
            throw std::runtime_error("vkResetCommandPool failed.");

        VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(this->commandBuffer, &commandBufferBeginInfo);

        VkRenderingAttachmentInfo colorAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        if (auto imageView = dynamic_cast<FV::Vulkan::ImageView*>(rp.colorAttachments.front().renderTarget.get());
            imageView) {
            colorAttachment.imageView = imageView->imageView;
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        }

        VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderingInfo.flags = 0;
        renderingInfo.layerCount = 1;
        renderingInfo.renderArea = { 
            int32_t(frame.origin.x), int32_t(frame.origin.y),
            uint32_t(frame.width()), uint32_t(frame.height()) };
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

        vkCmdBeginRendering(this->commandBuffer, &renderingInfo);
        ImGui_ImplVulkan_RenderDrawData(draw_data, this->commandBuffer);
        vkCmdEndRendering(this->commandBuffer);

        vkEndCommandBuffer(this->commandBuffer);

        VkCommandBufferSubmitInfo cbufferSubmitInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
        cbufferSubmitInfo.commandBuffer = this->commandBuffer;
        cbufferSubmitInfo.deviceMask = 0;
        VkSubmitInfo2 submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &cbufferSubmitInfo;

        err = vkQueueSubmit2(cqueue->queue, 1, &submitInfo, this->fence);
        if (err != VK_SUCCESS)
            throw std::runtime_error("vkQueueSubmit2 failed.");
#endif
    }
}
