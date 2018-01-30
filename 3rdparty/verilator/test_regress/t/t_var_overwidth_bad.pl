#!/usr/bin/perl
if (!$::Driver) { use FindBin; exec("$FindBin::Bin/bootstrap.pl", @ARGV, $0); die; }
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2010 by Wilson Snyder. This program is free software; you can
# redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.

compile (
    make_main => 0,
    verilator_flags2 => ["--exe $Self->{t_dir}/t_var_overwidth_bad.cpp"],
    );

execute (
    fails=>1,
    expect=>
qr{%Error: unknown:0: Testbench C set input 'clk' to value that overflows what the signal's width can fit
Aborting....*}
    );

ok(1);
1;
