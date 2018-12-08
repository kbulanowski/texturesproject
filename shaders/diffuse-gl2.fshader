#version 130
#extension GL_EXT_gpu_shader4 : enable
uniform vec3 uLight, uLight2, uColor;
uniform sampler2D uTexUnit;

varying vec3 vNormal;
varying vec3 vPosition;
varying vec2 vTexCoord;

void main() {
  vec3 tolight = normalize(uLight - vPosition);
  vec3 tolight2 = normalize(uLight2 - vPosition);
  
  vec3 normal = normalize(vNormal);
  vec3 viewDir = normalize(-vPosition);
	vec3 bouncevector1 =  normalize((dot(tolight,normal)*2)*normal-tolight);
	vec3 bouncevector2 = normalize((dot(tolight2,normal)*2)*normal-tolight2);
  float diffuse = max(0.0, dot(normal, tolight));
  diffuse += max(0.0, dot(normal, tolight2));
 
  float specular = pow(max(0.0,dot(bouncevector1,viewDir)),256.0);
  specular += pow(max(0.0,dot(bouncevector2,viewDir)),256.0);
  vec3 intensity = vec3(0.1,0.1,0.1) + uColor * diffuse + vec3(0.6,0.6,0.6) * specular;

  gl_FragColor = vec4(intensity, 1.0);
}