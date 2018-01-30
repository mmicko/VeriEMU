#!/usr/bin/perl
if (!$::Driver) { use FindBin; exec("$FindBin::Bin/bootstrap.pl", @ARGV, $0); die; }
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2003 by Wilson Snyder. This program is free software; you can
# redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.

if (eval "use Bit::Vector; return 2;" != 2) { $Self->error("Please install Bit::Vector"); }

top_filename("$Self->{obj_dir}/vgen.v");

$Self->run(cmd=>["./vgen.pl",
		 "-o $Self->{top_filename}",
                 #"--seed 0",
           ]);

compile (
    );

execute (
    check_finished=>1,
    );

ok(1);
1;
