/**
 * @file
 * vuo.scene.make.cube node implementation.
 *
 * @copyright Copyright © 2012–2013 Kosada Incorporated.
 * This code may be modified and distributed under the terms of the MIT License.
 * For more information, see http://vuo.org/license.
 */

#include "node.h"

VuoModuleMetadata({
					 "title" : "Make Cube",
					 "keywords" : [ "box", "d6", "hexahedron", "Platonic", "rectangular", "square" ],
					 "version" : "1.0.0",
					 "node": {
						 "isInterface" : false
					 }
				 });

void nodeEvent
(
		VuoInputData(VuoTransform) transform,
		VuoInputData(VuoShader) frontShader,
		VuoInputData(VuoShader) leftShader,
		VuoInputData(VuoShader) rightShader,
		VuoInputData(VuoShader) backShader,
		VuoInputData(VuoShader) topShader,
		VuoInputData(VuoShader) bottomShader,
		VuoOutputData(VuoSceneObject) cube
)
{
	VuoList_VuoSceneObject cubeChildObjects = VuoListCreate_VuoSceneObject();

	VuoList_VuoVertices quadVertices = VuoListCreate_VuoVertices();
	VuoListAppendValue_VuoVertices(quadVertices, VuoVertices_getQuad());

	VuoList_VuoSceneObject noChildObjects = VuoListCreate_VuoSceneObject();

	// Front Face
	{
		VuoSceneObject so = VuoSceneObject_make(
					quadVertices,
					frontShader,
					VuoTransform_makeEuler(VuoPoint3d_make(0,0,.5), VuoPoint3d_make(0,0,0), VuoPoint3d_make(1,1,1)),
					noChildObjects
					);
		VuoListAppendValue_VuoSceneObject(cubeChildObjects, so);
	}

	// Left Face
	{
		VuoSceneObject so = VuoSceneObject_make(
					quadVertices,
					leftShader,
					VuoTransform_makeEuler(VuoPoint3d_make(-.5,0,0), VuoPoint3d_make(0,-M_PI/2.,0), VuoPoint3d_make(1,1,1)),
					noChildObjects
					);
		VuoListAppendValue_VuoSceneObject(cubeChildObjects, so);
	}

	// Right Face
	{
		VuoSceneObject so = VuoSceneObject_make(
					quadVertices,
					rightShader,
					VuoTransform_makeEuler(VuoPoint3d_make(.5,0,0), VuoPoint3d_make(0,M_PI/2.,0), VuoPoint3d_make(1,1,1)),
					noChildObjects
					);
		VuoListAppendValue_VuoSceneObject(cubeChildObjects, so);
	}

	// Back Face
	{
		VuoSceneObject so = VuoSceneObject_make(
					quadVertices,
					backShader,
					VuoTransform_makeEuler(VuoPoint3d_make(0,0,-.5), VuoPoint3d_make(0,M_PI,0), VuoPoint3d_make(1,1,1)),
					noChildObjects
					);
		VuoListAppendValue_VuoSceneObject(cubeChildObjects, so);
	}

	// Top Face
	{
		VuoSceneObject so = VuoSceneObject_make(
					quadVertices,
					topShader,
					VuoTransform_makeEuler(VuoPoint3d_make(0,.5,0), VuoPoint3d_make(-M_PI/2.,0,0), VuoPoint3d_make(1,1,1)),
					noChildObjects
					);
		VuoListAppendValue_VuoSceneObject(cubeChildObjects, so);
	}

	// Bottom Face
	{
		VuoSceneObject so = VuoSceneObject_make(
					quadVertices,
					bottomShader,
					VuoTransform_makeEuler(VuoPoint3d_make(0,-.5,0), VuoPoint3d_make(M_PI/2.,0,0), VuoPoint3d_make(1,1,1)),
					noChildObjects
					);
		VuoListAppendValue_VuoSceneObject(cubeChildObjects, so);
	}

	*cube = VuoSceneObject_make(VuoListCreate_VuoVertices(), NULL, transform, cubeChildObjects);
}