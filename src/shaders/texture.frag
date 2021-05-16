#version 330 core
in vec2 uv;
out vec4 FragColor;
uniform sampler2D tex;

void main()
{    
    FragColor = texture(tex, uv);
    //vec2 a = gl_FragCoord.xy / vec2(1920, 1080);
    //FragColor = vec4(1.f, 0.f, 0.f, 1.f);
}
