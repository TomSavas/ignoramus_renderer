#version 330 core
in vec2 uv;
out vec4 FragColor;

uniform sampler2D frontBlender;
uniform sampler2D backBlender;

void main()
{    
    vec4 frontColor = texture(frontBlender, gl_FragCoord.xy / vec2(1920.f, 1080.f));
    vec4 backColor = texture(backBlender, gl_FragCoord.xy / vec2(1920.f, 1080.f));
    float alphaMultiplier = 1.f - frontColor.w;
    
    FragColor = vec4(frontColor.rgb + backColor.rgb * alphaMultiplier, frontColor.a + backColor.a);
}
