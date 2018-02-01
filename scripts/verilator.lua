premake.check_paths = true

MAME_DIR = (path.getabsolute("..") .. "/")
local MAME_BUILD_DIR = (MAME_DIR .. "/build/")

solution "verilator"
	location (MAME_BUILD_DIR)
	configurations {
		"Release",
		"Debug"
	}

	configuration {}

	msgcompile ("Compiling $(subst ../,,$<)...")

	msgcompile_objc ("Objective-C compiling $(subst ../,,$<)...")
	
	msgresource ("Compiling resources $(subst ../,,$<)...")
	
	msglinking ("Linking $(notdir $@)...")
	
	msgarchiving ("Archiving $(notdir $@)...")
	
	msgprecompile ("Precompiling $(subst ../,,$<)...")
	
	messageskip { "SkipCreatingMessage", "SkipBuildingMessage", "SkipCleaningMessage" }
	
project("verilator")
	uuid ("b982314a-80d2-4ae9-af83-795c2b03dda0")
	targetname "verilator"
	language "C++"

	kind "ConsoleApp"
	flags {
		"ExtraWarnings",
		"StaticRuntime"
	}

	includedirs {
		MAME_DIR .. "3rdparty/verilator/include",
		MAME_DIR .. "3rdparty/verilator/include/vltstd",
	}
	includedirs {
		MAME_DIR .. "3rdparty/verilator/src",
		MAME_DIR .. "3rdparty/verilator/generated/",
	}

	defines {
		"YYDEBUG",
		"PLI_DLLISPEC=",
		"DPI_DLLISPEC=",
		"VM_TRACE=0",
		"VM_SC=0",
		"VL_PRINTF=printf",
		"DEFENV_SYSTEMC=\\\"\\\"",
		"DEFENV_SYSTEMC_ARCH=\\\"\\\"",
		"DEFENV_SYSTEMC_INCLUDE=\\\"\\\"",
		"DEFENV_SYSTEMC_LIBDIR=\\\"\\\"",
		"DEFENV_VERILATOR_ROOT=\\\"/opt/share/verilator\\\"",
	}
	
	configuration { "gmake or ninja" }
		buildoptions {
			"-Wno-undef",
			"-Wno-maybe-uninitialized",
			"-Wno-format",
			"-Wno-format-security",
			"-Wno-format-extra-args",
			"-Wno-nonnull-compare",
			"-Wno-unused-parameter"
		}

	configuration { }

	files {
		MAME_DIR .. "3rdparty/verilator/src/V3Active.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3ActiveTop.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Assert.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3AssertPre.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Ast.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3AstNodes.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Begin.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Branch.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Broken.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Case.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Cast.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3CCtors.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Cdc.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Changed.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Clean.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3ClkGater.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Clock.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Combine.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Config.cpp",
		--MAME_DIR .. "3rdparty/verilator/src/V3Const.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Coverage.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3CoverageJoin.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Dead.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Delayed.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Depth.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3DepthBlock.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Descope.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3EmitC.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3EmitCInlines.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3EmitCSyms.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3EmitMk.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3EmitV.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3EmitXml.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Error.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Expand.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3File.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3FileLine.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Gate.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3GenClk.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Graph.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3GraphAcyc.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3GraphAlg.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3GraphDfa.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3GraphTest.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Hashed.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Inline.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Inst.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Life.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3LifePost.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3LinkCells.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3LinkDot.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3LinkJump.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3LinkLevel.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3LinkLValue.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3LinkParse.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3LinkResolve.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Localize.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Name.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Number.cpp",
		--MAME_DIR .. "3rdparty/verilator/src/V3Number_test.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Options.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Order.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Os.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Param.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3ParseGrammar.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3ParseImp.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3ParseLex.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Premit.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3PreProc.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3PreShell.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Scope.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Slice.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Split.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3SplitAs.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Stats.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3StatsReport.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3String.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Subst.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Table.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Task.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Trace.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3TraceDecl.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Tristate.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Undriven.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Unknown.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Unroll.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3Width.cpp",
		MAME_DIR .. "3rdparty/verilator/src/V3WidthSel.cpp",
		MAME_DIR .. "3rdparty/verilator/src/Verilator.cpp",
		MAME_DIR .. "3rdparty/verilator/generated/V3Const__gen.cpp",
	}

	configuration "Debug"
		defines     { "_DEBUG" }
		flags       { "Symbols" }
		objdir (MAME_BUILD_DIR .. "obj/")
	
	configuration "Release"
		defines     { "NDEBUG" }
		flags       { "OptimizeSize" }
		objdir (MAME_BUILD_DIR .. "obj/")

	configuration "windows"
		targetdir (MAME_BUILD_DIR .. "obj/")
	
	configuration "linux"
		targetdir (MAME_BUILD_DIR .. "obj/")
		