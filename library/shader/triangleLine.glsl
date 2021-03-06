// Inputs from VuoSceneRenderer
uniform mat4 projectionMatrix;
uniform float aspectRatio;
uniform float primitiveHalfSize;
uniform vec3 cameraPosition;

// Inputs from vertex shader
varying in vec4 positionForGeometry[2];

// Outputs to fragment shader
varying out vec4 vertexPosition;
varying out mat3 vertexPlaneToWorld;
varying out vec4 fragmentTextureCoordinate;

void main()
{
	vec2 lineSize = vec2(primitiveHalfSize, primitiveHalfSize * aspectRatio);
	vec3 perpendicular = normalize(cross(gl_PositionIn[1].xyz - gl_PositionIn[0].xyz, gl_PositionIn[0].xyz - (projectionMatrix * vec4(cameraPosition,1)).xyz));
	vec4 perpendicularOffset = vec4(perpendicular.x*lineSize.x, perpendicular.y*lineSize.y, 0, 0);

	mat3 vertexPlaneToWorldTemp;
	vertexPlaneToWorldTemp[0] =  vec3(1,0,0); // tangent
	vertexPlaneToWorldTemp[1] = -vec3(0,1,0); // bitangent
	vertexPlaneToWorldTemp[2] =  vec3(0,0,1); // normal

	gl_Position               = gl_PositionIn[1]       - perpendicularOffset;
	vertexPosition            = positionForGeometry[1] - perpendicularOffset;
	vertexPlaneToWorld        = vertexPlaneToWorldTemp;
	fragmentTextureCoordinate = vec4(0,1,0,0);
	EmitVertex();
	gl_Position               = gl_PositionIn[0]       - perpendicularOffset;
	vertexPosition            = positionForGeometry[0] - perpendicularOffset;
	vertexPlaneToWorld        = vertexPlaneToWorldTemp;
	fragmentTextureCoordinate = vec4(0,0,0,0);
	EmitVertex();
	gl_Position               = gl_PositionIn[0]       + perpendicularOffset;
	vertexPosition            = positionForGeometry[0] + perpendicularOffset;
	vertexPlaneToWorld        = vertexPlaneToWorldTemp;
	fragmentTextureCoordinate = vec4(1,0,0,0);
	EmitVertex();
	EndPrimitive();

	gl_Position               = gl_PositionIn[0]       + perpendicularOffset;
	vertexPosition            = positionForGeometry[0] + perpendicularOffset;
	vertexPlaneToWorld        = vertexPlaneToWorldTemp;
	fragmentTextureCoordinate = vec4(1,0,0,0);
	EmitVertex();
	gl_Position               = gl_PositionIn[1]       + perpendicularOffset;
	vertexPosition            = positionForGeometry[1] + perpendicularOffset;
	vertexPlaneToWorld        = vertexPlaneToWorldTemp;
	fragmentTextureCoordinate = vec4(1,1,0,0);
	EmitVertex();
	gl_Position               = gl_PositionIn[1]       - perpendicularOffset;
	vertexPosition            = positionForGeometry[1] - perpendicularOffset;
	vertexPlaneToWorld        = vertexPlaneToWorldTemp;
	fragmentTextureCoordinate = vec4(0,1,0,0);
	EmitVertex();
	EndPrimitive();
}
