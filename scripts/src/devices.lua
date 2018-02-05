-- license:BSD-3-Clause
-- copyright-holders:MAMEdev Team

---------------------------------------------------------------------------
--
--   devices.lua
--
--   Rules for building device cores
--
---------------------------------------------------------------------------

function devicesProject(_target, _subtarget)

	disasm_files = { }
	disasm_dependency = { }
	disasm_custombuildtask = { }

	project ("optional")
	uuid (os.uuid("optional-" .. _target .."_" .. _subtarget))
	kind (LIBTYPE)
	targetsubdir(_target .."_" .. _subtarget)
	
	defines {
		"VM_TRACE=0",
		"VL_THREADED=0",
		"VM_SC=0",
		"VL_PRINTF=printf",
	}

	if (_OPTIONS["targetos"] ~= "asmjs") then
		options {
			"ArchiveSplit",
		}
	end

	addprojectflags()
	precompiledheaders()

	includedirs {
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/devices",
		MAME_DIR .. "src/mame", -- used for sound amiga
		MAME_DIR .. "src/lib",
		MAME_DIR .. "src/lib/util",
		MAME_DIR .. "3rdparty",
		MAME_DIR .. "3rdparty/verilator/include",
		GEN_DIR  .. "emu",
		GEN_DIR  .. "emu/layout",
		GEN_DIR  .. "rtl/devices",
		ext_includedir("expat"),
		ext_includedir("flac"),
		MAME_DIR .. "3rdparty/asio/include",
	}

	dofile(path.join("src", "cpu.lua"))

	dofile(path.join("src", "sound.lua"))

	dofile(path.join("src", "video.lua"))

	dofile(path.join("src", "machine.lua"))

	dofile(path.join("src", "bus.lua"))

if #disasm_files > 0 then
	project ("dasm")
	uuid ("f2d28b0a-6da5-4f78-b629-d834aa00429d")
	kind (LIBTYPE)
	targetsubdir(_target .."_" .. _subtarget)
	addprojectflags()
	precompiledheaders()

	includedirs {
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/devices",
		MAME_DIR .. "src/lib",
		MAME_DIR .. "src/lib/util",
		MAME_DIR .. "3rdparty",
		MAME_DIR .. "3rdparty/asio/include",
		GEN_DIR  .. "emu",
		ext_includedir("expat"),
	}

	files {
		disasm_files
	}

	if #disasm_dependency > 0 then
		dependency(disasm_dependency)
	end

	if #disasm_custombuildtask > 0 then
		custombuildtask(disasm_custombuildtask)
	end
end

end
