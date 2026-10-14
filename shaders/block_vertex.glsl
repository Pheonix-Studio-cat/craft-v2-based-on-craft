uniform mat4 matrix;
uniform vec3 camera;
uniform float fog_distance;
uniform int ortho;
uniform float wave;          // seconds, drives water and swaying plants

attribute vec4 position;
attribute vec3 normal;
attribute vec4 uv;

varying vec2 fragment_uv;
varying float fragment_ao;
varying float fragment_light;
varying float fog_factor;
varying float fog_height;
varying float diffuse;
varying float fragment_shimmer;

const float pi = 3.14159265;
const vec3 light_direction = normalize(vec3(-1.0, 1.0, -1.0));

void main() {
    // Which tile a vertex uses says what it is. The rows are fixed by
    // tools/make_textures.py: row 4 column 0 is water, row 3 columns 0..6
    // are the plants.
    bool water = uv.x < 0.0626 && uv.y > 0.2499 && uv.y < 0.3126;
    bool plant = uv.x < 0.4376 && uv.y > 0.1874 && uv.y < 0.2501;

    vec4 pos = position;
    fragment_shimmer = 1.0;
    if (water) {
        if (normal.y > 0.5) {
            // the surface moves; the sides stay where they are
            pos.y += sin(wave * 1.7 + pos.x * 0.7 + pos.z * 0.9) * 0.07 - 0.07;
        }
        fragment_shimmer = 0.88 + 0.12 * sin(wave * 2.1 + pos.x * 0.6 + pos.z * 0.4);
    }
    else if (plant && fract(uv.y * 16.0) > 0.5) {
        // only the upper half of a blade of grass leans in the wind
        pos.x += sin(wave * 1.4 + pos.x * 0.6 + pos.z * 0.5) * 0.06;
        pos.z += cos(wave * 1.1 + pos.x * 0.5) * 0.04;
    }

    gl_Position = matrix * pos;
    fragment_uv = uv.xy;
    fragment_ao = 0.3 + (1.0 - uv.z) * 0.7;
    fragment_light = uv.w;
    diffuse = max(0.0, dot(normal, light_direction));
    if (bool(ortho)) {
        fog_factor = 0.0;
        fog_height = 0.0;
    }
    else {
        float camera_distance = distance(camera, vec3(pos));
        fog_factor = pow(clamp(camera_distance / fog_distance, 0.0, 1.0), 4.0);
        float dy = pos.y - camera.y;
        float dx = distance(pos.xz, camera.xz);
        fog_height = (atan(dy, dx) + pi / 2.0) / pi;
    }
}
