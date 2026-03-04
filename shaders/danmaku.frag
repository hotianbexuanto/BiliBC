#version 330 core
in vec2 vTexCoord;
in vec4 vColor;
out vec4 FragColor;

uniform sampler2D uAtlas;  /* RG8: R=stroke alpha, G=fill alpha */

void main() {
    vec2 s = texture(uAtlas, vTexCoord).rg;
    float strokeA = s.r;
    float fillA   = s.g;

    /* Composite: black stroke behind colored fill */
    vec3 strokeCol = vec3(0.0);
    vec3 fillCol   = vColor.rgb;

    /* Over compositing: fill over stroke */
    vec3 col = mix(strokeCol, fillCol, fillA);
    float a  = max(strokeA, fillA) * vColor.a;

    FragColor = vec4(col, a);
}
