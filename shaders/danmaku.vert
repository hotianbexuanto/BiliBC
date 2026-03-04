#version 330 core
layout(location = 0) in vec2 aPos;    /* glyph quad corner */
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;  /* RGBA fill color */

out vec2 vTexCoord;
out vec4 vColor;

uniform mat4 uProjection;

void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
