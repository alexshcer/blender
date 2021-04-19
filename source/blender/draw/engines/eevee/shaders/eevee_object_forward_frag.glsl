
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_object_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_culling_iter_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shader_shared.hh)

layout(std140) uniform lights_block
{
  LightData lights[CULLING_ITEM_BATCH];
};

layout(std140) uniform lights_culling_block
{
  CullingData light_culling;
};

uniform usampler2D lights_culling_tx;
uniform sampler2DArray utility_tx;

layout(location = 0, index = 0) out vec4 outRadiance;
layout(location = 0, index = 1) out vec4 outTransmittance;

MeshData g_surf;
ivec4 g_closure_data[8];

void main(void)
{
  g_surf = init_from_interp();

  /* Prevent precision issues on unit coordinates. */
  vec3 p = (g_surf.P + 0.000001) * 0.999999;
  int xi = int(abs(floor(p.x)));
  int yi = int(abs(floor(p.y)));
  int zi = int(abs(floor(p.z)));
  bool check = ((mod(xi, 2) == mod(yi, 2)) == bool(mod(zi, 2)));

  float vP_z = get_view_z_from_depth(gl_FragCoord.z);

  vec3 radiance = vec3(0);
  ITEM_FOREACH_BEGIN (light_culling, lights_culling_tx, vP_z, l_idx) {
    LightData light = lights[l_idx];
    vec3 L;
    float dist;
    light_vector_get(light, g_surf.P, L, dist);

    float intensity = light_diffuse(utility_tx, light, g_surf.N, cameraVec(g_surf.P), L, dist) *
                      light.diffuse_power;
    radiance += light.color * intensity * light_attenuation(light, L, dist);
  }
  ITEM_FOREACH_END

  outRadiance = vec4(radiance * mix(0.2, 0.8, check), 1.0);
  outTransmittance = vec4(0.0, 0.0, 0.0, 1.0);
}