#version 330 core
in vec2 uv;
out vec4 FragColor;
uniform sampler2D tex;

void main()
{    
    vec4 color = texture(tex, uv);
    //if (color.a <= 0.f)
    //    //discard;
    //    FragColor = vec4(1.f, 0.f, 0.f, 1.f);
    //else
    FragColor = color;

    //vec2 a = gl_FragCoord.xy / vec2(1920, 1080);
    //FragColor = vec4(1.f, 0.f, 0.f, 1.f);
}
