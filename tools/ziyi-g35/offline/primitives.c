/* SPDX-License-Identifier: MIT
 * Offline command producer ONLY. Mesa's noop DRM shim captures buffers without
 * executing them. This program is never a hardware/triangle success test.
 */
#include <vulkan/vulkan.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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


static VkImage make_image(VkDevice d,VkPhysicalDevice physical,VkFormat format,
 uint32_t width,uint32_t height,VkImageUsageFlags usage,VkMemoryPropertyFlags flags,
 VkDeviceMemory *memory,VkSubresourceLayout *layout,VkImageAspectFlags aspect) {
 VkImageCreateInfo ci={.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,.imageType=VK_IMAGE_TYPE_2D,
  .format=format,.extent={width,height,1},.mipLevels=1,.arrayLayers=1,.samples=VK_SAMPLE_COUNT_1_BIT,
  .tiling=VK_IMAGE_TILING_LINEAR,.usage=usage,.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
  .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED};
 VkImage image;CHECK(vkCreateImage(d,&ci,NULL,&image));
 VkMemoryRequirements mr;vkGetImageMemoryRequirements(d,image,&mr);
 VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(physical,&mp);
 uint32_t mt=0;while(mt<mp.memoryTypeCount&&(!(mr.memoryTypeBits&(1u<<mt))||
  (mp.memoryTypes[mt].propertyFlags&flags)!=flags)) { mt++; }
 assert(mt<mp.memoryTypeCount);
 VkMemoryAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=mr.size,.memoryTypeIndex=mt};
 CHECK(vkAllocateMemory(d,&ai,NULL,memory));CHECK(vkBindImageMemory(d,image,*memory,0));
 VkImageSubresource sub={.aspectMask=aspect};vkGetImageSubresourceLayout(d,image,&sub,layout);
 fprintf(stderr,"G35_IMAGE format=%u width=%u height=%u bytes=%llu offset=%llu rowPitch=%llu size=%llu\n",
  format,width,height,(unsigned long long)mr.size,(unsigned long long)layout->offset,
  (unsigned long long)layout->rowPitch,(unsigned long long)layout->size);
 return image;
}
static VkImageView image_view(VkDevice d,VkImage image,VkFormat format,VkImageAspectFlags aspect) {
 VkImageViewCreateInfo ci={.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,.image=image,
  .viewType=VK_IMAGE_VIEW_TYPE_2D,.format=format,.subresourceRange={aspect,0,1,0,1}};
 VkImageView view;CHECK(vkCreateImageView(d,&ci,NULL,&view));return view;
}
int main(int argc, char **argv) {
    assert(argc==5);
    VkApplicationInfo ai={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName="G35 offline A662 texture blend depth producer",.apiVersion=VK_API_VERSION_1_1};
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
    
    VkDeviceMemory depth_memory,texture_memory;VkSubresourceLayout depth_layout,texture_layout;
    VkImage depth=make_image(device,physical,VK_FORMAT_D16_UNORM,64,64,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,0,
       &depth_memory,&depth_layout,VK_IMAGE_ASPECT_DEPTH_BIT);
    assert(depth_layout.offset==0 && depth_layout.rowPitch==128 && depth_layout.size==8192);
    VkImage texture=make_image(device,physical,VK_FORMAT_R8G8B8A8_UNORM,2,2,VK_IMAGE_USAGE_SAMPLED_BIT,
       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
       &texture_memory,&texture_layout,VK_IMAGE_ASPECT_COLOR_BIT);
    assert(texture_layout.offset==0 && texture_layout.rowPitch>=8 && texture_layout.size<=4096);
    void *texmap;CHECK(vkMapMemory(device,texture_memory,0,VK_WHOLE_SIZE,0,&texmap));
    const uint8_t texels[16]={255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
    memset(texmap,0,(size_t)texture_layout.size);
    memcpy(texmap,texels,8);memcpy((uint8_t*)texmap+texture_layout.rowPitch,texels+8,8);
    vkUnmapMemory(device,texture_memory);
    VkImageView depth_view=image_view(device,depth,VK_FORMAT_D16_UNORM,VK_IMAGE_ASPECT_DEPTH_BIT);
    VkImageView texture_view=image_view(device,texture,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT);
    VkSamplerCreateInfo sci={.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,.magFilter=VK_FILTER_NEAREST,
      .minFilter=VK_FILTER_NEAREST,.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,.maxLod=0};
    VkSampler sampler;CHECK(vkCreateSampler(device,&sci,NULL,&sampler));
    VkDescriptorSetLayoutBinding binding={.binding=0,.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount=1,.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutCreateInfo dsci={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,.bindingCount=1,.pBindings=&binding};
    VkDescriptorSetLayout ds_layout;CHECK(vkCreateDescriptorSetLayout(device,&dsci,NULL,&ds_layout));
    VkDescriptorPoolSize poolsize={VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1};
    VkDescriptorPoolCreateInfo dpci={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,.maxSets=1,.poolSizeCount=1,.pPoolSizes=&poolsize};
    VkDescriptorPool dpool;CHECK(vkCreateDescriptorPool(device,&dpci,NULL,&dpool));
    VkDescriptorSetAllocateInfo dsai={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,.descriptorPool=dpool,
      .descriptorSetCount=1,.pSetLayouts=&ds_layout};
    VkDescriptorSet ds;CHECK(vkAllocateDescriptorSets(device,&dsai,&ds));
    VkDescriptorImageInfo di={.sampler=sampler,.imageView=texture_view,.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wr={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=ds,.dstBinding=0,
      .descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,.pImageInfo=&di};
    vkUpdateDescriptorSets(device,1,&wr,0,NULL);
    VkAttachmentDescription ad={.format=imci.format,.samples=VK_SAMPLE_COUNT_1_BIT,
        .loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    
    VkAttachmentDescription attachments[2]={ad,{.format=VK_FORMAT_D16_UNORM,.samples=VK_SAMPLE_COUNT_1_BIT,
      .loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};
    VkAttachmentReference ar={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dr={1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp={.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount=1,.pColorAttachments=&ar,.pDepthStencilAttachment=&dr};
    VkRenderPassCreateInfo rpci={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount=2,.pAttachments=attachments,.subpassCount=1,.pSubpasses=&sp};
    VkRenderPass pass; CHECK(vkCreateRenderPass(device,&rpci,NULL,&pass));
    VkImageView views[2]={view,depth_view};
    VkFramebufferCreateInfo fbci={.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass=pass,.attachmentCount=2,.pAttachments=views,.width=64,.height=64,.layers=1};
    VkFramebuffer fb; CHECK(vkCreateFramebuffer(device,&fbci,NULL,&fb));
    VkPipelineLayoutCreateInfo plci={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,.setLayoutCount=1,.pSetLayouts=&ds_layout};
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
    
    VkPipelineDepthStencilStateCreateInfo depthstate={.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthCompareOp=VK_COMPARE_OP_LESS,.minDepthBounds=0,.maxDepthBounds=1};
    VkDynamicState dynamic=VK_DYNAMIC_STATE_SCISSOR;
    VkPipelineDynamicStateCreateInfo dyn={.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,.dynamicStateCount=1,.pDynamicStates=&dynamic};
    VkGraphicsPipelineCreateInfo pci={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount=2,.pStages=stages,.pVertexInputState=&vi,.pInputAssemblyState=&ia,.pViewportState=&vs,
        .pRasterizationState=&rs,.pMultisampleState=&ms,.pColorBlendState=&bs,.pDepthStencilState=&depthstate,.pDynamicState=&dyn,
        .layout=pl,.renderPass=pass,.subpass=0};
    
    VkPipeline pipelines[3];CHECK(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pci,NULL,&pipelines[0]));
    stages[1].module=shader(device,argv[3]);ba.blendEnable=VK_TRUE;
    ba.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;ba.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ba.colorBlendOp=VK_BLEND_OP_ADD;ba.srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;
    ba.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;ba.alphaBlendOp=VK_BLEND_OP_ADD;
    CHECK(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pci,NULL,&pipelines[1]));
    stages[1].module=shader(device,argv[4]);ba.blendEnable=VK_FALSE;
    depthstate.depthTestEnable=VK_TRUE;depthstate.depthWriteEnable=VK_TRUE;
    CHECK(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pci,NULL,&pipelines[2]));
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
    
    VkImageMemoryBarrier other[2]={barrier,barrier};
    other[0].image=depth;other[0].dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    other[0].newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;other[0].subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
    other[1].image=texture;other[1].srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;other[1].dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    other[1].newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_HOST_BIT|VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,NULL,0,NULL,2,other);
    VkClearValue clear[2]={ {.color={{64.0f/255.0f,64.0f/255.0f,64.0f/255.0f,1.0f}}}, {.depthStencil={1.0f,0}} };
    VkRenderPassBeginInfo rbi={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass=pass,.framebuffer=fb,.renderArea={{0,0},{64,64}},.clearValueCount=2,.pClearValues=clear};
    vkCmdBeginRenderPass(cb,&rbi,VK_SUBPASS_CONTENTS_INLINE);
    
    vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_GRAPHICS,pl,0,1,&ds,0,NULL);
    for(unsigned test=0;test<3;test++){
      VkRect2D clip={{test==0?0:test==1?16:32,0},{test==2?32:16,64}};
      vkCmdSetScissor(cb,0,1,&clip);vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[test]);
      vkCmdDraw(cb,3,1,0,0);
      if(test==2)vkCmdDraw(cb,3,1,0,1); /* farther red must fail LESS behind nearer green */
    }
    vkCmdEndRenderPass(cb); CHECK(vkEndCommandBuffer(cb));
    VkSubmitInfo si={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&cb};
    CHECK(vkQueueSubmit(queue,1,&si,VK_NULL_HANDLE));
    fprintf(stderr,"G35_OFFLINE_COMMAND_CAPTURE_COMPLETE; nativeExecution=false\n");
    fflush(NULL); _Exit(0);
}
