#!/opt/local/bin/perl

#   This script is part of
#   Continuous Imaging 'fly'
#   Copyright (C) 2008-2012  David Lowy & Tom Rathborne
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.

# This script merges the _lists/* metapixel list files into cidb.db.
# * is the list of filenames given on input <>.

use strict;
use warnings;

use Data::SExpression;
use DB_File;

print "--- Making CIDB\n";

my %NUM = ();
my $serial = 0;

# assign serial numbers
while (<>) {
    chomp;
    $NUM{$_} = ++$serial; # start at 1
}

tie my %DBF, 'DB_File', 'cidb.db', O_RDWR|O_CREAT or die 'output cidb.db unopen/creatable';
my $maxlen = 0;

foreach my $name (sort keys %NUM) {
    my $num = $NUM{$name};
    printf("Reading $name #$num: ");

    open my $LISPF, "<_lists/$name.jpg" or die "input file _lists/$name unreadable";
    my $data = '';
    # read data
    while (<$LISPF>) {
        chomp;
        s/\s*;.*$//;
        $data .= $_;
    }
    close $LISPF;

    my $ds = new Data::SExpression;
    my @list = @{($ds->read($data))->[2]};
    shift @list;

    # process data

    my @subs = ();
    my $sx = 0;
    my $sy = 0;

    for my $T (@list) {
        my $x = $T->[0];
        my $y = $T->[1];
        my $sname = $T->[4];

        $sx = $x if $x > $sx;
        $sy = $y if $y > $sy;

        $sname =~ s/^_library\/(.+)\.jpg\.png$/$1/;
        $subs[$y][$x] = $NUM{$sname} || die "Not found: $sname";
    }

    $sx++;
    $sy++;

    # write DBF
    my $value =
        pack('S!2', $sx, $sy) # 4 bytes
      . join('', map { pack("S!$sy", @{$subs[$_]}) } 0 .. $#subs) # e.g. 25*25*2 = 1250 bytes
      . "$name\0"; # e.g. 'foo\0' = 4 bytes

    my $length = length($value);
    print "$length bytes\n";
    $maxlen = $length if $length > $maxlen;

    $DBF{pack('S!', $num)} = $value;
}

$DBF{pack('S!', 0)} = pack('S!2', $serial, $maxlen); # record count and maxbytes
untie %DBF;

print "--- Done Making CIDB\n";

