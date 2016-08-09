import maya.cmds as cmds

def DS_ConnectToNormalAlign(vertexMode = False):

    tempSel = cmds.ls(sl = True, l = True)
    if not len(tempSel) == 2:
        return False
        
    source = tempSel[0]
    target = tempSel[1]
    
    sourceTransform = tempSel[0].split('.')[0]
    
    index = int(source.split('[')[-1].split(']')[0])
    shapes = cmds.listRelatives(sourceTransform, s = True)
    if not shapes:
        return False
        
    sourceShape = shapes[0]
    
    normalAlign = cmds.createNode('NormalAlign')
    
    if not (cmds.getAttr(target +'.translate', se = True)):
        return False
    if not (cmds.getAttr(target +'.rotate', se = True)):
        return False
        
    cmds.connectAttr(sourceShape +'.outMesh', normalAlign +'.sourceShape')
    cmds.connectAttr(sourceTransform +'.worldMatrix[0]', normalAlign +'.sourceWorldMatrix')
    cmds.connectAttr(target +'.parentInverseMatrix', normalAlign +'.targetParentInverseMatrix')
    cmds.connectAttr(target +'.rotateOrder', normalAlign +'.targetRotateOrder')
    cmds.connectAttr(normalAlign +'.outputT', target +'.translate')
    cmds.connectAttr(normalAlign +'.outputR', target +'.rotate')
    cmds.setAttr(normalAlign +'.index', index)

    if vertexMode:
        cmds.setAttr(normalAlign +'.vertexMode', 1)