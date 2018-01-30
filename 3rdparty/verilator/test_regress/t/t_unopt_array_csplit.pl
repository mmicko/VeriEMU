#!/usr/bin/perl
if (!$::Driver) { use FindBin; exec("$FindBin::Bin/bootstrap.pl", @ARGV, $0); die; }
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2003 by Wilson Snyder. This program is free software; you can
# redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.

$Self->{vlt} or $Self->skip("Verilator only test");

top_filename("t/t_unopt_array.v");
compile (
    v_flags2 => ["--trace --output-split 1 --output-split-cfuncs 1 -Wno-UNOPTFLAT"],
    );

execute (
    check_finished=>1,
    );

ok(1);
1;
