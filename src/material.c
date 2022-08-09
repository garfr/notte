/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Dynamic programmable material system.
 */

#include <notte/material.h>
#include <notte/bson.h>

/* === GLOBALS === */

VkVertexInputBindingDescription vertexBindingDescription =
{
  .binding = 0,
  .stride = sizeof(Static_Vert),
  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
};

VkVertexInputAttributeDescription vertexAttributeDescription[] =
{
  {
    .binding = 0,
    .location = 0,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = OFFSETOF(Static_Vert, pos),
  },
  {
    .binding = 0,
    .location = 1,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = OFFSETOF(Static_Vert, nor),
  },
  {
    .binding = 0,
    .location = 2,
    .format = VK_FORMAT_R32G32_SFLOAT,
    .offset = OFFSETOF(Static_Vert, tex),
  },
};

/* === PROTOTYPES === */

static void TechDestroy(void *ud, String name, void *ptr);
static void ShaderDestroy(void *ud, String name, void *item);
static void ShaderMonitorEvent(void *ud, String root, String path);
static Err_Code ShaderRebuild(Renderer *ren, Shader_Manager *shaders, 
    Shader *shader);
static Err_Code ShaderInit(Renderer *ren, Shader_Manager *shaders,
    Shader *shader);
static Err_Code TechniqueInit(Renderer *ren, Technique *tech);

/* === PUBLIC FUNCTIONS === */

Err_Code 
ShaderManagerInit(Renderer *ren, 
                  Shader_Manager *shaders)
{
  Err_Code err;

  shaders->dict = DICT_CREATE(ren->alloc, Shader, false);
  shaders->compiler = shaderc_compiler_initialize();
  shaders->fs = ren->fs;
  err = FsDirMonitorCreate(ren->alloc, STRING_CSTR("../shaders/"), 
      &shaders->monitor);
  if (err)
  {
    return err;
  }
  return ERR_OK;
}

void 
ShaderManagerDeinit(Renderer *ren, 
                   Shader_Manager *shaders)
{
  FsDirMonitorDestroy(shaders->monitor);
  DictDestroyWithDestructor(shaders->dict, ren, ShaderDestroy);
  shaderc_compiler_release(shaders->compiler);
}

Err_Code
ShaderManagerReload(Renderer *ren, Shader_Manager *shaders)
{
  usize nEvents;
  Fs_Dir_Monitor_Event *events;
  Err_Code err;

  events = FsDirMonitorGetEvents(shaders->monitor, &nEvents);
  for (usize i = 0; i < nEvents; i++)
  {
    Shader *shader = DictFind(shaders->dict, events[i].path);
    if (shader != NULL)
    {
      err = ShaderRebuild(ren, shaders, shader);
      if (err)
      {
        return err;
      }
      LOG_DEBUG_FMT("rebuilt shader '%.*s'", shader->name.len, 
          shader->name.buf);
    }
  }
  return ERR_OK;
}

Shader *
ShaderManagerOpen(Renderer *ren, 
                  Shader_Manager *shaders, 
                  String name, 
                  Shader_Type type)
{
  Err_Code err;
  String path;
  Shader *shader, *iter;

  shader = DictFind(shaders->dict, name);
  if (shader != NULL)
  {
    return shader;
  }

  shader = DictInsertWithoutInit(shaders->dict, name);

  shader->name = StringClone(ren->alloc, name);
  shader->type = type;

  err = ShaderInit(ren, shaders, shader);
  if (err)
  {
    return NULL;
  }

  return shader;

fail:
  StringDestroy(ren->alloc, path);
  return NULL;
}

Err_Code 
TechniqueManagerInit(Renderer *ren, 
                     Technique_Manager *techs)
{
  techs->dict = DICT_CREATE(ren->alloc, Technique, false);
  return ERR_OK;
}

void 
TechniqueManagerDeinit(Renderer *ren, 
                       Technique_Manager *techs)
{
  DictDestroyWithDestructor(techs->dict, ren, TechDestroy);
}

Err_Code
TechniqueManagerOpen(Renderer *ren, 
                     Technique_Manager *techs, 
                     String name)
{
  Err_Code err;
  Membuf buf;
  Parse_Result result;
  Bson_Ast *ast;
  Bson_Dict_Iterator iter;
  Bson_Value *globals, *techDict;
  String techName, path;

  path = StringConcat(ren->alloc, STRING_CSTR("assets/"), name);

  err = FsFileLoad(ren->fs, path, &buf);
  if (err)
  {
    goto fail;
  }

  err = BsonAstParse(&ast, ren->alloc, &result, buf);
  if (err)
  {
    goto fail;
  }

  globals = BsonAstGetValue(ast);

  BsonDictIteratorCreate(globals, &iter);
  while (BsonDictIteratorNext(&iter, &techName, &techDict))
  {
    Technique *tech;

    tech = DictInsertWithoutInit(techs->dict, techName);

    String vertStr, fragStr;
    Bson_Value *vertName, *fragName;
    vertName = BsonValueLookup(techDict, STRING_CSTR("vert"));
    if (vertName == NULL)
    {
      LOG_DEBUG_FMT("failed to find vert shader in '%.*s'", 
          (int) techName.len, techName.buf);
      err = ERR_FAILED_PARSE;
      goto fail;
    }
    fragName = BsonValueLookup(techDict, STRING_CSTR("frag"));
    if (fragName == NULL)
    {
      LOG_DEBUG_FMT("failed to find frag shader in '%.*s'", 
          (int) techName.len, techName.buf);
      err = ERR_FAILED_PARSE;
      goto fail;
    }

    vertStr = BsonValueGetString(vertName);
    fragStr = BsonValueGetString(fragName);

    Shader *vertShader, *fragShader;
    vertShader = ShaderManagerOpen(ren, &ren->shaders, vertStr, SHADER_VERT);
    fragShader = ShaderManagerOpen(ren, &ren->shaders, fragStr, SHADER_FRAG);

    tech->vert = vertShader;
    tech->frag = fragShader;

    err = TechniqueInit(ren, tech);
    if (err)
    {
      return err;
    }

    LOG_DEBUG_FMT("loaded technique '%.*s'", (int) techName.len, techName.buf);
  }
  
  BsonAstDestroy(ast, ren->alloc);
  FsFileDestroy(ren->fs, &buf);
  StringDestroy(ren->alloc, path);
  return ERR_OK;

fail:
  StringDestroy(ren->alloc, path);
  return err;
}


Technique *
TechniqueManagerLookup(Technique_Manager *techs, 
                       String name)
{
  return DictFind(techs->dict, name);
}

Err_Code 
EffectManagerInit(Renderer *ren, 
                  Effect_Manager *effects)
{
  effects->dict = DICT_CREATE(ren->alloc, Effect, false);

  return ERR_OK;
}

void 
EffectManagerDeinit(Renderer *ren, 
                    Effect_Manager *effects)
{
  DictDestroy(effects->dict);
}

Err_Code 
MaterialManagerInit(Renderer *ren, 
                    Material_Manager *materials)
{
  materials->dict = DICT_CREATE(ren->alloc, Material, false);

  return ERR_OK;
}

Err_Code 
EffectManagerOpen(Renderer *ren, 
                  Effect_Manager *effects, 
                  String name)
{
  Err_Code err;
  Membuf buf;
  Parse_Result result;
  Bson_Ast *ast;
  Bson_Dict_Iterator iter;
  Bson_Value *globals, *effectDict;
  String effectName, path;

  path = StringConcat(ren->alloc, STRING_CSTR("assets/"), name);

  err = FsFileLoad(ren->fs, path, &buf);
  if (err)
  {
    return err;
  }

  err = BsonAstParse(&ast, ren->alloc, &result, buf);
  if (err)
  {
    return err;
  }

  globals = BsonAstGetValue(ast);

  BsonDictIteratorCreate(globals, &iter);
  while (BsonDictIteratorNext(&iter, &effectName, &effectDict))
  {
    Effect *effect;

    effect = DictInsertWithoutInit(effects->dict, effectName);

    Bson_Value *gbuffer = BsonValueLookup(effectDict, STRING_CSTR("gbuffer"));
    String gbufferName = BsonValueGetString(gbuffer);

    Technique *gbufferTech = TechniqueManagerLookup(&ren->techs, gbufferName);
    effect->techs[RENDER_PASS_GBUFFER] = gbufferTech;

    LOG_DEBUG_FMT("loaded effect '%.*s'", (int) effectName.len, effectName.buf);
  }
  BsonAstDestroy(ast, ren->alloc);
  FsFileDestroy(ren->fs, &buf);
  StringDestroy(ren->alloc, path);

  return ERR_OK;
}

Effect *
EffectManagerLookup(Effect_Manager *effects, 
                    String name)
{
  return DictFind(effects->dict, name);
}

void 
MaterialManagerDeinit(Renderer *ren, 
                      Material_Manager *materials)
{
  DictDestroy(materials->dict);
}

Err_Code 
MaterialManagerOpen(Renderer *ren, 
                    Material_Manager *materials, 
                    String name)
{
  Err_Code err;
  Membuf buf;
  Parse_Result result;
  Bson_Ast *ast;
  Bson_Dict_Iterator iter;
  Bson_Value *globals, *materialDict;
  String materialName, path;

  path = StringConcat(ren->alloc, STRING_CSTR("assets/"), name);

  err = FsFileLoad(ren->fs, path, &buf);
  if (err)
  {
    return err;
  }

  err = BsonAstParse(&ast, ren->alloc, &result, buf);
  if (err)
  {
    return err;
  }

  globals = BsonAstGetValue(ast);

  BsonDictIteratorCreate(globals, &iter);
  while (BsonDictIteratorNext(&iter, &materialName, &materialDict))
  {
    Material *material;

    material = DictInsertWithoutInit(materials->dict, materialName);

    Bson_Value *effectBson = BsonValueLookup(materialDict, STRING_CSTR("effect"));
    String effectName = BsonValueGetString(effectBson);

    Effect *effect = EffectManagerLookup(&ren->effects, effectName);
    material->effect = effect;

    LOG_DEBUG_FMT("loaded material '%.*s'", (int) effectName.len, effectName.buf);
  }

  BsonAstDestroy(ast, ren->alloc);
  FsFileDestroy(ren->fs, &buf);
  StringDestroy(ren->alloc, path);

  return ERR_OK;

}

Material *
MaterialManagerLookup(Material_Manager *mats, 
                      String name)
{
  return DictFind(mats->dict, name);
}

/* === PRIVATE_FUNCTIONS === */

static void
ShaderDestroy(void *ud, String name, void *item)
{
  Renderer *ren = (Renderer *) ud;
  Shader *shader = (Shader *) item;
  vkDestroyShaderModule(ren->dev, shader->mod, ren->allocCbs);
  StringDestroy(ren->alloc, shader->name);
}

static void
TechDestroy(void *ud, 
            String name,
            void *ptr)
{
  Renderer *ren = (Renderer *) ud;
  Technique *tech = (Technique *) ptr;

  vkDestroyDescriptorSetLayout(ren->dev, tech->descriptorLayout, ren->allocCbs);
  vkDestroyPipeline(ren->dev, tech->pipeline, ren->allocCbs);
  vkDestroyPipelineLayout(ren->dev, tech->layout, ren->allocCbs);
  vkDestroyRenderPass(ren->dev, tech->fakePass, ren->allocCbs);
}

static Err_Code
ShaderRebuild(Renderer *ren, 
              Shader_Manager *shaders, 
              Shader *shader)
{
  Dict_Iterator iter;
  String techName;
  Technique *tech;
  Err_Code err;

  vkDeviceWaitIdle(ren->dev);

  vkDestroyShaderModule(ren->dev, shader->mod, ren->allocCbs);
  err = ShaderInit(ren, shaders, shader);
  if (err)
  {
    return err;
  }

  DictIteratorInit(ren->techs.dict, &iter);
  while (DictIteratorNext(&iter, &techName, &tech))
  {
    if (tech->vert == shader || tech->frag == shader)
    {
      TechDestroy(ren, techName, tech);
      TechniqueInit(ren, tech);
    }
  }

  return ERR_OK;
}

static Err_Code
ShaderInit(Renderer *ren, 
                 Shader_Manager *shaders,
                 Shader *shader)
{
  String path;
  String name = shader->name;
  Err_Code err;
  Membuf buf;
  VkResult vkErr;

  path = StringConcat(ren->alloc, STRING_CSTR("shaders/"), name);

  err = FsFileLoad(shaders->fs, path, &buf);
  if (err)
  {
    return err;
  }

  shaderc_compilation_result_t result = 
    shaderc_compile_into_spv(shaders->compiler, buf.data, buf.size, 
        shader->type == SHADER_VERT ? shaderc_glsl_vertex_shader 
                            : shaderc_glsl_fragment_shader, 
        name.buf, "main", NULL);

  if (shaderc_result_get_compilation_status(result))
  {
    LOG_DEBUG_FMT("failed to compile shader: \n\n%s", 
        shaderc_result_get_error_message(result));
    return ERR_INVALID_SHADER;
  }

  shader->name = name;

  VkShaderModuleCreateInfo createInfo =
  {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = shaderc_result_get_length(result),
    .pCode = (u32 *) shaderc_result_get_bytes(result),
  };

  vkErr = vkCreateShaderModule(ren->dev, &createInfo, ren->allocCbs, 
      &shader->mod);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  shaderc_result_release(result);
  FsFileDestroy(shaders->fs, &buf);

  StringDestroy(ren->alloc, path);
  return ERR_OK;
}

static Err_Code 
TechniqueInit(Renderer *ren, 
              Technique *tech)
{
  VkResult vkErr;
  VkDescriptorSetLayout descriptorLayouts[MAX_FRAMES_IN_FLIGHT];

  VkDescriptorSetLayoutBinding uboBinding = 
  {
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
  };

  VkDescriptorSetLayoutBinding samplerBinding = 
  {
    .binding = 1,
    .descriptorCount = 1,
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
  };

  VkDescriptorSetLayoutBinding bindings[2] = {
    uboBinding, samplerBinding,
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo = 
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 2,
    .pBindings = bindings,
  };

  vkErr = vkCreateDescriptorSetLayout(ren->dev, &layoutInfo, ren->allocCbs, 
      &tech->descriptorLayout);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    descriptorLayouts[i] = tech->descriptorLayout;
  }

  VkDescriptorSetAllocateInfo descriptorSetInfo = 
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = ren->descriptorPool,
    .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
    .pSetLayouts = descriptorLayouts,
  };

  vkErr = vkAllocateDescriptorSets(ren->dev, &descriptorSetInfo, 
      tech->descriptorSets);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }
  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    VkDescriptorBufferInfo bufferInfo = 
    {
      .buffer = ren->uniformBuffers[i],
      .offset = 0,
      .range = sizeof(Camera_Uniform),
    };

    VkDescriptorImageInfo imageInfo = 
    {
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .imageView = ren->textureView,
      .sampler = ren->textureSampler,
    };

    VkWriteDescriptorSet descriptorWrites[2] =
    {
      {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = tech->descriptorSets[i],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .pBufferInfo = &bufferInfo,
      },
      {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = tech->descriptorSets[i],
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo = &imageInfo,
      },
    };

    vkUpdateDescriptorSets(ren->dev, 2, descriptorWrites, 0, NULL);
  }

  VkDynamicState dynamicStates[] =
  {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamicState =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = 2,
    .pDynamicStates = dynamicStates,
  };

  VkPipelineVertexInputStateCreateInfo vertexInputInfo =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 1,
    .vertexAttributeDescriptionCount = 3,
    .pVertexBindingDescriptions = &vertexBindingDescription,
    .pVertexAttributeDescriptions = vertexAttributeDescription,
  };

  VkPipelineInputAssemblyStateCreateInfo inputAssembly =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };

  VkViewport viewport =
  {
    .x = 0.0f,
    .y = 0.0f,
    .width = (float) ren->swapchain.extent.width,
    .height = (float) ren->swapchain.extent.height,
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  VkRect2D scissor =
  {
    .offset = {0, 0},
    .extent = ren->swapchain.extent,
  };

  VkPipelineViewportStateCreateInfo viewportState =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .lineWidth = 1.0f,
    //.cullMode = VK_CULL_MODE_BACK_BIT,
    .cullMode = VK_CULL_MODE_NONE,
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
  };

  VkPipelineMultisampleStateCreateInfo multisampler =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .sampleShadingEnable = VK_FALSE,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  VkPipelineColorBlendAttachmentState colorBlendAttachment =
  {

    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
      | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    .blendEnable = VK_FALSE,
  };

  VkPipelineColorBlendStateCreateInfo colorBlend =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    .attachmentCount = 1,
    .pAttachments = &colorBlendAttachment,
  };

  VkPushConstantRange pushConstant = 
  {
    .offset = 0,
    .size = sizeof(Mesh_Push_Constant),
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
  };

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = 
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &tech->descriptorLayout,
    .pushConstantRangeCount = 1,
    .pPushConstantRanges = &pushConstant,
  };

  vkErr = vkCreatePipelineLayout(ren->dev, &pipelineLayoutInfo, ren->allocCbs, 
      &tech->layout);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  VkAttachmentDescription colorAttachment =
  {
    .format = ren->swapchain.format.format,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference colorAttachmentRef =
  {
    .attachment = 0,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkAttachmentDescription depthAttachment = 
  {
    .format = VK_FORMAT_D32_SFLOAT,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  VkAttachmentReference depthAttachmentRef = 
  {
    .attachment = 1,
    .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass =
  {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments = &colorAttachmentRef,
    .pDepthStencilAttachment = &depthAttachmentRef,
  };

  VkAttachmentDescription attachments[2] = {colorAttachment, depthAttachment};

  VkSubpassDependency dependency =
  {
    .srcSubpass = VK_SUBPASS_EXTERNAL,
    .dstSubpass = 0,
    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | 
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    .srcAccessMask = 0,
    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | 
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
  };

  VkPipelineDepthStencilStateCreateInfo depthStencil = 
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_TRUE,
    .depthCompareOp = VK_COMPARE_OP_LESS,
    .depthBoundsTestEnable = VK_FALSE,
    .stencilTestEnable = VK_FALSE,
  };

  VkRenderPassCreateInfo renderPassInfo =
  {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 2,
    .pAttachments = attachments,
    .subpassCount = 1,
    .pSubpasses = &subpass,
    .dependencyCount = 1,
    .pDependencies = &dependency,
  };

  vkErr = vkCreateRenderPass(ren->dev, &renderPassInfo, ren->allocCbs, 
      &tech->fakePass);
  if (vkErr)
  {
    return ERR_OK;
  }

  VkPipelineShaderStageCreateInfo vertShaderStageInfo =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module = tech->vert->mod,
    .pName = "main",
  };

  VkPipelineShaderStageCreateInfo fragShaderStageInfo =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = tech->frag->mod,
    .pName = "main",
  };

  VkPipelineShaderStageCreateInfo shaderStages[] = 
  {
    vertShaderStageInfo, fragShaderStageInfo
  };

  VkGraphicsPipelineCreateInfo pipelineInfo =
  {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2,
    .pStages = shaderStages,
    .pVertexInputState = &vertexInputInfo,
    .pInputAssemblyState = &inputAssembly,
    .pViewportState = &viewportState,
    .pRasterizationState = &rasterizer,
    .pMultisampleState = &multisampler,
    .pColorBlendState = &colorBlend,
    .pDepthStencilState = &depthStencil,
    .pDynamicState = &dynamicState,
    .layout = tech->layout,
    .renderPass = tech->fakePass,
    .subpass = 0,
  };

  vkErr = vkCreateGraphicsPipelines(ren->dev, VK_NULL_HANDLE, 1, &pipelineInfo,
      ren->allocCbs, &tech->pipeline);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  return ERR_OK;
}

