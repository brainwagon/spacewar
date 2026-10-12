#!/usr/local/bin/perl

%flex = ('a', 061,
	 'b', 062,
	 'c', 063,
	 'd', 064,
	 'e', 065,
	 'f', 066,
	 'g', 067,
	 'h', 070,
	 'i', 071,
	 'j', 041,
	 'k', 042,
	 'l', 043,
	 'm', 044,
	 'n', 045,
	 'o', 046,
	 'p', 047,
	 'q', 050,
	 'r', 051,
	 's', 022,
	 't', 023,
	 'u', 024,
	 'v', 025,
	 'w', 026,
	 'x', 027,
	 'y', 030,
	 'z', 031,
	 '0', 020,
	 '1', 01,
	 '2', 02,
	 '3', 03,
	 '4', 04,
	 '5', 05,
	 '6', 06,
	 '7', 07,
	 '8', 010,
	 '9', 011
	 );

%symbols = (
'i',   0010000,
'add', 0400000,
'and', 0020000,
'cal', 0160000,
'dac', 0240000,
'dap', 0260000,
'dio', 0320000,
'dip', 0300000,
'dis', 0560000,
'div', 0560000,
'dzm', 0340000,
'dpy', 0730007,
'idx', 0440000,
'ioh', 0730000,
'ior', 0040000,
'iot', 0720000,
'isp', 0460000,
'jda', 0170000,
'jmp', 0600000,
'jsp', 0620000,
'lac', 0200000,
'law', 0700000,
'lio', 0220000,
'mul', 0540000,
'mus', 0540000,
'opr', 0760000,
'sad', 0500000,
'sas', 0520000,
'sft', 0660000,
'skp', 0640000,
'sub', 0420000,
'xct', 0100000,
'xor', 0060000,
'cla', 0760200,
'clf', 0760000,
'cli', 0764000,
'cma', 0761000,
'hlt', 0760400,
'lap', 0760100,
'lat', 0762200,
'nop', 0760000,
'stf', 0760010,
'rpa', 0720001,
'rpb', 0720002,
'rrb', 0720030,
'ppa', 0720005,
'ppb', 0720006,
'tyo', 0720003,
'tyi', 0720004,
'esm', 0720055,
'lsm', 0720054,
'cbs', 0720056,
'dsc', 0720050,
'asc', 0720051,
'isb', 0720052,
'cac', 0720053,
'swc', 0720046,
'sia', 0720346,
'sdf', 0720146,
'rlc', 0720366,
'shr', 0720446,
'gpl', 0722027,
'gpr', 0720027,
'glf', 0722026,
'gsp', 0720026,
'sdb', 0722007,
'dpp', 0720407,
'lag', 0720044,
'pac', 0720043,
'rac', 0720041,
'rbc', 0720042,
'rcc', 0720032,
'msm', 0720073,
'mcs', 0720034,
'mcb', 0720070,
'mwc', 0720071,
'mrc', 0720072,
'muf', 0720076,
'mic', 0720075,
'mrf', 0720067,
'mri', 0720066,
'mes', 0720035,
'mel', 0720036,
'inr', 0720067,
'ccr', 0720067,
'sfc', 0720072,
'rsr', 0720172,
'crf', 0720272,
'cpm', 0720472,
'dur', 0720070,
'mtf', 0730071,
'cgo', 0720073,
'rcb', 0720031,
'cad', 0720040,
'scv', 0720047,
'icv', 0720060,
'clrbuf', 0722045,
'lpb', 0720045,
'pas', 0721045,
'sma', 0640400,
'spa', 0640200,
'spi', 0642000,
'sza', 0640100,
'szf', 0640000,
'szo', 0641000,
'szs', 0640000,
'skip',0640000,
'ral', 0661000,
'rar', 0671000,
'rcl', 0663000,
'rcr', 0673000,
'ril', 0662000,
'rir', 0672000,
'sal', 0665000,
'sar', 0675000,
'scl', 0667000,
'scr', 0677000,
'sil', 0666000,
'sir', 0676000
);

%literals = ();
%resolvedLiterals = ();
$literalIndex = 1;
%variables = ();
%resolvedVariables = ();
$variableIndex = 1;

$loc = 0;

$infile = shift;
open(FILE, "<$infile") || die "Could not open $infile: $!";

# do pass 1
$octal = 1;

while(<FILE>){
    if (/^-/){
	next;
    }
    chop;
    next if (/^\s*$/);
    next if (/^\s*\//);
    if (/^[\*\+]/){
	s/^[\*\+]//;
    }
    if (/^\s+[\.\+\s]*\d+\//){
	($loc1) = m/^\s+([\.\w \+\-,\d]+)\//;
	$loc = assemble($loc1);
    }
    if (/^\s+[\.\+\s]*\d+\//){
    } elsif (/^\s*\w+,/) {
	($label) = m/^\s*(\w+),/;
	$symbols{$label} = $loc;
    } elsif (/^\s+\w+\=/) {
	($lhs, $rhs) = m/\s+(\w+)\s*=\s*([\(\~\.\w ,\+\-]+)/;
	$symbols{$lhs} = assemble($rhs);
    } elsif (/^\s+start/) {
    } elsif (/^\s+decimal/) {
	$octal = 0;
    } elsif (/^\s+constants/){
	foreach $literal (keys(%literals)){
	    $address = $loc + $literals{$literal} - 1;
	    $resolvedLiterals{$literal} = $address;
	    $resolvedAddress{$address} = $literal;
	}
	$loc = $loc + $literalIndex - 1;
	%literals = ();
    } elsif (/^\s+variables/){
	foreach $variable (keys(%variables)){
	    $address = $loc + $variables{$variable}- 1;
	    $resolvedVariables{$variable} = $address;
	    $resolvedAddress{$address} = $variable;
	}
	$loc = $loc + $variableIndex - 1;
	%variables = ();
    } else {
	$loc++;
	if (/\s\([\.\~\w]+/) {
	    ($literal) = m/[,\s]+(\([\~\w\s\.\+\-]+)$/;
	    $literal =~ s/\s*\(//;
	    $literal =~ s/\s+$//;
	    if ($literal && !$literals{$literal}){
		$literals{$literal} = $literalIndex;
		$literalIndex++;
	    }
	}
	if (/\s\~\w+/) {
	    ($variable) = m/[,\s]+(\~\w+)/;
	    $variable =~ s/\s*\~//;
	    $variable =~ s/\s+$//;
	    if ($variable && !$variables{$variable}){
		$variables{$variable} = $variableIndex;
		$variableIndex++;
	    }
	}
    }
}

seek(FILE, 0, 0);

# Do Pass 2

$octal = 1;

while(<FILE>){
    if (/^-/){
	print;
	next;
    }
    chop;
    next if (/^\s*$/);
    next if (/^\s*\//);
    if (/^\+/){
	$noprint = 0;
	print "+";
	s/^\+//;
    } elsif (/^\*/){
	$noprint = 1;
	s/^\*//;
    } else {
	$noprint = 0;
	print " ";
    }
    if (/^\s+[\.\+\s]*\d+\//){
	($loc1) = m/^\s+([\.\w \+\-,\d]+)\//;
	$loc = assemble($loc1);
    }
    printf("%06o\t", $loc) unless $noprint;
    $value = "";
    if (/^\s+[\.\+\s]*\d+\//){
    } elsif (/^\s*\w+,/) {
	($label) = m/^\s*(\w+),/;
	if ($symbols{$label} != $loc){
	    print "Phase Error at $loc: $symbols{$label}\n";
	};
    } elsif (/^\s+\w+\=/) {
	($lhs, $rhs) = m/\s+(\w+)\s*=\s*([\(\~\.\w ,\+\-]+)/;
	$symbols{$lhs} = $value = assemble($rhs);
	$value = sprintf("%06o", $value);
    } elsif (/^\s+flex/){
	($rhs) = m/\s+flex\s+([a-z]+)/;
	$value = flex($rhs);
	$value = sprintf("%06o", $value);
	$loc++;
    } elsif (/^\s+start/) {
    } elsif (/^\s+decimal/) {
	$octal = 0;
    } elsif (/^\s+constants/){
	print "\t$_\n" unless $noprint;
#	$loc++;
	while(1) {
	    last if (length($resolvedAddress{$loc}) == 0);
	    $rhs = $resolvedAddress{$loc};
	    printf("+%06o\t", $loc) unless $noprint;
	    $value = assemble($rhs);
	    $value = sprintf("%06o", $value);
	    print "$value\t$rhs\n" unless $noprint;
	    $loc++;
	}
	next;
    } elsif (/^\s+variables/){
	print "\t$_\n";
#	$loc++;
	while(1) {
	    last if (length($resolvedAddress{$loc}) == 0);
	    $rhs = $resolvedAddress{$loc};
	    printf("+%06o\t", $loc) unless $noprint;
	    $value = 0;
	    $value = sprintf("%06o", $value);
	    print "$value\t$rhs\n" unless $noprint;
	    $loc++;
	}
	next;
    } else {
	($rhs) = m/\s+([\(\-\~\.\w][\w ,\+\-\.\(\~]*)/;
	$value = assemble($rhs);
	$value = sprintf("%06o", $value);
	$loc++;
    }
    print "$value\t$_\n" unless $noprint;
}

sub assemble {
    local($line) = @_;
    local($value, $token, @tokens, $lit, $temp);
    if ($line =~ /\(/){
	$line =~ s/(\(.*$)//;
	$lit = $1;		
    }
    $line =~ s/\+/ \+ /g;
    $line =~ s/\-/ \-/g;
    $line =~ s/\s+/ /;
    @tokens = split(/\s/, $line);
    push @tokens,$lit if $lit;
    $value = 0;
    foreach $token (@tokens) {
	if ($token =~ /^[\-]*\d+$/){
	    if ($octal) {
		if($token =~ /^\-/) {
		    $value += -oct(substr($token,1)) ;
		} else {
		    $value += oct($token);
		}
	    } else {
		if($token =~ /^\-/) {
		    $value += (0 - (substr($token,1))) ;
		} else {
		    $value += $token;
		}
	    }
	} elsif ($token =~ /^\(/) {
	    $token =~ s/_/ /;
	    if($resolvedLiterals{substr($token,1)}){
		$value += $resolvedLiterals{substr($token,1)};
	    } else {
		$token =~ s/\s*\(//;
		$token =~ s/\s+$//;
		if ($token && !$literals{$token}){
		    $literals{$token} = $literalIndex;
		    $literalIndex++;
		}
	    }
	} elsif ($token =~ /^\~/) {
	    $token =~ s/_/ /;
	    if ($resolvedVariables{substr($token,1)}){
		$value += $resolvedVariables{substr($token,1)};
	    } else {
		$token =~ s/\s*\~//;
		$token =~ s/\s+$//;
		if ($token && !$variables{$token}){
		    $variables{$token} = $variableIndex;
		    $variableIndex++;
		}
	    }
	} elsif ($token eq '.') {
	    $value += $loc;
	} elsif ($token =~ /^\d+s$/) {
	    $token =~ s/s$//;
	    $value += ((2**$token) - 1); 
	} else {
	    if($token =~ /^\-/) {
		$value += -$symbols{substr($token,1)};
	    } else {
		$value += $symbols{$token};
	    }
	}
    }

    if ($value < 0) {
	$value--;
    }
    $value &= 0777777;
    return $value;
}

sub flex {
    local($string) = @_;
    local(@chars, $char, $value);
    $value = 0;
    @chars = split(//,$string);
    foreach $char (@chars){
	$value = ($value * 64) + $flex{$char};
    }
    return $value;
}
