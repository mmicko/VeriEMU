// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Replicate modules for parameterization
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
// PARAM TRANSFORMATIONS:
//   Top down traversal:
//	For each cell:
//	    If parameterized,
//		Determine all parameter widths, constant values.
//		(Interfaces also matter, as if an interface is parameterized
//		this effectively changes the width behavior of all that
//		reference the iface.)
//	    	Clone module cell calls, renaming with __{par1}_{par2}_...
//		Substitute constants for cell's module's parameters.
//		Relink pins and cell and ifacerefdtype to point to new module.
//
//		For interface Parent's we have the AstIfaceRefDType::cellp()
//		pointing to this module.  If that parent cell's interface
//		module gets parameterized, AstIfaceRefDType::cloneRelink
//		will update AstIfaceRefDType::cellp(), and AstLinkDot will
//		see the new interface.
//
//		However if a submodule's AstIfaceRefDType::ifacep() points
//		to the old (unparameterized) interface and needs correction.
//		To detect this we must walk all pins looking for interfaces
//		that the parent has changed and propagate down.
//
//	    Then process all modules called by that cell.
//	    (Cells never referenced after parameters expanded must be ignored.)
//
//   After we complete parameters, the varp's will be wrong (point to old module)
//   and must be relinked.
//
//*************************************************************************

#include "config_build.h"
#include "verilatedos.h"
#include <cstdio>
#include <cstdarg>
#include <unistd.h>
#include <map>
#include <vector>
#include <deque>

#include "V3Global.h"
#include "V3Param.h"
#include "V3Ast.h"
#include "V3Case.h"
#include "V3Const.h"
#include "V3Width.h"
#include "V3Unroll.h"

//######################################################################
// Param state, as a visitor of each AstNode

class ParamVisitor : public AstNVisitor {
private:
    // NODE STATE
    //	 AstNodeModule::user5()	// bool	  True if processed
    //	 AstGenFor::user5()	// bool	  True if processed
    //	 AstVar::user5()	// bool	  True if constant propagated
    //   AstVar::user4()	// int    Global parameter number (for naming new module)
    //				//        (0=not processed, 1=iterated, but no number, 65+ parameter numbered)
    AstUser4InUse	m_inuser4;
    AstUser5InUse	m_inuser5;
    // User1/2/3 used by constant function simulations

    // TYPES
    typedef deque<pair<AstIfaceRefDType*,AstIfaceRefDType*> > IfaceRefRefs;  // Note may have duplicate entries

    // STATE
    typedef map<AstNode*,AstNode*> CloneMap;
    struct ModInfo {
	AstNodeModule*	m_modp;		// Module with specified name
	CloneMap	m_cloneMap;	// Map of old-varp -> new cloned varp
	explicit ModInfo(AstNodeModule* modp) { m_modp=modp; }
    };
    typedef map<string,ModInfo> ModNameMap;
    ModNameMap	m_modNameMap;	// Hash of created module flavors by name

    typedef map<string,string> LongMap;
    LongMap	m_longMap;	// Hash of very long names to unique identity number
    int		m_longId;

    typedef map<AstNode*,int> ValueMap;
    typedef map<int,int> NextValueMap;
    ValueMap	m_valueMap;	// Hash of node to param value
    NextValueMap m_nextValueMap;// Hash of param value to next value to be used

    typedef multimap<int,AstNodeModule*> LevelModMap;
    LevelModMap	m_todoModps;	// Modules left to process

    typedef deque<AstCell*> CellList;
    CellList	m_cellps;	// Cells left to process (in this module)

    AstNodeModule* m_modp;  // Current module being processed

    string m_unlinkedTxt;	// Text for AstUnlinkedRef

    // METHODS
    static int debug() {
	static int level = -1;
	if (VL_UNLIKELY(level < 0)) level = v3Global.opt.debugSrcLevel(__FILE__);
	return level;
    }

    void makeSmallNames(AstNodeModule* modp) {
	vector<int> usedLetter; usedLetter.resize(256);
	// Pass 1, assign first letter to each gparam's name
	for (AstNode* stmtp = modp->stmtsp(); stmtp; stmtp=stmtp->nextp()) {
	    if (AstVar* varp = stmtp->castVar()) {
		if (varp->isGParam()||varp->isIfaceRef()) {
		    char ch = varp->name()[0];
		    ch = toupper(ch); if (ch<'A' || ch>'Z') ch='Z';
		    varp->user4(usedLetter[static_cast<int>(ch)]*256 + ch);
		    usedLetter[static_cast<int>(ch)]++;
		}
	    } else if (AstParamTypeDType* typep = stmtp->castParamTypeDType()) {
	        char ch = 'T';
	        typep->user4(usedLetter[static_cast<int>(ch)]*256 + ch);
	        usedLetter[static_cast<int>(ch)]++;
	    }
	}
    }
    string paramSmallName(AstNodeModule* modp, AstNode* varp) {
	if (varp->user4()<=1) {
	    makeSmallNames(modp);
	}
	int index = varp->user4()/256;
	char ch   = varp->user4()&255;
	string st = cvtToStr(ch);
	while (index) {
	    st += cvtToStr(char((index%25)+'A'));
	    index /= 26;
	}
	return st;
    }
    string paramValueNumber(AstNode* nodep) {
	// Given a complicated object create a number to use for param module assignment
	// Ideally would be relatively stable if design changes (not use pointer value),
	// and must return same value given same input node
	// Return must presently be numeric so doesn't collide with 'small' alphanumeric parameter names
	ValueMap::iterator it = m_valueMap.find(nodep);
	if (it != m_valueMap.end()) {
	    return cvtToStr(it->second);
	} else {
	    static int BUCKETS = 1000;
	    V3Hash hash (nodep->name());
	    int bucket = hash.hshval() % BUCKETS;
	    int offset = 0;
	    NextValueMap::iterator it = m_nextValueMap.find(bucket);
	    if (it != m_nextValueMap.end()) { offset = it->second; it->second = offset + 1; }
	    else { m_nextValueMap.insert(make_pair(bucket, offset + 1)); }
	    int num = bucket + offset * BUCKETS;
	    m_valueMap.insert(make_pair(nodep, num));
	    // 'z' just to make sure we don't collide with a normal non-hashed number
	    return (string)"z"+cvtToStr(num);
	}
    }
    void collectPins(CloneMap* clonemapp, AstNodeModule* modp) {
	// Grab all I/O so we can remap our pins later
	for (AstNode* stmtp=modp->stmtsp(); stmtp; stmtp = stmtp->nextp()) {
	    if (AstVar* varp = stmtp->castVar()) {
		if (varp->isIO() || varp->isGParam() || varp->isIfaceRef()) {
		    // Cloning saved a pointer to the new node for us, so just follow that link.
		    AstVar* oldvarp = varp->clonep();
		    //UINFO(8,"Clone list 0x"<<hex<<(uint32_t)oldvarp<<" -> 0x"<<(uint32_t)varp<<endl);
		    clonemapp->insert(make_pair(oldvarp, varp));
		}
	    }
	    else if (AstParamTypeDType* ptp = stmtp->castParamTypeDType()) {
		if (ptp->isGParam()) {
		    AstParamTypeDType* oldptp = ptp->clonep();
		    clonemapp->insert(make_pair(oldptp, ptp));
		}
	    }
	}
    }
    void relinkPins(CloneMap* clonemapp, AstPin* startpinp) {
	for (AstPin* pinp = startpinp; pinp; pinp=pinp->nextp()->castPin()) {
	    if (pinp->modVarp()) {
		// Find it in the clone structure
		//UINFO(8,"Clone find 0x"<<hex<<(uint32_t)pinp->modVarp()<<endl);
		CloneMap::iterator cloneiter = clonemapp->find(pinp->modVarp());
		UASSERT(cloneiter != clonemapp->end(), "Couldn't find pin in clone list");
		pinp->modVarp(cloneiter->second->castVar());
	    }
	    else if (pinp->modPTypep()) {
		CloneMap::iterator cloneiter = clonemapp->find(pinp->modPTypep());
		UASSERT(cloneiter != clonemapp->end(), "Couldn't find pin in clone list");
		pinp->modPTypep(cloneiter->second->castParamTypeDType());
	    }
	    else {
		pinp->v3fatalSrc("Not linked?");
	    }
	}
    }
    void visitCell(AstCell* nodep);
    void visitModules() {
	// Loop on all modules left to process
	// Hitting a cell adds to the appropriate level of this level-sorted list,
	// so since cells originally exist top->bottom we process in top->bottom order too.
	while (!m_todoModps.empty()) {
	    LevelModMap::iterator it = m_todoModps.begin();
	    AstNodeModule* nodep = it->second;
	    m_todoModps.erase(it);
	    if (!nodep->user5SetOnce()) {  // Process once; note clone() must clear so we do it again
		m_modp = nodep;
		UINFO(4," MOD   "<<nodep<<endl);
		nodep->iterateChildren(*this);
		// Note above iterate may add to m_todoModps
		//
		// Process interface cells, then non-interface which may ref an interface cell
		for (int nonIf=0; nonIf<2; ++nonIf) {
		    for (CellList::iterator it=m_cellps.begin(); it!=m_cellps.end(); ++it) {
			AstCell* nodep = *it;
			if ((nonIf==0 && nodep->modp()->castIface())
			    || (nonIf==1 && !nodep->modp()->castIface())) {
			    visitCell(nodep);
			}
		    }
		}
		m_cellps.clear();
		m_modp = NULL;
	    }
	}
    }

    // VISITORS
    virtual void visit(AstNetlist* nodep) {
	// Modules must be done in top-down-order
	nodep->iterateChildren(*this);
    }
    virtual void visit(AstNodeModule* nodep) {
	if (nodep->dead()) {
	    UINFO(4," MOD-dead.  "<<nodep<<endl);  // Marked by LinkDot
	} else if (nodep->recursiveClone()) {
	    UINFO(4," MOD-recursive-dead.  "<<nodep<<endl);  // Fake, made for recursive elimination
	    nodep->dead(true);  // So Dead checks won't count references to it
	} else if (nodep->level() <= 2  // Haven't added top yet, so level 2 is the top
		   || nodep->castPackage()) {	// Likewise haven't done wrapTopPackages yet
	    // Add request to END of modules left to process
	    m_todoModps.insert(make_pair(nodep->level(), nodep));
	    visitModules();
	} else if (nodep->user5()) {
	    UINFO(4," MOD-done   "<<nodep<<endl);  // Already did it
	} else {
	    UINFO(4," MOD-dead?  "<<nodep<<endl);  // Should have been done by now, if not dead
	}
    }
    virtual void visit(AstCell* nodep) {
	// Must do ifaces first, so push to list and do in proper order
	m_cellps.push_back(nodep);
    }

    // Make sure all parameters are constantified
    virtual void visit(AstVar* nodep) {
	if (!nodep->user5SetOnce()) {  // Process once
	    nodep->iterateChildren(*this);
	    if (nodep->isParam()) {
		if (!nodep->valuep()) { nodep->v3fatalSrc("Parameter without initial value"); }
		V3Const::constifyParamsEdit(nodep);  // The variable, not just the var->init()
		if (!nodep->valuep()->castConst()) {  // Complex init, like an array
		    // Make a new INITIAL to set the value.
		    // This allows the normal array/struct handling code to properly initialize the parameter
		    nodep->addNext(new AstInitial(nodep->fileline(),
						  new AstAssign(nodep->fileline(),
								new AstVarRef(nodep->fileline(), nodep, true),
								nodep->valuep()->cloneTree(true))));
		}
	    }
	}
    }
    // Make sure varrefs cause vars to constify before things above
    virtual void visit(AstVarRef* nodep) {
	if (nodep->varp()) nodep->varp()->iterate(*this);
    }
    bool ifaceParamReplace(AstVarXRef* nodep, AstNode* candp) {
	for (; candp; candp = candp->nextp()) {
	    if (nodep->name() == candp->name()) {
		if (AstVar* varp = candp->castVar()) {
		    UINFO(9,"Found interface parameter: "<<varp<<endl);
		    nodep->varp(varp);
		    return true;
		} else if (AstPin* pinp = candp->castPin()) {
		    UINFO(9,"Found interface parameter: "<<pinp<<endl);
		    if (!pinp->exprp()) pinp->v3fatalSrc("Interface parameter pin missing expression");
		    nodep->replaceWith(pinp->exprp()->cloneTree(false)); VL_DANGLING(nodep);
		    return true;
		}
	    }
	}
	return false;
    }
    virtual void visit(AstVarXRef* nodep) {
	// Check to see if the scope is just an interface because interfaces are special
	string dotted = nodep->dotted();
	if (!dotted.empty() && nodep->varp() && nodep->varp()->isParam()) {
	    AstNode* backp = nodep;
	    while ((backp = backp->backp())) {
		if (backp->castNodeModule()) {
		    UINFO(9,"Hit module boundary, done looking for interface"<<endl);
		    break;
		}
		if (backp->castVar()
		    && backp->castVar()->isIfaceRef()
		    && backp->castVar()->childDTypep()
		    && backp->castVar()->childDTypep()->castIfaceRefDType()) {
		    AstIfaceRefDType* ifacerefp = backp->castVar()->childDTypep()->castIfaceRefDType();
		    // Interfaces passed in on the port map have ifaces
		    if (AstIface* ifacep = ifacerefp->ifacep()) {
			if (dotted == backp->name()) {
			    UINFO(9,"Iface matching scope:  "<<ifacep<<endl);
			    if (ifaceParamReplace(nodep, ifacep->stmtsp())) {
				return;
			    }
			}
		    }
		    // Interfaces declared in this module have cells
		    else if (AstCell* cellp = ifacerefp->cellp()) {
			if (dotted == cellp->name()) {
			    UINFO(9,"Iface matching scope:  "<<cellp<<endl);
			    if (ifaceParamReplace(nodep, cellp->paramsp())) {
				return;
			    }
			}
		    }
		}
	    }
	}
	nodep->varp(NULL);  // Needs relink, as may remove pointed-to var
    }

    virtual void visit(AstUnlinkedRef* nodep) {
	AstVarXRef* varxrefp = nodep->op1p()->castVarXRef();
	AstNodeFTaskRef* taskrefp = nodep->op1p()->castNodeFTaskRef();
	if (varxrefp) {
	    m_unlinkedTxt = varxrefp->dotted();
	} else if (taskrefp) {
	    m_unlinkedTxt = taskrefp->dotted();
	} else {
	    nodep->v3fatalSrc("Unexpected AstUnlinkedRef node");
	    return;
	}
	nodep->cellrefp()->iterate(*this);

	if (varxrefp) {
	    varxrefp->dotted(m_unlinkedTxt);
	} else {
	    taskrefp->dotted(m_unlinkedTxt);
	}
	nodep->replaceWith(nodep->op1p()->unlinkFrBack());
	pushDeletep(nodep); VL_DANGLING(nodep);
    }
    virtual void visit(AstCellArrayRef* nodep) {
	V3Const::constifyParamsEdit(nodep->selp());
	if (AstConst* constp = nodep->selp()->castConst()) {
	    string index = AstNode::encodeNumber(constp->toSInt());
	    string replacestr = nodep->name() + "__BRA__??__KET__";
	    size_t pos = m_unlinkedTxt.find(replacestr);
	    if (pos == string::npos) {
		nodep->v3error("Could not find array index in unlinked text: '" << m_unlinkedTxt << "' for node: " << nodep);
		return;
	    }
	    m_unlinkedTxt.replace(pos, replacestr.length(), nodep->name() + "__BRA__"+index+"__KET__");
	} else {
	    nodep->v3error("Could not expand constant selection inside dotted reference: "<<nodep->selp()->prettyName());
	    return;
	}
    }

    // Generate Statements
    virtual void visit(AstGenerate* nodep) {
	if (debug()>=9) nodep->dumpTree(cout,"-genin: ");
	nodep->iterateChildren(*this);
	// After expanding the generate, all statements under it can be moved
	// up, and the generate block deleted as it's not relevant
	if (AstNode* stmtsp = nodep->stmtsp()) {
	    stmtsp->unlinkFrBackWithNext();
	    nodep->replaceWith(stmtsp);
	    if (debug()>=9) stmtsp->dumpTree(cout,"-genout: ");
	} else {
	    nodep->unlinkFrBack();
	}
	nodep->deleteTree(); VL_DANGLING(nodep);
    }
    virtual void visit(AstGenIf* nodep) {
	UINFO(9,"  GENIF "<<nodep<<endl);
	nodep->condp()->iterateAndNext(*this);
	// We suppress errors when widthing params since short-circuiting in
	// the conditional evaluation may mean these error can never occur. We
	// then make sure that short-circuiting is used by constifyParamsEdit.
	V3Width::widthGenerateParamsEdit(nodep);  // Param typed widthing will
						  // NOT recurse the body.
	V3Const::constifyGenerateParamsEdit(nodep->condp()); // condp may change
	if (AstConst* constp = nodep->condp()->castConst()) {
	    AstNode* keepp = (constp->isZero()
			      ? nodep->elsesp()
			      : nodep->ifsp());
	    if (keepp) {
		keepp->unlinkFrBackWithNext();
		nodep->replaceWith(keepp);
	    } else {
		nodep->unlinkFrBack();
	    }
	    nodep->deleteTree(); VL_DANGLING(nodep);
	    // Normal edit rules will now recurse the replacement
	} else {
	    nodep->condp()->v3error("Generate If condition must evaluate to constant");
	}
    }

    //! Parameter subsitution for generated for loops.
    //! @todo Unlike generated IF, we don't have to worry about short-circuiting the conditional
    //!       expression, since this is currently restricted to simple comparisons. If we ever do
    //!       move to more generic constant expressions, such code will be needed here.
    virtual void visit(AstBegin* nodep) {
	if (nodep->genforp()) {
	    AstGenFor* forp = nodep->genforp()->castGenFor();
	    if (!forp) nodep->v3fatalSrc("Non-GENFOR under generate-for BEGIN");
	    // We should have a GENFOR under here.  We will be replacing the begin,
	    // so process here rather than at the generate to avoid iteration problems
	    UINFO(9,"  BEGIN "<<nodep<<endl);
	    UINFO(9,"  GENFOR "<<forp<<endl);
	    V3Width::widthParamsEdit(forp);  // Param typed widthing will NOT recurse the body
	    // Outer wrapper around generate used to hold genvar, and to insure genvar
	    // doesn't conflict in V3LinkDot resolution with other genvars
	    // Now though we need to change BEGIN("zzz",GENFOR(...)) to
	    // a BEGIN("zzz__BRA__{loop#}__KET__")
	    string beginName = nodep->name();
	    // Leave the original Begin, as need a container for the (possible) GENVAR
	    // Note V3Unroll will replace some AstVarRef's to the loop variable with constants
	    V3Unroll::unrollGen(forp, beginName); VL_DANGLING(forp);
	    // Blocks were constructed under the special begin, move them up
	    // Note forp is null, so grab statements again
	    if (AstNode* stmtsp = nodep->genforp()) {
		stmtsp->unlinkFrBackWithNext();
		nodep->addNextHere(stmtsp);
		// Note this clears nodep->genforp(), so begin is no longer special
	    }
	} else {
	    nodep->iterateChildren(*this);
	}
    }
    virtual void visit(AstGenFor* nodep) {
	nodep->v3fatalSrc("GENFOR should have been wrapped in BEGIN");
    }
    virtual void visit(AstGenCase* nodep) {
	UINFO(9,"  GENCASE "<<nodep<<endl);
	AstNode* keepp = NULL;
	nodep->exprp()->iterateAndNext(*this);
	V3Case::caseLint(nodep);
	V3Width::widthParamsEdit(nodep);  // Param typed widthing will NOT recurse the body,
					  // don't trigger errors yet.
	V3Const::constifyParamsEdit(nodep->exprp());  // exprp may change
	AstConst* exprp = nodep->exprp()->castConst();
	// Constify
	for (AstCaseItem* itemp = nodep->itemsp(); itemp; itemp=itemp->nextp()->castCaseItem()) {
	    for (AstNode* ep = itemp->condsp(); ep; ) {
		AstNode* nextp = ep->nextp(); //May edit list
		ep->iterateAndNext(*this);
		V3Const::constifyParamsEdit(ep); VL_DANGLING(ep); // ep may change
		ep = nextp;
	    }
	}
	// Item match
	for (AstCaseItem* itemp = nodep->itemsp(); itemp; itemp=itemp->nextp()->castCaseItem()) {
	    if (!itemp->isDefault()) {
		for (AstNode* ep = itemp->condsp(); ep; ep=ep->nextp()) {
		    if (AstConst* ccondp = ep->castConst()) {
			V3Number match (nodep->fileline(), 1);
			match.opEq(ccondp->num(), exprp->num());
			if (!keepp && match.isNeqZero()) {
			    keepp = itemp->bodysp();
			}
		    } else {
			itemp->v3error("Generate Case item does not evaluate to constant");
		    }
		}
	    }
	}
	// Else default match
	for (AstCaseItem* itemp = nodep->itemsp(); itemp; itemp=itemp->nextp()->castCaseItem()) {
	    if (itemp->isDefault()) {
		if (!keepp) keepp=itemp->bodysp();
	    }
	}
	// Replace
	if (keepp) {
	    keepp->unlinkFrBackWithNext();
	    nodep->replaceWith(keepp);
	}
	else nodep->unlinkFrBack();
	nodep->deleteTree(); VL_DANGLING(nodep);
    }

    // Default: Just iterate
    virtual void visit(AstNode* nodep) {
	nodep->iterateChildren(*this);
    }

public:
    // CONSTUCTORS
    explicit ParamVisitor(AstNetlist* nodep) {
	m_longId = 0;
	m_modp = NULL;
	//
	nodep->accept(*this);
    }
    virtual ~ParamVisitor() {}
};

//----------------------------------------------------------------------
// VISITs

void ParamVisitor::visitCell(AstCell* nodep) {
    // Cell: Check for parameters in the instantiation.
    nodep->iterateChildren(*this);
    if (!nodep->modp()) nodep->v3fatalSrc("Not linked?");
    // We always run this, even if no parameters, as need to look for interfaces,
    // and remove any recursive references
    {
	UINFO(4,"De-parameterize: "<<nodep<<endl);
	// Create new module name with _'s between the constants
	if (debug()>=10) nodep->dumpTree(cout,"-cell:\t");
	// Evaluate all module constants
	V3Const::constifyParamsEdit(nodep);
	AstNodeModule* srcModp = nodep->modp();

	// Make sure constification worked
	// Must be a separate loop, as constant conversion may have changed some pointers.
	//if (debug()) nodep->dumpTree(cout,"-cel2:\t");
	string longname = srcModp->name();
	bool any_overrides = false;
	if (nodep->recursive()) any_overrides = true;  // Must always clone __Vrcm (recursive modules)
	longname += "_";
	if (debug()>8) nodep->paramsp()->dumpTreeAndNext(cout,"-cellparams:\t");
	for (AstPin* pinp = nodep->paramsp(); pinp; pinp=pinp->nextp()->castPin()) {
	    if (!pinp->exprp()) continue; // No-connect
	    if (AstVar* modvarp = pinp->modVarp()) {
		if (!modvarp->isGParam()) {
		    pinp->v3error("Attempted parameter setting of non-parameter: Param "<<pinp->prettyName()<<" of "<<nodep->prettyName());
		} else if (pinp->exprp()->castInitArray()
			   && modvarp->subDTypep()->castUnpackArrayDType()) {
		    // Array assigned to array
		    AstNode* exprp = pinp->exprp();
		    longname += "_" + paramSmallName(srcModp, modvarp) + paramValueNumber(exprp);
		    any_overrides = true;
		} else {
		    AstConst* exprp = pinp->exprp()->castConst();
		    AstConst* origp = modvarp->valuep()->castConst();
		    if (!exprp) {
			//if (debug()) pinp->dumpTree(cout,"error:");
			pinp->v3error("Can't convert defparam value to constant: Param "<<pinp->name()<<" of "<<nodep->prettyName());
			pinp->exprp()->replaceWith(new AstConst(pinp->fileline(), V3Number(pinp->fileline(), modvarp->width(), 0)));
		    } else if (origp && exprp->sameTree(origp)) {
			// Setting parameter to its default value.  Just ignore it.
			// This prevents making additional modules, and makes coverage more
			// obvious as it won't show up under a unique module page name.
		    } else if (exprp->num().isDouble()
			       || exprp->num().isString()
			       ||  exprp->num().isFourState()) {
			longname += "_" + paramSmallName(srcModp, modvarp) + paramValueNumber(exprp);
			any_overrides = true;
		    } else {
			longname += "_" + paramSmallName(srcModp, modvarp) + exprp->num().ascii(false);
			any_overrides = true;
		    }
		}
	    } else if (AstParamTypeDType* modvarp = pinp->modPTypep()) {
		AstNodeDType* exprp = pinp->exprp()->castNodeDType();
		AstNodeDType* origp = modvarp->subDTypep();
		if (!exprp) {
		    pinp->v3error("Parameter type pin value isn't a type: Param "<<pinp->prettyName()<<" of "<<nodep->prettyName());
		} else if (!origp) {
		    pinp->v3error("Parameter type variable isn't a type: Param "<<modvarp->prettyName());
		} else {
		    UINFO(9,"Parameter type assignment expr="<<exprp<<" to "<<origp<<endl);
		    if (origp && exprp->sameTree(origp)) {
			// Setting parameter to its default value.  Just ignore it.
			// This prevents making additional modules, and makes coverage more
			// obvious as it won't show up under a unique module page name.
		    } else {
			longname += "_" + paramSmallName(srcModp, modvarp) + paramValueNumber(exprp);
			any_overrides = true;
		    }
		}
	    } else {
		pinp->v3error("Parameter not found in sub-module: Param "<<pinp->prettyName()<<" of "<<nodep->prettyName());
	    }
	}
	IfaceRefRefs ifaceRefRefs;
	for (AstPin* pinp = nodep->pinsp(); pinp; pinp=pinp->nextp()->castPin()) {
	    AstVar* modvarp = pinp->modVarp();
	    if (modvarp->isIfaceRef()) {
		AstIfaceRefDType* portIrefp = modvarp->subDTypep()->castIfaceRefDType();
		if (!portIrefp && modvarp->subDTypep()->castUnpackArrayDType()) {
		    portIrefp = modvarp->subDTypep()->castUnpackArrayDType()->subDTypep()->castIfaceRefDType();
		}

		AstIfaceRefDType* pinIrefp = NULL;
		AstNode* exprp = pinp->exprp();
		if (exprp
		    && exprp->castVarRef()
		    && exprp->castVarRef()->varp()
		    && exprp->castVarRef()->varp()->subDTypep()
		    && exprp->castVarRef()->varp()->subDTypep()->castIfaceRefDType())
		    pinIrefp = exprp->castVarRef()->varp()->subDTypep()->castIfaceRefDType();
		else if (exprp
			 && exprp->op1p()
			 && exprp->op1p()->castVarRef()
			 && exprp->op1p()->castVarRef()->varp()
			 && exprp->op1p()->castVarRef()->varp()->subDTypep()
			 && exprp->op1p()->castVarRef()->varp()->subDTypep()->castUnpackArrayDType()
			 && exprp->op1p()->castVarRef()->varp()->subDTypep()->castUnpackArrayDType()->subDTypep()
			 && exprp->op1p()->castVarRef()->varp()->subDTypep()->castUnpackArrayDType()->subDTypep()->castIfaceRefDType())
		    pinIrefp = exprp->op1p()->castVarRef()->varp()->subDTypep()->castUnpackArrayDType()->subDTypep()->castIfaceRefDType();
		else if (exprp
			 && exprp->castVarRef()
			 && exprp->castVarRef()->varp()
			 && exprp->castVarRef()->varp()->subDTypep()
			 && exprp->castVarRef()->varp()->subDTypep()->castUnpackArrayDType()
			 && exprp->castVarRef()->varp()->subDTypep()->castUnpackArrayDType()->subDTypep()
			 && exprp->castVarRef()->varp()->subDTypep()->castUnpackArrayDType()->subDTypep()->castIfaceRefDType())
		    pinIrefp = exprp->castVarRef()->varp()->subDTypep()->castUnpackArrayDType()->subDTypep()->castIfaceRefDType();

		UINFO(9,"     portIfaceRef "<<portIrefp<<endl);

		if (!portIrefp) {
		    pinp->v3error("Interface port '"<<modvarp->prettyName()<<"' is not an interface " << modvarp);
		} else if (!pinIrefp) {
		    pinp->v3error("Interface port '"<<modvarp->prettyName()<<"' is not connected to interface/modport pin expression");
		} else {
		    UINFO(9,"     pinIfaceRef "<<pinIrefp<<endl);
		    if (portIrefp->ifaceViaCellp() != pinIrefp->ifaceViaCellp()) {
			UINFO(9,"     IfaceRefDType needs reconnect  "<<pinIrefp<<endl);
			longname += "_" + paramSmallName(srcModp, pinp->modVarp()) + paramValueNumber(pinIrefp);
			any_overrides = true;
			ifaceRefRefs.push_back(make_pair(portIrefp,pinIrefp));
		    }
		}
	    }
	}

	if (!any_overrides) {
	    UINFO(8,"Cell parameters all match original values, skipping expansion.\n");
	} else {
	    // If the name is very long, we don't want to overwhelm the filename limit
	    // We don't do this always, as it aids debugability to have intuitive naming.
	    // TODO can use new V3Name hash replacement instead of this
	    string newname = longname;
	    if (longname.length()>30) {
		LongMap::iterator iter = m_longMap.find(longname);
		if (iter != m_longMap.end()) {
		    newname = iter->second;
		} else {
		    newname = srcModp->name();
		    newname += "__pi"+cvtToStr(++m_longId);  // We use all upper case above, so lower here can't conflict
		    m_longMap.insert(make_pair(longname, newname));
		}
	    }
	    UINFO(4,"Name: "<<srcModp->name()<<"->"<<longname<<"->"<<newname<<endl);

	    //
	    // Already made this flavor?
	    AstNodeModule* cellmodp = NULL;
	    ModNameMap::iterator iter = m_modNameMap.find(newname);
	    if (iter != m_modNameMap.end()) cellmodp = iter->second.m_modp;
	    if (!cellmodp) {
		// Deep clone of new module
		// Note all module internal variables will be re-linked to the new modules by clone
		// However links outside the module (like on the upper cells) will not.
		cellmodp = srcModp->cloneTree(false);
		cellmodp->name(newname);
		cellmodp->user5(false); // We need to re-recurse this module once changed
		cellmodp->recursive(false);
		cellmodp->recursiveClone(false);
		nodep->recursive(false);
		// Recursion may need level cleanups
		if (cellmodp->level() <= m_modp->level()) cellmodp->level(m_modp->level()+1);
		if ((cellmodp->level() - srcModp->level()) >= (v3Global.opt.moduleRecursionDepth() - 2)) {
		    nodep->v3error("Exceeded maximum --module-recursion-depth of "<<v3Global.opt.moduleRecursionDepth());
		}
		// Keep tree sorted by level
		AstNodeModule* insertp = srcModp;
		while (insertp->nextp()->castNodeModule()
		       && insertp->nextp()->castNodeModule()->level() < cellmodp->level()) {
		    insertp = insertp->nextp()->castNodeModule();
		}
		insertp->addNextHere(cellmodp);

		m_modNameMap.insert(make_pair(cellmodp->name(), ModInfo(cellmodp)));
		iter = m_modNameMap.find(newname);
		CloneMap* clonemapp = &(iter->second.m_cloneMap);
		UINFO(4,"     De-parameterize to new: "<<cellmodp<<endl);

		// Grab all I/O so we can remap our pins later
		// Note we allow multiple users of a parameterized model, thus we need to stash this info.
		collectPins(clonemapp, cellmodp);
		// Relink parameter vars to the new module
		relinkPins(clonemapp, nodep->paramsp());

		// Fix any interface references
		for (IfaceRefRefs::iterator it=ifaceRefRefs.begin(); it!=ifaceRefRefs.end(); ++it) {
		    AstIfaceRefDType* portIrefp = it->first;
		    AstIfaceRefDType* pinIrefp = it->second;
		    AstIfaceRefDType* cloneIrefp = portIrefp->clonep();
		    UINFO(8,"     IfaceOld "<<portIrefp<<endl);
		    UINFO(8,"     IfaceTo  "<<pinIrefp<<endl);
		    if (!cloneIrefp) portIrefp->v3fatalSrc("parameter clone didn't hit AstIfaceRefDType");
		    UINFO(8,"     IfaceClo "<<cloneIrefp<<endl);
		    cloneIrefp->ifacep(pinIrefp->ifaceViaCellp());
		    UINFO(8,"     IfaceNew "<<cloneIrefp<<endl);
		}

		// Assign parameters to the constants specified
		// DOES clone() so must be finished with module clonep() before here
		for (AstPin* pinp = nodep->paramsp(); pinp; pinp=pinp->nextp()->castPin()) {
		    if (pinp->exprp()) {
			if (AstVar* modvarp = pinp->modVarp()) {
			    AstNode* newp = pinp->exprp();  // Const or InitArray
			    // Remove any existing parameter
			    if (modvarp->valuep()) modvarp->valuep()->unlinkFrBack()->deleteTree();
			    // Set this parameter to value requested by cell
			    modvarp->valuep(newp->cloneTree(false));
			}
			else if (AstParamTypeDType* modptp = pinp->modPTypep()) {
			    AstNodeDType* dtypep = pinp->exprp()->castNodeDType();
			    if (!dtypep) pinp->v3fatalSrc("unlinked param dtype");
			    if (modptp->childDTypep()) pushDeletep(modptp->childDTypep()->unlinkFrBack());
			    // Set this parameter to value requested by cell
			    modptp->childDTypep(dtypep->cloneTree(false));
			    // Later V3LinkDot will convert the ParamDType to a Typedef
			    // Not done here as may be localparams, etc, that also need conversion
			}
		    }
		}

	    } else {
		UINFO(4,"     De-parameterize to old: "<<cellmodp<<endl);
	    }

	    // Have child use this module instead.
	    nodep->modp(cellmodp);
	    nodep->modName(newname);

	    // We need to relink the pins to the new module
	    CloneMap* clonemapp = &(iter->second.m_cloneMap);
	    relinkPins(clonemapp, nodep->pinsp());
	    UINFO(8,"     Done with "<<cellmodp<<endl);
	} // if any_overrides

	nodep->recursive(false);

	// Delete the parameters from the cell; they're not relevant any longer.
	if (nodep->paramsp()) nodep->paramsp()->unlinkFrBackWithNext()->deleteTree();
	UINFO(8,"     Done with "<<nodep<<endl);
	//if (debug()>=10) v3Global.rootp()->dumpTreeFile(v3Global.debugFilename("param-out.tree"));
    }

    // Now remember to process the child module at the end of the module
    m_todoModps.insert(make_pair(nodep->modp()->level(), nodep->modp()));
}

//######################################################################
// Param class functions

void V3Param::param(AstNetlist* rootp) {
    UINFO(2,__FUNCTION__<<": "<<endl);
    ParamVisitor visitor (rootp);
    V3Global::dumpCheckGlobalTree("param", 0, v3Global.opt.dumpTreeLevel(__FILE__) >= 6);
}
