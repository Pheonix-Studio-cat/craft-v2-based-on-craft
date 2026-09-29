uniform sampler2D sampler;
uniform float timer;
uniform float underwater;
uniform float underworld;

varying vec2 fragment_uv;

void main() {
    vec2 uv = vec2(timer, fragment_uv.t);
    vec4 color = texture2D(sampler, uv);
    if (underworld > 0.5) {
        // no sky down there, just the glow of the rock
        color.rgb = mix(color.rgb, vec3(0.22, 0.05, 0.04), 0.92);
    }
    if (underwater > 0.5) {
        color.rgb = mix(color.rgb, vec3(0.06, 0.26, 0.44), 0.75);
    }
    gl_FragColor = color;
}
