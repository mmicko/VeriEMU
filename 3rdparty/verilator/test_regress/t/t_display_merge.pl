#!/usr/bin/perl
if (!$::Driver) { use FindBin; exec("$FindBin::Bin/bootstrap.pl", @ARGV, $0); die; }
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2003 by Wilson Snyder. This program is free software; you can
# redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.

compile (
    verilator_flags2=>["--stats"],
    );

execute (
    check_finished=>1,
    expect=> (quotemeta(
'Merge:
This should merge
f
 a=top.t
 b=top.t
 pre
 t=0
 t2=0
 post
 t3=0
 t4=0 t5=00000000000000000
m
 t=0 t2=0 t3=0 t4=0 t5=0
 t=0 t2=0 t3=0 t4=0 t5=0
mm

f a=top.t b=top.t pre t=0 t2=0 post t3=0 t4=0 t5=00000000000000000m t=0 t2=0 t3=0 t4=0 t5=0 t=0 t2=0 t3=0 t4=0 t5=0mm

*-* All Finished *-*')
	      .'.*')
    );

file_grep ("$Self->{obj_dir}/$Self->{VM_PREFIX}__stats.txt",
	   qr/Node count, DISPLAY \s+ 32 \s+ 25 \s+ 25 \s+ 25/);

ok(1);
1;
