#version 330 core
in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_TexY;
uniform sampler2D u_TexU;
uniform sampler2D u_TexV;

void main() {
    float y = texture(u_TexY, v_TexCoord).r;
    float u = texture(u_TexU, v_TexCoord).r - 0.5;
    float v = texture(u_TexV, v_TexCoord).r - 0.5;

    // BT.709 转换 (高清视频标准)
    float r = y + 1.5748 * v;
    float g = y - 0.1873 * u - 0.4681 * v;
    float b = y + 1.8556 * u;

    FragColor = vec4(r, g, b, 1.0);
}
