#version 130
#extension GL_EXT_gpu_shader4 : enable
uniform vec3 uColor;
uniform sampler2D uTexUnit;

varying vec2 vTexCoord;
void main() {

	vec4 texcoord = texture2D(uTexUnit,vTexCoord);
	gl_FragColor = texcoord;
  
}