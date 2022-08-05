/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Dynamic render graph.
 */

#ifndef NOTTE_RENDER_GRAPH_H
#define NOTTE_RENDER_GRAPH_H

#include <notte/renderer_priv.h>

Err_Code RenderGraphInit(Renderer *ren, Render_Graph *graph);
void RenderGraphDeinit(Renderer *ren, Render_Graph *graph);
void RenderGraphRecord(Renderer *ren, Render_Graph *graph,
                  u32 imageIndex);
Err_Code RenderGraphRebuild(Renderer *ren, Render_Graph *graph);

#endif /* NOTTE_RENDER_GRAPH_H */
