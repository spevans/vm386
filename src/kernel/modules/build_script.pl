#!/usr/bin/perl -w

use strict;
use warnings;

prolog();
my $last_mod = '__first_static_module';
while (<STDIN>) {
    next if ($_ =~ /^\s*#/);
    chomp $_;
    if ($_ !~ /^(.*)\/([^\/]*)$/) {
        die "Bad module spec $_\n";
    }
    my $modname = $2;
    my $modpath = "../../${_}.module.o";
    print "$last_mod = _${modname}_start;\n";
    print "    $modpath(.text)\n";
    $last_mod = "_${modname}_end";
}
print "$last_mod = .;\n";
print "__last_static_module = $last_mod;\n";
epilog();



sub prolog {
print <<EOF
OUTPUT(modules.o)
FORCE_COMMON_ALLOCATION


SECTIONS
{
  .text.start : {
    module.o(.text)
    load.o(.text)
  }

  .text : {
EOF
}


sub epilog {
print <<EOF
    *(.rodata)
  }

  .data : {
    *(.data)
  }

  .bss : {
    *(.bss) *(COMMON)
  }
}
EOF
}
