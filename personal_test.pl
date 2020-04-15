#!/usr/bin/perl
use 5.16.0;
use warnings FATAL => 'all';

use Test::Simple tests => 42;
use IO::Handle;
use Data::Dumper;

sub mount {
    system("(make mount 2>> error.log) >> test.log &");
    sleep 1;
}

sub unmount {
    system("(make unmount 2>> error.log) >> test.log");
}

sub write_text {
    my ($name, $data) = @_;
    open my $fh, ">", "mnt/$name" or return;
    $fh->say($data);
    close $fh;
}

sub read_text {
    my ($name) = @_;
    open my $fh, "<", "mnt/$name" or return "";
    local $/ = undef;
    my $data = <$fh> || "";
    close $fh;
    $data =~ s/\s*$//;
    return $data;
}

sub read_text_slice {
    my ($name, $count, $offset) = @_;
    open my $fh, "<", "mnt/$name" or return "";
    my $data;
    seek $fh, $offset, 0;
    read $fh, $data, $count;
    close $fh;
    return $data;
}

sub write_text_off {
    my ($name, $offset, $data) = @_;
    open my $fh, "+<", "mnt/$name" or return "";
    seek $fh, $offset, 0;
    syswrite $fh, $data;
    close $fh;
}

#system("rm -f data.cow test.log");
system("(make 2>error.log) > test.log");
#system("./cowtool new data.cow");

say "#           == Basic Tests ==";

my $part1 = 0;

sub p1ok {
    my ($cond, $msg) = @_;
    if ($cond) {
        ++$part1;
    }
    else {
        ok(0, $msg);
    }
}


write_text("1.txt", "111");
write_text("2.txt", "222");
write_text("3.txt", "333");

system("mkdir mnt/a");
write_text("a/4.txt", "444");
write_text("a/5.txt", "555");


my $huge0 = "=This string is fourty characters long.=" x 1000;
write_text("40k.txt", $huge0);
my $huge1 = read_text("40k.txt");
ok($huge0 eq $huge1, "Read back 40k correctly.");

my $huge2 = read_text_slice("40k.txt", 10, 8050);
my $right = "ng is four";
ok($huge2 eq $right, "Read with offset & length");

my $nfiles = 90;


system("mkdir mnt/numbers");
for my $ii (1..$nfiles) {
    write_text("numbers/$ii.num", "$ii");
}


my $nn = `ls mnt/numbers | wc -l`;
say "# nn = '$nn'";

ok($nn == $nfiles, "created '$nfiles' files");

my $fileincr = $nfiles / 5;

for my $ii (1..$fileincr) {
    my $xx = $ii * 5;
    my $yy = read_text("numbers/$xx.num") || -10;
    ok($xx == $yy, "check value $xx");
}

#unmount();

#mount();

# system("mkdir mnt/numbersb");
# for my $ii (1..$nfiles) {
#     write_text("numbersb/$ii.num", "$ii");
# }


# my $mm = `ls mnt/numbersb | wc -l`;
# say "# mm = '$mm'";

# ok($mm == $nfiles, "created '$nfiles' files");


# for my $ii (1..$fileincr) {
#     my $xx = $ii * 5;
#     my $yy = read_text("numbersb/$xx.num") || -10;
#     ok($xx == $yy, "check value $xx");
# }


# for my $ii (1..5) {
#     my $xx = $ii * 2;
#     system("rm mnt/numbers/$xx.num");
# }
