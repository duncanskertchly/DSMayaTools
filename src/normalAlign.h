#include <maya/MObject.h>
#include <maya/MTypeId.h>
#include <maya/MStatus.h>
#include <maya/MPlug.h>
#include <maya/MDataBlock.h>
#include <maya/MPxNode.h>

class NormalAlign : public MPxNode
{
public:
			 NormalAlign();
    virtual ~NormalAlign();
    virtual MStatus compute(const MPlug& plug, MDataBlock& data);

    static MTypeId id;

    static MObject sourceShape;

    static MObject index;

    static MObject vertexMode;

    static MObject sourceWorldMatrix;
    static MObject targetParentInverseMatrix;

    static MObject flipTargetAimAxis;
    static MObject flipTargetUpAxis;

    static MObject targetAimAxis;
    static MObject targetUpAxis;
    static MObject targetRotateOrder;

    static MObject offsetT;
    static MObject offsetTX;
    static MObject offsetTY;
    static MObject offsetTZ;

    static MObject offsetR;
    static MObject offsetRX;
    static MObject offsetRY;
    static MObject offsetRZ;
    
    static MObject outputT;
    static MObject outputTX;
    static MObject outputTY;
    static MObject outputTZ;

    static MObject outputR;
    static MObject outputRX;
    static MObject outputRY;
    static MObject outputRZ;

    static void* creator();
    static MStatus initialize();

};
