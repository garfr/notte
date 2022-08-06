/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Dynamic render graph.
 */

#include <notte/render_graph.h>
#include <notte/material.h>

/* === CONSTANTS === */

Vec3 X_AXIS = {1.0f, 0.0f, 0.0f};
Vec3 Y_AXIS = {0.0f, 1.0f, 0.0f};
Vec3 Z_AXIS = {0.0f, 0.0f, 1.0f};

/* === PROTOTYPES === */

static Err_Code CreateSwapchainFramebuffers(Renderer *ren, Render_Graph *graph);
static void TransformToMatrix(Transform trans, Mat4 out);

/* === PUBLIC FUNCTIONS === */

Err_Code 
RenderGraphInit(Renderer *ren, 
                Render_Graph *graph)
{
  Err_Code err;
  VkResult vkErr;

  err = CreateSwapchainFramebuffers(ren, graph);
  if (err)
  {
    return err;
  }

  VkCommandPoolCreateInfo poolInfo =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = ren->queueInfo.graphicsFamily,
  };

  vkErr = vkCreateCommandPool(ren->dev, &poolInfo, ren->allocCbs, 
      &graph->commandPool);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  VkCommandBufferAllocateInfo allocInfo =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = graph->commandPool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
  };

  vkErr = vkAllocateCommandBuffers(ren->dev, &allocInfo, graph->commandBuffers);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  VkSemaphoreCreateInfo semaphoreInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  VkFenceCreateInfo fenceInfo =
  {
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
  };


  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    vkErr = vkCreateSemaphore(ren->dev, &semaphoreInfo, ren->allocCbs, 
        &graph->imageAvailableSemaphores[i]);
    vkErr = vkCreateSemaphore(ren->dev, &semaphoreInfo, ren->allocCbs, 
        &graph->renderFinishedSemaphores[i]);
    vkErr = vkCreateFence(ren->dev, &fenceInfo, ren->allocCbs, 
        &graph->inFlightFences[i]);
    if (vkErr)
    {
      return ERR_LIBRARY_FAILURE;
    }
  }

  return ERR_OK;
}

void 
RenderGraphDeinit(Renderer *ren, 
                  Render_Graph *graph)
{

  vkDestroyCommandPool(ren->dev, graph->commandPool, ren->allocCbs);

  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    vkDestroySemaphore(ren->dev, graph->imageAvailableSemaphores[i], 
        ren->allocCbs);
    vkDestroySemaphore(ren->dev, graph->renderFinishedSemaphores[i], 
        ren->allocCbs);
    vkDestroyFence(ren->dev, graph->inFlightFences[i], ren->allocCbs);
  }

  for (usize i = 0; i < ren->swapchain.nImages; i++)
  {
    vkDestroyFramebuffer(ren->dev, ren->graph.swapFbs[i], ren->allocCbs);
  }

  FREE_ARR(ren->alloc, ren->graph.swapFbs, VkFramebuffer, ren->swapchain.nImages, 
      MEMORY_TAG_RENDERER);
}

void 
RenderGraphRecord(Renderer *ren, 
                  Render_Graph *graph,
                  u32 imageIndex)
{
  Technique *tech = TechniqueManagerLookup(&ren->techs, STRING_CSTR("tri"));
  VkCommandBuffer buf = graph->commandBuffers[ren->currentFrame];

  VkResult vkErr;

  VkCommandBufferBeginInfo beginInfo =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };

  vkErr = vkBeginCommandBuffer(buf, &beginInfo);
  if (vkErr)
  {
    LOG_ERROR("failed to begin command buffer");
    return;
  }

  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

  VkRenderPassBeginInfo renderPassInfo =
  {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = tech->fakePass,
    .framebuffer = graph->swapFbs[imageIndex],
    .renderArea =
      {
        .offset = {0, 0},
        .extent = ren->swapchain.extent,
      },
    .clearValueCount = 1,
    .pClearValues = &clearColor,
  };

  vkCmdBeginRenderPass(buf, &renderPassInfo, 
      VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, 
      tech->pipeline);

  VkViewport viewport =
  {
    .x = 0.0f,
    .y = 0.0f,
    .width = (float) ren->swapchain.extent.width,
    .height = (float) ren->swapchain.extent.height,
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  vkCmdSetViewport(buf, 0, 1, &viewport);

  VkRect2D scissor =
  {
    .offset = {0, 0},
    .extent = ren->swapchain.extent,
  };

  vkCmdSetScissor(buf, 0, 1, &scissor);

  if (ren->cam == NULL)
  {
    goto skipDraw;
  }

  Camera_Uniform camUniform;
  Mat4Copy(ren->cam->view, camUniform.view);
  Mat4Copy(ren->cam->proj, camUniform.proj);

  void *data;
  vkMapMemory(ren->dev, ren->uniformMemory[ren->currentFrame], 0, 
      sizeof(Camera_Uniform), 0, &data);
  MemoryCopy(data, &camUniform, sizeof(Camera_Uniform));
  vkUnmapMemory(ren->dev, ren->uniformMemory[ren->currentFrame]);

  for (usize i = 0; i < ren->drawCalls.elemsUsed; i++)
  {
    Draw_Call *call = VectorIdx(&ren->drawCalls, i);

    switch (call->t)
    {
      case DRAW_CALL_STATIC_MESH:
      {
        Static_Mesh *mesh = call->staticMesh;
        Mesh_Push_Constant meshConstants;
        TransformToMatrix(call->transform, meshConstants.model);

        VkBuffer vertexBuffers[] = {mesh->vertexBuffer};
        VkDeviceSize offsets[] = {0};

        vkCmdBindVertexBuffers(buf, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(buf, mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, 
            tech->layout, 0, 1, &tech->descriptorSets[ren->currentFrame], 0, 
            NULL);
        vkCmdPushConstants(buf, tech->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 
            sizeof(Mesh_Push_Constant), &meshConstants);
        vkCmdDrawIndexed(buf, mesh->nIndices, 1, 0, 0, 0);
      }
    }
  }

skipDraw:

  VectorEmpty(&ren->drawCalls);

  vkCmdEndRenderPass(buf);

  vkEndCommandBuffer(buf);
}

Err_Code
RenderGraphRebuild(Renderer *ren, Render_Graph *graph)
{
  Err_Code err;

  for (usize i = 0; i < ren->swapchain.nImages; i++)
  {
    vkDestroyFramebuffer(ren->dev, graph->swapFbs[i], ren->allocCbs);
  }

  FREE_ARR(ren->alloc, graph->swapFbs, VkFramebuffer, ren->swapchain.nImages, 
      MEMORY_TAG_RENDERER);

  err = CreateSwapchainFramebuffers(ren, graph);
  if (err)
  {
    return err;
  }
  return ERR_OK;
}

/* === PRIVATE FUNCTIONS === */

static Err_Code 
CreateSwapchainFramebuffers(Renderer *ren, Render_Graph *graph)
{
  VkResult vkErr;
  Technique *tech;
  graph->swapFbs = NEW_ARR(ren->alloc, VkFramebuffer, ren->swapchain.nImages, 
      MEMORY_TAG_RENDERER);

  tech = TechniqueManagerLookup(&ren->techs, STRING_CSTR("tri"));
  for (usize i = 0; i < ren->swapchain.nImages; i++)
  {
    VkImageView attachments[] =
    {
      ren->swapchain.imageViews[i],
    };

    VkFramebufferCreateInfo framebufferInfo = 
    {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = tech->fakePass,
      .attachmentCount = 1,
      .pAttachments = attachments,
      .width = ren->swapchain.extent.width,
      .height = ren->swapchain.extent.height,
      .layers = 1,
    };

    vkErr = vkCreateFramebuffer(ren->dev, &framebufferInfo, ren->allocCbs, 
        &graph->swapFbs[i]);
    if (vkErr)
    {
      return ERR_LIBRARY_FAILURE;
    }
  }

  return ERR_OK;
}

static void
TransformToMatrix(Transform trans, Mat4 out)
{
  Mat4Identity(out);
  Mat4Translate(out, trans.pos, out);
  Mat4Rotate(out, DegToRad(trans.rot[0]), X_AXIS, out);
  Mat4Rotate(out, DegToRad(trans.rot[1]), Y_AXIS, out);
  Mat4Rotate(out, DegToRad(trans.rot[2]), Z_AXIS, out);
  return;
}
