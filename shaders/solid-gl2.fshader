#version 130
#extension GL_EXT_gpu_shader4 : enable
uniform vec3 uColor;
uniform sampler2D uTexUnit;

varying vec2 vTexCoord;
void main() {

	int x = int (vTexCoord.x * 8.0);
	int y = int (vTexCoord.y * 8.0);
	if(((x+y) % 2) == 0){
   gl_FragColor = vec4(uColor, 1.0);
   }
   	else {
   	gl_FragColor = vec4(0.2,0.2,0.2, 1.0);
   	}
  
}