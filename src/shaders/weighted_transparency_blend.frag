#version 460 core
out vec4 FragColor;

in vec2 uv;

layout (std140) uniform SceneParams
{
    int pixelSize;
    bool wireframe;
    float gamma;
    float specularPower;
    float viewportWidth;
    float viewportHeight;
};

uniform sampler2D accumulator;
uniform sampler2D revealage;

const float EPSILON = 0.00001f;

bool isApproximatelyEqual(float a, float b)
{
    return abs(a - b) <= (abs(a) < abs(b) ? abs(b) : abs(a)) * EPSILON;
}

float max3(vec3 v) 
{
    return max(max(v.x, v.y), v.z);
}

vec3 gammaCorrect(vec3 color, float gamma);

void main()
{
    float revealage = texture(revealage, uv).r;

    // save the blending and color texture fetch cost if there is not a transparent fragment
    if (isApproximatelyEqual(revealage, 1.0f)) 
        discard;

    vec4 accumulation = texture(accumulator, uv);

    // suppress overflow
    if (isinf(max3(abs(accumulation.rgb)))) 
        accumulation.rgb = vec3(accumulation.a);

    // prevent floating point precision bug
    vec3 average_color = accumulation.rgb / max(accumulation.a, EPSILON);

    FragColor = vec4(gammaCorrect(average_color, gamma), 1.0f - revealage);
}
