#include <stdio.h>
#include "normalAlign.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MFnAttribute.h>
#include <maya/MPlug.h>
#include <maya/MFnData.h>
#include <maya/MTypeId.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MPlug.h>
#include <maya/MDataHandle.h>
#include <maya/MFnMesh.h>
#include <maya/MVector.h>
#include <maya/MPoint.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MMatrix.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnTransform.h>
#include <maya/MFnDagNode.h>
#include <maya/MPlugArray.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MIntArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFloatVector.h>
#include <maya/MDagPath.h>
#include <maya/MItMeshVertex.h>

using namespace std;

// #define _DEBUG

#define RADTODEG 57.2958

#define ROT_ORDER_XYZ 0
#define ROT_ORDER_YZX 1
#define ROT_ORDER_ZXY 2
#define ROT_ORDER_XZY 3
#define ROT_ORDER_YXZ 4
#define ROT_ORDER_ZYX 5

NormalAlign::NormalAlign(){};
NormalAlign::~NormalAlign(){};

MObject NormalAlign::sourceShape;
MObject NormalAlign::index;

MObject NormalAlign::vertexMode;

MObject NormalAlign::targetAimAxis;
MObject NormalAlign::targetUpAxis;
MObject NormalAlign::targetRotateOrder;

MObject NormalAlign::flipTargetUpAxis;
MObject NormalAlign::flipTargetAimAxis;

// MObject NormalAlign::uvValue;

MObject NormalAlign::sourceWorldMatrix;

MObject NormalAlign::targetParentInverseMatrix;

MObject NormalAlign::outputT;
MObject NormalAlign::outputTX;
MObject NormalAlign::outputTY;
MObject NormalAlign::outputTZ;

MObject NormalAlign::outputR;
MObject NormalAlign::outputRX;
MObject NormalAlign::outputRY;
MObject NormalAlign::outputRZ;

MTypeId NormalAlign::id(209401);

//gets the center of a poly by averaging the its component vertices
MPoint getPolyCenter(MFnMesh &mesh, unsigned int index)
{
    MIntArray polyVerts;
    mesh.getPolygonVertices(index, polyVerts);
    MPoint polyCenter(0.0, 0.0, 0.0, 0.0);

    unsigned int j;
    for (j = 0; j <  polyVerts.length(); j++)
    {
        MPoint tempPoint;
        mesh.getPoint(polyVerts[j], tempPoint, MSpace::kWorld);
        polyCenter += tempPoint;
    }

    polyCenter = polyCenter / float(polyVerts.length());

    return polyCenter;
}

//gets a tangent vector for a poly by averaging its component vertex tangents
MFloatVector getPolyTangent(MFnMesh &mesh, unsigned int index)
{
    MFloatVector polyTangentVector(0.0, 0.0, 0.0);
    MFloatVectorArray polyVertexTangents;
    mesh.getFaceVertexTangents(index, polyVertexTangents, MSpace::kWorld);

    unsigned int j;
    for (j = 0; j < polyVertexTangents.length(); j++)
    {
        MFloatVector tempVector;
        tempVector = polyVertexTangents[j];
        polyTangentVector += tempVector;
    }

    polyTangentVector = polyTangentVector / float(polyTangentVector.length());

    return polyTangentVector;

}

MStatus NormalAlign::compute(const MPlug& plug, MDataBlock& data)
{
    MStatus stat;
    MStringArray spl;
    plug.name().split('.', spl);
    MString plugName = spl[spl.length()-1];

    if ( plugName.substring(0, 5) == "output" )
    {
        //handles to all the different bits of data
        MDataHandle meshDataHandle = data.inputValue(sourceShape, &stat);
        MDataHandle indexDataHandle = data.inputValue(index, &stat);
        MDataHandle aimAxisHandle = data.inputValue(targetAimAxis, &stat);
        MDataHandle upAxisHandle = data.inputValue(targetUpAxis, &stat);
        MDataHandle rotOrderHandle = data.inputValue(targetRotateOrder, &stat);
        MDataHandle targetParentInverseHandle = data.inputValue(targetParentInverseMatrix, &stat);
        MDataHandle sourceWorldMatrixHandle = data.inputValue(sourceWorldMatrix, &stat);
        MDataHandle flipAimAxisHandle = data.inputValue(flipTargetAimAxis, &stat);
        MDataHandle flipUpAxisHandle = data.inputValue(flipTargetUpAxis, &stat);
        MDataHandle vertexModeHandle = data.inputValue(vertexMode, &stat);
        // MDataHandle uvValueHandle = data.inputValue(uvValue, &stat);

        //get the various values from the data handles
        MMatrix targetParentInverseMatrix = targetParentInverseHandle.asMatrix();
        MMatrix sourceWorldMatrix = sourceWorldMatrixHandle.asMatrix();
        
        bool flipAimAxis = flipAimAxisHandle.asBool();
        bool flipUpAxis = flipUpAxisHandle.asBool();

        // float2 &uvValue = uvValueHandle.asFloat2();

        short aimAxis = aimAxisHandle.asShort();
        short upAxis = upAxisHandle.asShort();
        short rotOrder = rotOrderHandle.asShort();

        int ind = indexDataHandle.asInt();
        bool vertexMode = vertexModeHandle.asBool();

        MObject meshObject = meshDataHandle.asMesh();
        MFnMesh mesh(meshObject, &stat);

        //stuff we're going to use to store the poly normal etc
        MVector normalVec;
        MVector upVec;
        MVector forwardVec;
        MPoint position;

        if (!vertexMode)
        {
            //make sure the users poly index is clamped between 0 and the number of
            //polys in the mesh
            int numPolygons = mesh.numPolygons();
            if (ind >= numPolygons){ ind = numPolygons - 1; }
            if (ind < 0){ ind = 0; }       
            
            //get the polys normal
            mesh.getPolygonNormal(ind, normalVec, MSpace::kWorld);

            //get the polys position in space
            position = getPolyCenter(mesh, ind);

            
            // //test thing
            // mesh.getPointAtUV(ind, position, uvValue, MSpace::kObject);
            // #ifdef _DEBUG
            //     cout << "uValue: " << *uvValue << " vValue: " << *(uvValue + 1) << endl;
            //     cout << "position: " << position << endl;
            // #endif
            

            //get the polys tangent vector
            upVec = getPolyTangent(mesh, ind);
        }
        else
        {
            //make sure the users vert index is clamped between 0 and the number of
            //polys in the mesh
            int numVerts = mesh.numVertices();
            if (ind >= numVerts){ ind = numVerts - 1;}
            if (ind < 0){ ind = 0; }

            //create a mesh iterator and set it's index to the users index value
            MItMeshVertex meshVertIter(meshObject);
            int notNeeded;
            meshVertIter.setIndex(ind, notNeeded);

            //get an array of face indices that are connected to this vertex
            MIntArray connectedFaces;
            meshVertIter.getConnectedFaces(connectedFaces);

            //get the vert normalnd position
            mesh.getVertexNormal(ind, normalVec, MSpace::kWorld);
            mesh.getPoint(ind, position, MSpace::kWorld);

            if (connectedFaces.length() > 0)
            {
                //get the tangent using the first item in the list of faces found earlier
                mesh.getFaceVertexTangent(connectedFaces[0], ind, upVec, MSpace::kWorld);
            }
        }

        //flip the aim / up axis if required
        if (flipAimAxis)
        {
            normalVec = normalVec * -1.0;
        }
        if (flipUpAxis)
        {
            upVec = upVec * -1.0;
        }

        #ifdef _DEBUG
            cout << "normalVec: " << normalVec << endl;
            cout << "aimAxis: " << aimAxis << endl;
            cout << "upAxis: " << upAxis << endl;
        #endif

        //get the forward vector, cross product of normal and up vectors
        forwardVec = normalVec^upVec;
        forwardVec.normalize();

        //the default matrix, aim = x, upVec = y, z = forwardVec
        double temp4x4[4][4] =  {    normalVec.x,        normalVec.y,        normalVec.z,          0.0,
                                     upVec.x,            upVec.y,            upVec.z,              0.0,
                                     forwardVec.x,       forwardVec.y,       forwardVec.z,         0.0,
                                     position.x,         position.y,         position.z,           1.0 
                                };
    
        //customize the matrix if the user has chosen special aim / up axes
        if (aimAxis != upAxis)
        {
            temp4x4[aimAxis][0] = normalVec.x;
            temp4x4[aimAxis][1] = normalVec.y;
            temp4x4[aimAxis][2] = normalVec.z;

            temp4x4[upAxis][0] = upVec.x;
            temp4x4[upAxis][1] = upVec.y;
            temp4x4[upAxis][2] = upVec.z;

            temp4x4[3 - (aimAxis + upAxis)][0] = forwardVec.x;
            temp4x4[3 - (aimAxis + upAxis)][1] = forwardVec.y;
            temp4x4[3 - (aimAxis + upAxis)][2] = forwardVec.z;
        }

        //put together the final matrix
        //matrix we computed * world matrix of input meshes transform  * parent inverse matrix of target
        MMatrix tempMatrix(temp4x4);
        MMatrix finalMatrix = tempMatrix * sourceWorldMatrix * targetParentInverseMatrix;
        MTransformationMatrix finalTransform(finalMatrix);

        //get the correct rotation order from the users selection for use when we get the rotation
        //values from the matrix
        MTransformationMatrix::RotationOrder order;
        switch(rotOrder) 
        {
            case 0:
                order = MTransformationMatrix::kXYZ;
                break;
            case 1:
                order = MTransformationMatrix::kYZX;
                break;
            case 2:
                order = MTransformationMatrix::kZXY;
                break;
            case 3:
                order = MTransformationMatrix::kXZY;
                break;
            case 4:
                order = MTransformationMatrix::kYXZ;
                break;
            case 5:
                order = MTransformationMatrix::kZYX;
                break;
            default:
                order = MTransformationMatrix::kXYZ;
        }

        //store the final translation and rotation
        double finalRotation[3];
        MVector finalTranslation = finalTransform.getTranslation(MSpace::kWorld);
        finalTransform.getRotation(finalRotation, order);

        #ifdef _DEBUG
            cout << "finalTranslation: " << finalTranslation.x << " " << finalTranslation.y << " " << finalTranslation.z << endl;
            cout << "finalRotation: " << finalRotation[0] * RADTODEG << " " << finalRotation[1] * RADTODEG << " " << finalRotation[2] * RADTODEG << endl;
        #endif

        //get the output handles and set the final values
        MDataHandle hOutputTrans = data.outputValue(outputT);
        MDataHandle hOutputRot = data.outputValue(outputR);

        hOutputTrans.set3Double(finalTranslation.x, finalTranslation.y, finalTranslation.z);
        hOutputRot.set3Double(finalRotation[0], finalRotation[1], finalRotation[2]);

    }

    data.setClean(plug);
    return MStatus::kSuccess;
}

void* NormalAlign::creator()
{
    return new NormalAlign();
}

MStatus NormalAlign::initialize()
{
    MFnNumericAttribute numAttr;
    MFnUnitAttribute unitAttr;
    MFnCompoundAttribute compAttr;
    MFnTypedAttribute typedAttr;
    MFnEnumAttribute enumAttr;
    MFnMatrixAttribute matAttr;
    MStatus stat;

    sourceShape = typedAttr.create("sourceShape", "ss", MFnData::kMesh, MObject::kNullObj, &stat);
    typedAttr.setWritable(true);
    typedAttr.setStorable(true);
    typedAttr.setReadable(false);
    addAttribute(sourceShape);

    index = numAttr.create("index", "i", MFnNumericData::kInt, 0.0, &stat);
    numAttr.setKeyable(false);
    numAttr.setWritable(true);
    numAttr.setReadable(true);
    numAttr.setStorable(true);
    addAttribute(index);

    vertexMode = numAttr.create("vertexMode", "vm", MFnNumericData::kBoolean);
    numAttr.setKeyable(false);
    addAttribute(vertexMode);

    // uvValue = numAttr.create("uvValue", "uv", MFnNumericData::k2Float);
    // numAttr.setKeyable(false);
    // numAttr.setMin(0.0);
    // numAttr.setMax(1.0);
    // numAttr.setDefault(0.5);
    // addAttribute(uvValue);

    targetAimAxis = enumAttr.create("targetAimAxis", "taim", 0, &stat);
    enumAttr.addField("X", 0);
    enumAttr.addField("Y", 1);
    enumAttr.addField("Z", 2);
    enumAttr.setKeyable(false);
    enumAttr.setReadable(false);
    enumAttr.setConnectable(false);
    addAttribute(targetAimAxis);

    flipTargetAimAxis = numAttr.create("flipTargetAimAxis", "ftaa", MFnNumericData::kBoolean);
    numAttr.setKeyable(false);
    addAttribute(flipTargetAimAxis);
    
    targetUpAxis = enumAttr.create("targetUpAxis", "tup", 1, &stat);
    enumAttr.addField("X", 0);
    enumAttr.addField("Y", 1);
    enumAttr.addField("Z", 2);
    enumAttr.setKeyable(false);
    enumAttr.setReadable(false);
    enumAttr.setConnectable(false);
    addAttribute(targetUpAxis);

    flipTargetUpAxis = numAttr.create("flipTargetUpAxis", "ftua", MFnNumericData::kBoolean);
    numAttr.setKeyable(false);
    addAttribute(flipTargetUpAxis);

    targetParentInverseMatrix = matAttr.create("targetParentInverseMatrix", "tpim", MFnMatrixAttribute::kDouble, &stat);
    matAttr.setWritable(true);
    matAttr.setConnectable(true);
    matAttr.setReadable(false);
    matAttr.setKeyable(false);
    addAttribute(targetParentInverseMatrix);

    sourceWorldMatrix = matAttr.create("sourceWorldMatrix", "swm", MFnMatrixAttribute::kDouble, &stat);
    matAttr.setWritable(true);
    matAttr.setConnectable(true);
    matAttr.setReadable(false);
    matAttr.setKeyable(false);
    addAttribute(sourceWorldMatrix);

    targetRotateOrder = enumAttr.create("targetRotateOrder", "tro", 0, &stat);
    enumAttr.addField("XYZ", ROT_ORDER_XYZ);
    enumAttr.addField("YZX", ROT_ORDER_YZX);
    enumAttr.addField("ZXY", ROT_ORDER_ZXY);
    enumAttr.addField("XZY", ROT_ORDER_XZY);
    enumAttr.addField("YXZ", ROT_ORDER_YXZ);
    enumAttr.addField("ZYX", ROT_ORDER_ZYX);
    enumAttr.setKeyable(false);
    enumAttr.setReadable(false);
    enumAttr.setConnectable(true);
    addAttribute(targetRotateOrder);

    outputT = compAttr.create("outputT", "ot");
    compAttr.setWritable(false);
    compAttr.setReadable(true);
    compAttr.setKeyable(false);
    outputTX = unitAttr.create("outputTX", "otx", MFnUnitAttribute::kDistance);
    unitAttr.setWritable(false);
    unitAttr.setReadable(true);
    unitAttr.setKeyable(false);
    outputTY = unitAttr.create("outputTY", "oty", MFnUnitAttribute::kDistance);
    unitAttr.setWritable(false);
    unitAttr.setReadable(true);
    unitAttr.setKeyable(false);
    outputTZ = unitAttr.create("outputTZ", "otz", MFnUnitAttribute::kDistance);
    unitAttr.setWritable(false);
    unitAttr.setReadable(true);
    unitAttr.setKeyable(false);

    compAttr.addChild(outputTX);
    compAttr.addChild(outputTY);
    compAttr.addChild(outputTZ);
    addAttribute(outputT);

    outputR = compAttr.create("outputR", "or");
    compAttr.setWritable(false);
    compAttr.setReadable(true);
    compAttr.setKeyable(false);
    outputRX = unitAttr.create("outputRX", "orx", MFnUnitAttribute::kDistance);
    unitAttr.setWritable(false);
    unitAttr.setReadable(true);
    unitAttr.setKeyable(false);
    outputRY = unitAttr.create("outputRY", "ory", MFnUnitAttribute::kDistance);
    unitAttr.setWritable(false);
    unitAttr.setReadable(true);
    unitAttr.setKeyable(false);
    outputRZ = unitAttr.create("outputRZ", "orz", MFnUnitAttribute::kDistance);
    unitAttr.setWritable(false);
    unitAttr.setReadable(true);
    unitAttr.setKeyable(false);

    compAttr.addChild(outputRX);
    compAttr.addChild(outputRY);
    compAttr.addChild(outputRZ);
    addAttribute(outputR);

    attributeAffects(index, outputTX);
    attributeAffects(index, outputTY);
    attributeAffects(index, outputTZ);
    attributeAffects(index, outputRX);
    attributeAffects(index, outputRY);
    attributeAffects(index, outputRZ);

    attributeAffects(vertexMode, outputTX);
    attributeAffects(vertexMode, outputTY);
    attributeAffects(vertexMode, outputTZ);
    attributeAffects(vertexMode, outputRX);
    attributeAffects(vertexMode, outputRY);
    attributeAffects(vertexMode, outputRZ);

    attributeAffects(targetRotateOrder, outputTX);
    attributeAffects(targetRotateOrder, outputTY);
    attributeAffects(targetRotateOrder, outputTZ);
    attributeAffects(targetRotateOrder, outputRX);
    attributeAffects(targetRotateOrder, outputRY);
    attributeAffects(targetRotateOrder, outputRZ);    

    // attributeAffects(uvValue, outputTX);
    // attributeAffects(uvValue, outputTY);
    // attributeAffects(uvValue, outputTZ);
    // attributeAffects(uvValue, outputRX);
    // attributeAffects(uvValue, outputRY);
    // attributeAffects(uvValue, outputRZ);    

    attributeAffects(targetAimAxis, outputTX);
    attributeAffects(targetAimAxis, outputTY);
    attributeAffects(targetAimAxis, outputTZ);
    attributeAffects(targetAimAxis, outputRX);
    attributeAffects(targetAimAxis, outputRY);
    attributeAffects(targetAimAxis, outputRZ);

    attributeAffects(flipTargetAimAxis, outputTX);
    attributeAffects(flipTargetAimAxis, outputTY);
    attributeAffects(flipTargetAimAxis, outputTZ);
    attributeAffects(flipTargetAimAxis, outputRX);
    attributeAffects(flipTargetAimAxis, outputRY);
    attributeAffects(flipTargetAimAxis, outputRZ);

    attributeAffects(targetParentInverseMatrix, outputTX);
    attributeAffects(targetParentInverseMatrix, outputTY);
    attributeAffects(targetParentInverseMatrix, outputTZ);
    attributeAffects(targetParentInverseMatrix, outputRX);
    attributeAffects(targetParentInverseMatrix, outputRY);
    attributeAffects(targetParentInverseMatrix, outputRZ);

    attributeAffects(sourceWorldMatrix, outputTX);
    attributeAffects(sourceWorldMatrix, outputTY);
    attributeAffects(sourceWorldMatrix, outputTZ);
    attributeAffects(sourceWorldMatrix, outputRX);
    attributeAffects(sourceWorldMatrix, outputRY);
    attributeAffects(sourceWorldMatrix, outputRZ);

    attributeAffects(targetUpAxis, outputTX);
    attributeAffects(targetUpAxis, outputTY);
    attributeAffects(targetUpAxis, outputTZ);
    attributeAffects(targetUpAxis, outputRX);
    attributeAffects(targetUpAxis, outputRY);
    attributeAffects(targetUpAxis, outputRZ);    

    attributeAffects(flipTargetUpAxis, outputTX);
    attributeAffects(flipTargetUpAxis, outputTY);
    attributeAffects(flipTargetUpAxis, outputTZ);
    attributeAffects(flipTargetUpAxis, outputRX);
    attributeAffects(flipTargetUpAxis, outputRY);
    attributeAffects(flipTargetUpAxis, outputRZ);

    attributeAffects(sourceShape, outputTX);
    attributeAffects(sourceShape, outputTY);
    attributeAffects(sourceShape, outputTZ);
    attributeAffects(sourceShape, outputRX);
    attributeAffects(sourceShape, outputRY);
    attributeAffects(sourceShape, outputRZ);
    
    return MStatus::kSuccess;
}