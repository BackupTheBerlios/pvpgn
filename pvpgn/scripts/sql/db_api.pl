#!/usr/bin/perl

# A MySQL module for converting accounts

use DBI;

sub db_init {
    my $dbhost = shift;
    my $dbname = shift;
    my $dbuser = shift;
    my $dbpass = shift;

    $dbh = DBI->connect("DBI:mysql:$dbname;host=$dbhost", $dbuser, $dbpass) or die "Error connectding to DB!\n";
    
    return $dbh;
}

sub db_set {
    my $dbh = shift;
    my $userid = shift;
    my $alist = shift;
    my ($i);

    $dbh->do("INSERT INTO BNET (uid) VALUES ($userid)");
    $dbh->do("INSERT INTO profile (uid) VALUES ($userid)");
    $dbh->do("INSERT INTO Record (uid) VALUES ($userid)");
    $dbh->do("INSERT INTO friend (uid) VALUES ($userid)");
    $dbh->do("INSERT INTO Team (uid) VALUES ($userid)");
    for($i=0; $i<scalar(@alist);$i++) {
	my $tab = $alist[$i]{tab};
	my $col = $alist[$i]{col};
	my $val = $alist[$i]{val};
	
	$nval = &add_slashes($val);
	$sth = $dbh->prepare("SHOW FIELDS FROM $tab LIKE '$col'");
	$sth->execute();
	if ($sth->rows < 1) {
	    $sth->finish;
	    $rv = $dbh->do("ALTER TABLE $tab ADD COLUMN `$col` VARCHAR(128)") or die "Erorr while doing ALTER!\n";
	}
	$query = "UPDATE $tab SET `$col` = '$nval' WHERE uid = $userid";
#	print "$query\n";
	$rv = $dbh->do($query) or die "Erorr while doing UPDATE!\n"
    }
}

sub add_slashes {
    my $str = shift;
    
    $str =~ s/\\/\\\\/g;
    $str =~ s/\'/\\\'/g;
    
    return $str;
}


return 1;
