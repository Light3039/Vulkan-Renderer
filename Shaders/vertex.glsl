#version 450 core
#pragma shader_stage(vertex)

#include "scene_descriptors.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_tangent;
layout(location = 3) in vec2 in_uv;

layout(location = 0) out vec3 out_fragment_position;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 out_tangent_light_position;
layout(location = 3) out vec3 out_tangent_view_position;
layout(location = 4) out vec3 out_tangent_fragment_position;
layout(location = 5) out flat int out_instance_index;

void main() 
{
    vec3 light_position = vec3(40.0, 40.0, 2.0);

    ModelData model_data = ub_models.arr[gl_InstanceIndex];
    mat4 model = model_data.model;

    out_fragment_position = vec3(model * vec4(in_position, 1.0));
    out_uv = in_uv;

    mat3 normal_matrix = transpose(inverse(mat3(model)));
    vec3 T = normalize(normal_matrix * in_tangent);
    vec3 N = normalize(normal_matrix * in_normal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    mat3 TBN = transpose(mat3(T, B, N));    

    out_tangent_light_position = TBN * light_position;
    out_tangent_view_position = TBN * vec3(u_camera.view_position);
    out_tangent_fragment_position = TBN * out_fragment_position;

    out_instance_index = gl_InstanceIndex;

    gl_Position =  u_camera.proj * u_camera.view * model * vec4(in_position, 1.0);
}


