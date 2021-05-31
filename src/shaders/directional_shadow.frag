#version 330 core
uniform vec3 lightPos;
uniform float farPlane;

in vec4 FragPos;

void main()
{
    //vec3 lightToFrag = FragPos.xyz - lightPos;
    //float lengthSqr = lightToFrag.x * lightToFrag.x + lightToFrag.y * lightToFrag.y + lightToFrag.z * lightToFrag.z;
    //gl_FragDepth = lengthSqr;

    float lightDistance = length(FragPos.xyz - lightPos);
    lightDistance = lightDistance / farPlane;
    gl_FragDepth = lightDistance;

    //gl_FragDepth = FragPos.z / FragPos.w;
}
