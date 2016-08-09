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
#include <maya/MEulerRotation.h>

using namespace std;

// #define _DEBUG

#define RADTODEG 57.2958
#define DEGTORAD 0.0174533

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

MObject NormalAlign::sourceWorldMatrix;

MObject NormalAlign::targetParentInverseMatrix;

MObject NormalAlign::offsetT;
MObject NormalAlign::offsetTX;
MObject NormalAlign::offsetTY;
MObject NormalAlign::offsetTZ;

MObject NormalAlign::offsetR;
MObject NormalAlign::offsetRX;
MObject NormalAlign::offsetRY;
MObject NormalAlign::offsetRZ;

MObject NormalAlign::outputT;
MObject NormalAlign::outputTX;
MObject NormalAlign::outputTY;
MObject NormalAlign::outputTZ;

MObject NormalAlign::outputR;
MObject NormalAlign::outputRX;
MObject NormalAlign::outputRY;
MObject NormalAlign::outputRZ;

MTypeId NormalAlign::id(209401);

//gets the center of a poly by averaging its component vertices
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
        MDataHandle offsetTransHandle = data.inputValue(offsetT, &stat);
        MDataHandle offsetRotHandle = data.inputValue(offsetR, &stat);

        //get the various values from the data handles
        MMatrix targetParentInverseMatrix = targetParentInverseHandle.asMatrix();
        MMatrix sourceWorldMatrix = sourceWorldMatrixHandle.asMatrix();
        
        bool flipAimAxis = flipAimAxisHandle.asBool();
        bool flipUpAxis = flipUpAxisHandle.asBool();

        short aimAxis = aimAxisHandle.asShort();
        short upAxis = upAxisHandle.asShort();
        short rotOrder = rotOrderHandle.asShort();

        int ind = indexDataHandle.asInt();
        bool vertexMode = vertexModeHandle.asBool();

        MObject meshObject = meshDataHandle.asMesh();
        MFnMesh mesh(meshObject, &stat);

        double3& translationOffset = offsetTransHandle.asDouble3();
        double3& tempRotationOffset = offsetRotHandle.asDouble3();

        //stuff we're going to use to store the poly normal etc
        MVector normalVec;
        MVector upVec;
        MVector forwardVec;
        MPoint position;

        //get the correct rotation order from the users selection for use when we get the rotation
        //values from the matrix
        MTransformationMatrix::RotationOrder order;
        MEulerRotation::RotationOrder eulerOrder;
        switch(rotOrder) 
        {
            case ROT_ORDER_XYZ:
                order = MTransformationMatrix::kXYZ;
                eulerOrder = MEulerRotation::kXYZ;
                break;
            case ROT_ORDER_YZX:
                order = MTransformationMatrix::kYZX;
                eulerOrder = MEulerRotation::kYZX;
                break;
            case ROT_ORDER_ZXY:
                order = MTransformationMatrix::kZXY;
                eulerOrder = MEulerRotation::kZXY;
                break;
            case ROT_ORDER_XZY:
                order = MTransformationMatrix::kXZY;
                eulerOrder = MEulerRotation::kZXY;
                break;
            case ROT_ORDER_YXZ:
                order = MTransformationMatrix::kYXZ;
                eulerOrder = MEulerRotation::kYXZ;
                break;
            case ROT_ORDER_ZYX:
                order = MTransformationMatrix::kZYX;
                eulerOrder = MEulerRotation::kZYX;
                break;
            default:
                order = MTransformationMatrix::kXYZ;
                eulerOrder = MEulerRotation::kXYZ;
        }

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

            //create a mesh iterator and set its index to the users index value
            MItMeshVertex meshVertIter(meshObject);
            int notNeeded;
            meshVertIter.setIndex(ind, notNeeded);

            //get an array of face indices that are connected to this vertex
            MIntArray connectedFaces;
            meshVertIter.getConnectedFaces(connectedFaces);

            //get the vert normalnd position
            mesh.getVertexNormal(ind, normalVec, MSpace::kWorld);
            mesh.getPoint(ind, position, MSpace::kWorld);

            //get the tangent using the first item in the list of faces found earlier
            if (connectedFaces.length() > 0)
            {
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

        //obtain an offset for the rotation
        MEulerRotation rotationOffset(tempRotationOffset[0], tempRotationOffset[1], tempRotationOffset[2], eulerOrder);

        //starting matrix that got computed from the face / vertex and its normals etc
        MMatrix faceMatrix(temp4x4);

        //put together the offset translation matrix
        double offset4x4[4][4] = {  0.0,                    0.0,                    0.0,                    0.0,
                                    0.0,                    0.0,                    0.0,                    0.0,
                                    0.0,                    0.0,                    0.0,                    0.0,
                                    translationOffset[0],   translationOffset[1],   translationOffset[2],   1.0};
        //blank the last row to make it a rotation only matrix
        MMatrix rotationOnlyMatrix = faceMatrix;
        //set the translation values to default to make it a rotation only matrix 
        rotationOnlyMatrix[3][0] = 0.0;
        rotationOnlyMatrix[3][1] = 0.0;
        rotationOnlyMatrix[3][2] = 0.0;
        rotationOnlyMatrix[3][3] = 0.0;

        //rotate the translation offset matrix by the rotation matrix so the
        //offset ends up in the same space as the normal
        MMatrix tempOffsetMatrix(offset4x4);
        MMatrix offsetMatrix = tempOffsetMatrix * rotationOnlyMatrix;

        //compute the final matrix
        MMatrix finalMatrix = (faceMatrix + offsetMatrix) * sourceWorldMatrix * targetParentInverseMatrix;
        MTransformationMatrix finalTransform(finalMatrix);

        //store the final translation and rotation
        double finalRotation[3];
        MVector finalTranslation = finalTransform.getTranslation(MSpace::kWorld);
        finalTransform.getRotation(finalRotation, order);

        //add the rotation offsets
        finalRotation[0] += rotationOffset.x;
        finalRotation[1] += rotationOffset.y;
        finalRotation[2] += rotationOffset.z;

        #ifdef _DEBUG
            cout << "finalTranslation: " << finalTranslation.x << " " << finalTranslation.y << " " << finalTranslation.z << endl;
            cout << "finalRotation: " << finalRotation[0] * RADTODEG << " " << finalRotation[1] * RADTODEG << " " << finalRotation[2] * RADTODEG << endl;
            cout << "translationOffset: " << translationOffset[0] << " " << translationOffset[1] << " " << translationOffset[2] << endl;
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

    offsetT = compAttr.create("offsetT", "offt");
    compAttr.setKeyable(false);
    offsetTX = unitAttr.create("offsetTX", "offtx", MFnUnitAttribute::kDistance);
    unitAttr.setKeyable(false);
    offsetTY = unitAttr.create("offsetTY", "offty", MFnUnitAttribute::kDistance);
    unitAttr.setKeyable(false);
    offsetTZ = unitAttr.create("offsetTZ", "offtz", MFnUnitAttribute::kDistance);
    unitAttr.setKeyable(false);

    compAttr.addChild(offsetTX);
    compAttr.addChild(offsetTY);
    compAttr.addChild(offsetTZ);
    addAttribute(offsetT);

    offsetR = compAttr.create("offsetR", "offr");
    compAttr.setKeyable(false);
    offsetRX = unitAttr.create("offsetRX", "offrx", MFnUnitAttribute::kAngle);
    unitAttr.setKeyable(false);
    offsetRY = unitAttr.create("offsetRY", "offry", MFnUnitAttribute::kAngle);
    unitAttr.setKeyable(false);
    offsetRZ = unitAttr.create("offsetRZ", "offrz", MFnUnitAttribute::kAngle);
    unitAttr.setKeyable(false);

    compAttr.addChild(offsetRX);
    compAttr.addChild(offsetRY);
    compAttr.addChild(offsetRZ);
    addAttribute(offsetR);

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

    attributeAffects(offsetTX, outputTX);
    attributeAffects(offsetTY, outputTY);
    attributeAffects(offsetTZ, outputTZ);
    attributeAffects(offsetTX, outputRX);
    attributeAffects(offsetTY, outputRY);
    attributeAffects(offsetTZ, outputRZ);

    attributeAffects(offsetRX, outputTX);
    attributeAffects(offsetRY, outputTY);
    attributeAffects(offsetRZ, outputTZ);
    attributeAffects(offsetRX, outputRX);
    attributeAffects(offsetRY, outputRY);
    attributeAffects(offsetRZ, outputRZ);

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