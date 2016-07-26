#include <stdio.h>

#include <maya/MString.h>
#include <maya/MIOStream.h>
#include <maya/MObject.h>
#include <maya/MGlobal.h>
#include <maya/MArgParser.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MPoint.h>
#include <maya/MSelectionList.h>
#include <maya/MArgDatabase.h>
#include <maya/MMeshIntersector.h>
#include <maya/MDagPath.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MIntArray.h>
#include <maya/MPointArray.h>
#include <maya/MTransformationMatrix.h>

#include "closestVertex.h"

using namespace std;

ClosestVertex::ClosestVertex(){};
ClosestVertex::~ClosestVertex(){};

bool ClosestVertex::isUndoable() const
{
	return false;
}

MSyntax ClosestVertex::newSyntax()
{
	MSyntax syntax;

	syntax.addFlag("p", "position", MSyntax::kDouble, MSyntax::kDouble, MSyntax::kDouble); 
	syntax.addFlag("f", "faceMode", MSyntax::kBoolean);
	syntax.setObjectType(MSyntax::kSelectionList, 1, 1);
	
	return syntax;
}


MStatus ClosestVertex::doIt(const MArgList& args)
{
	MStatus status;
	MSelectionList objectsList;
	MDagPath meshObj;
	MStringArray meshStrings;
	MArgDatabase argData(syntax(), args);
	MMeshIntersector meshIntersector;
	MMatrix objMatrix;

	bool faceMode = false;
	argData.getFlagArgument("faceMode", 0, faceMode);
	
	MPoint sourcePoint;
	MPointOnMesh meshPoint;

	status = argData.getObjects(objectsList);
	if (status != MStatus::kSuccess)
	{
		MGlobal::displayInfo("Requires one object.");
		return MStatus::kFailure;
	}

	if (argData.isFlagSet("position") != true) 
	{
		MGlobal::displayInfo("Position flag not set.");
		return MStatus::kFailure;
	}

	double pos[3];
	for (int i = 0; i < 3; i++)
	{
		pos[i] = argData.flagArgumentDouble("position", i, &status);
		if (status != MStatus::kSuccess)
		{
			MGlobal::displayInfo("Require three floats.");
			return MStatus::kFailure; 
		}
	}

	if (objectsList.getDagPath(0, meshObj) != MStatus::kSuccess)
	{
		MGlobal::displayInfo("Problem with Dag Path.");
		return MStatus::kFailure;
	}
	
	objectsList.getSelectionStrings(0, meshStrings);
	//set the transformation matrix of the object
	objMatrix = meshObj.inclusiveMatrix();

	unsigned int numShapes;
	meshObj.numberOfShapesDirectlyBelow(numShapes);
	if (numShapes < 1)
	{
		MGlobal::displayInfo("No shape found.");
		return MStatus::kFailure;
		
	}

	status = meshObj.extendToShapeDirectlyBelow(0);

	sourcePoint = MPoint(pos[0], pos[1], pos[2], 1.0);
	MObject shapeNode = meshObj.node();

	status = meshIntersector.create( shapeNode, objMatrix );
	status = meshIntersector.getClosestPoint(sourcePoint, meshPoint);

	if (status != MStatus::kSuccess)
	{
		MGlobal::displayInfo("Closest Point Failed.");
		return MStatus::kFailure;
	}

	//index of closest face
	int polyIndex = meshPoint.faceIndex();
	MItMeshPolygon meshIter = MItMeshPolygon(meshObj);
	meshIter.setIndex(polyIndex, polyIndex );
	
	//array of possible closest vert indices
	MIntArray vertIndices;
	meshIter.getVertices(vertIndices);
	//get an array of vert positions on the face we're dealing with
	MPointArray vertLocations;
	meshIter.getPoints(vertLocations, MSpace::kWorld, &status);
	if (status != MStatus::kSuccess)
	{
		MGlobal::displayInfo("Problem obtaining vert locations.");
		return MStatus::kFailure;
	}

	//run through the faces vertices and get the smallest distance from the vertex to the source location.
	double winningDist = sourcePoint.distanceTo(vertLocations[0]);
	int winningInd = vertIndices[0];
	
	for (unsigned int i = 0; i < vertLocations.length(); i++)
	{

		double tempDist = sourcePoint.distanceTo(vertLocations[i]);
		//cout << "Vert:" << vertIndices[i] << " " << vertLocations[i].x << " " << vertLocations[i].y << " " << vertLocations[i].z << " " << vertLocations[i].w << " Distance: " << tempDist << endl;
		if (tempDist < winningDist)
		{
			winningDist = tempDist;
			winningInd = vertIndices[i];
		}
	}

	//if the faceMode flag is set return the closest face
	if (faceMode == true)
	{
		ClosestVertex::setResult(meshPoint.faceIndex());
		return MS::kSuccess;
	}
 
	//otherwise return the closest vert
	ClosestVertex::setResult(winningInd);
	return MS::kSuccess;

}

void* ClosestVertex::creator()
{
	return new ClosestVertex;
}