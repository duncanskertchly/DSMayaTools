#include <maya/MStatus.h>
#include <maya/MArgList.h>
#include <maya/MSyntax.h>
#include <maya/MPxCommand.h>

class ClosestVertex : public MPxCommand
{
public:

			 ClosestVertex();
	virtual ~ClosestVertex();
	MStatus doIt(const MArgList& args);

	static MSyntax newSyntax();

	bool isUndoable() const;
	static void* creator();
};
