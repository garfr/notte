/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Dynamic render graph.
 */

#ifndef NOTTE_RENDER_GRAPH_H
#define NOTTE_RENDER_GRAPH_H

#include <notte/renderer_priv.h>

Err_Code RenderGraphInit(Renderer *ren, Render_Graph *graph);
void RenderGraphDeinit(Render_Graph *graph);
void RenderGraphRecord(Render_Graph *graph,
                  u32 imageIndex);

Render_Graph_Texture *RenderGraphGetSwapchainTexture(Render_Graph *graph);
Err_Code RenderGraphCreatePass(Render_Graph *graph, Render_Graph_Pass **passOut);
Err_Code RenderGraphWriteTexture(Render_Graph *graph, Render_Graph_Pass *pass,
    Render_Graph_Texture *tex);

Err_Code RenderGraphRebuild(Render_Graph *graph);

#endif /* NOTTE_RENDER_GRAPH_H */
