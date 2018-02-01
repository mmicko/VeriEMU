// Generated by astgen // -*- mode: C++; c-file-style: "cc-mode" -*-
class AstNode;
class AstActive;           // AstNode         
class AstAdd;              // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    AstNodeBiComAsv 
class AstAddD;             // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    AstNodeBiComAsv 
class AstAlways;           // AstNode         
class AstAlwaysPost;       // AstNode         
class AstAlwaysPublic;     // AstNode         AstNodeStmt     
class AstAnd;              // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    AstNodeBiComAsv 
class AstArg;              // AstNode         
class AstArraySel;         // AstNode         AstNodeMath     AstNodeBiop     AstNodeSel      
class AstAssign;           // AstNode         AstNodeStmt     AstNodeAssign   
class AstAssignAlias;      // AstNode         AstNodeStmt     AstNodeAssign   
class AstAssignDly;        // AstNode         AstNodeStmt     AstNodeAssign   
class AstAssignPost;       // AstNode         AstNodeStmt     AstNodeAssign   
class AstAssignPre;        // AstNode         AstNodeStmt     AstNodeAssign   
class AstAssignVarScope;   // AstNode         AstNodeStmt     AstNodeAssign   
class AstAssignW;          // AstNode         AstNodeStmt     AstNodeAssign   
class AstAttrOf;           // AstNode         
class AstBasicDType;       // AstNode         AstNodeDType    
class AstBegin;            // AstNode         
class AstBind;             // AstNode         
class AstBitsToRealD;      // AstNode         AstNodeMath     AstNodeUniop    
class AstBreak;            // AstNode         AstNodeStmt     
class AstBufIf1;           // AstNode         AstNodeMath     AstNodeBiop     
class AstCCall;            // AstNode         AstNodeStmt     
class AstCCast;            // AstNode         AstNodeMath     AstNodeUniop    
class AstCFile;            // AstNode         
class AstCFunc;            // AstNode         
class AstCLog2;            // AstNode         AstNodeMath     AstNodeUniop    
class AstCMath;            // AstNode         AstNodeMath     
class AstCReset;           // AstNode         AstNodeStmt     
class AstCReturn;          // AstNode         AstNodeStmt     
class AstCStmt;            // AstNode         AstNodeStmt     
class AstCase;             // AstNode         AstNodeStmt     AstNodeCase     
class AstCaseItem;         // AstNode         
class AstCast;             // AstNode         
class AstCastParse;        // AstNode         
class AstCastSize;         // AstNode         
class AstCeilD;            // AstNode         AstNodeMath     AstNodeUniop    
class AstCell;             // AstNode         
class AstCellArrayRef;     // AstNode         
class AstCellInline;       // AstNode         
class AstCellRef;          // AstNode         
class AstChangeDet;        // AstNode         AstNodeStmt     
class AstChangeXor;        // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    AstNodeBiComAsv 
class AstClocking;         // AstNode         
class AstComment;          // AstNode         AstNodeStmt     
class AstConcat;           // AstNode         AstNodeMath     AstNodeBiop     
class AstConcatN;          // AstNode         AstNodeMath     AstNodeBiop     
class AstCond;             // AstNode         AstNodeMath     AstNodeTriop    AstNodeCond     
class AstCondBound;        // AstNode         AstNodeMath     AstNodeTriop    AstNodeCond     
class AstConst;            // AstNode         AstNodeMath     
class AstConstDType;       // AstNode         AstNodeDType    
class AstContinue;         // AstNode         AstNodeStmt     
class AstCountOnes;        // AstNode         AstNodeMath     AstNodeUniop    
class AstCoverDecl;        // AstNode         AstNodeStmt     
class AstCoverInc;         // AstNode         AstNodeStmt     
class AstCoverToggle;      // AstNode         AstNodeStmt     
class AstCvtPackString;    // AstNode         AstNodeMath     AstNodeUniop    
class AstDefImplicitDType; // AstNode         AstNodeDType    
class AstDefParam;         // AstNode         
class AstDisable;          // AstNode         AstNodeStmt     
class AstDisplay;          // AstNode         AstNodeStmt     
class AstDiv;              // AstNode         AstNodeMath     AstNodeBiop     
class AstDivD;             // AstNode         AstNodeMath     AstNodeBiop     
class AstDivS;             // AstNode         AstNodeMath     AstNodeBiop     
class AstDot;              // AstNode         
class AstDpiExport;        // AstNode         
class AstEnumDType;        // AstNode         AstNodeDType    
class AstEnumItem;         // AstNode         
class AstEnumItemRef;      // AstNode         AstNodeMath     
class AstEq;               // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    
class AstEqCase;           // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    
class AstEqD;              // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    
class AstEqN;              // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    
class AstEqWild;           // AstNode         AstNodeMath     AstNodeBiop     
class AstExpD;             // AstNode         AstNodeMath     AstNodeUniop    
class AstExtend;           // AstNode         AstNodeMath     AstNodeUniop    
class AstExtendS;          // AstNode         AstNodeMath     AstNodeUniop    
class AstFClose;           // AstNode         AstNodeStmt     
class AstFEof;             // AstNode         AstNodeMath     AstNodeUniop    
class AstFFlush;           // AstNode         AstNodeStmt     
class AstFGetC;            // AstNode         AstNodeMath     AstNodeUniop    
class AstFGetS;            // AstNode         AstNodeMath     AstNodeBiop     
class AstFOpen;            // AstNode         AstNodeStmt     
class AstFScanF;           // AstNode         AstNodeMath     
class AstFinal;            // AstNode         
class AstFinish;           // AstNode         AstNodeStmt     
class AstFloorD;           // AstNode         AstNodeMath     AstNodeUniop    
class AstForeach;          // AstNode         AstNodeStmt     
class AstFunc;             // AstNode         AstNodeFTask    
class AstFuncRef;          // AstNode         AstNodeFTaskRef 
class AstGatePin;          // AstNode         AstNodeMath     
class AstGenCase;          // AstNode         AstNodeStmt     AstNodeCase     
class AstGenFor;           // AstNode         AstNodeStmt     AstNodeFor      
class AstGenIf;            // AstNode         AstNodeStmt     AstNodeIf       
class AstGenerate;         // AstNode         
class AstGt;               // AstNode         AstNodeMath     AstNodeBiop     
class AstGtD;              // AstNode         AstNodeMath     AstNodeBiop     
class AstGtN;              // AstNode         AstNodeMath     AstNodeBiop     
class AstGtS;              // AstNode         AstNodeMath     AstNodeBiop     
class AstGte;              // AstNode         AstNodeMath     AstNodeBiop     
class AstGteD;             // AstNode         AstNodeMath     AstNodeBiop     
class AstGteN;             // AstNode         AstNodeMath     AstNodeBiop     
class AstGteS;             // AstNode         AstNodeMath     AstNodeBiop     
class AstIToRD;            // AstNode         AstNodeMath     AstNodeUniop    
class AstIf;               // AstNode         AstNodeStmt     AstNodeIf       
class AstIface;            // AstNode         AstNodeModule   
class AstIfaceRefDType;    // AstNode         AstNodeDType    
class AstImplicit;         // AstNode         
class AstInitArray;        // AstNode         
class AstInitial;          // AstNode         
class AstInside;           // AstNode         AstNodeMath     
class AstInsideRange;      // AstNode         AstNodeMath     
class AstIsUnknown;        // AstNode         AstNodeMath     AstNodeUniop    
class AstJumpGo;           // AstNode         AstNodeStmt     
class AstJumpLabel;        // AstNode         AstNodeStmt     
class AstLenN;             // AstNode         AstNodeMath     AstNodeUniop    
class AstLog10D;           // AstNode         AstNodeMath     AstNodeUniop    
class AstLogAnd;           // AstNode         AstNodeMath     AstNodeBiop     
class AstLogD;             // AstNode         AstNodeMath     AstNodeUniop    
class AstLogIf;            // AstNode         AstNodeMath     AstNodeBiop     
class AstLogIff;           // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    
class AstLogNot;           // AstNode         AstNodeMath     AstNodeUniop    
class AstLogOr;            // AstNode         AstNodeMath     AstNodeBiop     
class AstLt;               // AstNode         AstNodeMath     AstNodeBiop     
class AstLtD;              // AstNode         AstNodeMath     AstNodeBiop     
class AstLtN;              // AstNode         AstNodeMath     AstNodeBiop     
class AstLtS;              // AstNode         AstNodeMath     AstNodeBiop     
class AstLte;              // AstNode         AstNodeMath     AstNodeBiop     
class AstLteD;             // AstNode         AstNodeMath     AstNodeBiop     
class AstLteN;             // AstNode         AstNodeMath     AstNodeBiop     
class AstLteS;             // AstNode         AstNodeMath     AstNodeBiop     
class AstMemberDType;      // AstNode         AstNodeDType    
class AstMemberSel;        // AstNode         AstNodeMath     
class AstMethodSel;        // AstNode         
class AstModDiv;           // AstNode         AstNodeMath     AstNodeBiop     
class AstModDivS;          // AstNode         AstNodeMath     AstNodeBiop     
class AstModport;          // AstNode         
class AstModportFTaskRef;  // AstNode         
class AstModportVarRef;    // AstNode         
class AstModule;           // AstNode         AstNodeModule   
class AstMul;              // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    AstNodeBiComAsv 
class AstMulD;             // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    AstNodeBiComAsv 
class AstMulS;             // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    AstNodeBiComAsv 
class AstNegate;           // AstNode         AstNodeMath     AstNodeUniop    
class AstNegateD;          // AstNode         AstNodeMath     AstNodeUniop    
class AstNeq;              // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    
class AstNeqCase;          // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    
class AstNeqD;             // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    
class AstNeqN;             // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    
class AstNeqWild;          // AstNode         AstNodeMath     AstNodeBiop     
class AstNetlist;          // AstNode         
class AstNode;             // 
class AstNodeArrayDType;   // AstNode         AstNodeDType    
class AstNodeAssign;       // AstNode         AstNodeStmt     
class AstNodeBiCom;        // AstNode         AstNodeMath     AstNodeBiop     
class AstNodeBiComAsv;     // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    
class AstNodeBiop;         // AstNode         AstNodeMath     
class AstNodeCase;         // AstNode         AstNodeStmt     
class AstNodeClassDType;   // AstNode         AstNodeDType    
class AstNodeCond;         // AstNode         AstNodeMath     AstNodeTriop    
class AstNodeDType;        // AstNode         
class AstNodeFTask;        // AstNode         
class AstNodeFTaskRef;     // AstNode         
class AstNodeFor;          // AstNode         AstNodeStmt     
class AstNodeIf;           // AstNode         AstNodeStmt     
class AstNodeMath;         // AstNode         
class AstNodeModule;       // AstNode         
class AstNodePreSel;       // AstNode         
class AstNodeRange;        // AstNode         
class AstNodeSel;          // AstNode         AstNodeMath     AstNodeBiop     
class AstNodeSenItem;      // AstNode         
class AstNodeStmt;         // AstNode         
class AstNodeStream;       // AstNode         AstNodeMath     AstNodeBiop     
class AstNodeTermop;       // AstNode         AstNodeMath     
class AstNodeText;         // AstNode         
class AstNodeTriop;        // AstNode         AstNodeMath     
class AstNodeUniop;        // AstNode         AstNodeMath     
class AstNodeVarRef;       // AstNode         AstNodeMath     
class AstNot;              // AstNode         AstNodeMath     AstNodeUniop    
class AstNotFoundModule;   // AstNode         AstNodeModule   
class AstOneHot;           // AstNode         AstNodeMath     AstNodeUniop    
class AstOneHot0;          // AstNode         AstNodeMath     AstNodeUniop    
class AstOr;               // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    AstNodeBiComAsv 
class AstPackArrayDType;   // AstNode         AstNodeDType    AstNodeArrayDType 
class AstPackage;          // AstNode         AstNodeModule   
class AstPackageExport;    // AstNode         
class AstPackageExportStarStar; // AstNode         
class AstPackageImport;    // AstNode         
class AstPackageRef;       // AstNode         
class AstParamTypeDType;   // AstNode         AstNodeDType    
class AstParseRef;         // AstNode         
class AstParseTypeDType;   // AstNode         AstNodeDType    
class AstPatMember;        // AstNode         AstNodeMath     
class AstPattern;          // AstNode         AstNodeMath     
class AstPin;              // AstNode         
class AstPort;             // AstNode         
class AstPow;              // AstNode         AstNodeMath     AstNodeBiop     
class AstPowD;             // AstNode         AstNodeMath     AstNodeBiop     
class AstPowSS;            // AstNode         AstNodeMath     AstNodeBiop     
class AstPowSU;            // AstNode         AstNodeMath     AstNodeBiop     
class AstPowUS;            // AstNode         AstNodeMath     AstNodeBiop     
class AstPragma;           // AstNode         
class AstPrimitive;        // AstNode         AstNodeModule   
class AstPslClocked;       // AstNode         
class AstPslCover;         // AstNode         AstNodeStmt     
class AstPull;             // AstNode         
class AstRToIRoundS;       // AstNode         AstNodeMath     AstNodeUniop    
class AstRToIS;            // AstNode         AstNodeMath     AstNodeUniop    
class AstRand;             // AstNode         AstNodeMath     AstNodeTermop   
class AstRange;            // AstNode         AstNodeRange    
class AstReadMem;          // AstNode         AstNodeStmt     
class AstRealToBits;       // AstNode         AstNodeMath     AstNodeUniop    
class AstRedAnd;           // AstNode         AstNodeMath     AstNodeUniop    
class AstRedOr;            // AstNode         AstNodeMath     AstNodeUniop    
class AstRedXnor;          // AstNode         AstNodeMath     AstNodeUniop    
class AstRedXor;           // AstNode         AstNodeMath     AstNodeUniop    
class AstRefDType;         // AstNode         AstNodeDType    
class AstRepeat;           // AstNode         AstNodeStmt     
class AstReplicate;        // AstNode         AstNodeMath     AstNodeBiop     
class AstReplicateN;       // AstNode         AstNodeMath     AstNodeBiop     
class AstReturn;           // AstNode         AstNodeStmt     
class AstSFormat;          // AstNode         AstNodeStmt     
class AstSFormatF;         // AstNode         
class AstSScanF;           // AstNode         AstNodeMath     
class AstScCtor;           // AstNode         AstNodeText     
class AstScDtor;           // AstNode         AstNodeText     
class AstScHdr;            // AstNode         AstNodeText     
class AstScImp;            // AstNode         AstNodeText     
class AstScImpHdr;         // AstNode         AstNodeText     
class AstScInt;            // AstNode         AstNodeText     
class AstScope;            // AstNode         
class AstScopeName;        // AstNode         AstNodeMath     
class AstSel;              // AstNode         AstNodeMath     AstNodeTriop    
class AstSelBit;           // AstNode         AstNodePreSel   
class AstSelExtract;       // AstNode         AstNodePreSel   
class AstSelMinus;         // AstNode         AstNodePreSel   
class AstSelPlus;          // AstNode         AstNodePreSel   
class AstSenGate;          // AstNode         AstNodeSenItem  
class AstSenItem;          // AstNode         AstNodeSenItem  
class AstSenTree;          // AstNode         
class AstShiftL;           // AstNode         AstNodeMath     AstNodeBiop     
class AstShiftR;           // AstNode         AstNodeMath     AstNodeBiop     
class AstShiftRS;          // AstNode         AstNodeMath     AstNodeBiop     
class AstSigned;           // AstNode         AstNodeMath     AstNodeUniop    
class AstSliceSel;         // AstNode         AstNodeMath     AstNodeTriop    
class AstSqrtD;            // AstNode         AstNodeMath     AstNodeUniop    
class AstStop;             // AstNode         AstNodeStmt     
class AstStreamL;          // AstNode         AstNodeMath     AstNodeBiop     AstNodeStream   
class AstStreamR;          // AstNode         AstNodeMath     AstNodeBiop     AstNodeStream   
class AstStructDType;      // AstNode         AstNodeDType    AstNodeClassDType 
class AstSub;              // AstNode         AstNodeMath     AstNodeBiop     
class AstSubD;             // AstNode         AstNodeMath     AstNodeBiop     
class AstSysIgnore;        // AstNode         AstNodeStmt     
class AstSystemF;          // AstNode         AstNodeMath     
class AstSystemT;          // AstNode         AstNodeStmt     
class AstTask;             // AstNode         AstNodeFTask    
class AstTaskRef;          // AstNode         AstNodeFTaskRef 
class AstTestPlusArgs;     // AstNode         AstNodeMath     
class AstText;             // AstNode         AstNodeText     
class AstTime;             // AstNode         AstNodeMath     AstNodeTermop   
class AstTimeD;            // AstNode         AstNodeMath     AstNodeTermop   
class AstTopScope;         // AstNode         
class AstTraceDecl;        // AstNode         AstNodeStmt     
class AstTraceInc;         // AstNode         AstNodeStmt     
class AstTypeTable;        // AstNode         
class AstTypedef;          // AstNode         
class AstTypedefFwd;       // AstNode         
class AstUCFunc;           // AstNode         AstNodeMath     
class AstUCStmt;           // AstNode         AstNodeStmt     
class AstUdpTable;         // AstNode         
class AstUdpTableLine;     // AstNode         
class AstUnionDType;       // AstNode         AstNodeDType    AstNodeClassDType 
class AstUnlinkedRef;      // AstNode         
class AstUnpackArrayDType; // AstNode         AstNodeDType    AstNodeArrayDType 
class AstUnsigned;         // AstNode         AstNodeMath     AstNodeUniop    
class AstUnsizedArrayDType; // AstNode         AstNodeDType    
class AstUnsizedRange;     // AstNode         AstNodeRange    
class AstUntilStable;      // AstNode         AstNodeStmt     
class AstVAssert;          // AstNode         AstNodeStmt     
class AstValuePlusArgs;    // AstNode         AstNodeMath     
class AstVar;              // AstNode         
class AstVarRef;           // AstNode         AstNodeMath     AstNodeVarRef   
class AstVarScope;         // AstNode         
class AstVarXRef;          // AstNode         AstNodeMath     AstNodeVarRef   
class AstWhile;            // AstNode         AstNodeStmt     
class AstWordSel;          // AstNode         AstNodeMath     AstNodeBiop     AstNodeSel      
class AstXnor;             // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    AstNodeBiComAsv 
class AstXor;              // AstNode         AstNodeMath     AstNodeBiop     AstNodeBiCom    AstNodeBiComAsv 
