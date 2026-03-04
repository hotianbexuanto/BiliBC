#version 330 core
/* UI elements: rounded rect SDF rendering */
in vec2 vTexCoord;
out vec4 FragColor;

uniform vec4 uRect;       /* x, y, w, h in pixels */
uniform float uRadius;    /* corner radius */
uniform vec4 uColor;      /* fill color */
uniform vec4 uBorderColor;
uniform float uBorderWidth;
uniform vec2 uResolution;

/* Signed distance to rounded rectangle */
float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    /* Convert from tex coords to pixel coords */
    vec2 pixelPos = vTexCoord * uResolution;
    vec2 center = uRect.xy + uRect.zw * 0.5;
    vec2 halfSize = uRect.zw * 0.5;

    float d = sdRoundedBox(pixelPos - center, halfSize, uRadius);

    /* Anti-aliased edge */
    float aa = fwidth(d);
    float fill = 1.0 - smoothstep(-aa, aa, d);
    float border = 1.0 - smoothstep(-aa, aa, abs(d + uBorderWidth * 0.5) - uBorderWidth * 0.5);

    vec4 col = mix(uColor, uBorderColor, border * step(0.01, uBorderWidth));
    FragColor = vec4(col.rgb, col.a * fill);
}
