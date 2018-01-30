#!/usr/bin/perl
if (!$::Driver) { use FindBin; exec("$FindBin::Bin/bootstrap.pl", @ARGV, $0); die; }
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2003 by Wilson Snyder. This program is free software; you can
# redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.

my $root = "..";

if (!-r "$root/.git") {
    $Self->skip("Not in a git repository");
} else {
    ### Must trim output before and after our file list
    my %warns;
    my $prefix;
    my $summary;
    {
        my $diff = `cd $root && git diff HEAD`;
        #print "DS $diff\n" if $Debug;
        my $file;
        my $atab;
        my $btab;
        foreach my $line ((split /\n/, $diff), "+++ b/_the_end") {
            if ($line =~ m!^\+\+\+ b/(.*)!) {
                if ($file && !$atab && $btab) {
                    $summary = "File modifications adds new tabs (please untabify the patch):";
                    $warns{$file} = "File modification adds new tabs (please untabify the patch): $file";
                }
                # Next
                $file = $1;
                $atab = 0;
                $btab = 0;
                print " File $file\n" if $Self->{verbose};
            }
            elsif ($line =~ m!^[- ].*\t!) {
                print "  Had tabs\n" if $Self->{verbose} && !$atab;
                $atab = 1;
            }
            elsif ($line =~ m!^\+.*\t!) {
                print "  Inserts tabs\n" if $Self->{verbose} && !$btab;
                $btab = 1;
            }
        }
    }
    if (keys %warns) {
        # First warning lists everything as that's shown in the driver summary
        $Self->error($summary." ",join(' ',sort keys %warns));
        foreach my $file (sort keys %warns) {
            $Self->error($warns{$file});
        }
    }
}

sub _has_tabs {
    my $filename = shift;
    my $contents = file_contents($filename);
    if ($filename =~ /\.out$/) {
        # Ignore golden files
    } elsif ($contents =~ /[\001\002\003\004\005\006]/) {
        # Ignore binrary files
    } elsif ($contents =~ /\t/) {
        return 1;
    }
    return 0;
}

ok(1);
1;
