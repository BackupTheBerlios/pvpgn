#!/usr/bin/perl


require "db_api.pl";

if (scalar(@ARGV) != 5) {
    &usage;
    exit;
}

&header;

# remove a trailing /
$dirpath = $ARGV[0];
$dirpath =~ s!(.*)/$!$1!;

$max_uid = -1;

$dbh = &db_init($ARGV[1], $ARGV[2], $ARGV[3], $ARGV[4]);

opendir FILEDIR, $dirpath or die "Error opening filedir!\n";

while ($filename = readdir FILEDIR) {
    if ($filename =~ m/^\./) { next; } #ignore . and ..

    
    if ( ! (($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
    $atime,$mtime,$ctime,$blksize,$blocks) = stat "$dirpath/$filename" )) {
        print "Error stat-ing the file $pathdir/$filename!\n" ; next; }
    
    $type = ($mode & 070000) >> 12;
    
    if ($type != 0) {
	print "File $dirpath/$filename its not regular\n";
	next;
    }
    convertfile2db("$dirpath/$filename");
}

closedir FILEDIR;

&db_maxuid($max_uid);

sub convertfile2db {
    my $filen = shift;
    my ($userid, $count, $alist);
    
    open FILE, $filen or die "Error opening file $filen\n";
    print "Converting file $filen ... ";
    
    $count = 0;
    $userid = ""; undef @alist;
    while($line = <FILE>) {
	if ($line =~ m/".*"=".*"/) {

	    $line =~ m/^"([A-Za-z0-9]+)\\\\(.*)"="(.*)"/;

	    $alist[$count]{tab} = $1;
	    $alist[$count]{col} = $2;
	    $alist[$count]{val} = $3;

	    $alist[$count]{col} =~ s!\\\\!_!g;

	    if ($alist[$count]{col} =~ m!userid$!) {
		$userid = $alist[$count]{val};
	    }
	    $count++;
	}
    }
    if ($userid ne "") {
	&db_set($dbh, $userid, $alist);
	if ($userid > $max_uid) {
	    $max_uid = $userid;
	}
    }
    close FILE;
    print "done\n";
}

sub header {
    print "Account files to db accounts converting tool.\n";
    print " Copyright (C) 2002 Dizzy (dizzy\@roedu.net)\n";
    print " People Versus People Gaming Network (www.pvpgn.org)\n\n";
}

sub usage {
    &header;
    print "Usage:\n\n\tfiles2db.pl <filedir> <dbhost> <dbname> <dbuser> <dbpass>\n\n";
    print "\t <filedir>\t: directory with the account files\n";
    print "\t <dbhost>\t: the database host\n";
    print "\t <dbname>\t: the database name\n";
    print "\t <dbuser>\t: the database user\n";
    print "\t <dbpass>\t: the database password\n";
}
