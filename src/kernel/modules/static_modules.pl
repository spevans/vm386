#!/usr/bin/perl

use strict;
use warnings;

my @modules = ();

while (<STDIN>) {
    chomp;
    if ($_ =~ /.*\/([^\/]+)/) {
        push(@modules, $1);
    }
}

foreach my $mod (@modules) {
    print "extern struct module ${mod}_module;\n";
}

print "\n\nstatic struct module *static_modules[] = {\n";
foreach my $mod (@modules) {
    printf "    &%s_module, \n", $mod;
}
print q|};

static const size_t static_module_count = sizeof(static_modules) / sizeof(struct module *);

|;

