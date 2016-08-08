#include <maya/MStatus.h>
#include <maya/MGlobal.h>
#include <maya/MFnPlugin.h>
#include <maya/MObject.h>

#include "closestVertex.h"
#include "normalAlign.h"

MStatus initializePlugin(MObject obj)
{
	MStatus stat;
	MFnPlugin plugin(obj, "duncanskertchly@yahoo.co.uk", "0.1", "");
	
	stat = plugin.registerCommand("closestVertex", ClosestVertex::creator, ClosestVertex::newSyntax );
	if (stat != MStatus::kSuccess)
	{
		MGlobal::displayInfo("Problem registering closestVertex command!");
	}

	stat = plugin.registerNode("NormalAlign", NormalAlign::id, NormalAlign::creator, NormalAlign::initialize );
	if (stat != MStatus::kSuccess){
		MGlobal::displayInfo("Problem registering NormalAlign node!");
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

	stat = plugin.deregisterNode(NormalAlign::id);
	if (stat != MStatus::kSuccess){
		MGlobal::displayInfo("Problem de-registering NormalAlign node!");
	}

	return MS::kSuccess;
}