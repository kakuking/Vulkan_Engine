#version 450
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outUV;

struct Vertex {
	vec3 position;
	float uvX;
	vec3 normal;
	float uvY;
	vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{
	Vertex vertices[];
};

layout(push_constant) uniform constants{
	mat4 renderMatrix;
	VertexBuffer vertexBuffer;
} PushConstants;


void main() 
{
	//Vertices uploaded and called from teh address in PushConstants
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

	//Basically Proj * view * position
	gl_Position = PushConstants.renderMatrix * vec4(v.position, 1.0f);
	outColor = v.color.xyzw;
	outUV = vec2(v.uvX, v.uvY);
}
