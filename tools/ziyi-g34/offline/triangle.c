/* SPDX-License-Identifier: MIT
 * Offline command producer ONLY. Mesa's noop DRM shim captures buffers without
 * executing them. This program is never a hardware/triangle success test.
 */
#include <vulkan/vulkan.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do { VkResult r_=(expr); if(r_!=VK_SUCCESS) { \
 fprintf(stderr,"%s: VkResult %d at %d\n",#expr,r_,__LINE__); exit(1); } } while(0)

static VkShaderModule shader(VkDevice d, const char *path) {
    FILE *f=fopen(path,"rb"); assert(f);
    assert(!fseek(f,0,SEEK_END)); long size=ftell(f);
    assert(size>20 && !(size&3) && !fseek(f,0,SEEK_SET));
    uint32_t *code=malloc((size_t)size); assert(code);
    assert(fread(code,1,(size_t)size,f)==(size_t)size); fclose(f);
    VkShaderModuleCreateInfo c={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize=(size_t)size,.pCode=code};
    VkShaderModule s; CHECK(vkCreateShaderModule(d,&c,NULL,&s)); free(code); return s;
}

int main(int argc, char **argv) {
    assert(argc==3);
    VkApplicationInfo ai={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName="G34 offline A662 triangle producer",.apiVersion=VK_API_VERSION_1_1};
    VkInstanceCreateInfo ici={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&ai};
    VkInstance instance; CHECK(vkCreateInstance(&ici,NULL,&instance));
    uint32_t n=0; CHECK(vkEnumeratePhysicalDevices(instance,&n,NULL)); assert(n==1);
    VkPhysicalDevice physical; CHECK(vkEnumeratePhysicalDevices(instance,&n,&physical));
    VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(physical,&props);
    fprintf(stderr,"Offline producer: %s / deviceID %08x; NO GPU EXECUTION\n",props.deviceName,props.deviceID);
    uint32_t qcount=0; vkGetPhysicalDeviceQueueFamilyProperties(physical,&qcount,NULL);
    VkQueueFamilyProperties *qp=calloc(qcount,sizeof(*qp)); assert(qp);
    vkGetPhysicalDeviceQueueFamilyProperties(physical,&qcount,qp);
    uint32_t qi=0; while(qi<qcount && !(qp[qi].queueFlags&VK_QUEUE_GRAPHICS_BIT)) qi++;
    assert(qi<qcount); free(qp);
    float priority=1.0f;
    VkDeviceQueueCreateInfo qci={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex=qi,.queueCount=1,.pQueuePriorities=&priority};
    VkDeviceCreateInfo dci={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount=1,.pQueueCreateInfos=&qci};
    VkDevice device; CHECK(vkCreateDevice(physical,&dci,NULL,&device));
    VkQueue queue; vkGetDeviceQueue(device,qi,0,&queue);
    VkImageCreateInfo imci={.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType=VK_IMAGE_TYPE_2D,.format=VK_FORMAT_B8G8R8A8_UNORM,.extent={64,64,1},
        .mipLevels=1,.arrayLayers=1,.samples=VK_SAMPLE_COUNT_1_BIT,
        .tiling=VK_IMAGE_TILING_LINEAR,.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode=VK_SHARING_MODE_EXCLUSIVE,.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED};
    VkImage image; CHECK(vkCreateImage(device,&imci,NULL,&image));
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(device,image,&mr);
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(physical,&mp);
    uint32_t mt=0; while(mt<mp.memoryTypeCount && !(mr.memoryTypeBits&(1u<<mt))) mt++;
    assert(mt<mp.memoryTypeCount);
    VkMemoryAllocateInfo mai={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize=mr.size,.memoryTypeIndex=mt};
    VkDeviceMemory memory; CHECK(vkAllocateMemory(device,&mai,NULL,&memory));
    CHECK(vkBindImageMemory(device,image,memory,0));
    VkImageSubresource sub={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT}; VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(device,image,&sub,&layout);
    fprintf(stderr,"TARGET bytes=%llu align=%llu offset=%llu rowPitch=%llu size=%llu\n",
       (unsigned long long)mr.size,(unsigned long long)mr.alignment,
       (unsigned long long)layout.offset,(unsigned long long)layout.rowPitch,(unsigned long long)layout.size);
    assert(layout.offset==0 && layout.rowPitch==256 && layout.size==16384);
    VkImageViewCreateInfo vci={.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image=image,.viewType=VK_IMAGE_VIEW_TYPE_2D,.format=imci.format,
        .subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
    VkImageView view; CHECK(vkCreateImageView(device,&vci,NULL,&view));
    VkAttachmentDescription ad={.format=imci.format,.samples=VK_SAMPLE_COUNT_1_BIT,
        .loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference ar={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp={.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount=1,.pColorAttachments=&ar};
    VkRenderPassCreateInfo rpci={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount=1,.pAttachments=&ad,.subpassCount=1,.pSubpasses=&sp};
    VkRenderPass pass; CHECK(vkCreateRenderPass(device,&rpci,NULL,&pass));
    VkFramebufferCreateInfo fbci={.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass=pass,.attachmentCount=1,.pAttachments=&view,.width=64,.height=64,.layers=1};
    VkFramebuffer fb; CHECK(vkCreateFramebuffer(device,&fbci,NULL,&fb));
    VkPipelineLayoutCreateInfo plci={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkPipelineLayout pl; CHECK(vkCreatePipelineLayout(device,&plci,NULL,&pl));
    VkPipelineShaderStageCreateInfo stages[2]={
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_VERTEX_BIT,
         .module=shader(device,argv[1]),.pName="main"},
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_FRAGMENT_BIT,
         .module=shader(device,argv[2]),.pName="main"}};
    VkPipelineVertexInputStateCreateInfo vi={.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia={.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkViewport vp={0,0,64,64,0,1}; VkRect2D sc={{0,0},{64,64}};
    VkPipelineViewportStateCreateInfo vs={.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount=1,.pViewports=&vp,.scissorCount=1,.pScissors=&sc};
    VkPipelineRasterizationStateCreateInfo rs={.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode=VK_POLYGON_MODE_FILL,.cullMode=VK_CULL_MODE_NONE,.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth=1.0f};
    VkPipelineMultisampleStateCreateInfo ms={.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples=VK_SAMPLE_COUNT_1_BIT};
    VkPipelineColorBlendAttachmentState ba={.colorWriteMask=15};
    VkPipelineColorBlendStateCreateInfo bs={.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount=1,.pAttachments=&ba};
    VkGraphicsPipelineCreateInfo pci={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount=2,.pStages=stages,.pVertexInputState=&vi,.pInputAssemblyState=&ia,.pViewportState=&vs,
        .pRasterizationState=&rs,.pMultisampleState=&ms,.pColorBlendState=&bs,
        .layout=pl,.renderPass=pass,.subpass=0};
    VkPipeline pipeline; CHECK(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pci,NULL,&pipeline));
    VkCommandPoolCreateInfo cpci={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.queueFamilyIndex=qi};
    VkCommandPool pool; CHECK(vkCreateCommandPool(device,&cpci,NULL,&pool));
    VkCommandBufferAllocateInfo cai={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool=pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
    VkCommandBuffer cb; CHECK(vkAllocateCommandBuffers(device,&cai,&cb));
    VkCommandBufferBeginInfo cbi={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}; CHECK(vkBeginCommandBuffer(cb,&cbi));
    VkImageMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask=0,.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .image=image,.subresourceRange=vci.subresourceRange};
    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,0,NULL,0,NULL,1,&barrier);
    VkRenderPassBeginInfo rbi={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass=pass,.framebuffer=fb,.renderArea={{0,0},{64,64}}};
    vkCmdBeginRenderPass(cb,&rbi,VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    vkCmdDraw(cb,3,1,0,0);
    vkCmdEndRenderPass(cb); CHECK(vkEndCommandBuffer(cb));
    VkSubmitInfo si={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&cb};
    CHECK(vkQueueSubmit(queue,1,&si,VK_NULL_HANDLE));
    fprintf(stderr,"G34_OFFLINE_COMMAND_CAPTURE_COMPLETE; nativeExecution=false\n");
    fflush(NULL); _Exit(0);
}
