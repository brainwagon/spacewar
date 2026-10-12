#!/usr/local/bin/perl

@memory = 4096 x 0;

while(<>){
    chop;
    next if(/^\-/);
    next if(/^[ \+]\d+\t\t/);
    ($address, $value) = m/^[ \+](\d+)\s+(\d+)/;
    $memory[oct($address)] = oct($value);
}

open(OUT, ">spacewar.bin");
binmode OUT;
$out = pack("N4096", @memory);
print OUT $out;
close(OUT);
