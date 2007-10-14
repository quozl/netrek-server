#!/usr/bin/env perl
#
# $Id: end_tourney.pl,v 1.2 2005/09/28 12:14:05 quozl Exp $
# 
# end_tourney.pl
#
# If this fails to work, change the first line of this file to point to
# a perl 5 (NOT perl 4) interpreter--"#!/usr/local/bin/perl5" for example.
#
# Generates PWstats and others from an ltd_stats file.
# Mails stats to statboys if REGISTER was used.

######################################################################
# NEW FEATURE: There is a central stats archive at
#              http://www.netrek.org/stats/
#
# By default, all stats are automatically archived there via the
# auto-archive.pl script.  YOU SHOULD EDIT auto-archive.pl to
# work under your environment.
#
# The auto-archiving is independent of the statboy email for
# registered games and the $dropdir features below.
######################################################################

#
# usage: end_tourney.pl [-register] [ID] 
# If "ID" exists, will open "ltd_stats.<ID>", else will open ltd_stats.
#
# The stats are saved in a hash of hashes, $stat{playername}->{stat}.  

$debug = 1;

$bgcolor="6699CC";
$th = "<th bgcolor=$bgcolor>";
$td = "<td align=center>";
$statboys = "netrek\@localhost";
$mailprog = "/usr/lib/sendmail";
$tickspersec = 10;

# Enter your local time zone here, standard time
$tz = "EST";
# Enter your local time zone here, daylight savings time (if applicable)
$tzdst = "EDT";

# Edit this to point to a valid key.html key file
$keyloc = "http://stats.psychosis.net/key.html";

# Uncomment this if you want pwstats to be automatically dropped in a
# directory.  This should be a FULL UNIX DIRECTORY PATHNAME, without
# a trailing slash.
# This is for keeping a local stats archive in addition to the stats
# that auto-archive.pl sends to http://www.netrek.org/stats/.
#
#$dropdir = "/home/netrek/www/stats";
#
# Uncomment this if $dropdir is defined and stats should be dumped
# into subdirectories within $dropdir.
#
$dropsub = 1;
#
# Uncomment this to use a more readable form for dropdir entries.
#
$newdrop = 1;
#
# Set this to call a directory indexing script after generating stats.
#
#$dirindex = "/home/netrek/bin/statsindex.py";
#

if ($keyloc) {
    $key = "<font size=+1><a href=$keyloc>(View Key)</a></font>";
}

foreach $argument ( @ARGV ) {
    if ( $argument eq "-register" ) {
	$register = 1;
    } else {
	$id = $argument;
    }
}

if ( $id ) {
    $inputfile = "ltd_dump.txt." . $id;
    $logfile = "INL_log." . $id;
    $outputfile = "pwstats." . $id . ".html";
    $cambotfile = "cambot.pkt." . $id;
    $endtime = $id;
} else { 
    $inputfile = "ltd_dump.txt";
    $logfile = "INL_log";
    $outputfile = "pwstats.html";
    $cambotfile = "cambot.pkt";
    $endtime = time;
}

@months = qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);
@weekdays = qw(Sun Mon Tue Wed Thu Fri Sat Sun);
($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($endtime);
if ($hour < 12) {
    $ampm = "AM";
} else {
    $hour = $hour - 12;
    $ampm = "PM";
}
if ($hour == 0) {
    $hour = 12;
}
if ($min < 10) {
    $min = "0${min}";
}
if ($isdst) {
    $tz = $tzdst;
}
$endtimestr = "<b>$weekdays[$wday] $months[$mon] $mday ". ($year + 1900) .
    "</b> at <b>$hour:$min $ampm $tz</b>";

system("./tools/ltd_dump `./tools/getpath --localstatedir`/players." . $id . " > $inputfile");

open (INPUT,"$inputfile");
open (OUTPUT,">$outputfile");

# make unbuffered I/O
select(OUTPUT); $| = 1;
select(STDERR); $| = 1;
select(STDOUT); $| = 1;

$homecurrentplanets=10;
$awaycurrentplanets=10;
$neuts=0;

&parselog;
&parsestats;
&computescore;
&printstdout;
&printstats;

# Run the auto-archiver after pwstats have been generated.
# Stats are sent immediately while the cambot file is sent
# in the background.

system("./auto-archive.pl $id");

$hometeam =~ s/[\`\"\']//g;
$awayteam =~ s/[\`\"\']//g;

if ( $dropdir ) {
    if ( -d $dropdir ) {
	if ( $dropsub ) {
	    if ($newdrop) {
		if ($neuts == 0) {
    	    	    $dirpath = "$dropdir/$hometeam -vs- $awayteam ($homecurrentplanets-$awaycurrentplanets)";
    	    	} else {
    	    	    $dirpath = "$dropdir/$hometeam -vs- $awayteam ($homecurrentplanets-$awaycurrentplanets-$neuts)";
	    	}
		if ( -d $dirpath ) {
	    	    $dirnum = 2;
		    while ( -d "$dirpath [$dirnum]" ) {
		    	$dirnum++;
		    }
		    $dirpath = "$dirpath [$dirnum]";
	    	}
	    } else {
	    	$dirpath = "$dropdir/$id";
	    }
	    umask 022;
	    system("mkdir \"$dirpath\"");
	    system("cp $outputfile \"$dirpath/pwstats.html\"");
	    system("ln -s \"$dirpath/pwstats.html\" \"$dirpath/index.html\"");
	    system("cp $logfile \"$dirpath/INL_log.txt\"");
	    system("cp $inputfile \"$dirpath/ltd_dump.txt\"");
	    if ( -f $cambotfile ) {
	    	system("gzip -9c $cambotfile > \"$dirpath/cambot.pkt.gz\"");
	    }
	    system("chmod 755 \"$dirpath\"");
	    system("chmod 644 \"$dirpath\"/*");
	    if ($dirindex) {
		system("$dirindex $dropdir > $dropdir/index.html");
	    }
	} else {
	    system("cp -f $outputfile $dropdir/$outputfile");
	}
    } else {
	print STDERR "Dropdir " , $dropdir , " is not a directory!\n";
    }
}

if ( $register ) {
    &maillog;
}


sub computescore { 
    # After parsing stats, add up taken and destroyed planets for each
    # team to figure out the score.
    
    foreach $player ( sort keys %stat ) {

	if ( $debug > 1 ) {
	    print "$player\n";
	}

	my $tpt = $stat{$player}->{ptt};
	my $tpd = $stat{$player}->{pdt};

	if ( $debug > 1 ) {
	    print "${player} ($stat{$player}->{team}): $stat{$player}->{ptt}, $stat{$player}->{pdt}\n";
	}

	if ( $stat{$player}->{team} eq $homerace ) {
	    $homescore += $tpt + $tpd;
	    $homecurrentplanets += $tpt;
	    $awaycurrentplanets -= $tpd;
	} else {
	    $awayscore += $tpt + $tpd;
	    $awaycurrentplanets += $tpt;
	    $homecurrentplanets -= $tpd;
	}
	$neuts = 20 - $homecurrentplanets - $awaycurrentplanets;
	if ( $debug > 1 ) {
	    print "Score: $homescore, $awayscore, $neuts\n";
	}
    }

} ### end sub computescore

sub parselog {
    # Go through the INL_log to find the team names (possibly other stuff)

    open ( LOG,"$logfile") || die ("Could not open $logfile; $!");

    while ( $line = <LOG> ) {

	last if $line =~ /^\s*\d+: INL->ALL  Teams chosen/;

	next if $line !~ /^\s*\d+: INL->ALL\s+.*team is/;

	$line =~ /^\s*\d+: INL->ALL  (....) team is   (.*) \((.*)\):/;

	if ( $1 eq "Home" ) {
	    $hometeam = $2;
	    $homerace = $3;
	} else {
	    $awayteam = $2;
	    $awayrace = $3;
	}

	if ( $debug > 1 ) {
	    print "$hometeam vs. $awayteam, $homerace vs. $awayrace\n";
	}

    }

} ### end sub parselog

sub parsestats {
    # Go through the ltd_stats and get the data we need.  This is the main
    # loop of the program.

    $player = 0;

while ( $line = <INPUT> ) {

    study $line;

    if ( ! $player ) {
	# $player is set to the name of the player we're currently reading
	# stats for; if it's false, we're not in a player, look for the
	# beginning line
	next if $line !~ /^\+\+ LTD statistics for player \[(.*)\] \((...)\)$/;

	$player = $1;
	$team = $2;


	if ( $stat{$player}->{team} && $stat{$player}->{team} ne $team ) {
	    # This is a horrible hack for players who played on both teams

	    $player .= $team;
	}

    }

    if ( $line =~ /^\+\+ LTD statistics for player \[(.*)\] \((...)\)$/ ) {
	$player = $1;
	$team = $2;

	if ( $stat{$player}->{team} && $stat{$player}->{team} ne $team ) {
	    # This is a horrible hack for players who played on both teams

	    $player .= $team;
	}

    }

    if ( $debug > 1 ) {
	print $player , "\n";
    }

    $stat{$player}->{team} = $team;
    $stat{$player}->{name} = $player;

    if ( $line =~ /^\((.*)\).*:(.*):(.*):(.*):(.*):(.*):(.*):(.*):(.*):$/ ) {
	# $1 is the stat abbreviation; store the stats in 
	# $stat{$player}->{$abbr}, one for each ship type.

	if ( $debug > 5 ) {
	    print $line;
	}

	$stat1 = $1 . "sc";
	$stat2 = $1 . "dd";
	$stat3 = $1 . "ca";
	$stat4 = $1 . "bb";
	$stat5 = $1 . "as";
	$stat6 = $1 . "sb";
	$stat7 = $1 . "ga";
	$stat8 = $1;

	$stat{$player}->{$stat1}=$2;
	$stat{$player}->{$stat2}=$3;
	$stat{$player}->{$stat3}=$4;
	$stat{$player}->{$stat4}=$5;
	$stat{$player}->{$stat5}=$6;
	$stat{$player}->{$stat6}=$7;
	$stat{$player}->{$stat7}=$8;
	$stat{$player}->{$stat8}=$9;

	if ( $debug > 1 ) {
	    print "$player, $stat1, $stat{$player}->{$stat1}\n";
	    print "$player, $stat3, $stat{$player}->{$stat3}\n";
	}

    }

} ### end while ( $line = <INPUT> ) 

close INPUT;

} ### end sub parsestats


sub printstdout {
    # Prints a plain text summary of stats so the server can pipe it to
    # stdout on the clients
    # This is deprecated due to most clients being run without visible stdout

format STDOUT =
@<<<<<<<<<<<<<<< @>>> @>>> @>>> @>>> @>>> @>>>
$name, $tpt, $tpd, $tab, $tek, $def, $acc
.

    if ($neuts) {
        print "FINAL SCORE: $hometeam -vs- $awayteam, ${homecurrentplanets}-${awaycurrentplanets}-${neuts}\n\n";
    } else {
        print "FINAL SCORE: $hometeam -vs- $awayteam, ${homecurrentplanets}-${awaycurrentplanets}\n\n";
    }

    print "\nPLAYER STATS for $hometeam\n";

    ( $name, $tpt, $tpd, $tab, $tek, $def, $acc ) =
    ( "Name" , "tpt" , "tpd" , "tab", "tek" , "def" , "acc" );

    write STDOUT;

    foreach $player ( sort keys %stat ) {

	next if $stat{$player}->{team} ne $homerace;

	( $name, $tpt, $tpd, $tab, $tek, $def, $acc ) =
	    ( $stat{$player}->{name} ,
	      $stat{$player}->{ptt} , $stat{$player}->{pdt} ,
	      $stat{$player}->{bat} ,
	      $stat{$player}->{kt} , $stat{$player}->{dt} ,
	      $stat{$player}->{acc} );

	write STDOUT;
    }

    print "\nPLAYER STATS for $awayteam\n";

    ( $name, $tpt, $tpd, $tab, $tek, $def, $acc ) =
    ( "Name" , "tpt" , "tpd" , "tab", "tek" , "def" , "acc" );

    write STDOUT;

    foreach $player ( sort keys %stat ) {

	next if $stat{$player}->{team} ne $awayrace;

	( $name, $tpt, $tpd, $tab, $tek, $def, $acc ) =
	    ( $stat{$player}->{name} ,
	      $stat{$player}->{ptt} , $stat{$player}->{pdt} ,
	      $stat{$player}->{bat} ,
	      $stat{$player}->{kt} , $stat{$player}->{dt} ,
	      $stat{$player}->{acc} );

	write STDOUT;
    }

} ### end sub printstdout


sub printstats {

    # After the log has been wholly parsed, get the information
    # (mostly from the %stat hash) and print it.

if ($neuts) {
    $planetscore = "${homecurrentplanets}-${awaycurrentplanets}-${neuts}";
} else {
    $planetscore = "${homecurrentplanets}-${awaycurrentplanets}";
}
print OUTPUT  <<END;
<BODY BGCOLOR="#FFFFFF" TEXT="#111100" LINK="#112299" ALINK="AA0000"
VLINK="#BB7711">

<title>PWStats: $hometeam ($homerace) -vs- $awaytem ($awayrace)</title>

<center>
<font size=+1>Game Ended: $endtimestr</font>
<br><br>Warning: The stats code is currently buggy. Planet counts and the winning team may be incorrect.
<br><br>
<table border cellpadding=2>
<tr>${th}<font size=+2>Home Team</font></th>${th}<font size=+2>Away Team</font></th></tr>
<tr><td><font size=+1><b>$hometeam</b></font> <font color=#444444 size=+1>($homerace)</font></td><td><font size=+1><b>$awayteam</b></font> <font color=#444444 size=+1>($awayrace)</font></td></tr>
</table>
<br>
    <table><tr><th><font size=+3>FINAL SCORE:&nbsp;</font></th>$th<font size=+3>$planetscore</font></th></tr></table>
<table><tr><td>Scoring Mode: <b>Planet Count</b> (11-8-1 or 8-11-1 required for victory)</td></tr></table>
END

if ($homecurrentplanets <= 8) {
    print OUTPUT "<h2>$awayteam (Away) <u>-defeats-</u> $hometeam (Home)!</h2>";
} elsif ($awaycurrentplanets <= 8) {
    print OUTPUT "<h2>$hometeam (Home) <u>-defeats-</u> $awayteam (Away)!</h2>";
} else {
    print OUTPUT "<h2>$hometeam (Home) <u>-ties-</u> $awayteam (Away).</h2>";
}
print OUTPUT "</center>";

# Change all undef values to 0's for printing

    @stats = ("name","playerslot","tpd","tpt","tpb","tab","tad","tac",
	      "cak","eao","eck","pck","tek","def","acc",
	      "sbtek","sbdef","sbtac","sbtad","sbcak","sbeao","sbeck","sbpck",
	      "kbp","kbt","kbe","kbd","kbs","dbp","dbt","dbe","dbd","dbs",
	      "dbpf","sbkbp","sbkbt","sbkbe","sbkbd","sbkbs","sbdbp",
	      "sbdbt","sbdbe","sbdbd","sbdbs");


foreach $value ( @stats ) {

    foreach $player ( sort keys %stat ) {
	if ( ! $stat{$player}->{$value} ) {
	    $stat{$player}->{$value} = 0;
	}
    }
}


# Go through printing the home team first, then their totals, then 
# the away team and their totals, then SB stats.

$count=1;

print OUTPUT  <<END;

<p>
<font size=+1>Download:</font> <a href=ltd_dump.txt>LTD Stats</a>&nbsp;|&nbsp;
<a href=INL_log.txt>INL Log</a>&nbsp;|&nbsp;<a href=cambot.pkt.gz>Game Recording (Cambot)</a>
<p>
<font size=+2>Player Stats</font> $key

<h4>Home Team</h4>

<table border cellpadding=4>
    <tr>${th}Name${th}Minutes${th}tpd${th}tpt${th}tpb${th}tab
    ${th}tad/tac/pad${th}cak${th}eao${th}eck${th}pck${th}tek${th}def${th}acc

<tr>

END

foreach $player ( sort keys %stat ) {

    next if $stat{$player}->{team} ne $homerace;

printrecord($player);

$home{tpd}+=$stat{$player}->{pdt};
$home{tpt}+=$stat{$player}->{ptt};
$home{tpb}+=$stat{$player}->{bpt};
$home{tab}+=$stat{$player}->{bat};
$home{tac}+=$stat{$player}->{at};
#
# Add armies used to attack, armies used to reinforce, and armies ferried 
# for tad
$home{tad}+=$stat{$player}->{aa} + $stat{$player}->{ar} + $stat{$player}->{af};
$home{cak}+=$stat{$player}->{ak};
$home{eao}+=$stat{$player}->{oat};
$home{eck}+=$stat{$player}->{odc};
$home{pck}+=$stat{$player}->{opc};
$home{tek}+=$stat{$player}->{kt};
$home{def}+=$stat{$player}->{dt};
$home{acc}+=$stat{$player}->{acc};

}

print OUTPUT  <<END;
<tr><td>
    ${th}Home Total
    ${th}$home{tpd}
${th}$home{tpt}
${th}$home{tpb}
${th}$home{tab}
${th}$home{tad}/$home{tac},
END

    if ( $home{tac} ) {
	$home{pad} = ($home{tad}/$home{tac})*100;
    } else {
	$home{pad} = 0;
    }
    printf OUTPUT ("%2.1f",$home{pad});

print OUTPUT  <<END;
%
${th}$home{cak}
${th}$home{eao}
${th}$home{eck}
${th}$home{pck}
${th}$home{tek}
${th}$home{def}
${th}$home{acc}

</table>
<p>
<h4>Away Team</h4>

<table border cellpadding=4>
    <tr>${th}Name${th}Minutes${th}tpd${th}tpt${th}tpb${th}tab
    ${th}tad/tac/pad${th}cak${th}eao${th}eck${th}pck${th}tek${th}def${th}acc

END

foreach $player ( sort keys %stat ) {

    next if $stat{$player}->{team} ne $awayrace;

printrecord($player);

$away{tpd}+=$stat{$player}->{pdt};
$away{tpt}+=$stat{$player}->{ptt};
$away{tpb}+=$stat{$player}->{bpt};
$away{tab}+=$stat{$player}->{bat};
$away{tac}+=$stat{$player}->{at};
#
# Add armies used to attack, armies used to reinforce, and armies ferried 
# for tad
$away{tad}+=$stat{$player}->{aa} + $stat{$player}->{ar} + $stat{$player}->{af};
$away{cak}+=$stat{$player}->{ak};
$away{eao}+=$stat{$player}->{oat};
$away{eck}+=$stat{$player}->{odc};
$away{pck}+=$stat{$player}->{opc};
$away{tek}+=$stat{$player}->{kt};
$away{def}+=$stat{$player}->{dt};
$away{acc}+=$stat{$player}->{acc};

}


print OUTPUT  <<END;
<tr><td>
${th}Away Total
${th}$away{tpd}
${th}$away{tpt}
${th}$away{tpb}
${th}$away{tab}
${th}$away{tad}/$away{tac},
END

    if ( $away{tac} ) {
	$away{pad} = ($away{tad}/$away{tac})*100;
    } else {
	$away{pad} = 0;
    }
    printf OUTPUT ("%2.1f",$away{pad});

print OUTPUT  <<END;
%
${th}$away{cak}
${th}$away{eao}
${th}$away{eck}
${th}$away{pck}
${th}$away{tek}
${th}$away{def}
${th}$away{acc}
END

print OUTPUT  <<END;
</table>
<p>
<h3>Normalized Player Stats</h3>
<i>(Statistics normalized to game length)</i>

<h4>Home Team</h4>

<table border cellpadding=4>
    <tr>${th}Name${th}Minutes${th}tpd${th}tpt${th}tpb${th}tab
    ${th}tad/tac/pad${th}cak${th}eao${th}eck${th}pck${th}tek${th}def${th}acc

<tr>
END

    foreach $player ( sort keys %stat ) {
	next if $stat{$player}->{team} ne $homerace;
	printnormalizedrecord($player);
    }

print OUTPUT <<END;
</table>
<p>
<h4>Away Team</h4>

<table border cellpadding=4>
    <tr>${th}Name${th}Min ${th}tpd${th}tpt${th}tpb${th}tab
    ${th}tad/tac/pad${th}cak${th}eao${th}eck${th}pck${th}tek${th}def${th}acc

END

    foreach $player ( sort keys %stat ) {
	next if $stat{$player}->{team} ne $awayrace;
	printnormalizedrecord($player);
    }

print OUTPUT <<END;
</table>
<p>

<h3>Starbase Stats</h3>
<table border cellpadding=4>
    <tr>${th}Name ${th}Min ${th}tek ${th}def ${th}tad/tac/pad ${th}cak
END

    foreach $player (sort keys %stat) {
	next if $stat{$player}->{ttsb} < 2;
	my $sbmin = int($stat{$player}->{ttsb}/($tickspersec*60))+1;
	my $sbtad = $stat{$player}->{atsb} - $stat{$player}->{aksb};

	if ( $stat{$player}->{atsb} > 0 ) {
	    my $sbpad = ($sbtac/$stat{$player}->{atsb}-$stat{$player}->{aksb}) * 100;
	} else {
	    my $sbpad = 0;
	}

	print OUTPUT <<END;
	<tr>	${td} $player ${td} $sbmin ${td} $stat{$player}->{ktsb}
${td} $stat{$player}->{dtsb} ${td} $sbtad/$stat{$player}->{atsb}
END

        printf OUTPUT ("%2.1f",$sbpad);
        print OUTPUT ${td} , $stat{$player}->{aksb};

    }


print OUTPUT <<END;

</table>
<p>

<h3>Kill Stats</h3>

<table border cellpadding=4>
<tr>${th}Name ${th}kbp ${th}kbt ${th}kbs
    ${th}dbp ${th}dbt ${th}dbs
END

foreach $player ( sort keys %stat ) {

print OUTPUT  <<END;
<tr>
    $td $stat{$player}->{name}
$td $stat{$player}->{kbp}
$td $stat{$player}->{kbt}
$td $stat{$player}->{kbs}
$td $stat{$player}->{dbp}
$td $stat{$player}->{dbt}
$td $stat{$player}->{dbs}
END
}

print OUTPUT <<END;
</table>
<p>
<h3>Starbase Kill Stats</h3>

<table border cellpadding=4>
<tr>${th}Name ${th}kbp ${th}kbt ${th}kbs
    ${th}dbp ${th}dbt ${th}dbs
END

    foreach $player ( sort keys %stat ) {

	next if $stat{$player}->{ktsb} < 1;

print OUTPUT  <<END;
<tr>
    $td $stat{$player}->{name}
$td $stat{$player}->{kbpsb}
$td $stat{$player}->{kbtsb}
$td $stat{$player}->{kbssb}
$td $stat{$player}->{dbpsb}
$td $stat{$player}->{dbtsb}
$td $stat{$player}->{dbssb}
END
	
    }

print OUTPUT "</table>\n";

} ### end sub printstats


sub printrecord {
    # Takes one argument, a player name, and prints the information in
    # tabular form.

    my $player=$_[0];

    $td = "<td align=right>";

#    my $minutes = int(($stat{$player}->{tt}-$stat{$player}->{ttsb})
    my $minutes = int(($stat{$player}->{tt})
		      /($tickspersec*60))+1;
    my $tad = $stat{$player}->{aa} + $stat{$player}->{ar} + $stat{$player}->{af};

print OUTPUT  <<END;
<tr>
    $td $stat{$player}->{name}
    $td $minutes
    $td $stat{$player}->{pdt}
    $td $stat{$player}->{ptt}
    $td $stat{$player}->{bpt}
    $td $stat{$player}->{bat}
    $td $tad/$stat{$player}->{at}
END

    if ( $stat{$player}->{at} > 0 ) {
	my $pad = $tad/$stat{$player}->{at}*100;
	printf OUTPUT ("%2.1f",$pad);
	print OUTPUT  "%";
    } else {
	print OUTPUT  "0.0%";
    }

print OUTPUT  <<END;

    $td $stat{$player}->{ak}
    $td $stat{$player}->{oat}
    $td $stat{$player}->{odc}
    $td $stat{$player}->{opc}
    $td $stat{$player}->{kt}
    $td $stat{$player}->{dt}
    $td $stat{$player}->{acc}
END

} ### end sub printrecord

sub printnormalizedrecord {
    # Takes one argument, a player name, and prints the information in
    # tabular form, normalized to the game length (actually the same number
    # of ticks as the highest player.

    my $player=$_[0];

    if ( ! $gamelength ) {
	# First time through, figure out the gamelength by looking at the
	# greatest number of ticks 

	foreach $player1 ( sort keys %stat ) {
	    if ( $stat{$player1}->{tt} > $gamelength ) {
		$gamelength = $stat{$player1}->{tt};
	    }
	}
    }

    $td = "<td align=right>";

#    my $minutes = int(($stat{$player}->{tt}-$stat{$player}->{ttsb})
    my $minutes = int(($stat{$player}->{tt})
		      /($tickspersec*60))+1;

    my $ticks = $stat{$player}->{tt};
    my %normal;
    $normal{tad} = normalize (($stat{$player}->{at}-$stat{$player}->{ak})
			      , $ticks);

    $normal{pdt} = normalize($stat{$player}->{pdt}, $ticks);
    $normal{ptt} = normalize($stat{$player}->{pdt}, $ticks);
    $normal{bpt} = normalize($stat{$player}->{bpt}, $ticks);
    $normal{bat} = normalize($stat{$player}->{bat}, $ticks);
    $normal{at} = normalize($stat{$player}->{at}, $ticks);
    $normal{ak} = normalize($stat{$player}->{ak}, $ticks);
    $normal{oat} = normalize($stat{$player}->{oat}, $ticks);
    $normal{odc} = normalize($stat{$player}->{odc}, $ticks);
    $normal{opc} = normalize($stat{$player}->{opc}, $ticks);
    $normal{kt} = normalize($stat{$player}->{kt}, $ticks);
    $normal{dt} = normalize($stat{$player}->{dt}, $ticks);
    $normal{acc} = normalize($stat{$player}->{acc}, $ticks);

print OUTPUT  <<END;
<tr>
    $td $stat{$player}->{name}
    $td $minutes
    $td $normal{pdt}
    $td $normal{ptt}
    $td $normal{bpt}
    $td $normal{bat}
    $td $normal{tad}/$normal{at}
END

    if ( $normal{at} > 0 ) {
	my $pad = $tad/$normal{at}*100;
	printf OUTPUT ("%2.1f",$pad);
	print OUTPUT  "%";
    } else {
	print OUTPUT  "0.0%";
    }

print OUTPUT  <<END;

    $td $normal{ak}
    $td $normal{oat}
    $td $normal{odc}
    $td $normal{opc}
    $td $normal{kt}
    $td $normal{dt}
    $td $normal{acc}
END

} ### end sub printnormalizedrecord


sub normalize {
    # Given a number and a number of ticks, returns 
    # number * (gamelength / ticks), for normalized stats.

    my ( $number, $ticks ) = @_;

    if ( $ticks == 0 ) {
	return 0;
    } else {
	my $value = int($number * ( $gamelength / $ticks )+0.5);
    }

} ### end sub normalize

sub getteam {
    # Given a single character, returns the team associated with it (in caps)

    my $character = $_[0];

    if ( $character eq "R" ) {
	return ("ROM");
    } elsif ( $character eq "F" ) {
	return ("FED");
    } elsif ( $character eq "K" ) {
	return ("KLI");
    } elsif ( $character eq "O" ) {
	return ("ORI");
    }
} ### end sub getteam


sub maillog {
    # Takes a completed log and mails it to statboys, along with the
    # INL log.

    if ( ! -e $mailprog ) {
	# they don't have /usr/lib/sendmail?  What the heck are they
	# using?  Let's try random shit:
	#
	if ( -e "/usr/sbin/sendmail" ) {
	    $mailprog = "/usr/sbin/sendmail";
	} elsif ( -e "/usr/bin/sendmail" ) {
	    $mailprog = "/usr/bin/sendmail";
	} elsif ( -e "/usr/bin/mailx" ) {
	    $mailprog = "/usr/bin/mailx"; 
	} elsif ( -e "/usr/bin/mail" ) {
	    $mailprog = "/usr/bin/mail"; 
	} elsif ( -e "/bin/mail" ) {
	    $mailprog = "/bin/mail";
	} else {
	    die "I give up, I can't find a mail program";
	}
    }

    open (MAIL,"|$mailprog -t");
    print MAIL "Subject: INL RESULTS: $awayteam at $hometeam\n";
    print MAIL "To: " , $statboys , "\n\n";

    open (PWSTATS,"$outputfile");
    while ( $line = <PWSTATS> ) {
	print MAIL $line;
    }
    print MAIL "--- end PWstats ---\n";
    close PWSTATS;

    open (LTDSTATS,"$inputfile");
    while ( $line = <LTDSTATS> ) {
	print MAIL $line;
    }
    print MAIL "--- end INL log ---\n";
    close LTDSTATS;

    open (INLLOG,"$logfile");
    while ( $line = <INLLOG> ) {
	print MAIL $line;
    }
    print MAIL "--- end INL log ---\n";
    close INLLOG;
    close MAIL;

}



