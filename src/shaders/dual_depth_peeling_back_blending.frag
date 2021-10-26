#version 330 core
in vec2 uv;
out vec4 FragColor;

uniform sampler2D tempBackBlender;

void main()
{    
    FragColor = texture(tempBackBlender, gl_FragCoord.xy / vec2(1920.f, 1080.f));

    if (FragColor.a == 0) 
    {
        discard;
    }
}
