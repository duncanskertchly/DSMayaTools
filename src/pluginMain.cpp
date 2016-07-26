#include <maya/MStatus.h>
#include <maya/MGlobal.h>
#include <maya/MFnPlugin.h>
#include <maya/MObject.h>

#include "closestVertex.h"

MStatus initializePlugin(MObject obj)
{
	MStatus stat;
	MFnPlugin plugin(obj, "duncanskertchly@yahoo.co.uk", "0.1", "");
	
	stat = plugin.registerCommand("closestVertex", ClosestVertex::creator, ClosestVertex::newSyntax );
	if (stat != MStatus::kSuccess)
	{
		MGlobal::displayInfo("Problem registering closestVertex command!");
	}

	return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj)
{
	MStatus stat;
	MFnPlugin plugin(obj);
	stat = plugin.deregisterCommand("closestVertex");
	
	if (stat != MStatus::kSuccess)
	{
		MGlobal::displayInfo("Problem de-registering closestVertex command!");
	}

	return MS::kSuccess;
}