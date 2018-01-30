#!/usr/bin/perl
if (!$::Driver) { use FindBin; exec("$FindBin::Bin/bootstrap.pl", @ARGV, $0); die; }
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2003 by Wilson Snyder. This program is free software; you can
# redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.

compile (
	 v_flags2 => ["--lint-only"],
	 fails=>1,
	 expect=>
q{%Error: t/t_func_const_bad.v:11: Expecting expression to be constant, but can't determine constant for FUNCREF 'f_bad_output'
%Error: t/t_func_const_bad.v:12: ... Location of non-constant VAR 'o': Language violation: Outputs not allowed in constant functions
%Error: t/t_func_const_bad.v:20: Expecting expression to be constant, but can't determine constant for FUNCREF 'f_bad_dotted'
%Error: t/t_func_const_bad.v:22: ... Location of non-constant VARXREF 'EIGHT': Language violation: Dotted hierarchical references not allowed in constant functions
Called from:
t/t_func_const_bad.v:20:  f_bad_dotted() with parameters:
    a = ?32?sh2
%Error: t/t_func_const_bad.v:27: Expecting expression to be constant, but can't determine constant for FUNCREF 'f_bad_nonparam'
%Error: t/t_func_const_bad.v:29: ... Location of non-constant VARREF 'modvar': Language violation: reference to non-function-local variable
Called from:
t/t_func_const_bad.v:27:  f_bad_nonparam() with parameters:
    a = ?32?sh3
%Error: t/t_func_const_bad.v:35: Expecting expression to be constant, but can't determine constant for FUNCREF 'f_bad_infinite'
%Error: t/t_func_const_bad.v:37: ... Location of non-constant WHILE: Loop unrolling took too long; probably this is an infinite loop, or set --unroll-count above 1024
Called from:
t/t_func_const_bad.v:35:  f_bad_infinite() with parameters:
    a = ?32?sh3
%Error: t/t_func_const_bad.v:43: Expecting expression to be constant, but can't determine constant for FUNCREF 'f_bad_stop'
%Error: t/t_func_const_bad.v:45: ... Location of non-constant STOP: $stop executed during function constification; maybe indicates assertion firing
Called from:
t/t_func_const_bad.v:43:  f_bad_stop() with parameters:
    a = ?32?sh3
-Info: Printing in loop:           0
-Info: Printing in loop:           1
-Info: Printing in loop:           2
%Warning-USERFATAL: Fatal Error
%Warning-USERFATAL: Use "/* verilator lint_off USERFATAL */" and lint_on around source to disable this message.
%Error: t/t_func_const_bad.v:49: Expecting expression to be constant, but can't determine constant for FUNCREF 'f_bad_fatal'
%Error: t/t_func_const_bad.v:54: ... Location of non-constant STOP: $stop executed during function constification; maybe indicates assertion firing
Called from:
t/t_func_const_bad.v:49:  f_bad_fatal() with parameters:
    a = ?32?sh3
},
	 );

ok(1);
1;
