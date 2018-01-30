// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Rename scope references to module-local references
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
// DESCOPE TRANSFORMATIONS:
//	All modules:
//	    Each VARREF/FUNCCALL
//		Change varref name() to be relative to current module
//		Remove varScopep()
//	    This allows for better V3Combine'ing.
//
//*************************************************************************

#include "config_build.h"
#include "verilatedos.h"
#include <cstdio>
#include <cstdarg>
#include <unistd.h>
#include <map>

#include "V3Global.h"
#include "V3Descope.h"
#include "V3Ast.h"
#include "V3EmitCBase.h"

//######################################################################

class DescopeVisitor : public AstNVisitor {
private:
    // NODE STATE
    //  Cleared entire netlist
    //   AstCFunc::user()		// bool.  Indicates processing completed
    AstUser1InUse	m_inuser1;

    // TYPES
    typedef multimap<string,AstCFunc*>	FuncMmap;

    // STATE
    AstNodeModule*	m_modp;		// Current module
    AstScope*		m_scopep;	// Current scope
    bool		m_modSingleton; // m_modp is only instanced once
    bool		m_needThis;	// Make function non-static
    FuncMmap		m_modFuncs;	// Name of public functions added

    // METHODS
    static int debug() {
	static int level = -1;
	if (VL_UNLIKELY(level < 0)) level = v3Global.opt.debugSrcLevel(__FILE__);
	return level;
    }

    static bool modIsSingleton(AstNodeModule* modp) {
        // True iff there's exactly one instance of this module in the design.
        int instances = 0;
        for (AstNode* stmtp = modp->stmtsp(); stmtp; stmtp=stmtp->nextp()) {
            if (stmtp->castScope()) {
                if (++instances > 1) { return false; }
            }
        }
        return (instances == 1);
    }

    // Construct the best prefix to reference an object in 'scopep'
    // from a CFunc in 'm_scopep'. Result may be relative
    // ("this->[...]") or absolute ("vlTOPp->[...]").
    //
    // Using relative references allows V3Combine'ing
    // code across multiple instances of the same module.
    //
    // Sets 'hierThisr' true if the object is local to this scope
    // (and could be made into a function-local later in V3Localize),
    // false if the object is in another scope.
    string descopedName(const AstScope* scopep, bool& hierThisr,
                        const AstVar* varp=NULL) {
	UASSERT(scopep, "Var/Func not scoped\n");
	hierThisr = (scopep == m_scopep);

        // It's possible to disable relative references. This is a concession
        // to older compilers (gcc < 4.5.x) that don't understand __restrict__
        // well and emit extra ld/st to guard against pointer aliasing
        // when this-> and vlTOPp-> are mixed in the same function.
        //
        // "vlTOPp" is declared "restrict" so better compilers understand
        // that it won't alias with "this".
        bool relativeRefOk = v3Global.opt.relativeCFuncs();

        // Use absolute refs in top-scoped routines, keep them static.
        // The DPI callback registration depends on representing top-level
        // static routines as plain function pointers. That breaks if those
        // become true OO routines.
        //
        // V3Combine wouldn't likely be able to combine top-level
        // routines anyway, so there's no harm in keeping these static.
        if (m_modp->isTop()) {
            relativeRefOk = false;
        }

        // Use absolute refs if this scope is the only instance of the module.
        // Saves a bit of overhead on passing the 'this' pointer, and there's no
        // need to be nice to V3Combine when we have only a single instance.
        // The risk that this prevents combining identical logic from differently-
        // named but identical modules seems low.
        if (m_modSingleton) {
            relativeRefOk = false;
        }

	if (varp && varp->isFuncLocal()) {
            hierThisr = true;
	    return "";  // Relative to function, not in this
        } else if (relativeRefOk && scopep == m_scopep) {
            m_needThis = true;
            return "this->";
        } else if (relativeRefOk && scopep->aboveScopep()
                   && scopep->aboveScopep()==m_scopep) {
            // Reference to scope of cell directly under this module, can just "cell->"
            string name = scopep->name();
            string::size_type pos;
            if ((pos = name.rfind(".")) != string::npos) {
                name.erase(0,pos+1);
            }
            m_needThis = true;
            return name+"->";
        } else {
            // Reference to something elsewhere, or relative refences
            // are disabled. Use global variable
            UINFO(8,"      Descope "<<scopep<<endl);
            UINFO(8,"           to "<<scopep->name()<<endl);
            UINFO(8,"        under "<<m_scopep->name()<<endl);
            if (!scopep->aboveScopep()) { // Top
                // We could also return "vlSymsp->TOPp->" here, but GCC would
                // suspect aliases.
                return "vlTOPp->";
            } else {
                return scopep->nameVlSym()+".";
            }
        }
    }

    void makePublicFuncWrappers() {
	// We recorded all public functions in m_modFuncs.
	// If for any given name only one function exists, we can use that function directly.
	// If multiple functions exist, we need to select the appropriate scope.
	for (FuncMmap::iterator it = m_modFuncs.begin(); it!=m_modFuncs.end(); ++it) {
	    string name = it->first;
	    AstCFunc* topFuncp = it->second;
	    FuncMmap::iterator nextIt1 = it; ++nextIt1;
	    bool moreOfSame1 =  (nextIt1!=m_modFuncs.end() && nextIt1->first == name);
	    if (moreOfSame1) {
		// Multiple functions under this name, need a wrapper function
		UINFO(6,"  Wrapping "<<name<<" multifuncs\n");
		AstCFunc* newfuncp = topFuncp->cloneTree(false);
		if (newfuncp->initsp())  newfuncp->initsp()->unlinkFrBackWithNext()->deleteTree();
		if (newfuncp->stmtsp())  newfuncp->stmtsp()->unlinkFrBackWithNext()->deleteTree();
		if (newfuncp->finalsp()) newfuncp->finalsp()->unlinkFrBackWithNext()->deleteTree();
		newfuncp->name(name);
		newfuncp->isStatic(false);
		newfuncp->addInitsp(
		    new AstCStmt(newfuncp->fileline(),
				 EmitCBaseVisitor::symClassVar()+" = this->__VlSymsp;\n"));
		newfuncp->addInitsp(new AstCStmt(newfuncp->fileline(), EmitCBaseVisitor::symTopAssign()+"\n"));
		topFuncp->addNextHere(newfuncp);
		// In the body, call each function if it matches the given scope
		for (FuncMmap::iterator eachIt = it; eachIt!=m_modFuncs.end() && eachIt->first==name; ++eachIt) {
		    it = eachIt;
		    AstCFunc* funcp = eachIt->second;
		    FuncMmap::iterator nextIt2 = eachIt; ++nextIt2;
		    bool moreOfSame =  (nextIt2!=m_modFuncs.end() && nextIt2->first == name);
		    if (!funcp->scopep()) funcp->v3fatalSrc("Not scoped");

		    UINFO(6,"     Wrapping "<<name<<" "<<funcp<<endl);
		    UINFO(6,"  at "<<newfuncp->argTypes()<<" und "<<funcp->argTypes()<<endl);
		    funcp->declPrivate(true);
		    AstNode* argsp = NULL;
		    for (AstNode* stmtp = newfuncp->argsp(); stmtp; stmtp=stmtp->nextp()) {
			if (AstVar* portp = stmtp->castVar()) {
			    if (portp->isIO() && !portp->isFuncReturn()) {
				AstNode* newp = new AstVarRef(portp->fileline(), portp, portp->isOutput());
				if (argsp) argsp = argsp->addNextNull(newp);
				else argsp = newp;
			    }
			}
		    }

		    AstNode* returnp = new AstCReturn (funcp->fileline(),
						       new AstCCall (funcp->fileline(),
								     funcp,
								     argsp));

		    if (moreOfSame) {
			AstIf* ifp = new AstIf (funcp->fileline(),
						new AstEq(funcp->fileline(),
							  new AstCMath(funcp->fileline(),
								       "this", 64),
							  new AstCMath(funcp->fileline(),
								       string("&(")
								       +funcp->scopep()->nameVlSym()
								       +")", 64)),
						returnp, NULL);
			newfuncp->addStmtsp(ifp);
		    } else {
			newfuncp->addStmtsp(returnp);
		    }
		}
		// Not really any way the user could do this, and we'd need to come up with some return value
		//newfuncp->addStmtsp(new AstDisplay (newfuncp->fileline(), AstDisplayType::DT_WARNING,
		//				      string("%%Error: ")+name+"() called with bad scope", NULL));
		//newfuncp->addStmtsp(new AstStop (newfuncp->fileline()));
		if (debug()>=9) newfuncp->dumpTree(cout,"   newfunc: ");
	    } else {
		// Only a single function under this name, we can simply rename it
		UINFO(6,"  Wrapping "<<name<<" just one "<<topFuncp<<endl);
		topFuncp->name(name);
	    }
	}
    }

    // VISITORS
    virtual void visit(AstNodeModule* nodep) {
	m_modp = nodep;
	m_modFuncs.clear();
        m_modSingleton = modIsSingleton(m_modp);
	nodep->iterateChildren(*this);
	makePublicFuncWrappers();
	m_modp = NULL;
    }
    virtual void visit(AstScope* nodep) {
	m_scopep = nodep;
	nodep->iterateChildren(*this);
	m_scopep = NULL;
    }
    virtual void visit(AstVarScope* nodep) {
	// Delete the varscope when we're finished
	nodep->unlinkFrBack();
	pushDeletep(nodep);
    }
    virtual void visit(AstNodeVarRef* nodep) {
	nodep->iterateChildren(*this);
	// Convert the hierch name
	if (!m_scopep) nodep->v3fatalSrc("Node not under scope");
	bool hierThis;
	nodep->hiername(descopedName(nodep->varScopep()->scopep(), hierThis/*ref*/, nodep->varScopep()->varp()));
	nodep->hierThis(hierThis);
	nodep->varScopep(NULL);
    }
    virtual void visit(AstCCall* nodep) {
	//UINFO(9,"       "<<nodep<<endl);
	nodep->iterateChildren(*this);
	// Convert the hierch name
	if (!m_scopep) nodep->v3fatalSrc("Node not under scope");
	if (!nodep->funcp()->scopep()) nodep->v3fatalSrc("CFunc not under scope");
	bool hierThis;
	nodep->hiername(descopedName(nodep->funcp()->scopep(), hierThis/*ref*/));
	// Can't do this, as we may have more calls later
	// nodep->funcp()->scopep(NULL);
    }
    virtual void visit(AstCFunc* nodep) {
	if (!nodep->user1()) {
	    m_needThis = false;
	    nodep->iterateChildren(*this);
	    nodep->user1(true);
	    if (m_needThis) {
                nodep->isStatic(false);
	    }
	    // If it's under a scope, move it up to the top
	    if (m_scopep) {
		nodep->unlinkFrBack();
		m_modp->addStmtp(nodep);

		if (nodep->funcPublic()) {
		    // There may be multiple public functions by the same name;
		    // record for later correction or making of shells
		    m_modFuncs.insert(make_pair(nodep->name(), nodep));
		    nodep->name(m_scopep->nameDotless() +"__" + nodep->name());
		}
	    }
	}
    }
    virtual void visit(AstVar*) {}
    virtual void visit(AstNode* nodep) {
	nodep->iterateChildren(*this);
    }
public:
    // CONSTRUCTORS
    explicit DescopeVisitor(AstNetlist* nodep)
        : m_modp(NULL),
          m_scopep(NULL),
          m_modSingleton(false),
          m_needThis(false) {
        nodep->accept(*this);
    }
    virtual ~DescopeVisitor() {}
};

//######################################################################
// Descope class functions

void V3Descope::descopeAll(AstNetlist* nodep) {
    UINFO(2,__FUNCTION__<<": "<<endl);
    DescopeVisitor visitor (nodep);
    V3Global::dumpCheckGlobalTree("descope", 0, v3Global.opt.dumpTreeLevel(__FILE__) >= 3);
}
