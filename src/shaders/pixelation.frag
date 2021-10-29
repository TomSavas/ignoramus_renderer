#version 330 core
in vec2 uv;
out vec4 FragColor;
uniform sampler2D tex;

layout (std140) uniform SceneParams
{
    int pixelSize;
    bool wireframe;
    float gamma;
    float specularPower;
    float viewportWidth;
    float viewportHeight;
};

void main()
{    
    int x = int(gl_FragCoord.x) - (int(gl_FragCoord.x) % pixelSize) + pixelSize / 2;
    int y = int(gl_FragCoord.y) - (int(gl_FragCoord.y) % pixelSize) + pixelSize / 2;

    vec2 new_uv = vec2(x, y) / vec2(1920, 1080);
    FragColor = texture(tex, new_uv);
}
