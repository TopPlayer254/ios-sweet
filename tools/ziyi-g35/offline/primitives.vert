#version 450
layout(location=0) flat out int instance;
void main(){vec2 p[3]=vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
gl_Position=vec4(p[gl_VertexIndex],gl_InstanceIndex==0?0.25:0.75,1);instance=gl_InstanceIndex;}
