#!/usr/bin/perl

$bncdb = "bncdb";

if (scalar(@ARGV) != 1) {
    &usage;
    exit;
}

&header;

# remove a trailing /
$dirpath = $ARGV[0];
$dirpath =~ s!(.*)/$!$1!;

opendir FILEDIR, $dirpath or die "Error opening filedir!\n";

while ($filename = readdir FILEDIR) {
    if ($filename =~ m/^\./) { next; } #ignore . and ..

    if ( ! (($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
    $atime,$mtime,$ctime,$blksize,$blocks) = stat "$dirpath/$filename" )) {
        print STDERR "Error stat-ing the file $pathdir/$filename!\n" ; next; }

    $type = ($mode & 070000) >> 12;
    
    if ($type != 0) {
	print STDERR "File $dirpath/$filename its not regular\n";
	next;
    }
    convert_cdb2sql("$dirpath/$filename");
}

closedir FILEDIR;

sub convert_cdb2sql {
    my $filen = shift;
    my ($userid, $count, $alist);

    $pid  = open(FILE, "$bncdb -d \"".$filen."\" |");
    print STDERR "Converting file $filen ... ";

    $count = 0;
    $userid = ""; undef @alist;
    while($line = <FILE>) {
	if ($line =~ m/:.*->.*/) {
	    $line =~ m/^\+[0-9]+,[0-9]+:([A-Za-z0-9]+)\\(.*)->(.*)$/;

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
    }
    close FILE;
    waitpid $pid, 0;
    print STDERR "done\n";
}

sub header {
    print STDERR "Account files to db accounts converting tool.\n";
    print STDERR " Copyright (C) 2002,2005 Dizzy (dizzy\@roedu.net)\n";
    print STDERR " Player Versus Player Gaming Network (www.pvpgn.org)\n\n";
}

sub usage {
    &header;
    print STDERR "Usage:\n\n\tcdb2sql.pl <filedir>\n\n";
    print STDERR "\t <filedir>\t: directory with the account files\n";
}

sub db_set {
    my $dbh = shift;
    my $userid = shift;
    my $alist = shift;
    my ($i);

    print("INSERT INTO BNET (uid) VALUES ($userid);\n");
    print("INSERT INTO profile (uid) VALUES ($userid);\n");
    print("INSERT INTO Record (uid) VALUES ($userid);\n");
    print("INSERT INTO friend (uid) VALUES ($userid);\n");
    for($i=0; $i<scalar(@alist);$i++) {
        my $tab = $alist[$i]{tab};
        my $col = $alist[$i]{col};
        my $val = $alist[$i]{val};

        $nval = &add_slashes($val);
        print("ALTER TABLE $tab ADD COLUMN '$col' VARCHAR(128);\n");
        print("UPDATE $tab SET '$col' = '$nval' WHERE uid = $userid;\n");
    }
}

sub add_slashes {
    my $str = shift;

    $str =~ s/\\/\\\\/g;
    $str =~ s/\'/\\\'/g;

    return $str;
}
