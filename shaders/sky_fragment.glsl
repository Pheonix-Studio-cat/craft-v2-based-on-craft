uniform sampler2D sampler;
uniform float timer;
uniform float underwater;

varying vec2 fragment_uv;

void main() {
    vec2 uv = vec2(timer, fragment_uv.t);
    vec4 color = texture2D(sampler, uv);
    if (underwater > 0.5) {
        color.rgb = mix(color.rgb, vec3(0.06, 0.26, 0.44), 0.75);
    }
    gl_FragColor = color;
}
