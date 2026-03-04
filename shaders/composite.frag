#version 330 core
/* Composite: video + blurred background + UI overlay */
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uVideo;      /* scene / video texture */
uniform sampler2D uBlurred;    /* Kawase blurred texture */
uniform sampler2D uUI;         /* UI layer with alpha */
uniform float uBarY;           /* control bar top Y (0..1, from bottom) */
uniform float uBarAlpha;       /* control bar opacity (0..1 for fade) */

void main() {
    vec4 video = texture(uVideo, vTexCoord);
    vec4 blurred = texture(uBlurred, vTexCoord);
    vec4 ui = texture(uUI, vTexCoord);

    /* Show frosted glass in the control bar region */
    float inBar = step(1.0 - uBarY, vTexCoord.y);
    vec4 bg = mix(video, blurred, inBar * uBarAlpha * 0.85);

    /* Tint the blurred region slightly */
    bg.rgb = mix(bg.rgb, bg.rgb * 0.9 + 0.05, inBar * uBarAlpha);

    /* Composite UI on top */
    vec3 final = mix(bg.rgb, ui.rgb, ui.a * uBarAlpha);
    FragColor = vec4(final, 1.0);
}
