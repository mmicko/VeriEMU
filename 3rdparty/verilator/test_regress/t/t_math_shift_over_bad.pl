#!/usr/bin/perl
if (!$::Driver) { use FindBin; exec("$FindBin::Bin/bootstrap.pl", @ARGV, $0); die; }
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2010 by Wilson Snyder. This program is free software; you can
# redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.

$Self->{vlt} or $Self->skip("Verilator only test");

compile (
    verilator_flags2 => ["--lint-only"],
    fails=>1,
    expect=>
'%Error: Internal Error: .*: Value too wide for 32-bits expected in this context 64\'h123456789abcdef
.',
    );

ok(1);
1;
