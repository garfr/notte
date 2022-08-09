/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Dynamic programmable material system.
 */

#ifndef NOTTE_MATERIAL_H
#define NOTTE_MATERIAL_H

#include <notte/renderer_priv.h>

Err_Code ShaderManagerInit(Renderer *ren, Shader_Manager *shaders);
Shader *ShaderManagerOpen(Renderer *ren, Shader_Manager *shaders, String path, 
    Shader_Type type);
void ShaderManagerDeinit(Renderer *ren, Shader_Manager *shaders);
Err_Code ShaderManagerReload(Renderer *ren, Shader_Manager *shaders);
Err_Code TechniqueManagerInit(Renderer *ren, Technique_Manager *techs);
Err_Code TechniqueManagerOpen(Renderer *ren, Technique_Manager *techs, 
    String name);
Technique *TechniqueManagerLookup(Technique_Manager *techs, String name);
void TechniqueManagerDeinit(Renderer *ren, Technique_Manager *techs);
Err_Code EffectManagerInit(Renderer *ren, Effect_Manager *effects);
void EffectManagerDeinit(Renderer *ren, Effect_Manager *effects);
Err_Code EffectManagerOpen(Renderer *ren, Effect_Manager *effects, String file);
Effect *EffectManagerLookup(Effect_Manager *effects, String name);
Err_Code MaterialManagerInit(Renderer *ren, Material_Manager *materials);
void MaterialManagerDeinit(Renderer *ren, Material_Manager *materials);
Err_Code MaterialManagerOpen(Renderer *ren, Material_Manager *materials, 
    String file);
Material *MaterialManagerLookup(Material_Manager *mats, String name);

#endif /* NOTTE_MATERIAL_H */
