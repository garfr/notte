/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Dynamic render graph.
 */

#include <notte/render_graph.h>
#include <notte/material.h>

/* === TYPES === */

/* === CONSTANTS === */

#define MARK_NONE 0
#define MARK_TEMP 1
#define MARK_PERM 2

/* === PROTOTYPES === */

static Err_Code CreateSwapchainFramebuffers(Render_Graph *graph);
static Err_Code Rebake(Render_Graph *graph);
static Err_Code TopoSortVisit(Render_Graph *graph, Render_Graph_Pass *pass);

/* === PUBLIC FUNCTIONS === */

Err_Code 
RenderGraphInit(Renderer *ren, 
                Render_Graph *graph)
{
  Err_Code err;
  VkResult vkErr;

  graph->swapFbs = NEW_ARR(ren->alloc, VkFramebuffer, ren->swapchain.nImages, 
      MEMORY_TAG_RENDERER);

  graph->ren = ren;

  err = CreateSwapchainFramebuffers(graph);
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

  /* 
   * We don't need to initialize the fbs in graph->swap, because we will use 
   * graph->swapFbs. 
   */
  graph->swap.isSwapchain = true;

  graph->passes = VECTOR_CREATE(graph->ren->alloc, Render_Graph_Pass);
  graph->bakedPasses = VECTOR_CREATE(graph->ren->alloc, Render_Graph_Pass *);

  return ERR_OK;
}

void 
RenderGraphDeinit(Render_Graph *graph)
{
  Renderer *ren = graph->ren;

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

  for (usize i = 0; i < graph->passes.elemsUsed; i++)
  {
    Render_Graph_Pass *pass = VectorIdx(&graph->passes, i);
    VectorDestroy(&pass->reads, graph->ren->alloc);
    VectorDestroy(&pass->writes, graph->ren->alloc);
  }

  VectorDestroy(&graph->passes, graph->ren->alloc);
  VectorDestroy(&graph->bakedPasses, graph->ren->alloc);

  FREE_ARR(ren->alloc, ren->graph.swapFbs, VkFramebuffer, ren->swapchain.nImages, 
      MEMORY_TAG_RENDERER);
}


Err_Code 
RenderGraphWriteTexture(Render_Graph *graph, 
                        Render_Graph_Pass *pass,
                        Render_Graph_Texture *tex)
{
  VectorPush(&pass->writes, graph->ren->alloc, &tex);

  Rebake(graph);
  return ERR_OK;
}

void 
RenderGraphRecord(Render_Graph *graph,
                  u32 imageIndex)
{
  Renderer *ren = graph->ren;
  Technique *tech = TechniqueManagerLookup(&ren->techs, STRING_CSTR("tri"));
  VkCommandBuffer buf = graph->commandBuffers[ren->currentFrame];

  VkResult vkErr;

  for (usize i = 0; i < graph->bakedPasses.elemsUsed; i++)
  {
    Render_Graph_Pass *pass = *((Render_Graph_Pass **) 
        VectorIdx(&graph->bakedPasses, i));

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

    VkClearValue clearValues[2] = {
      {{{0.0f, 0.0f, 0.0f, 1.0f}}},
      {1.0f, 0},
    };

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
      .clearValueCount = 2,
      .pClearValues = clearValues,
    };

    vkCmdBeginRenderPass(buf, &renderPassInfo, 
        VK_SUBPASS_CONTENTS_INLINE);

    pass->fn(graph->ren, buf);

    vkCmdEndRenderPass(buf);

    vkEndCommandBuffer(buf);
  }
}

Err_Code
RenderGraphRebuild(Render_Graph *graph)
{
  Err_Code err;
  Renderer *ren = graph->ren;

  for (usize i = 0; i < ren->swapchain.nImages; i++)
  {
    vkDestroyFramebuffer(ren->dev, graph->swapFbs[i], ren->allocCbs);
  }
  err = CreateSwapchainFramebuffers(graph);
  LOG_DEBUG("here");
  if (err)
  {
    return err;
  }
  return ERR_OK;
}

Render_Graph_Texture *
RenderGraphGetSwapchainTexture(Render_Graph *graph)
{
  return &graph->swap;
}

Err_Code 
RenderGraphCreatePass(Render_Graph *graph, 
                      Render_Graph_Pass **passOut)
{
  Render_Graph_Pass pass = 
  {
    .writes = VECTOR_CREATE(graph->ren->alloc, Render_Graph_Texture *),
    .reads = VECTOR_CREATE(graph->ren->alloc, Render_Graph_Texture *),
  };

  *passOut = VectorPush(&graph->passes, graph->ren->alloc, &pass);
  return ERR_OK;
}

/* === PRIVATE FUNCTIONS === */

static Err_Code 
CreateSwapchainFramebuffers(Render_Graph *graph)
{
  VkResult vkErr;
  Technique *tech;
  Renderer *ren = graph->ren;

  tech = TechniqueManagerLookup(&ren->techs, STRING_CSTR("tri"));
  for (usize i = 0; i < ren->swapchain.nImages; i++)
  {
    VkImageView attachments[] =
    {
      ren->swapchain.imageViews[i],
      ren->depthView,
    };

    VkFramebufferCreateInfo framebufferInfo = 
    {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = tech->fakePass,
      .attachmentCount = 2,
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

static Err_Code 
Rebake(Render_Graph *graph)
{
  graph->bakedPasses.elemsUsed = 0;

  for (usize i = 0; i < graph->passes.elemsUsed; i++)
  {
    Render_Graph_Pass *pass = VectorIdx(&graph->passes, i);
    pass->mark = MARK_NONE;
  }

  for (usize i = 0; i < graph->passes.elemsUsed; i++)
  {
    Render_Graph_Pass *pass = VectorIdx(&graph->passes, i);
    TopoSortVisit(graph, pass);
  }

  for (usize i = 0; i < graph->bakedPasses.elemsUsed; i++)
  {
    Render_Graph_Pass *pass = *((Render_Graph_Pass **) 
        VectorIdx(&graph->bakedPasses, i));
  }

  return ERR_OK;
}

static Err_Code
TopoSortVisit(Render_Graph *graph, Render_Graph_Pass *pass)
{
  Err_Code err;

  if (pass->mark == MARK_PERM)
  {
    return ERR_OK;
  }
  if (pass->mark == MARK_TEMP)
  {
    return ERR_CYCLICAL_RENDER_GRAPH;
  }

  pass->mark = MARK_TEMP;

  for (usize i = 0; i < pass->reads.elemsUsed; i++)
  {
    Render_Graph_Texture *read = *((Render_Graph_Texture **) 
        VectorIdx(&pass->reads, i));

    for (usize j = 0; j < graph->passes.elemsUsed; j++)
    {
      Render_Graph_Pass *tmpPass = VectorIdx(&graph->passes, j);
      for (usize k = 0; k < tmpPass->writes.elemsUsed; k++)
      {
        Render_Graph_Texture *write = *((Render_Graph_Texture **) 
            VectorIdx(&tmpPass->writes, k));
        if (write == read)
        {
          err = TopoSortVisit(graph, tmpPass);
          if (err)
          {
            return err;
          }
        }
      }
      
    }
  }

  pass->mark = MARK_PERM;
  VectorPush(&graph->bakedPasses, graph->ren->alloc, &pass);
  return ERR_OK;
}
