#version 450
layout(set=0,binding=0) uniform sampler2D sourceTexture;
layout(location=0) out vec4 color;
void main(){color=texelFetch(sourceTexture,ivec2(gl_FragCoord.xy)&ivec2(1),0);}
