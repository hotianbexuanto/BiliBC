#version 330 core
/* Kawase upsample: 8-tap filter */
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec2 uHalfPixel;

void main() {
    vec4 sum  = texture(uTexture, vTexCoord + vec2(-uHalfPixel.x * 2.0, 0.0));
    sum += texture(uTexture, vTexCoord + vec2(-uHalfPixel.x, uHalfPixel.y)) * 2.0;
    sum += texture(uTexture, vTexCoord + vec2(0.0, uHalfPixel.y * 2.0));
    sum += texture(uTexture, vTexCoord + vec2(uHalfPixel.x, uHalfPixel.y)) * 2.0;
    sum += texture(uTexture, vTexCoord + vec2(uHalfPixel.x * 2.0, 0.0));
    sum += texture(uTexture, vTexCoord + vec2(uHalfPixel.x, -uHalfPixel.y)) * 2.0;
    sum += texture(uTexture, vTexCoord + vec2(0.0, -uHalfPixel.y * 2.0));
    sum += texture(uTexture, vTexCoord + vec2(-uHalfPixel.x, -uHalfPixel.y)) * 2.0;
    FragColor = sum / 12.0;
}
