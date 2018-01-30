// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Parse module/signal name references
//
// Code available from: http://www.veripool.org/verilator
//
//*************************************************************************
//
// Copyright 2003-2018 by Wilson Snyder.  This program is free software; you can
// redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
//
// Verilator is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//*************************************************************************
// Slice TRANSFORMATIONS:
//	Top-down traversal (SliceVisitor):
//	  NODEASSIGN
//	    ARRAYSEL
//	      Compare the dimensions to the Var to check for implicit slices.
//	      Using ->length() calculate the number of clones needed.
//	    VARREF
//	      Check the dimensions of the Var for an implicit slice.
//	      Replace with ArraySel nodes if needed.
//	    SEL, EXTEND
//	      We might be assigning a 1-D packed array to a 2-D packed array,
//	      this is unsupported.
//	    SliceCloneVisitor (called if this node is a slice):
//	      NODEASSIGN
//	        Clone and iterate the clone:
//		  ARRAYSEL
//		    Modify bitp() for the new value and set ->length(1)
//
// TODO: This code was written before SLICESEL was a type it might be
// simplified to look primarily for SLICESELs.
//*************************************************************************

#include "config_build.h"
#include "verilatedos.h"
#include <cstdio>
#include <cstdarg>
#include <unistd.h>

#include "V3Global.h"
#include "V3Slice.h"
#include "V3Ast.h"

//*************************************************************************

class SliceVisitor : public AstNVisitor {
    // NODE STATE
    // Cleared on netlist
    //  AstNodeAssign::user1()	    -> bool.  True if find is complete
    //  AstUniop::user1()	    -> bool.  True if find is complete
    //  AstArraySel::user1p()	    -> AstVarRef. The VarRef that the final ArraySel points to
    AstUser1InUse	m_inuser1;

    // STATE
    AstNode*		m_assignp;	// Assignment we are under
    bool		m_assignError;	// True if the current assign already has an error

    // METHODS
    static int debug() {
	static int level = -1;
	if (VL_UNLIKELY(level < 0)) level = v3Global.opt.debugSrcLevel(__FILE__);
	return level;
    }

    AstNode* cloneAndSel(AstNode* nodep, int elements, int offset) {
	// Insert an ArraySel, except for a few special cases
	AstUnpackArrayDType* arrayp = nodep->dtypep()->skipRefp()->castUnpackArrayDType();
	if (!arrayp) {  // V3Width should have complained, but...
	    if (!m_assignError) nodep->v3error(nodep->prettyTypeName()<<" is not an unpacked array, but is in an unpacked array context");
	    m_assignError = true;
	    return nodep->cloneTree(false);  // Likely will cause downstream errors
	}
	if (arrayp->rangep()->elementsConst() != elements) {
	    if (!m_assignError) nodep->v3error("Slices of arrays in assignments have different unpacked dimensions, "
					       <<elements<<" versus "<<arrayp->rangep()->elementsConst());
	    m_assignError = true;
	    elements = 1; offset = 0;
	}
	AstNode* newp;
	if (AstInitArray* initp = nodep->castInitArray()) {
	    UINFO(9,"  cloneInitArray("<<elements<<","<<offset<<") "<<nodep<<endl);
	    AstNode* itemp = initp->initsp();
	    int leOffset = !arrayp->rangep()->littleEndian() ? arrayp->rangep()->elementsConst()-1-offset : offset;
	    for (int pos = 0; itemp && pos < leOffset; ++pos) {
		itemp = itemp->nextp();
	    }
	    if (!itemp) {
		nodep->v3error("Array initialization has too few elements, need element "<<offset);
		itemp = initp->initsp();
	    }
	    newp = itemp->cloneTree(false);
	}
	else if (AstNodeCond* snodep = nodep->castNodeCond()) {
	    UINFO(9,"  cloneCond("<<elements<<","<<offset<<") "<<nodep<<endl);
	    return snodep->cloneType(snodep->condp()->cloneTree(false),
				     cloneAndSel(snodep->expr1p(), elements, offset),
				     cloneAndSel(snodep->expr2p(), elements, offset));
	}
        else if (AstSliceSel* snodep = nodep->castSliceSel()) {
            UINFO(9,"  cloneSliceSel("<<elements<<","<<offset<<") "<<nodep<<endl);
            int leOffset = (snodep->declRange().lo()
                            + (!snodep->declRange().littleEndian() ? snodep->declRange().elements()-1-offset : offset));
            newp = new AstArraySel(nodep->fileline(), snodep->fromp()->cloneTree(false), leOffset);
        }
	else if (nodep->castArraySel()
		 || nodep->castNodeVarRef()
		 || nodep->castNodeSel()) {
	    UINFO(9,"  cloneSel("<<elements<<","<<offset<<") "<<nodep<<endl);
	    int leOffset = !arrayp->rangep()->littleEndian() ? arrayp->rangep()->elementsConst()-1-offset : offset;
	    newp = new AstArraySel(nodep->fileline(), nodep->cloneTree(false), leOffset);
	}
	else {
	    if (!m_assignError) nodep->v3error(nodep->prettyTypeName()<<" unexpected in assignment to unpacked array");
	    m_assignError = true;
	    newp = nodep->cloneTree(false);  // Likely will cause downstream errors
	}
	return newp;
    }

    virtual void visit(AstNodeAssign* nodep) {
	// Called recursively on newly created assignments
	if (!nodep->user1()
	    && !nodep->castAssignAlias()) {
	    nodep->user1(true);
	    m_assignError = false;
	    if (debug()>=9) { cout<<endl; nodep->dumpTree(cout," Deslice-In: "); }
	    AstNodeDType* dtp = nodep->lhsp()->dtypep()->skipRefp();
	    if (AstUnpackArrayDType* arrayp = dtp->castUnpackArrayDType()) {
		// Left and right could have different msb/lsbs/endianness, but #elements is common
		// and all variables are realigned to start at zero
		// Assign of a little endian'ed slice to a big endian one must reverse the elements
		AstNode* newlistp = NULL;
		int elements = arrayp->rangep()->elementsConst();
		for (int offset = 0; offset < elements; ++offset) {
		    AstNode* newp = nodep->cloneType // AstNodeAssign
			(cloneAndSel(nodep->lhsp(), elements, offset),
			 cloneAndSel(nodep->rhsp(), elements, offset));
		    if (debug()>=9) { newp->dumpTree(cout,"-new "); }
		    newlistp = AstNode::addNextNull(newlistp, newp);
		}
		if (debug()>=9) { cout<<endl; nodep->dumpTree(cout," Deslice-Dn: "); }
		nodep->replaceWith(newlistp); nodep->deleteTree(); VL_DANGLING(nodep);
		// Normal edit iterator will now iterate on all of the expansion assignments
		// This will potentially call this function again to resolve next level of slicing
		return;
	    }
	    m_assignp = nodep;
	    nodep->iterateChildren(*this);
	    m_assignp = NULL;
	}
    }

    virtual void visit(AstInitArray* nodep) {
	if (m_assignp) {
	    nodep->v3fatalSrc("Array initialization should have been removed earlier");
	}
    }

    void expandBiOp(AstNodeBiop* nodep) {
	if (!nodep->user1()) {
	    nodep->user1(true);
	    // If it's an unpacked array, blow it up into comparing each element
	    AstNodeDType* fromDtp = nodep->lhsp()->dtypep()->skipRefp();
	    UINFO(9, "  Bi-Eq/Neq expansion "<<nodep<<endl);
	    if (AstUnpackArrayDType* adtypep = fromDtp->castUnpackArrayDType()) {
		AstNodeBiop* logp = NULL;
		for (int index = 0; index < adtypep->rangep()->elementsConst(); ++index) {
		    // EQ(a,b) -> LOGAND(EQ(ARRAYSEL(a,0), ARRAYSEL(b,0)), ...[1])
		    AstNodeBiop* clonep = nodep->cloneType
			(new AstArraySel(nodep->fileline(),
					 nodep->lhsp()->cloneTree(false),
					 index),
			 new AstArraySel(nodep->fileline(),
					 nodep->rhsp()->cloneTree(false),
					 index))->castNodeBiop();
		    if (!logp) logp = clonep;
		    else {
			switch (nodep->type()) {
			case AstType::atEq:  // FALLTHRU
			case AstType::atEqCase:
			    logp = new AstLogAnd(nodep->fileline(), logp, clonep);
			    break;
			case AstType::atNeq:  // FALLTHRU
			case AstType::atNeqCase:
			    logp = new AstLogOr(nodep->fileline(), logp, clonep);
			    break;
			default:
			    nodep->v3fatalSrc("Unknown node type processing array slice");
			    break;
			}
		    }
		}
		if (!logp) nodep->v3fatalSrc("Unpacked array with empty indices range");
		nodep->replaceWith(logp);
		pushDeletep(nodep); VL_DANGLING(nodep);
		nodep = logp;
	    }
	    nodep->iterateChildren(*this);
	}
    }
    virtual void visit(AstEq* nodep) {
	expandBiOp(nodep);
    }
    virtual void visit(AstNeq* nodep) {
	expandBiOp(nodep);
    }
    virtual void visit(AstEqCase* nodep) {
	expandBiOp(nodep);
    }
    virtual void visit(AstNeqCase* nodep) {
	expandBiOp(nodep);
    }

    virtual void visit(AstNode* nodep) {
	// Default: Just iterate
	nodep->iterateChildren(*this);
    }

public:
    // CONSTUCTORS
    explicit SliceVisitor(AstNetlist* rootp) {
	m_assignp = NULL;
	m_assignError = false;
	rootp->accept(*this);
    }
    virtual ~SliceVisitor() {}
};

//######################################################################
// Link class functions

void V3Slice::sliceAll(AstNetlist* rootp) {
    UINFO(2,__FUNCTION__<<": "<<endl);
    SliceVisitor visitor(rootp);
    V3Global::dumpCheckGlobalTree("slice", 0, v3Global.opt.dumpTreeLevel(__FILE__) >= 3);
}
