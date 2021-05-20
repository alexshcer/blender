
void node_bsdf_glass(vec4 color,
                     float roughness,
                     float ior,
                     vec3 N,
                     const float do_multiscatter,
                     const float ssr_id,
                     out Closure result)
{
  g_reflection_data.color = color.rgb;
  g_reflection_data.N = N;
  g_reflection_data.roughness = roughness;

  g_refraction_data.color = color.rgb;
  g_refraction_data.N = N;
  g_refraction_data.roughness = roughness;
}
